#include "tcp_server.h"

#include <Winsock2.h>
#include <Ws2tcpip.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

struct TcpBroadcaster::Impl {
  SOCKET listen_sock = INVALID_SOCKET;
  std::thread accept_thread;
  std::atomic<bool> running{false};
  std::mutex clients_mu;
  std::vector<SOCKET> clients;
  WSADATA wsa{};

  Impl(uint16_t port) {
    WSAStartup(MAKEWORD(2, 2), &wsa);
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) return;

    BOOL one = TRUE;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
      closesocket(listen_sock);
      listen_sock = INVALID_SOCKET;
      return;
    }
    if (listen(listen_sock, 4) == SOCKET_ERROR) {
      closesocket(listen_sock);
      listen_sock = INVALID_SOCKET;
      return;
    }

    running = true;
    accept_thread = std::thread([this]() {
      while (running) {
        SOCKET c = accept(listen_sock, nullptr, nullptr);
        if (c == INVALID_SOCKET) break;
        std::lock_guard<std::mutex> g(clients_mu);
        clients.push_back(c);
      }
    });
  }

  ~Impl() {
    running = false;
    if (listen_sock != INVALID_SOCKET) {
      closesocket(listen_sock);
      listen_sock = INVALID_SOCKET;
    }
    if (accept_thread.joinable()) accept_thread.join();
    {
      std::lock_guard<std::mutex> g(clients_mu);
      for (SOCKET c : clients) closesocket(c);
      clients.clear();
    }
    WSACleanup();
  }

  void Broadcast(const std::string& line) {
    std::string payload = line + "\n";
    std::lock_guard<std::mutex> g(clients_mu);
    for (auto it = clients.begin(); it != clients.end();) {
      int sent = send(*it, payload.data(), (int)payload.size(), 0);
      if (sent == SOCKET_ERROR) {
        closesocket(*it);
        it = clients.erase(it);
      } else {
        ++it;
      }
    }
  }
};

TcpBroadcaster::TcpBroadcaster(uint16_t port) : p_(new Impl(port)) {}
TcpBroadcaster::~TcpBroadcaster() { delete p_; }
void TcpBroadcaster::Broadcast(const std::string& line) { p_->Broadcast(line); }
