import { randomBytes, randomInt } from "node:crypto";
import { createServer as createHttpServer } from "node:http";
import { pathToFileURL } from "node:url";

import WebSocket, { WebSocketServer } from "ws";

const protocolVersion = 1;
const maximumPlayers = 8;
const maximumMessageSize = 128 * 1024;
const roomAlphabet = "ABCDEFGHJKLMNPQRSTWXYZ23456789";
const roomCodeLength = 6;

function positiveInteger(value, name) {
  const parsed = Number.parseInt(String(value), 10);
  if (!Number.isSafeInteger(parsed) || parsed <= 0)
    throw new Error(`${name} must be a positive integer`);
  return parsed;
}

function randomText(length, alphabet = roomAlphabet) {
  let result = "";
  for (let index = 0; index < length; ++index)
    result += alphabet[randomInt(alphabet.length)];
  return result;
}

function send(socket, message) {
  if (socket.readyState === WebSocket.OPEN)
    socket.send(JSON.stringify(message));
}

function rejectUpgrade(socket, status, message) {
  const names = {
    400: "Bad Request",
    404: "Not Found",
    409: "Conflict",
    429: "Too Many Requests",
  };
  const body = JSON.stringify({ error: message });
  socket.end(
    `HTTP/1.1 ${status} ${names[status] || "Error"}\r\n` +
      "Connection: close\r\n" +
      "Content-Type: application/json; charset=utf-8\r\n" +
      `Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`,
  );
}

function clientAddress(request, trustProxy) {
  if (trustProxy) {
    const forwarded = request.headers["x-forwarded-for"];
    if (typeof forwarded === "string" && forwarded.length <= 256)
      return forwarded.split(",", 1)[0].trim();
  }
  return request.socket.remoteAddress || "unknown";
}

