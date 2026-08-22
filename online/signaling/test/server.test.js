import assert from "node:assert/strict";
import { once } from "node:events";
import test from "node:test";

import WebSocket from "ws";

import { createSignalingServer } from "../src/server.js";

async function startService(overrides = {}) {
  const service = createSignalingServer({
    stunHost: "stun.example.test",
    ...overrides,
  });
  service.server.listen(0, "127.0.0.1");
  await once(service.server, "listening");
  const { port } = service.server.address();
  return { ...service, url: `ws://127.0.0.1:${port}` };
}

async function connect(url) {
  const socket = new WebSocket(url);
  const messages = [];
  const waiters = [];
  socket.on("message", (payload) => {
    const message = JSON.parse(payload.toString());
    const waiter = waiters.shift();
    if (waiter) waiter(message);
    else messages.push(message);
  });
  await once(socket, "open");
  return {
    socket,
    messages,
    nextMessage() {
      if (messages.length) return Promise.resolve(messages.shift());
      return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error("Timed out waiting for WebSocket message")), 2000);
        waiters.push((message) => {
          clearTimeout(timer);
          resolve(message);
        });
      });
    },
  };
}

test("creates rooms, supplies only STUN, and forwards signaling", async (t) => {
  const service = await startService();
  t.after(() => service.close());

  const host = await connect(`${service.url}/v1/rooms/host`);
  const hostWelcome = await host.nextMessage();
  assert.equal(hostWelcome.type, "welcome");
  assert.match(hostWelcome.code, /^[A-HJ-NP-TW-Z2-9]{6}$/);
  assert.equal(hostWelcome.hostId, hostWelcome.id);
  assert.deepEqual(hostWelcome.iceServers, [{ urls: ["stun:stun.example.test:3478"] }]);

  const guest = await connect(`${service.url}/v1/rooms/${hostWelcome.code.toLowerCase()}`);
  const guestWelcome = await guest.nextMessage();
  assert.equal(guestWelcome.hostId, hostWelcome.id);
  assert.deepEqual(await host.nextMessage(), { type: "peer-joined", id: guestWelcome.id });

  host.socket.send(
    JSON.stringify({
      type: "signal",
      to: guestWelcome.id,
      signalType: "offer",
      description: "test-sdp",
    }),
  );
  assert.deepEqual(await guest.nextMessage(), {
    type: "signal",
    from: hostWelcome.id,
    signalType: "offer",
    description: "test-sdp",
  });

  const guestClosed = once(guest.socket, "close");
  host.socket.close();
  const [code] = await guestClosed;
  assert.equal(code, 4001);
});

test("does not forward signaling from one guest to another", async (t) => {
  const service = await startService();
  t.after(() => service.close());

  const host = await connect(`${service.url}/v1/rooms/host`);
  const hostWelcome = await host.nextMessage();
  const first = await connect(`${service.url}/v1/rooms/${hostWelcome.code}`);
  const firstWelcome = await first.nextMessage();
  await host.nextMessage();
  const second = await connect(`${service.url}/v1/rooms/${hostWelcome.code}`);
  const secondWelcome = await second.nextMessage();
  await host.nextMessage();

  first.socket.send(
    JSON.stringify({
      type: "signal",
      to: secondWelcome.id,
      signalType: "answer",
      description: "must-not-arrive",
    }),
  );
  await new Promise((resolve) => setTimeout(resolve, 50));
  assert.deepEqual(second.messages, []);
  assert.notEqual(firstWelcome.id, secondWelcome.id);

  host.socket.close();
});

test("rejects unknown rooms before upgrading the connection", async (t) => {
  const service = await startService();
  t.after(() => service.close());

  const socket = new WebSocket(`${service.url}/v1/rooms/ABC234`);
  const [, response] = await once(socket, "unexpected-response");
  assert.equal(response.statusCode, 404);
  response.resume();
});

test("rejects room codes outside the six-character alphabet", async (t) => {
  const service = await startService();
  t.after(() => service.close());

  for (const code of ["ABCDEFGH", "ABCU23", "ABCV23"]) {
    const socket = new WebSocket(`${service.url}/v1/rooms/${code}`);
    const [, response] = await once(socket, "unexpected-response");
    assert.equal(response.statusCode, 404);
    response.resume();
  }
});
