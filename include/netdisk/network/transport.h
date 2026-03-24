#ifndef SYNC_TRANSPORT_H
#define SYNC_TRANSPORT_H

#include <deque>
#include <cstdint>
#include <memory>

#include "netdisk/network/protocol_codec.h"

namespace sync {

class TransportEndpoint {
public:
    virtual ~TransportEndpoint() = default;
    virtual void Send(const MessageFrame &frame) = 0;
    virtual bool Receive(MessageFrame *frame) = 0;
};

class InMemoryTransportEndpoint : public TransportEndpoint {
public:
    InMemoryTransportEndpoint(std::shared_ptr<std::deque<MessageFrame>> inbox,
                              std::shared_ptr<std::deque<MessageFrame>> outbox);

    void Send(const MessageFrame &frame) override;
    bool Receive(MessageFrame *frame) override;

private:
    std::shared_ptr<std::deque<MessageFrame>> inbox_;
    std::shared_ptr<std::deque<MessageFrame>> outbox_;
};

struct InMemoryTransportPair {
    std::unique_ptr<TransportEndpoint> left;
    std::unique_ptr<TransportEndpoint> right;
};

InMemoryTransportPair CreateInMemoryTransportPair();

class SocketTransportEndpoint : public TransportEndpoint {
public:
    explicit SocketTransportEndpoint(int fd);
    ~SocketTransportEndpoint() override;

    SocketTransportEndpoint(const SocketTransportEndpoint &) = delete;
    SocketTransportEndpoint &operator=(const SocketTransportEndpoint &) = delete;

    void Send(const MessageFrame &frame) override;
    bool Receive(MessageFrame *frame) override;

private:
    int fd_;
};

class TcpListener {
public:
    explicit TcpListener(std::uint16_t port);
    ~TcpListener();

    TcpListener(const TcpListener &) = delete;
    TcpListener &operator=(const TcpListener &) = delete;

    std::unique_ptr<TransportEndpoint> Accept();

private:
    int fd_;
};

std::unique_ptr<TransportEndpoint> ConnectTcp(const std::string &host, std::uint16_t port);

}  // namespace sync

#endif