export function createSignalingServer(options) {
  const stunHost = String(options.stunHost || "").trim();
  const rateLimitPerMinute = positiveInteger(
    options.rateLimitPerMinute ?? 30,
    "rateLimitPerMinute",
  );
  const trustProxy = Boolean(options.trustProxy);

  if (!/^[A-Za-z0-9.-]+$/.test(stunHost))
    throw new Error(
      "stunHost must be a DNS name or IPv4 address without a scheme or port",
    );

  const rooms = new Map();
  const connections = new WeakMap();
  const rateLimits = new Map();
  const webSockets = new WebSocketServer({
    noServer: true,
    maxPayload: maximumMessageSize,
  });

  function iceServers() {
    return [{ urls: [`stun:${stunHost}:3478`] }];
  }

  function permit(address) {
    const minute = Math.floor(Date.now() / 60000);
    const current = rateLimits.get(address);
    if (!current || current.minute !== minute) {
      rateLimits.set(address, { minute, count: 1 });
      return true;
    }
    if (current.count >= rateLimitPerMinute) return false;
    current.count += 1;
    return true;
  }

  function removeConnection(socket) {
    const connection = connections.get(socket);
    if (!connection) return;
    connections.delete(socket);
    const room = rooms.get(connection.code);
    if (!room) return;

    if (connection.role === "host" && room.host === socket) {
      rooms.delete(connection.code);
      for (const peer of room.members.values()) {
        if (peer !== socket && peer.readyState < WebSocket.CLOSING)
          peer.close(4001, "Host left the room");
      }
      room.members.clear();
      return;
    }

    room.members.delete(connection.id);
    if (room.host?.readyState === WebSocket.OPEN)
      send(room.host, { type: "peer-left", id: connection.id });
  }

  function receive(socket, payload, binary) {
    if (binary) {
      socket.close(1003, "Text signaling messages required");
      return;
    }

    let message;
    try {
      message = JSON.parse(payload.toString());
    } catch {
      send(socket, { type: "error", message: "Malformed JSON" });
      return;
    }
    if (!message || message.type !== "signal" || typeof message.to !== "string")
      return;
    if (!["offer", "answer", "candidate"].includes(message.signalType)) return;

    const sender = connections.get(socket);
    const room = sender && rooms.get(sender.code);
    const destination = room?.members.get(message.to);
    const receiver = destination && connections.get(destination);
    if (!sender || !receiver || sender.role === receiver.role) return;

    const forwarded = {
      type: "signal",
      from: sender.id,
      signalType: message.signalType,
    };
    if (message.signalType === "candidate") {
      if (
        typeof message.candidate !== "string" ||
        typeof message.mid !== "string"
      )
        return;
      forwarded.candidate = message.candidate;
      forwarded.mid = message.mid;
    } else {
      if (typeof message.description !== "string") return;
      forwarded.description = message.description;
    }
    send(destination, forwarded);
  }

  function admit(socket, role, code) {
    let room;
    if (role === "host") {
      room = { host: socket, members: new Map() };
      rooms.set(code, room);
    } else {
      room = rooms.get(code);
      if (
        !room ||
        room.host.readyState !== WebSocket.OPEN ||
        room.members.size >= maximumPlayers
      ) {
        socket.close(4004, room ? "Room is full" : "Room not found");
        return;
      }
    }

    const id = randomBytes(12).toString("hex");
    const hostConnection =
      role === "host" ? { id } : connections.get(room.host);
    const connection = { id, role, code };
    connections.set(socket, connection);
    room.members.set(id, socket);
    socket.isAlive = true;
    socket.on("pong", () => {
      socket.isAlive = true;
    });
    socket.on("message", (payload, binary) => receive(socket, payload, binary));
    socket.on("close", () => removeConnection(socket));
    socket.on("error", () => {});

    send(socket, {
      type: "welcome",
      version: protocolVersion,
      id,
      hostId: hostConnection.id,
      code,
      iceServers: iceServers(),
    });
    if (role === "guest") send(room.host, { type: "peer-joined", id });
  }

  const server = createHttpServer((request, response) => {
    if (request.method === "GET" && request.url === "/health") {
      response.writeHead(200, {
        "content-type": "text/plain; charset=utf-8",
        "cache-control": "no-store",
      });
      response.end(`Protocol version: ${protocolVersion}\n`);
      return;
    }
    response.writeHead(404, {
      "content-type": "application/json; charset=utf-8",
    });
    response.end(JSON.stringify({ error: "Not found" }));
  });

  server.on("upgrade", (request, socket, head) => {
    if (!permit(clientAddress(request, trustProxy))) {
      rejectUpgrade(socket, 429, "Too many room requests");
      return;
    }

    let route;
    try {
      route = new URL(request.url, "http://localhost").pathname.match(
        /^\/v1\/rooms\/(host|[A-HJ-NP-TW-Z2-9]{6})$/i,
      );
    } catch {
      route = null;
    }
    if (!route) {
      rejectUpgrade(socket, 404, "Not found");
      return;
    }

    const createsRoom = route[1] === "host";
    let code = createsRoom
      ? randomText(roomCodeLength)
      : route[1].toUpperCase();
    if (createsRoom) while (rooms.has(code)) code = randomText(roomCodeLength);

    const room = rooms.get(code);
    if (!createsRoom && !room) {
      rejectUpgrade(socket, 404, "Room not found");
      return;
    }
    if (!createsRoom && room.members.size >= maximumPlayers) {
      rejectUpgrade(socket, 409, "Room is full");
      return;
    }

    webSockets.handleUpgrade(request, socket, head, (webSocket) => {
      webSockets.emit("connection", webSocket, request);
      admit(webSocket, createsRoom ? "host" : "guest", code);
    });
  });

  const heartbeat = setInterval(() => {
    for (const socket of webSockets.clients) {
      if (!socket.isAlive) {
        socket.terminate();
        continue;
      }
      socket.isAlive = false;
      socket.ping();
    }
    const oldestMinute = Math.floor(Date.now() / 60000) - 1;
    for (const [address, window] of rateLimits)
      if (window.minute < oldestMinute) rateLimits.delete(address);
  }, 30000);
  heartbeat.unref();

  async function close() {
    clearInterval(heartbeat);
    for (const socket of webSockets.clients) socket.terminate();
    await new Promise((resolve) => webSockets.close(resolve));
    if (server.listening)
      await new Promise((resolve, reject) =>
        server.close((error) => (error ? reject(error) : resolve())),
      );
  }

  return { server, close, rooms };
}

function optionsFromEnvironment() {
  return {
    stunHost: process.env.STUN_HOST,
    rateLimitPerMinute: process.env.RATE_LIMIT_PER_MINUTE || 30,
    trustProxy: process.env.TRUST_PROXY === "1",
  };
}

if (
  process.argv[1] &&
  import.meta.url === pathToFileURL(process.argv[1]).href
) {
  try {
    const service = createSignalingServer(optionsFromEnvironment());
    const port = positiveInteger(process.env.PORT || 8080, "PORT");
    const host = process.env.HOST || "0.0.0.0";
    service.server.listen(port, host, () =>
      console.log(`Abuse signaling listening on ${host}:${port}`),
    );
    const stop = async () => {
      await service.close();
      process.exit(0);
    };
    process.once("SIGINT", stop);
    process.once("SIGTERM", stop);
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
