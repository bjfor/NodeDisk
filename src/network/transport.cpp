#include "netdisk/network/transport.h"

#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <string>
#include <stdexcept>

extern "C" int close(int);

namespace sync {

namespace {

void WriteAll(int fd, const void *buffer, std::size_t size) {
    const char *cursor = static_cast<const char *>(buffer);
    std::size_t written = 0;
    while (written < size) {
        const ssize_t rc = ::send(fd, cursor + written, size - written, 0);
        if (rc <= 0) {
            throw std::runtime_error("socket send failed");
        }
        written += static_cast<std::size_t>(rc);
    }
}

bool ReadExact(int fd, void *buffer, std::size_t size) {
    char *cursor = static_cast<char *>(buffer);
    std::size_t read_total = 0;
    while (read_total < size) {
        const ssize_t rc = ::recv(fd, cursor + read_total, size - read_total, 0);
        if (rc == 0) {
            return false;
        }
        if (rc < 0) {
            throw std::runtime_error("socket recv failed");
        }
        read_total += static_cast<std::size_t>(rc);
    }
    return true;
}

std::uint32_t HostToNet32(std::uint32_t value) {
    return htonl(value);
}

std::uint32_t NetToHost32(std::uint32_t value) {
    return ntohl(value);
}

}  // namespace

InMemoryTransportEndpoint::InMemoryTransportEndpoint(std::shared_ptr<std::deque<MessageFrame>> inbox,
                                                     std::shared_ptr<std::deque<MessageFrame>> outbox)
    : inbox_(inbox), outbox_(outbox) {}

void InMemoryTransportEndpoint::Send(const MessageFrame &frame) {
    if (!outbox_) {
        throw std::runtime_error("transport endpoint has no outbound queue");
    }
    outbox_->push_back(frame);
}

bool InMemoryTransportEndpoint::Receive(MessageFrame *frame) {
    if (frame == nullptr || !inbox_ || inbox_->empty()) {
        return false;
    }
    *frame = inbox_->front();
    inbox_->pop_front();
    return true;
}

InMemoryTransportPair CreateInMemoryTransportPair() {
    auto left_inbox = std::make_shared<std::deque<MessageFrame>>();
    auto right_inbox = std::make_shared<std::deque<MessageFrame>>();

    // The endpoints share the two queues to simulate a duplex in-memory link.
    return {
        std::make_unique<InMemoryTransportEndpoint>(left_inbox, right_inbox),
        std::make_unique<InMemoryTransportEndpoint>(right_inbox, left_inbox),
    };
}

SocketTransportEndpoint::SocketTransportEndpoint(int fd) : fd_(fd) {}

SocketTransportEndpoint::~SocketTransportEndpoint() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void SocketTransportEndpoint::Send(const MessageFrame &frame) {
    const std::uint32_t type = HostToNet32(static_cast<std::uint32_t>(frame.type));
    const std::uint32_t size = HostToNet32(static_cast<std::uint32_t>(frame.payload.size()));
    WriteAll(fd_, &type, sizeof(type));
    WriteAll(fd_, &size, sizeof(size));
    if (!frame.payload.empty()) {
        WriteAll(fd_, frame.payload.data(), frame.payload.size());
    }
}

bool SocketTransportEndpoint::Receive(MessageFrame *frame) {
    if (frame == nullptr) {
        return false;
    }

    std::uint32_t type = 0;
    if (!ReadExact(fd_, &type, sizeof(type))) {
        return false;
    }

    std::uint32_t size = 0;
    if (!ReadExact(fd_, &size, sizeof(size))) {
        return false;
    }

    type = NetToHost32(type);
    size = NetToHost32(size);

    frame->type = static_cast<MessageType>(type);
    frame->payload.assign(size, '\0');
    if (size > 0 && !ReadExact(fd_, frame->payload.data(), size)) {
        return false;
    }
    return true;
}

TcpListener::TcpListener(std::uint16_t port) : fd_(-1) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error(std::string("failed to create listen socket: ") + std::strerror(errno));
    }

    int opt = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("failed to bind listen socket: ") + std::strerror(errno));
    }
    if (::listen(fd_, 1) != 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(std::string("failed to listen on socket: ") + std::strerror(errno));
    }
}

TcpListener::~TcpListener() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

std::unique_ptr<TransportEndpoint> TcpListener::Accept() {
    const int client_fd = ::accept(fd_, nullptr, nullptr);
    if (client_fd < 0) {
        throw std::runtime_error(std::string("failed to accept socket: ") + std::strerror(errno));
    }
    return std::make_unique<SocketTransportEndpoint>(client_fd);
}

std::unique_ptr<TransportEndpoint> ConnectTcp(const std::string &host, std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error(std::string("failed to create client socket: ") + std::strerror(errno));
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("invalid IPv4 address: " + host);
    }

    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        throw std::runtime_error(std::string("failed to connect client socket: ") + std::strerror(errno));
    }

    return std::make_unique<SocketTransportEndpoint>(fd);
}

}  // namespace sync
