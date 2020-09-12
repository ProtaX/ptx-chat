#include "PtxChatServer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>

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

  int opt_val = 1;
  setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

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

bool PtxChatServer::SetIpPort_i(uint32_t ip, uint16_t port) {
  if (!accept_conn_thread_.stop)
    return false;
  ip_ = ip;
  port_ = port;
  return true;
}

bool PtxChatServer::SetIpPort_s(const std::string& ip, uint16_t port) {
  if (!accept_conn_thread_.stop)
    return false;
  struct in_addr ip_addr;
  if (inet_pton(AF_INET, ip.c_str(), &ip_addr) <= 0)
    return false;
  ip_ = ip_addr.s_addr;
  port_ = port;
  return true;
}

void PtxChatServer::Start() {
  accept_conn_thread_.stop = 0;
  accept_conn_thread_.thread = std::thread(&PtxChatServer::AcceptConnections, this);
  accept_conn_thread_.thread.detach();

  receive_msg_thread_.stop = 0;
  receive_msg_thread_.thread = std::thread(&PtxChatServer::ReceiveMessages, this);
  receive_msg_thread_.thread.detach();

  process_msg_thread_.stop = 0;
  process_msg_thread_.thread = std::thread(&PtxChatServer::ProcessMessages, this);
  process_msg_thread_.thread.detach();
}

void PtxChatServer::AcceptConnections() {
  Log("AcceptConnections: started");
  while (!accept_conn_thread_.stop) {
    struct sockaddr_in cl_addr;
    socklen_t cl_len = sizeof(cl_addr);
    int cl_fd = accept(socket_, reinterpret_cast<struct sockaddr*>(&cl_addr), &cl_len);
    // TODO(me): error handling for known sockets - forget clients
    if (cl_fd < 0) {
      perror("accept");
      return;
    }
    std::lock_guard<std::mutex> lc(receive_msg_thread_.mtx);
    clients_.emplace_back(cl_fd, cl_addr.sin_addr.s_addr, cl_addr.sin_port);
  }
  Log("AcceptConnections: finished");
}

void PtxChatServer::ReceiveMessages() {
  Log("ReceiveMessages: started");
  while (!receive_msg_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_TICK));
    std::lock_guard<std::mutex> lc_recv_msg(receive_msg_thread_.mtx);
    for (auto& cl : clients_) {
      int client_fd = cl.GetSocket();
      uint8_t raw_hdr[sizeof(struct ChatMsgHdr)];
      if (recv(client_fd, raw_hdr, sizeof(raw_hdr), 0) < 0) {
        perror("recv ChatMsgHdr");
        // TODO(me): handle recv errors
        return;
      }

      struct ChatMsgHdr* hdr = reinterpret_cast<struct ChatMsgHdr*>(raw_hdr);
      size_t buf_len = hdr->buf_len;
      if (buf_len > MAX_MSG_BUFFER_SIZE)
        Log("ReceiveMessages: Warning: message buffer is too big");
      std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
      msg->hdr = *hdr;
      msg->buf = reinterpret_cast<uint8_t*>(malloc(buf_len));

      uint8_t raw_data[MAX_MSG_BUFFER_SIZE];
      if (recv(client_fd, raw_data, buf_len, 0) < 0) {
        perror("recv ChatMsg");
        return;
      }
      memcpy(msg->buf, raw_data, buf_len);
      std::lock_guard<std::mutex> lc_proc_msg(process_msg_thread_.mtx);
      client_msgs_.push_back(std::move(msg));
    }
  }
  Log("ReceiveMessages: finished");
}

void PtxChatServer::ProcessMessages() {
  Log("ProcessMessages: started");
  while (!receive_msg_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_TICK));
    process_msg_thread_.mtx.lock();
    if (client_msgs_.empty()) {
      process_msg_thread_.mtx.unlock();
      return;
    }
    std::unique_ptr<struct ChatMsg> msg = std::move(client_msgs_.back());
    client_msgs_.pop_back();
    process_msg_thread_.mtx.unlock();
    ParseClientMsg(std::move(msg));
  }
  Log("ProcessMessages: finished");
}

