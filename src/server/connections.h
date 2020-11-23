#ifndef SERVER_CONNECTIONS_H_
#define SERVER_CONNECTIONS_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include <memory>

#include "Message.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

namespace ptxchat {

enum ConnStatus {
  UP,
  CLOSED,
  ERROR,
};

class Connection {
 public:
  Connection(int skt, uint32_t ip, uint16_t port) :
    socket_(skt),
    ip_(ip),
    port_(port),
    status_(UP),
    recv_data_sz_(0)
    {}

  static std::unique_ptr<ChatMsg> RecvMsgFromConn(std::shared_ptr<Connection> conn);

  static bool SendMsgToConn(std::shared_ptr<ChatMsg> msg, std::shared_ptr<Connection> conn);

  static int makeNonBlocking(int fd);

  static int addEventToEpoll(int epoll_fd, int fd, uint32_t ev);

  [[nodiscard]] int GetSocket() const { return socket_; }
  [[nodiscard]] uint32_t GetIP() const { return ip_; }
  [[nodiscard]] uint16_t GetPort() const { return port_; }
  [[nodiscard]] ConnStatus& Status() { return status_; }

 private:
  int socket_;
  uint32_t ip_;
  uint16_t port_;
  ConnStatus status_;

  uint8_t* recv_data_;
  uint32_t recv_data_sz_;
};

} // namespace ptxchat

#endif // SERVER_CONNECTIONS_H_