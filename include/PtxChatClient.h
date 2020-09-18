#ifndef PTXCHATCLIENT_H_
#define PTXCHATCLIENT_H_

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <string>
#include <memory>
#include <deque>
#include <utility>
#include <fstream>
#include <mutex>

#include "Message.h"
#include "Threads.h"
#include "PtxGuiBackend.h"

namespace ptxchat {

constexpr uint32_t DEFAULT_SERVER_IP = 2130706433;
constexpr uint16_t DEFAULT_SERVER_PORT = 1488;
constexpr uint64_t MSG_THREAD_SLEEP = 100;

class PtxChatClient : public GUIBackend {
 public:
  PtxChatClient() noexcept;
  PtxChatClient(const std::string& ip, uint16_t port) noexcept;
  ~PtxChatClient();

  /**
   * \brief Get the top element of gui events queue
   */
  std::unique_ptr<struct GuiEvent> PopGuiEvent();

  /**
   * \brief Set server ip as integer
   */
  bool SetIpPort_i(uint32_t ip, uint16_t port);

  /**
   * \brief Set server ip as a string
   */
  bool SetIpPort_s(const std::string& ip, uint16_t port);

  /**
   * \brief Connect and log in
   */
  void LogIn(const std::string& nickname);

  /**
   * \brief Log out and disconnect
   */
  void LogOut();

  /**
   * \brief Send private message
   */
  void SendMsgTo(const std::string& to, const std::string& text);

  /**
   * \brief Send public message
   */
  void SendMsg(const std::string& text);

 private:
  uint32_t server_ip_;        /**< Chat server ip (default=127.0.0.1) */
  uint16_t server_port_;      /**< Chat server port (default=1488) */
  std::string nick_;          /**< Nickname on server if logged in */
  std::ofstream chat_log_;
  std::mutex chat_log_mtx_;

  int socket_;
  struct sockaddr_in serv_addr_;

  struct ThreadState msg_in_thread_;
  struct ThreadState msg_out_thread_;
  std::deque<std::unique_ptr<struct ChatMsg>> msg_in_;
  std::deque<std::unique_ptr<struct ChatMsg>> msg_out_;

  void SendMsgToServer(std::unique_ptr<struct ChatMsg>&& msg);
  void PushGuiEvent(std::unique_ptr<struct GuiEvent>&& e);

  void ProcessReceiveMessages();
  void ProcessSendMessages();
  void InitLog();
  void Log(const std::string& text);
};

}  // namespace ptxchat

#endif  // PTXCHATCLIENT_H_
