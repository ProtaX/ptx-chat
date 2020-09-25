#ifndef PTXCHATSERVER_H_
#define PTXCHATSERVER_H_

#include <stdint.h>

#include <thread>
#include <mutex>
#include <memory>
#include <deque>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "Threads.h"
#include "Client.h"
#include "Message.h"
#include "PtxGuiBackend.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

namespace ptxchat {

static constexpr int MAX_LISTEN_Q_SIZE =          1000;
static constexpr int RECV_MESSAGES_SLEEP =        100;
static constexpr int DEF_LISTEN_Q_LEN =           1000;
static const char* DEF_SERVER_LOG_PATH =          "ptx_server.log";
static constexpr size_t MAX_LOG_FILE_SIZE =       10000000;
static constexpr size_t MAX_LOG_FILES_CNT =       10;

class PtxChatServer: public GUIBackend {
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
  bool SetIP_i(uint32_t ip);
  bool SetIP_s(const std::string& ip);
  bool SetPort_i(uint16_t port);
  bool SetPort_s(const std::string& port);

  [[nodiscard]] uint32_t    GetIp_i() const { return ip_; }
  [[nodiscard]] std::string GetIp_s() const { return std::to_string(ip_); }
  [[nodiscard]] uint16_t    GetPort() const { return port_; }

  /* Virtual because may be added derived class for tcp/udp server */
  virtual ~PtxChatServer();

 private:
  uint32_t ip_;             /**< Server ip (default = 0.0.0.0) */
  uint16_t port_;           /**< Server port (default = 8080) */
  int listen_q_len_;        /**< Max amount of clients in listen queue */
  int socket_;              /**< Server socket (blocking) */
  bool is_running_;         /**< True if server is running */

  std::shared_ptr<spdlog::logger> logger_;

  struct ThreadState accept_conn_thread_;                      /**< Accept client connections */
  struct ThreadState receive_msg_thread_;                      /**< Receive message from every client */
  struct ThreadState process_msg_thread_;                      /**< Process received messages */
  std::unique_ptr<SharedUDeque<struct ChatMsg>> client_msgs_;  /**< Client messages storage */

  std::mutex clients_mtx_;                                                      /**< Protects @accepted_clients_ and registered_clients_ */
  std::unordered_map<int, std::unique_ptr<Client>> accepted_clients_;           /**< Not registered clients */
  std::unordered_map<std::string, std::unique_ptr<Client>> registered_clients_; /**< Registered clients */

  void InitSocket();
  void InitLog();
  void Finalize();

  void AcceptConnections();
  void ReceiveMessages();
  void ProcessMessages();

  void ParseClientMsg(std::unique_ptr<struct ChatMsg>&& msg);
  bool SendMsgToClient(std::unique_ptr<struct ChatMsg>&& msg, std::unique_ptr<Client>& client);
  void SendMsgToAll(std::unique_ptr<struct ChatMsg>&& msg);
  bool RecvMsgFromClient(std::unique_ptr<Client>& cl);
  /* TODO(me): templates?
  template <typename M>
  void ProcessMsg(std::unique_ptr<struct ChatMsg>&& msg);
  */

  /**
   * Register and set nickname
   */
  void ProcessRegMsg(std::unique_ptr<struct ChatMsg>&& msg);
  /**
   * These functions are not under any mutex
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
