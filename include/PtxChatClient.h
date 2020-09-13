#ifndef PTXCHATCLIENT_H_
#define PTXCHATCLIENT_H_

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <string>
#include <memory>
#include <deque>
#include <utility>

#include "Message.h"
#include "Threads.h"

namespace ptxchat {

constexpr uint32_t DEFAULT_SERVER_IP = 2130706433;
constexpr uint16_t DEFAULT_SERVER_PORT = 1488;

class PtxChatClient {
 public:
  PtxChatClient() noexcept;
  PtxChatClient(const std::string& ip, uint16_t port) noexcept;
  ~PtxChatClient();

  void LogIn(const std::string& nickname);
  void LogOut();
  void SendPrivateMsg(const std::string& to, const std::string& text);
  void SendPublicMsg(const std::string& text);

 private:
  uint32_t server_ip_;     /**< Chat server ip (default=127.0.0.1) */
  uint16_t server_port_;   /**< Chat server port (default=1488) */
  std::string nick_;       /**< Nickname on server if logged in */

  int socket_;
  struct sockaddr_in serv_addr_;

  struct ThreadState msg_in_thread_;
  struct ThreadState msg_out_thread_;
  std::deque<std::unique_ptr<struct ChatMsg>> msg_in_;
  std::deque<std::unique_ptr<struct ChatMsg>> msg_out_;

  void SendServerMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void ReceiveMessages();
  void SendMessages();
};

}  // namespace ptxchat

#endif  // PTXCHATCLIENT_H_
