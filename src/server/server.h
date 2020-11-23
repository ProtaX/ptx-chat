#ifndef SERVER_SERVER_H_
#define SERVER_SERVER_H_

#include <stdint.h>

#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "Threads.h"
#include "Message.h"
#include "PtxGuiBackend.h"
#include "client.h"
#include "server_storage.h"

namespace ptxchat {

static constexpr int MAX_LISTEN_Q_SIZE =    1000;
static constexpr int MAX_EVENTS_NUM =       10000;
static constexpr int RECV_MESSAGES_SLEEP =  100;
static constexpr int DEF_LISTEN_Q_LEN =     1000;
static const char* DEF_SERVER_LOG_PATH =    "ptx_server.log";
static constexpr size_t MAX_LOG_FILE_SIZE = 10000000;
static constexpr size_t MAX_LOG_FILES_CNT = 10;

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
  [[nodiscard]] std::string GetIp_s() const { return std::to_string(ip_); }  // FIXME
  [[nodiscard]] uint16_t    GetPort() const { return port_; }

  /* Virtual because may be added derived class for tcp/udp server */
  virtual ~PtxChatServer();

 private:
  uint32_t ip_;       /**< Server ip (default = 0.0.0.0) */
  uint16_t port_;     /**< Server port (default = 8080) */
  int listen_q_len_;  /**< Max amount of clients in listen queue */
  int socket_;        /**< Server socket (blocking) */
  bool is_running_;   /**< True if server is running */
  int epoll_fd_;

  std::unique_ptr<ServerStorage> storage_;

  ThreadState accept_conn_thread_;                      /**< Accept client connections */
  ThreadState process_msg_thread_;                      /**< Process received messages */
  std::unique_ptr<SharedUDeque<ChatMsg>> client_msgs_;  /**< Client messages storage */

  std::mutex clients_mtx_;
  std::mutex conn_mtx_;
  std::unordered_map<int, std::shared_ptr<Connection>> connections_;
  std::unordered_map<std::string, std::shared_ptr<Client>> clients_;

  void InitSocket();
  void InitStorage();
  void Finalize();

  void AcceptClients();
  void ProcessMessages();

  void ParseClientMsg(std::unique_ptr<ChatMsg>&& msg);

  void CloseConnection(int c);
  bool AddMsgFromConn(std::shared_ptr<Connection> c);
  bool SendMsgToClient(std::shared_ptr<ChatMsg> msg, std::shared_ptr<Client> client);
  void SendMsgToAll(std::shared_ptr<ChatMsg> msg);

  /**
   * Register and set nickname
   */
  void ProcessRegMsg(std::shared_ptr<ChatMsg> msg);
  /**
   * These functions are not under any mutex
   */
  void ProcessUnregMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessPrivateMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessPublicMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessErrRegMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessErrUnregMsg(std::shared_ptr<ChatMsg> msg);
  void ProcessErrUnkMsg(std::shared_ptr<ChatMsg> msg);

  static bool CheckPortRange(uint16_t port);
};

}  // namespace ptxchat

#endif  // SERVER_SERVER_H_
