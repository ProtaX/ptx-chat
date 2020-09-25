#include "PtxChatServer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <memory>
#include <chrono>
#include <string>
#include <fstream>

#include "Message.h"

namespace ptxchat {

PtxChatServer::PtxChatServer() noexcept {
  ip_ = INADDR_ANY;
  port_ = 1488;
  listen_q_size_ = 100;
  socket_ = 0;
  use_log_ = true;
  stop_ = true;
  client_msgs_ = std::make_unique<SharedUDeque<struct ChatMsg>>();
  InitSocket();
  InitLog();
}

PtxChatServer::PtxChatServer(uint32_t ip, uint16_t port) noexcept {
  CheckPortRange(port);
  ip_ = ip;
  port_ = port;
  listen_q_size_ = 100;
  socket_ = 0;
  use_log_ = true;
  stop_ = true;
  client_msgs_ = std::make_unique<SharedUDeque<struct ChatMsg>>();
  InitSocket();
  InitLog();
}

PtxChatServer::PtxChatServer(const std::string& ip, uint16_t port) noexcept {
  CheckPortRange(port);
  struct in_addr ip_addr;
  if (inet_pton(AF_INET, ip.c_str(), &ip_addr) <= 0) {
    std::cout << "Error: bad ip address: " << ip << std::endl;
    exit(-1);
  }
  ip_ = ip_addr.s_addr;
  port_ = port;
  socket_ = 0;
  listen_q_size_ = 100;
  use_log_ = true;
  client_msgs_ = std::make_unique<SharedUDeque<struct ChatMsg>>();
  InitSocket();
  InitLog();
}

void PtxChatServer::SetListenQueueSize(int size) {
  if (size > MAX_LISTEN_Q_SIZE) {
    std::cout << "Error: max listen queue size cannot be greater than " << MAX_LISTEN_Q_SIZE << std::endl;
    exit(-1);
  }
  listen_q_size_ = size;
}

void PtxChatServer::InitSocket() {
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(ip_);

  int skt = socket(AF_INET, SOCK_STREAM, 0);
  if (skt == -1) {
    perror("socket");
    exit(-1);
  }

  int reuse_addr_opt = 1, keepalive_opt = 1;
  setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt));
  setsockopt(skt, SOL_SOCKET, SO_KEEPALIVE, &keepalive_opt, sizeof(keepalive_opt));

  if (bind(skt, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("bind");
    exit(-1);
  }

  if (listen(skt, listen_q_size_) < 0) {
    perror("listen");
    exit(-1);
  }

  socket_ = skt;
}

bool PtxChatServer::SetIP_i(uint32_t ip) {
  if (!accept_conn_thread_.stop)
    return false;
  ip_ = ip;
  return true;
}

bool PtxChatServer::SetIP_s(const std::string& ip) {
  if (!accept_conn_thread_.stop)
    return false;
  struct in_addr ip_addr;
  if (inet_pton(AF_INET, ip.c_str(), &ip_addr) <= 0)
    return false;
  ip_ = ip_addr.s_addr;
  return true;
}

bool PtxChatServer::SetPort_i(uint16_t port) {
  bool res = CheckPortRange(port);
  port_ = port;
  return res;
}

bool PtxChatServer::SetPort_s(const std::string& port) {
  uint16_t port_i = atoi(port.c_str());
  if (!port_i)
    return false;

  bool res = CheckPortRange(port_i);
  port_ = port_i;
  return res;
}

void PtxChatServer::Start() {
  if (!stop_)
    return;
  stop_ = false;
  client_msgs_->stop(false);

  accept_conn_thread_.stop = 0;
  accept_conn_thread_.thread = std::thread(&PtxChatServer::AcceptConnections, this);
  accept_conn_thread_.thread.detach();

  receive_msg_thread_.stop = 0;
  receive_msg_thread_.thread = std::thread(&PtxChatServer::ReceiveMessages, this);
  receive_msg_thread_.thread.detach();

  process_msg_thread_.stop = 0;
  process_msg_thread_.thread = std::thread(&PtxChatServer::ProcessMessages, this);
  process_msg_thread_.thread.detach();

  PushGuiEvent(GuiEvType::SRV_START, nullptr);
}

void PtxChatServer::AcceptConnections() {
  Log("AcceptConnections: started");
  while (!accept_conn_thread_.stop) {
    struct sockaddr_in cl_addr;
    socklen_t cl_len = sizeof(cl_addr);
    int cl_fd = accept(socket_, reinterpret_cast<struct sockaddr*>(&cl_addr), &cl_len);
    // TODO(me): error handling for known sockets - forget clients
    if (cl_fd < 0) {
      if (accept_conn_thread_.stop)
        return;
      perror("accept");
      return;
    }
    Log("Client accepted");
    std::unique_ptr<Client> client = std::make_unique<Client>(cl_fd, cl_addr.sin_addr.s_addr, cl_addr.sin_port);
    accepted_clients_.emplace(cl_fd, std::move(client));
  }
  Log("AcceptConnections: finished");
}

