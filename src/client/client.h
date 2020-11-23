#ifndef CLIENT_CLIENT_H_
#define CLIENT_CLIENT_H_

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
#include "SharedUDeque.h"

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
   * \brief Set server ip as integer
   */
  bool SetIP_i(uint32_t ip);

  /**
   * \brief Set server ip as a string
   */
  bool SetIP_s(const std::string& ip);

  /**
   * \brief Set server port as integer
   */
  bool SetPort_i(uint16_t port);

  /**
   * \brief Set server port as a string
   */
  bool SetPort_s(const std::string& port);

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
  uint32_t server_ip_;                     /**< Chat server ip (default=127.0.0.1) */
  uint16_t server_port_;                   /**< Chat server port (default=1488) */
  std::string nick_;                       /**< Nickname on server if logged in */
  std::mutex chat_log_mtx_;
  bool registered_;

  int socket_;
  sockaddr_in serv_addr_;

  ThreadState msg_in_thread_;
  ThreadState msg_out_thread_;

  std::unique_ptr<SharedUDeque<ChatMsg>> msg_in_;
  std::unique_ptr<SharedUDeque<ChatMsg>> msg_out_;

  void SendMsgToServer(std::shared_ptr<ChatMsg> msg);
  void ProcessRegisteredMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessUnregisteredMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessErrorMsg(std::shared_ptr<ChatMsg> msg);


  void ProcessReceiveMessages();
  void ProcessSendMessages();
  void Stop();
};

}  // namespace ptxchat

#endif  // CLIENT_CLIENT_H_