void PtxChatServer::ProcessRegMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;
  for (auto& client : clients_) {
    if (client.GetIp() == ip && client.GetPort() == port) {
      if (client.IsRegistered()) {
        std::unique_ptr<struct ChatMsg> reply = std::make_unique<struct ChatMsg>();
        reply->buf = nullptr;
        reply->hdr = ChatMsgHdr{MsgType::ERR_REGISTERED, ip_, port_, "ChatServer", "", 0};
        strcpy(reply->hdr.to, nick);
        SendMsgToClient(std::move(reply), client);
        Log("ProcessRegMsg: error: client is already registered");
        return;
      }
      if (!client.SetNickname(nick)) {
        Log("ProcessRegMsg: error: bad client nickanme");
        return;
      }
      client.Register();
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
  for (auto& client : clients_) {
    if (client.GetIp() == ip && client.GetPort() == port &&
        strcmp(nick, client.GetNickname()) == 0) {
      client.Unregister();
      Log("ProcessUnregMsg: client unregistered");
      return;
    }
  }
  Log("ProcessUnregMsg: error: client not found");
}

void PtxChatServer::ProcessPrivateMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;
  bool from_found = false;
  bool to_found = false;
  Client* to;

  for (auto& client : clients_) {
    if (client.GetIp() == ip && client.GetPort() == port &&
        strcmp(nick, client.GetNickname()) == 0) {
      from_found = true;
    }
    if (strcmp(msg->hdr.to, client.GetNickname()) == 0) {
      to_found = true;
      to = &client;
    }

    if (from_found && to_found)
      break;
  }

  if (!from_found || !to_found) {
    Log("ProcessPrivateMsg: Error: from or to not found");
    return;
  }

  SendMsgToClient(std::move(msg), *to);
}

void PtxChatServer::ProcessPublicMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;
  bool from_found = false;

  for (auto& client : clients_) {
    if (client.GetIp() == ip && client.GetPort() == port &&
        strcmp(nick, client.GetNickname()) == 0) {
      from_found = true;
    }
  }

  if (!from_found) {
    Log("ProcessPublicMsg: Error: client not found");
    return;
  }

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

void PtxChatServer::SendMsgToClient(std::unique_ptr<struct ChatMsg>&& msg, const Client& c) {
  int c_fd = c.GetSocket();
  if (send(c_fd, msg->buf, msg->hdr.buf_len, 0) < 0) {
    // TODO(me): handle errors - forget client?
    Log("SendMsgToClient: error: send");
    return;
  }
  Log("SendMsgToClient: message sent");
}

void PtxChatServer::SendMsgToAll(std::unique_ptr<struct ChatMsg>&& msg) {
  for (auto& client : clients_) {
    if (!client.IsRegistered())
      continue;
    int c_fd = client.GetSocket();
    size_t msg_size = sizeof(struct ChatMsg) + msg->hdr.buf_len;
    if (send(c_fd, msg.get(), msg_size, 0) < 0) {
      // TODO(me): handle errors - forget client?
      Log("SendMsgToClient: error: send");
      return;
    }
  }
  Log("SendMsgToAll: message sent");
}

void PtxChatServer::Stop() {
  accept_conn_thread_.mtx.lock();
  accept_conn_thread_.stop = 1;
  accept_conn_thread_.mtx.unlock();

  receive_msg_thread_.mtx.lock();
  receive_msg_thread_.stop = 1;
  receive_msg_thread_.mtx.unlock();

  process_msg_thread_.mtx.lock();
  process_msg_thread_.stop = 1;
  process_msg_thread_.mtx.unlock();
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
  if (use_log_)
    log_file_ << msg << "\n";
}

PtxChatServer::~PtxChatServer() {
  Finalize();
}

}  // namespace ptxchat