bool PtxChatServer::RecvMsgFromClient(std::unique_ptr<Client>& cl) {
  int client_fd = cl->GetSocket();
  uint8_t raw_hdr[sizeof(struct ChatMsgHdr)];
  ssize_t rec_bytes_hdr = recv(client_fd, raw_hdr, sizeof(raw_hdr), MSG_DONTWAIT);
  /* Client disconnected */
  if (rec_bytes_hdr == 0) {
    Log("Client disconnected");
    return false;
  }
  if (rec_bytes_hdr < 0) {
    /* No data */
    if (errno == EAGAIN)
      return true;

    /* Connection lost */
    if (errno == ECONNREFUSED || errno == ENOTCONN || errno == ENOTSOCK) {
      Log("Client connection lost");
      return false;
    }

    Stop();  // TODO(me): crash, not stop
    return true;
  }

  Log("Client msg receicved");
  struct ChatMsgHdr* hdr = reinterpret_cast<struct ChatMsgHdr*>(raw_hdr);
  size_t buf_len = hdr->buf_len;
  if (buf_len > MAX_MSG_BUFFER_SIZE)
    Log("ReceiveMessages: Warning: message buffer is too big");
  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();

  struct sockaddr_in cl_addr;
  socklen_t sock_len = sizeof(cl_addr);
  getpeername(client_fd, reinterpret_cast<struct sockaddr*>(&cl_addr), &sock_len);
  msg->hdr = *hdr;
  msg->hdr.src_ip = cl_addr.sin_addr.s_addr;
  msg->hdr.src_port = cl_addr.sin_port;

  if (buf_len != 0) {
    msg->buf = reinterpret_cast<uint8_t*>(malloc(buf_len));
    ssize_t rec_bytes_buf = recv(client_fd, msg->buf, buf_len, 0);
    if (rec_bytes_buf < 0)
      return true;
  }

  client_msgs_->push_front(std::move(msg));
  return true;
}

void PtxChatServer::ReceiveMessages() {
  Log("ReceiveMessages: started");
  while (!receive_msg_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_TICK));

    std::unique_lock<std::mutex> lc_storage(clients_mtx_);
    for (auto it = accepted_clients_.begin(); it != accepted_clients_.end(); ) {
      bool res = RecvMsgFromClient(it->second);
      if (!res)
        it = accepted_clients_.erase(it);
      else
        ++it;
    }
    for (auto it = registered_clients_.begin(); it != registered_clients_.end(); ) {
      bool res = RecvMsgFromClient(it->second);
      if (!res)
        it = registered_clients_.erase(it);
      else
        ++it;
    }
  }
  Log("ReceiveMessages: finished");
}

void PtxChatServer::ProcessMessages() {
  Log("ProcessMessages: started");
  while (!process_msg_thread_.stop) {
    std::unique_ptr<struct ChatMsg> msg = std::move(client_msgs_->back());
    if (!msg)
      return;
    ParseClientMsg(std::move(msg));
  }
  Log("ProcessMessages: finished");
}

void PtxChatServer::ProcessRegMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;

  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  auto res = registered_clients_.find(nick);
  if (res != registered_clients_.end()) {
    Log("ProcessRegMsg: error: client alredy registered with given nickname");
    return;
  }

  for (auto it = accepted_clients_.begin(); it != accepted_clients_.end(); ++it) {
    std::unique_ptr<Client>& client = it->second;
    if (client->GetIp() == ip && client->GetPort() == port) {
      std::unique_ptr<struct ChatMsg> reply = std::make_unique<struct ChatMsg>();
      if (!client->Register(nick)) {
        Log("ProcessRegMsg: error: bad client nickanme");
        return;  // TODO(me): send error to client
      }

      registered_clients_.emplace(nick, std::move(client));
      accepted_clients_.erase(it);

      reply->hdr = ChatMsgHdr{MsgType::ERR_REGISTERED, ip_, port_, "ChatServer", "", 0};
      strcpy(reply->hdr.from, nick);
      PushGuiEvent(GuiEvType::CLIENT_REG, std::move(reply));
      Log("ProcessRegMsg: client registered");
      return;
    }
  }

  Log("ProcessRegMsg: error: client not found");
}

void PtxChatServer::ProcessUnregMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;

  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  auto res = registered_clients_.find(std::string(nick));
  if (res == registered_clients_.end()) {
    Log("ProcessUnregMsg: error: client not found");
    return;
  }
  std::unique_ptr<Client>& client = res->second;
  if (client->GetIp() == ip && client->GetPort() == port) {
    client->Unregister();
    accepted_clients_.emplace(client->GetSocket(), std::move(client));
    registered_clients_.erase(res);

    std::unique_ptr<struct ChatMsg> gui_repl = std::make_unique<struct ChatMsg>();
    strcpy(gui_repl->hdr.from, nick);
    PushGuiEvent(GuiEvType::CLIENT_UNREG, std::move(gui_repl));
    Log("ProcessUnregMsg: client unregistered");
    return;
  }

  Log("ProcessUnregMsg: error: fot unreg msg from another address");
}

