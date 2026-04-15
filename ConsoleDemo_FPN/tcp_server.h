#pragma once
#include <cstdint>
#include <string>

class TcpBroadcaster {
public:
  // Listens on 127.0.0.1:port. Accepts multiple clients.
  // Each Broadcast() call sends `line` + '\n' to every connected client (best-effort).
  explicit TcpBroadcaster(uint16_t port = 5555);
  ~TcpBroadcaster();

  TcpBroadcaster(const TcpBroadcaster&) = delete;
  TcpBroadcaster& operator=(const TcpBroadcaster&) = delete;

  void Broadcast(const std::string& line);

private:
  struct Impl;
  Impl* p_;
};
