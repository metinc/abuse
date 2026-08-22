/*
 * WebRTC transport for Abuse room-code multiplayer.
 *
 * Signaling is intentionally provider-neutral JSON over WebSocket. The
 * matching self-hosted service lives in online/signaling.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "webrtc.h"

#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace
{
constexpr char datagram_label[] = "abuse-udp-v1";
constexpr char stream_label_prefix[] = "abuse-stream-v1:";
constexpr size_t datagram_header_size = 4;
constexpr size_t write_high_water_mark = size_t{256} * 1024;
constexpr auto signaling_timeout = 20s;
constexpr auto peer_timeout = 30s;
std::atomic<int> next_socket_id{100000};

class webrtc_address final : public net_address
{
  public:
    std::string peer;
    uint16_t port;

    explicit webrtc_address(std::string peer, const uint16_t port = 0) : peer(std::move(peer)), port(port)
    {
    }

    protocol protocol_type() const override
    {
        return OTHER;
    }
    int equal(const net_address *who) const override
    {
        const auto *other = dynamic_cast<const webrtc_address *>(who);
        return other && other->peer == peer;
    }
    int set_port(const int value) override
    {
        if (value < 0 || value > std::numeric_limits<uint16_t>::max())
            return 0;
        port = static_cast<uint16_t>(value);
        return 1;
    }
    int get_port() override
    {
        return port;
    }
    void print() override
    {
        DEBUG_LOG("webrtc:%s:%u", peer.c_str(), static_cast<unsigned>(port));
    }
    net_address *copy() override
    {
        return new webrtc_address(peer, port);
    }
    void store_string(char *out, const int length) override
    {
        if (out && length > 0)
            std::snprintf(out, length, "webrtc:%s:%u", peer.c_str(), static_cast<unsigned>(port));
    }
};

struct stream_state
{
    std::mutex mutex;
    std::condition_variable changed;
    std::shared_ptr<rtc::DataChannel> channel;
    std::deque<rtc::binary> input;
    size_t input_offset{0};
    bool open{false};
    bool closed{false};
};

struct pending_stream
{
    std::shared_ptr<stream_state> state;
    std::string peer;
};

struct listener_state
{
    std::mutex mutex;
    std::deque<pending_stream> pending;
    bool closed{false};
};

struct datagram_packet
{
    rtc::binary data;
    std::string peer;
    uint16_t source_port{0};
};

struct datagram_state
{
    std::mutex mutex;
    std::deque<datagram_packet> input;
    uint16_t local_port{0};
    bool closed{false};
    bool failed{false};
};

struct peer_state
{
    std::string id;
    std::shared_ptr<rtc::PeerConnection> connection;
    std::shared_ptr<rtc::DataChannel> datagram;
    bool failed{false};
};

bool begins_with(const std::string &text, const char *prefix)
{
    return text.compare(0, std::strlen(prefix), prefix) == 0;
}

std::string random_id(const size_t length)
{
    static constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937_64 random{std::random_device{}()};
    std::uniform_int_distribution<size_t> pick(0, sizeof(alphabet) - 2);
    std::string result(length, '0');
    for (char &c : result)
        c = alphabet[pick(random)];
    return result;
}

uint16_t read_u16(const rtc::binary &data, const size_t offset)
{
    return static_cast<uint16_t>(std::to_integer<uint8_t>(data[offset])) |
           static_cast<uint16_t>(std::to_integer<uint8_t>(data[offset + 1]) << 8);
}

void append_u16(rtc::binary &data, const uint16_t value)
{
    data.push_back(static_cast<std::byte>(value & 0xff));
    data.push_back(static_cast<std::byte>((value >> 8) & 0xff));
}

void bind_stream_channel(const std::shared_ptr<stream_state> &state, const std::shared_ptr<rtc::DataChannel> &channel)
{
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->channel = channel;
        state->open = channel->isOpen();
    }
    const std::weak_ptr<stream_state> weak = state;
    channel->onOpen([weak]() {
        if (const auto state = weak.lock())
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->open = true;
            state->changed.notify_all();
        }
    });
    channel->onClosed([weak]() {
        if (const auto state = weak.lock())
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->closed = true;
            state->open = false;
            state->changed.notify_all();
        }
    });
    channel->onError([weak](const std::string &) {
        if (const auto state = weak.lock())
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->closed = true;
            state->open = false;
            state->changed.notify_all();
        }
    });
    channel->onMessage([weak](rtc::message_variant message) {
        if (const auto state = weak.lock())
        {
            rtc::binary bytes;
            if (std::holds_alternative<rtc::binary>(message))
                bytes = std::get<rtc::binary>(std::move(message));
            else
            {
                const std::string &text = std::get<std::string>(message);
                bytes.resize(text.size());
                std::memcpy(bytes.data(), text.data(), text.size());
            }
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->closed)
                state->input.push_back(std::move(bytes));
            state->changed.notify_all();
        }
    });
}

struct parsed_ice_url
{
    std::string host;
    uint16_t port{3478};
};

bool parse_ice_url(const std::string &url, parsed_ice_url &result)
{
    std::string rest;
    if (begins_with(url, "stun:"))
        rest = url.substr(5);
    else
        return false;

    const size_t query = rest.find('?');
    rest.resize(query == std::string::npos ? rest.size() : query);

    if (!rest.empty() && rest.front() == '[')
    {
        const size_t close = rest.find(']');
        if (close == std::string::npos)
            return false;
        result.host = rest.substr(1, close - 1);
        if (close + 1 < rest.size() && rest[close + 1] == ':')
            result.port = static_cast<uint16_t>(std::stoi(rest.substr(close + 2)));
    }
    else
    {
        const size_t colon = rest.rfind(':');
        if (colon != std::string::npos)
        {
            result.host = rest.substr(0, colon);
            result.port = static_cast<uint16_t>(std::stoi(rest.substr(colon + 1)));
        }
        else
            result.host = rest;
    }
    return !result.host.empty();
}

rtc::Configuration make_rtc_configuration(const json &servers)
{
    rtc::Configuration configuration;
    if (servers.is_array())
    {
        for (const auto &server : servers)
        {
            std::vector<std::string> urls;
            if (server.contains("urls") && server["urls"].is_string())
                urls.push_back(server["urls"].get<std::string>());
            else if (server.contains("urls") && server["urls"].is_array())
                urls = server["urls"].get<std::vector<std::string>>();

            for (const std::string &url : urls)
            {
                try
                {
                    parsed_ice_url parsed;
                    if (!parse_ice_url(url, parsed))
                        continue;
                    configuration.iceServers.emplace_back(parsed.host, parsed.port);
                }
                catch (const std::exception &)
                {
                    DEBUG_LOG("Ignoring malformed ICE server URL: %s", url.c_str());
                }
            }
        }
    }
    if (configuration.iceServers.empty())
        throw std::runtime_error("signaling server did not provide an ICE server");
    return configuration;
}
} // namespace

class webrtc_socket_base : public net_socket
{
  protected:
    webrtc_protocol::impl *owner;
    int socket_id{next_socket_id++};
    bool read_selected{false};
    bool write_selected{false};

  public:
    explicit webrtc_socket_base(webrtc_protocol::impl *owner);
    ~webrtc_socket_base() override;
    int get_fd() override
    {
        return socket_id;
    }
    void read_selectable() override
    {
        read_selected = true;
    }
    void read_unselectable() override
    {
        read_selected = false;
    }
    void write_selectable() override
    {
        write_selected = true;
    }
    void write_unselectable() override
    {
        write_selected = false;
    }
    bool selected_ready()
    {
        return (read_selected && ready_to_read()) || (write_selected && ready_to_write()) || error();
    }
};

class webrtc_stream_socket final : public webrtc_socket_base
{
    std::shared_ptr<stream_state> state;

  public:
    webrtc_stream_socket(webrtc_protocol::impl *owner, std::shared_ptr<stream_state> state)
        : webrtc_socket_base(owner), state(std::move(state))
    {
    }
    ~webrtc_stream_socket() override;
    int error() override;
    int ready_to_read() override;
    int ready_to_write() override;
    int write(void const *buffer, int size, net_address *address = nullptr) override;
    int read(void *buffer, int size, net_address **address = nullptr) override;
};

class webrtc_server_socket final : public webrtc_socket_base
{
    std::shared_ptr<listener_state> state;

  public:
    webrtc_server_socket(webrtc_protocol::impl *owner, std::shared_ptr<listener_state> state)
        : webrtc_socket_base(owner), state(std::move(state))
    {
    }
    ~webrtc_server_socket() override;
    int error() override;
    int ready_to_read() override;
    int ready_to_write() override
    {
        return 0;
    }
    int write(void const *, int, net_address * = nullptr) override
    {
        return -1;
    }
    int read(void *, int, net_address ** = nullptr) override
    {
        return -1;
    }
    net_socket *accept(net_address *&from) override;
};

class webrtc_datagram_socket final : public webrtc_socket_base
{
    std::shared_ptr<datagram_state> state;

  public:
    webrtc_datagram_socket(webrtc_protocol::impl *owner, std::shared_ptr<datagram_state> state)
        : webrtc_socket_base(owner), state(std::move(state))
    {
    }
    ~webrtc_datagram_socket() override;
    int error() override;
    int ready_to_read() override;
    int ready_to_write() override;
    int write(void const *buffer, int size, net_address *address = nullptr) override;
    int read(void *buffer, int size, net_address **address = nullptr) override;
};

struct webrtc_protocol::impl
{
    enum class mode
    {
        disabled,
        host,
        client
    };

    mutable std::mutex mutex;
    std::condition_variable changed;
    mode requested_mode{mode::disabled};
    std::string signaling_url;
    std::string requested_room;
    std::string room;
    std::string local_id;
    std::string host_id;
    std::string failure;
    bool signaling_started{false};
    bool signaling_ready{false};
    bool shutting_down{false};
    rtc::Configuration configuration;
    std::shared_ptr<rtc::WebSocket> websocket;
    std::unordered_map<std::string, std::shared_ptr<peer_state>> peers;
    std::weak_ptr<listener_state> listener;
    std::weak_ptr<datagram_state> datagrams;
    std::vector<webrtc_socket_base *> sockets;

    void add_socket(webrtc_socket_base *socket)
    {
        std::lock_guard<std::mutex> lock(mutex);
        sockets.push_back(socket);
    }

    void remove_socket(webrtc_socket_base *socket)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = std::find(sockets.begin(), sockets.end(), socket);
        if (found != sockets.end())
            sockets.erase(found);
    }

    std::string endpoint() const
    {
        std::string result = signaling_url;
        while (!result.empty() && result.back() == '/')
            result.pop_back();
        if (requested_mode == mode::host)
            return result + "/v1/rooms/host";
        return result + "/v1/rooms/" + requested_room;
    }

    void fail(std::string message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!shutting_down && failure.empty())
            failure = std::move(message);
        changed.notify_all();
    }

    void send_json(const json &message)
    {
        std::shared_ptr<rtc::WebSocket> socket;
        {
            std::lock_guard<std::mutex> lock(mutex);
            socket = websocket;
        }
        if (!socket || !socket->isOpen())
            DEBUG_LOG("%s", "Unable to send WebRTC signaling message");
        else
        {
            try
            {
                socket->send(message.dump());
            }
            catch (const std::exception &error)
            {
                DEBUG_LOG("Unable to send WebRTC signaling message: %s", error.what());
            }
        }
    }

    void bind_datagram(const std::shared_ptr<peer_state> &peer, const std::shared_ptr<rtc::DataChannel> &channel)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            peer->datagram = channel;
            changed.notify_all();
        }
        const std::weak_ptr<peer_state> weak_peer = peer;
        channel->onOpen([this]() {
            std::lock_guard<std::mutex> lock(mutex);
            changed.notify_all();
        });
        channel->onClosed([this, weak_peer]() {
            if (const auto peer = weak_peer.lock())
            {
                std::lock_guard<std::mutex> lock(mutex);
                peer->failed = true;
                if (requested_mode == mode::client)
                {
                    if (const auto target = datagrams.lock())
                    {
                        std::lock_guard<std::mutex> datagram_lock(target->mutex);
                        target->failed = true;
                    }
                }
                changed.notify_all();
            }
        });
        channel->onError([this, weak_peer](const std::string &) {
            if (const auto peer = weak_peer.lock())
            {
                std::lock_guard<std::mutex> lock(mutex);
                peer->failed = true;
                if (requested_mode == mode::client)
                {
                    if (const auto target = datagrams.lock())
                    {
                        std::lock_guard<std::mutex> datagram_lock(target->mutex);
                        target->failed = true;
                    }
                }
                changed.notify_all();
            }
        });
        channel->onMessage([this, weak_peer](rtc::message_variant message) {
            const auto peer = weak_peer.lock();
            if (!peer || !std::holds_alternative<rtc::binary>(message))
                return;
            rtc::binary bytes = std::get<rtc::binary>(std::move(message));
            if (bytes.size() < datagram_header_size)
                return;
            const uint16_t source = read_u16(bytes, 0);
            const uint16_t destination = read_u16(bytes, 2);
            std::shared_ptr<datagram_state> target;
            {
                std::lock_guard<std::mutex> lock(mutex);
                target = datagrams.lock();
            }
            if (!target)
                return;
            std::lock_guard<std::mutex> lock(target->mutex);
            if (target->closed || (target->local_port && destination != target->local_port))
                return;
            bytes.erase(bytes.begin(), bytes.begin() + datagram_header_size);
            target->input.push_back({std::move(bytes), peer->id, source});
        });
    }

    void accept_channel(const std::shared_ptr<peer_state> &peer, const std::shared_ptr<rtc::DataChannel> &channel)
    {
        if (channel->label() == datagram_label)
        {
            bind_datagram(peer, channel);
            return;
        }
        if (!begins_with(channel->label(), stream_label_prefix))
        {
            channel->close();
            return;
        }
        std::shared_ptr<listener_state> target;
        {
            std::lock_guard<std::mutex> lock(mutex);
            target = listener.lock();
        }
        if (!target)
        {
            channel->close();
            return;
        }
        auto stream = std::make_shared<stream_state>();
        bind_stream_channel(stream, channel);
        std::lock_guard<std::mutex> lock(target->mutex);
        if (target->closed)
            channel->close();
        else
            target->pending.push_back({std::move(stream), peer->id});
    }

    std::shared_ptr<peer_state> create_peer(const std::string &id, const bool offerer)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (const auto found = peers.find(id); found != peers.end())
                return found->second;
        }

        auto peer = std::make_shared<peer_state>();
        peer->id = id;
        peer->connection = std::make_shared<rtc::PeerConnection>(configuration);
        const std::weak_ptr<peer_state> weak_peer = peer;
        peer->connection->onLocalDescription([this, id](const rtc::Description &description) {
            send_json({{"type", "signal"},
                       {"to", id},
                       {"signalType", description.typeString()},
                       {"description", std::string(description)}});
        });
        peer->connection->onLocalCandidate([this, id](const rtc::Candidate &candidate) {
            send_json({{"type", "signal"},
                       {"to", id},
                       {"signalType", "candidate"},
                       {"candidate", std::string(candidate)},
                       {"mid", candidate.mid()}});
        });
        peer->connection->onStateChange([this, weak_peer](const rtc::PeerConnection::State state) {
            if (const auto peer = weak_peer.lock())
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed)
                    peer->failed = true;
                changed.notify_all();
            }
        });
        peer->connection->onDataChannel([this, weak_peer](const std::shared_ptr<rtc::DataChannel> &channel) {
            if (const auto peer = weak_peer.lock())
                accept_channel(peer, channel);
        });

        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto [found, inserted] = peers.emplace(id, peer);
            if (!inserted)
                return found->second;
        }

        if (offerer)
        {
            rtc::DataChannelInit options;
            options.reliability.unordered = true;
            options.reliability.maxRetransmits = 0;
            bind_datagram(peer, peer->connection->createDataChannel(datagram_label, options));
        }
        return peer;
    }

    void handle_message(const std::string &text)
    {
        try
        {
            const json message = json::parse(text);
            const std::string type = message.value("type", "");
            if (type == "welcome")
            {
                rtc::Configuration received_configuration;
                try
                {
                    received_configuration = make_rtc_configuration(message.value("iceServers", json::array()));
                }
                catch (const std::exception &error)
                {
                    fail(std::string("signaling server provided invalid ICE configuration: ") + error.what());
                    return;
                }
                std::lock_guard<std::mutex> lock(mutex);
                local_id = message.value("id", "");
                host_id = message.value("hostId", "");
                room = message.value("code", requested_room);
                configuration = std::move(received_configuration);
                const std::string warning = message.value("warning", "");
                if (!warning.empty())
                    std::fprintf(stderr, "Online multiplayer warning: %s\n", warning.c_str());
                if (local_id.empty() || host_id.empty() || room.empty())
                    failure = "signaling server returned an incomplete welcome message";
                else
                {
                    signaling_ready = true;
                }
                changed.notify_all();
                return;
            }
            if (type == "error")
            {
                fail(message.value("message", "signaling server rejected the request"));
                return;
            }
            if (type == "peer-joined" && requested_mode == mode::host)
            {
                const std::string id = message.value("id", "");
                if (!id.empty())
                    create_peer(id, true);
                return;
            }
            if (type == "peer-left")
            {
                const std::string id = message.value("id", "");
                std::shared_ptr<peer_state> peer;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (const auto found = peers.find(id); found != peers.end())
                    {
                        peer = found->second;
                        peers.erase(found);
                    }
                    changed.notify_all();
                }
                if (peer && peer->connection)
                    peer->connection->close();
                return;
            }
            if (type != "signal")
                return;

            const std::string from = message.value("from", "");
            const std::string signal_type = message.value("signalType", "");
            if (from.empty() || signal_type.empty())
                return;
            std::shared_ptr<peer_state> peer;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (const auto found = peers.find(from); found != peers.end())
                    peer = found->second;
            }
            if (!peer && signal_type == "offer" && requested_mode == mode::client)
                peer = create_peer(from, false);
            if (!peer)
                return;
            if (signal_type == "offer" || signal_type == "answer")
                peer->connection->setRemoteDescription(
                    rtc::Description(message.at("description").get<std::string>(), signal_type));
            else if (signal_type == "candidate")
                peer->connection->addRemoteCandidate(
                    rtc::Candidate(message.at("candidate").get<std::string>(), message.value("mid", "0")));
        }
        catch (const std::exception &error)
        {
            DEBUG_LOG("Invalid WebRTC signaling message: %s", error.what());
        }
    }

    bool ensure_signaling()
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (signaling_ready)
            return true;
        if (!failure.empty())
            return false;
        if (!signaling_started)
        {
            static std::once_flag logger;
            std::call_once(logger, []() { rtc::InitLogger(rtc::LogLevel::Warning); });
            signaling_started = true;
            rtc::WebSocket::Configuration options;
            options.connectionTimeout = signaling_timeout;
            options.pingInterval = 20s;
            websocket = std::make_shared<rtc::WebSocket>(options);
            websocket->onError([this](const std::string &error) { fail("signaling connection failed: " + error); });
            websocket->onClosed([this]() { fail("signaling connection closed"); });
            websocket->onMessage([this](rtc::message_variant message) {
                if (std::holds_alternative<std::string>(message))
                    handle_message(std::get<std::string>(message));
            });
            const auto socket = websocket;
            const std::string url = endpoint();
            lock.unlock();
            try
            {
                socket->open(url);
            }
            catch (const std::exception &error)
            {
                fail("unable to open signaling URL " + url + ": " + error.what());
            }
            lock.lock();
        }
        changed.wait_for(lock, signaling_timeout, [this]() { return signaling_ready || !failure.empty(); });
        if (!signaling_ready && failure.empty())
            failure = "timed out while connecting to the signaling server";
        if (!failure.empty())
            std::fprintf(stderr, "Online multiplayer: %s\n", failure.c_str());
        return signaling_ready;
    }

    std::shared_ptr<peer_state> wait_for_server_peer()
    {
        if (!ensure_signaling())
            return nullptr;
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait_for(lock, peer_timeout, [this]() {
            const auto found = peers.find(host_id);
            return !failure.empty() ||
                   (found != peers.end() &&
                    (found->second->failed || (found->second->datagram && found->second->datagram->isOpen())));
        });
        const auto found = peers.find(host_id);
        if (found == peers.end() || !found->second->datagram || !found->second->datagram->isOpen())
        {
            std::fprintf(stderr, "Online multiplayer: timed out while establishing the peer connection\n");
            return nullptr;
        }
        return found->second;
    }

    std::string resolve_peer(const std::string &id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return id == "host" ? host_id : id;
    }

    bool send_datagram(const std::string &id, const uint16_t source, const uint16_t destination, const void *buffer,
                       const int size)
    {
        std::shared_ptr<rtc::DataChannel> channel;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const std::string peer_id = id == "host" ? host_id : id;
            const auto found = peers.find(peer_id);
            if (found != peers.end())
                channel = found->second->datagram;
        }
        if (!channel || !channel->isOpen() || channel->bufferedAmount() > write_high_water_mark)
            return false;
        rtc::binary message;
        message.reserve(datagram_header_size + static_cast<size_t>(size));
        append_u16(message, source);
        append_u16(message, destination);
        const auto *bytes = static_cast<const std::byte *>(buffer);
        message.insert(message.end(), bytes, bytes + size);
        try
        {
            // A false return means the message was accepted into the channel's
            // send buffer, not that it was discarded.
            channel->send(std::move(message));
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    void reset()
    {
        std::shared_ptr<rtc::WebSocket> socket;
        std::vector<std::shared_ptr<rtc::PeerConnection>> connections;
        {
            std::lock_guard<std::mutex> lock(mutex);
            shutting_down = true;
            socket = std::move(websocket);
            for (auto &[id, peer] : peers)
                connections.push_back(peer->connection);
            peers.clear();
            requested_mode = mode::disabled;
            signaling_started = false;
            signaling_ready = false;
            local_id.clear();
            host_id.clear();
            room.clear();
            failure.clear();
            listener.reset();
            datagrams.reset();
        }
        if (socket)
            socket->close();
        for (const auto &connection : connections)
            if (connection)
                connection->close();
        std::lock_guard<std::mutex> lock(mutex);
        shutting_down = false;
    }
};

webrtc_socket_base::webrtc_socket_base(webrtc_protocol::impl *owner) : owner(owner)
{
    owner->add_socket(this);
}

webrtc_socket_base::~webrtc_socket_base()
{
    owner->remove_socket(this);
}

webrtc_stream_socket::~webrtc_stream_socket()
{
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->closed = true;
        state->open = false;
        channel = state->channel;
        state->changed.notify_all();
    }
    if (channel)
    {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (channel->bufferedAmount() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);
        channel->close();
    }
}

int webrtc_stream_socket::error()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->closed && state->input.empty();
}

int webrtc_stream_socket::ready_to_read()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return !state->input.empty();
}

int webrtc_stream_socket::ready_to_write()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->open && state->channel && state->channel->bufferedAmount() < write_high_water_mark;
}

int webrtc_stream_socket::write(void const *buffer, const int size, net_address *address)
{
    if (!buffer || size < 0 || address)
        return -1;
    if (size == 0)
        return 0;
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->open || state->closed)
            return -1;
        channel = state->channel;
    }
    if (!channel)
        return -1;
    try
    {
        // libdatachannel returns false when it buffers a successfully accepted
        // message. The high-water check in ready_to_write() controls pressure.
        channel->send(static_cast<const std::byte *>(buffer), static_cast<size_t>(size));
        return size;
    }
    catch (const std::exception &)
    {
        return -1;
    }
}

int webrtc_stream_socket::read(void *buffer, const int size, net_address **address)
{
    if (address)
        *address = nullptr;
    if (!buffer || size < 0)
        return -1;
    if (size == 0)
        return 0;

    auto *output = static_cast<std::byte *>(buffer);
    int copied = 0;
    std::unique_lock<std::mutex> lock(state->mutex);
    while (copied < size)
    {
        state->changed.wait(lock, [this]() { return !state->input.empty() || state->closed; });
        if (state->input.empty())
            return copied ? copied : -1;
        rtc::binary &front = state->input.front();
        const size_t available = front.size() - state->input_offset;
        const size_t wanted = static_cast<size_t>(size - copied);
        const size_t amount = std::min(available, wanted);
        std::memcpy(output + copied, front.data() + state->input_offset, amount);
        copied += static_cast<int>(amount);
        state->input_offset += amount;
        if (state->input_offset == front.size())
        {
            state->input.pop_front();
            state->input_offset = 0;
        }
    }
    return copied;
}

webrtc_server_socket::~webrtc_server_socket()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    state->closed = true;
    for (auto &item : state->pending)
        if (item.state->channel)
            item.state->channel->close();
    state->pending.clear();
}

int webrtc_server_socket::error()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->closed;
}

int webrtc_server_socket::ready_to_read()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return !state->pending.empty();
}

net_socket *webrtc_server_socket::accept(net_address *&from)
{
    from = nullptr;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->pending.empty())
        return nullptr;
    pending_stream pending = std::move(state->pending.front());
    state->pending.pop_front();
    from = new webrtc_address(pending.peer);
    return new webrtc_stream_socket(owner, std::move(pending.state));
}

webrtc_datagram_socket::~webrtc_datagram_socket()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    state->closed = true;
    state->input.clear();
}

int webrtc_datagram_socket::error()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->closed || state->failed;
}

int webrtc_datagram_socket::ready_to_read()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return !state->input.empty();
}

int webrtc_datagram_socket::ready_to_write()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return !state->closed && !state->failed;
}

int webrtc_datagram_socket::write(void const *buffer, const int size, net_address *address)
{
    const auto *destination = dynamic_cast<webrtc_address *>(address);
    if (!buffer || size < 0 || !destination)
        return -1;
    uint16_t source_port;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->closed || state->failed)
            return -1;
        source_port = state->local_port;
    }
    return owner->send_datagram(destination->peer, source_port, destination->port, buffer, size) ? size : -1;
}

int webrtc_datagram_socket::read(void *buffer, const int size, net_address **address)
{
    if (address)
        *address = nullptr;
    if (!buffer || size < 0)
        return -1;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->input.empty())
        return 0;
    datagram_packet packet = std::move(state->input.front());
    state->input.pop_front();
    const int copied = std::min(size, static_cast<int>(packet.data.size()));
    std::memcpy(buffer, packet.data.data(), static_cast<size_t>(copied));
    if (address)
        *address = new webrtc_address(std::move(packet.peer), packet.source_port);
    return copied;
}

webrtc_protocol webrtc;

webrtc_protocol::webrtc_protocol() : data(std::make_unique<impl>())
{
}

webrtc_protocol::~webrtc_protocol()
{
    cleanup();
}

void webrtc_protocol::configure_host(std::string signaling_url)
{
    data->reset();
    std::lock_guard<std::mutex> lock(data->mutex);
    data->requested_mode = impl::mode::host;
    data->signaling_url = std::move(signaling_url);
    data->requested_room.clear();
}

void webrtc_protocol::configure_client(std::string signaling_url, std::string room_code)
{
    data->reset();
    std::transform(room_code.begin(), room_code.end(), room_code.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
    std::lock_guard<std::mutex> lock(data->mutex);
    data->requested_mode = impl::mode::client;
    data->signaling_url = std::move(signaling_url);
    data->requested_room = std::move(room_code);
}

bool webrtc_protocol::requested() const
{
    std::lock_guard<std::mutex> lock(data->mutex);
    return data->requested_mode != impl::mode::disabled;
}

std::string webrtc_protocol::room_code() const
{
    std::lock_guard<std::mutex> lock(data->mutex);
    return data->room;
}

net_address *webrtc_protocol::get_local_address()
{
    std::lock_guard<std::mutex> lock(data->mutex);
    return new webrtc_address(data->local_id.empty() ? "local" : data->local_id);
}

net_address *webrtc_protocol::get_node_address(char const *&server_host, const int def_port, int)
{
    if (!server_host || !*server_host)
        return nullptr;
    server_host += std::strlen(server_host);
    return new webrtc_address("host", static_cast<uint16_t>(def_port));
}

net_socket *webrtc_protocol::connect_to_server(net_address *address, const net_socket::socket_type socket_type)
{
    const auto *destination = dynamic_cast<webrtc_address *>(address);
    if (!destination || !data->ensure_signaling())
        return nullptr;

    if (socket_type == net_socket::SOCKET_FAST)
    {
        auto state = std::make_shared<datagram_state>();
        state->local_port = 0;
        {
            std::lock_guard<std::mutex> lock(data->mutex);
            data->datagrams = state;
        }
        return new webrtc_datagram_socket(data.get(), std::move(state));
    }

    std::shared_ptr<peer_state> peer;
    if (data->requested_mode == impl::mode::client)
        peer = data->wait_for_server_peer();
    else
    {
        const std::string id = data->resolve_peer(destination->peer);
        std::lock_guard<std::mutex> lock(data->mutex);
        if (const auto found = data->peers.find(id); found != data->peers.end())
            peer = found->second;
    }
    if (!peer || !peer->connection)
        return nullptr;

    auto state = std::make_shared<stream_state>();
    std::shared_ptr<rtc::DataChannel> channel;
    try
    {
        channel = peer->connection->createDataChannel(std::string(stream_label_prefix) + random_id(12));
    }
    catch (const std::exception &error)
    {
        DEBUG_LOG("Unable to create reliable WebRTC channel: %s", error.what());
        return nullptr;
    }
    bind_stream_channel(state, channel);
    std::unique_lock<std::mutex> lock(state->mutex);
    state->changed.wait_for(lock, peer_timeout, [&state]() { return state->open || state->closed; });
    if (!state->open)
    {
        channel->close();
        return nullptr;
    }
    return new webrtc_stream_socket(data.get(), std::move(state));
}

net_socket *webrtc_protocol::create_listen_socket(const int port, const net_socket::socket_type socket_type)
{
    if (port < 0 || port > std::numeric_limits<uint16_t>::max() || !data->ensure_signaling())
        return nullptr;
    if (socket_type == net_socket::SOCKET_SECURE)
    {
        auto state = std::make_shared<listener_state>();
        std::string room_code;
        {
            std::lock_guard<std::mutex> lock(data->mutex);
            data->listener = state;
            if (data->requested_mode == impl::mode::host)
                room_code = data->room;
        }
        if (!room_code.empty())
        {
            std::printf("Online room code: %s\n", room_code.c_str());
            std::fflush(stdout);
        }
        return new webrtc_server_socket(data.get(), std::move(state));
    }
    auto state = std::make_shared<datagram_state>();
    state->local_port = static_cast<uint16_t>(port);
    {
        std::lock_guard<std::mutex> lock(data->mutex);
        data->datagrams = state;
    }
    return new webrtc_datagram_socket(data.get(), std::move(state));
}

int webrtc_protocol::installed()
{
    return requested();
}

char const *webrtc_protocol::name()
{
    return "WebRTC room-code transport";
}

int webrtc_protocol::select(const bool block)
{
    do
    {
        std::vector<webrtc_socket_base *> sockets;
        {
            std::lock_guard<std::mutex> lock(data->mutex);
            sockets = data->sockets;
        }
        int ready = 0;
        for (auto *socket : sockets)
            if (socket->selected_ready())
                ++ready;
        if (ready || !block)
            return ready;
        std::this_thread::sleep_for(1ms);
    } while (block);
    return 0;
}

void webrtc_protocol::cleanup()
{
    data->reset();
}