void PtxChatServer::ProcessPrivateMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  auto from = registered_clients_.find(msg->hdr.from);
  if (from == registered_clients_.end()) {
    Log("ProcessPrivateMsg: error: from not found");
    return;
  }

  auto to = registered_clients_.find(msg->hdr.to);
  if (to == registered_clients_.end()) {
    Log("ProcessPrivateMsg: error: to not found");
    return;
  }
  clients_mtx_.unlock();

  if (!SendMsgToClient(std::move(msg), to->second))
    registered_clients_.erase(to);
}

void PtxChatServer::ProcessPublicMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  char* nick = msg->hdr.from;

  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  if (registered_clients_.find(nick) == registered_clients_.end()) {
    Log("ProcessPublicMsg: Error: client not found");
    return;
  }
  clients_mtx_.unlock();

  SendMsgToAll(std::move(msg));
}

void PtxChatServer::ParseClientMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  MsgType t = msg->hdr.type;
  switch (t) {
    case MsgType::REGISTER:
      ProcessRegMsg(std::move(msg));
      break;
    case MsgType::UNREGISTER:
      ProcessUnregMsg(std::move(msg));
      break;
    case MsgType::PRIVATE_DATA:
      ProcessPrivateMsg(std::move(msg));
      break;
    case MsgType::PUBLIC_DATA:
      ProcessPublicMsg(std::move(msg));
      break;
    case MsgType::ERR_UNREGISTERED:
    case MsgType::ERR_REGISTERED:
    case MsgType::ERR_UNKNOWN:
      break;
  }
}

bool PtxChatServer::SendMsgToClient(std::unique_ptr<struct ChatMsg>&& msg, std::unique_ptr<Client>& client) {
  if (send(client->GetSocket(), &msg->hdr, sizeof(struct ChatMsgHdr), 0) < 0) {
    if (errno == ECONNRESET)
      return false;
    Log("SendMsgToClient: error: send hdr");
    return false;
  }

  if (msg->hdr.buf_len) {
    if (send(client->GetSocket(), msg->buf, msg->hdr.buf_len, 0) < 0) {
      if (errno == ECONNRESET)
        return false;
      Log("SendMsgToClient: error: send buf");
      return false;
    }
  }

  PushGuiEvent(GuiEvType::PRIVATE_MSG, std::move(msg));
  Log("SendMsgToClient: message sent");
  return true;
}

void PtxChatServer::SendMsgToAll(std::unique_ptr<struct ChatMsg>&& msg) {
  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  for (auto it = registered_clients_.begin(); it != registered_clients_.end(); ++it) {
    std::unique_ptr<Client>& client = it->second;
    int c_fd = client->GetSocket();
    ssize_t sent_hdr_bytes = send(c_fd, &msg->hdr, sizeof(struct ChatMsgHdr), 0);
    if (sent_hdr_bytes < 0) {
      if (errno == ECONNRESET) {
        it = registered_clients_.erase(it);
        continue;
      }
      Log("SendMsgToClient: error: send hdr");
      continue;
    }

    ssize_t sent_buf_bytes = send(c_fd, msg->buf, msg->hdr.buf_len, 0);
    if (sent_buf_bytes < 0) {
      if (errno == ECONNRESET) {
        it = registered_clients_.erase(it);
        continue;
      }
      Log("SendMsgToClient: error: send buf");
      continue;
    }
  }

  PushGuiEvent(GuiEvType::PUBLIC_MSG, std::move(msg));
  Log("SendMsgToAll: message sent");
}

void PtxChatServer::Stop() {
  if (stop_)
    return;
  stop_ = true;

  accept_conn_thread_.mtx.lock();
  accept_conn_thread_.stop = 1;
  accept_conn_thread_.mtx.unlock();

  receive_msg_thread_.mtx.lock();
  receive_msg_thread_.stop = 1;
  receive_msg_thread_.mtx.unlock();

  process_msg_thread_.mtx.lock();
  process_msg_thread_.stop = 1;
  process_msg_thread_.mtx.unlock();

  accepted_clients_.clear();
  registered_clients_.clear();
  client_msgs_->stop(true);
  // client_msgs_->clear();

  PushGuiEvent(GuiEvType::SRV_STOP, nullptr);
}

void PtxChatServer::Finalize() {
  log_file_.close();
  Stop();
  if (socket_) {
    shutdown(socket_, SHUT_RDWR);
    close(socket_);
  }
}

void PtxChatServer::InitLog() {
  log_file_.open("server.log");
}

void PtxChatServer::Log(const char* msg) {
  std::lock_guard<std::mutex> lc(log_mtx_);
  if (use_log_) {
    char time_s[32];
    time_t log_time = time(0);
    struct tm log_local_time;
    localtime_r(&log_time, &log_local_time);
    strftime(time_s, 32, "[%d-%m-%Y %H:%M:%S] ", &log_local_time);
    std::thread::id t = std::this_thread::get_id();
    log_file_ << time_s << "[" << t << "] " << msg << std::endl;
  }
}

PtxChatServer::~PtxChatServer() {
  Finalize();
}

}  // namespace ptxchat
