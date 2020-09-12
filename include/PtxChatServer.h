#ifndef PTXCHATSERVER_H_
#define PTXCHATSERVER_H_

#include <stdint.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <deque>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>

#include "Threads.h"
#include "Client.h"
#include "Message.h"

namespace ptxchat {

class PtxChatServer {
 public:
  PtxChatServer() noexcept;
  PtxChatServer(const std::string& ip, uint16_t port) noexcept;
  PtxChatServer(uint32_t ip, uint16_t port) noexcept;

  /**
   * \brief Initialize a working thread that handles client connections
   **/
  void Start();

  /**
   * \brief Stop all threads, close sockets
   **/
  void Stop();
  void SetListenQueueSize(int size);
  bool SetIpPort_i(uint32_t ip, uint16_t port);
  bool SetIpPort_s(const std::string& ip, uint16_t port);
  void LogOn() { use_log_ = true; }
  void LogOff() { use_log_ = false; }

  [[nodiscard]] uint32_t    GetIp_i() const { return ip_; }
  [[nodiscard]] std::string GetIp_s() const { return std::to_string(ip_); }
  [[nodiscard]] uint16_t    GetPort() const { return port_; }

  /* Virtual because may be added derived class for tcp/udp server */
  virtual ~PtxChatServer();

 private:
  uint32_t ip_;             /**< Server ip (default = 0.0.0.0) */
  uint16_t port_;           /**< Server port (default = 8080) */
  int listen_q_size_;  /**< Max amount of clients in listen queue */
  int socket_;              /**< Server socket (blocking) */

  std::ofstream log_file_;
  bool use_log_;
  std::mutex log_mtx_;

  const int MAX_LISTEN_Q_SIZE = 1000;
  const uint32_t SERVER_TICK = 100;         /**< Client handler thread wakes up every 100 ms */

  struct ThreadState accept_conn_thread_;
  struct ThreadState receive_msg_thread_;
  struct ThreadState process_msg_thread_;

  std::deque<void*> gui_event_q_;
  std::deque<std::unique_ptr<ChatMsg>> client_msgs_;  /**< Client messages */
  std::vector<struct Client> clients_;  /**< Clients */

  void InitSocket();
  void InitLog();
  void Finalize();
  void Log(const char* msg);

  void AcceptConnections();  /**< Worker thread for accepting clients */
  void ReceiveMessages();    /**< Worker thread for receiving client data */
  void ProcessMessages();    /**< Worker thread for replying to clients */

  void ParseClientMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void SendMsgToClient(std::unique_ptr<struct ChatMsg>&& msg, const Client& c);
  void SendMsgToAll(std::unique_ptr<struct ChatMsg>&& msg);
  /* TODO(me): templates?
  template <typename M>
  void ProcessMsg(std::unique_ptr<struct ChatMsg>&& msg);
  */

  /**
   * Register and set nickname
   */
  void ProcessRegMsg(std::unique_ptr<struct ChatMsg>&& msg);
  /**
   * Unregister
   */
  void ProcessUnregMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void ProcessPrivateMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void ProcessPublicMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void ProcessErrRegMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void ProcessErrUnregMsg(std::unique_ptr<struct ChatMsg>&& msg);
  void ProcessErrUnkMsg(std::unique_ptr<struct ChatMsg>&& msg);

  static bool CheckPortRange(uint16_t port) {
    if (port < 1024) {
      std::cout << "Warning: port address is in system range. Consider using TCP port range." << std::endl;
      return false;
    }
    if (port > 49151) {
      std::cout << "Warning: port address is in UDP range. Consider using TCP port range." << std::endl;
      return false;
    }
    return true;
  }
};

}  // namespace ptxchat

#endif  // PTXCHATSERVER_H_
