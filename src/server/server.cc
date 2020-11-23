#include "server.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
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
#include "connections.h"
#include "log.h"

namespace ptxchat {

PtxChatServer::PtxChatServer() noexcept:
                            ip_(INADDR_ANY),
                            port_(1488),
                            listen_q_len_(DEF_LISTEN_Q_LEN),
                            socket_(0),
                            is_running_(false) {
  client_msgs_ = std::make_unique<SharedUDeque<struct ChatMsg>>();
  InitSocket();
  InitStorage();
  InitRotatingLogger("PTX Server");
}

PtxChatServer::PtxChatServer(uint32_t ip, uint16_t port) noexcept:
                            ip_(ip),
                            listen_q_len_(DEF_LISTEN_Q_LEN),
                            socket_(0),
                            is_running_(false) {
  CheckPortRange(port);
  port_ = port;
  client_msgs_ = std::make_unique<SharedUDeque<ChatMsg>>();
  InitSocket();
  InitStorage();
  InitRotatingLogger("PTX Server");
}

PtxChatServer::PtxChatServer(const std::string& ip, uint16_t port) noexcept:
                            listen_q_len_(DEF_LISTEN_Q_LEN),
                            socket_(0),
                            is_running_(false) {
  CheckPortRange(port);
  struct in_addr ip_addr;
  if (inet_pton(AF_INET, ip.c_str(), &ip_addr) <= 0) {
    std::cout << "Error: bad ip address: " << ip << std::endl;
    exit(-1);
  }
  ip_ = ip_addr.s_addr;
  port_ = port;
  client_msgs_ = std::make_unique<SharedUDeque<ChatMsg>>();
  InitSocket();
  InitStorage();
  InitRotatingLogger("PTX Server");
}

void PtxChatServer::SetListenQueueSize(int size) {
  if (size > MAX_LISTEN_Q_SIZE) {
    std::cout << "Error: max listen queue size cannot be greater than " << MAX_LISTEN_Q_SIZE << std::endl;
    exit(-1);
  }
  listen_q_len_ = size;
}

void PtxChatServer::InitSocket() {
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(ip_);

  int skt = socket(AF_INET, SOCK_STREAM, 0);
  if (skt == -1) {
    perror("socket");
    PtxChatCrash();
  }

  int reuse_addr_opt = 1, keepalive_opt = 1;
  setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt));
  setsockopt(skt, SOL_SOCKET, SO_KEEPALIVE, &keepalive_opt, sizeof(keepalive_opt));

  if (bind(skt, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    logger_->log(spdlog::level::critical, "Cannot bind socket");
    PtxChatCrash();
  }

  if (Connection::makeNonBlocking(skt) == -1) {
    logger_->log(spdlog::level::critical, "Cannot make socket nonblocking");
    PtxChatCrash();
  }

  if (listen(skt, listen_q_len_) < 0) {
    logger_->log(spdlog::level::critical, "Cannot listen on socket");
    PtxChatCrash();
  }

  socket_ = skt;
}

bool PtxChatServer::SetIP_i(uint32_t ip) {
  ip_ = ip;
  return true;
}

bool PtxChatServer::SetIP_s(const std::string& ip) {
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
  if (is_running_) {
    logger_->log(spdlog::level::err, "Cannot start server: server is already running");
    return;
  }
  is_running_ = true;
  client_msgs_->stop(false);

  accept_conn_thread_.stop = 0;
  accept_conn_thread_.thread = std::thread(&PtxChatServer::AcceptClients, this);
  accept_conn_thread_.thread.detach();

  process_msg_thread_.stop = 0;
  process_msg_thread_.thread = std::thread(&PtxChatServer::ProcessMessages, this);
  process_msg_thread_.thread.detach();

  PushGuiEvent(GuiEvType::SRV_START, nullptr);
  logger_->log(spdlog::level::info, "Server started");
}

void PtxChatServer::AcceptClients() {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ == -1) {
    logger_->log(spdlog::level::critical, "AcceptClients cannot create epoll");
    PtxChatCrash();
  }
  if (Connection::addEventToEpoll(epoll_fd_, socket_, EPOLLIN) == -1) {
    logger_->log(spdlog::level::critical, "AcceptClients cannot add EPOLLIN event to connection socket");
    PtxChatCrash();
  }

  epoll_event* events = (epoll_event*)calloc(MAX_EVENTS_NUM, sizeof(epoll_event));
  logger_->log(spdlog::level::debug, "AcceptClients thread started");
  while (!accept_conn_thread_.stop) {
    int ev_num = epoll_wait(epoll_fd_, events, MAX_EVENTS_NUM, 100);
    if (ev_num == -1 && errno != EINTR) {
      logger_->log(spdlog::level::critical, "AcceptClients error in epoll: " + std::string(strerror(errno)));
      PtxChatCrash();
    }

    for (int i = 0; i < ev_num; ++i) {
      if (events[i].events & (EPOLLHUP | EPOLLERR)) {
        if (events[i].data.fd == socket_) {
          logger_->log(spdlog::level::critical, "AcceptClients error connection socket: " + std::string(strerror(errno)));
          PtxChatCrash();
        }
        auto conn = connections_[events[i].data.fd];
        CloseConnection(conn->GetSocket());
        continue;
      }

      int event_fd = events[i].data.fd;
      if (events[i].events & EPOLLIN) {
        if (event_fd == socket_) {
          while (1) {
            sockaddr_in cl_addr;
            socklen_t cl_len = sizeof(cl_addr);
            int cl_fd = accept(socket_, reinterpret_cast<sockaddr*>(&cl_addr), &cl_len);
            if (cl_fd < 0) {
              if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
              logger_->log(spdlog::level::critical, strerror(errno));
              PtxChatCrash();
            }
            Connection::makeNonBlocking(cl_fd);// todo: handle errors
            Connection::addEventToEpoll(epoll_fd_, cl_fd, EPOLLIN);
            logger_->log(spdlog::level::info, "Client " + std::to_string(cl_addr.sin_addr.s_addr) + ":" +
                        std::to_string(cl_addr.sin_port) + ", skt " + std::to_string(cl_fd) + " accepted");
            auto conn = std::make_shared<Connection>(cl_fd, cl_addr.sin_addr.s_addr, cl_addr.sin_port);
            std::unique_lock<std::mutex> lc(conn_mtx_);
            connections_.emplace(cl_fd, conn);
          }
          continue;
        }

        auto conn = connections_[event_fd];
        if (conn->Status() != ConnStatus::UP) {
          CloseConnection(conn->GetSocket());
          continue;
        }
        if (!AddMsgFromConn(conn))
          CloseConnection(event_fd);
      }
    }
  }
  free(events);
  logger_->log(spdlog::level::debug, "AcceptClients thread finished");
}

bool PtxChatServer::AddMsgFromConn(std::shared_ptr<Connection> conn) {
  auto msg = Connection::RecvMsgFromConn(conn);
  if (!msg) {
    if (conn->Status() == ConnStatus::UP)
      return true;
    return false;
  }
  client_msgs_->push_front(std::move(msg));
  return true;
}

void PtxChatServer::ProcessMessages() {
  logger_->log(spdlog::level::debug, "ProcessMessages thread started");
  while (!process_msg_thread_.stop) {
    std::unique_ptr<struct ChatMsg> msg = std::move(client_msgs_->back());
    if (!msg) {
      logger_->log(spdlog::level::debug, "Client messages queue stopped");
      return;
    }
    ParseClientMsg(std::move(msg));
  }
  logger_->log(spdlog::level::debug, "ProcessMessages thread finished");
}

void PtxChatServer::ProcessRegMsg(std::shared_ptr<ChatMsg> msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;

  std::unique_lock<std::mutex> lc_cl(clients_mtx_);
  auto res = clients_.find(nick);
  if (res != clients_.end()) {
    logger_->log(spdlog::level::info, "Client already registered with given nickname: " + std::string(nick));
    return;
  }

  std::unique_lock<std::mutex> lc_conn(conn_mtx_);
  for (auto it = connections_.begin(); it != connections_.end(); ++it) {
    auto conn = it->second;
    if (conn->GetIP() == ip && conn->GetPort() == port) {
      auto reply = std::make_shared<ChatMsg>();
      auto client = std::make_shared<Client>(conn);
      if (!client->Register(nick)) {
        logger_->log(spdlog::level::info, "Cannot register client with given nickname: " + std::string(nick));
        reply->hdr = ChatMsgHdr{MsgType::ERR_REGISTERED, ip_, port_, "ChatServer", "", 0};
      } else {
        clients_.emplace(nick, client);
        reply->hdr = ChatMsgHdr{MsgType::REGISTERED, ip_, port_, "ChatServer", "", 0};
        PushGuiEvent(GuiEvType::CLIENT_REG, reply);
        logger_->log(spdlog::level::info, "Client registered: " + std::string(nick));
      }
      std::strcpy(reply->hdr.from, nick);
      SendMsgToClient(reply, client);
      return;
    }
  }

  logger_->log(spdlog::level::err, "Cannot register client " + std::string(nick) + ": no such connection");
}

void PtxChatServer::ProcessUnregMsg(std::shared_ptr<ChatMsg> msg) {
  char* nick = msg->hdr.from;
  uint32_t ip = msg->hdr.src_ip;
  uint16_t port = msg->hdr.src_port;

  std::unique_lock<std::mutex> lc(clients_mtx_);
  auto res = clients_.find(std::string(nick));
  if (res == clients_.end()) {
    logger_->log(spdlog::level::err, "Cannot unregister client " + std::string(nick) + ": client not found");
    return;
  }
  auto client = res->second;
  if (!client->IsRegistered()) {
    logger_->log(spdlog::level::err, "Cannot unregister client " + std::string(nick) + ": already unregistered");
    return;
  }
  if (client->GetIp() == ip && client->GetPort() == port) {
    client->Unregister();
    clients_.erase(res);
    auto gui_repl = std::make_shared<ChatMsg>();
    strcpy(gui_repl->hdr.from, nick);
    PushGuiEvent(GuiEvType::CLIENT_UNREG, gui_repl);
    logger_->log(spdlog::level::info, "Client " + std::string(nick) + " unregistered");
    return;
  }

  logger_->log(spdlog::level::err, "Cannot unregister client " + std::string(nick) + ": was registered from another address");
}

void PtxChatServer::ProcessPrivateMsg(std::shared_ptr<ChatMsg> msg) {
  std::unique_lock<std::mutex> lc(clients_mtx_);
  auto from = clients_.find(msg->hdr.from);
  if (from == clients_.end()) {
    logger_->log(spdlog::level::err, "Cannot send private message from " + std::string(msg->hdr.from) + ": client not found");
    return;
  }
  if (!from->second->IsRegistered()) {
    logger_->log(spdlog::level::err, "Cannot send private message from " + std::string(msg->hdr.from) + ": client not registered");
    return;
  }

  auto to = clients_.find(msg->hdr.to);
  if (to == clients_.end()) {
    logger_->log(spdlog::level::info, "Cannot send private message to " + std::string(msg->hdr.to) + ": client not found");
    return;
  }
  auto client = to->second;
  if (!client->IsRegistered()) {
    logger_->log(spdlog::level::info, "Cannot send private message to " + std::string(msg->hdr.to) + ": client not registered");
    return;
  }
  clients_mtx_.unlock();

  if (!SendMsgToClient(msg, to->second))
    client->GetConnection()->Status() = ConnStatus::ERROR;
  else
    storage_->AddPrivateMsg(msg);
}

void PtxChatServer::ProcessPublicMsg(std::shared_ptr<ChatMsg> msg) {
  char* nick = msg->hdr.from;

  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  auto client = clients_.find(nick);
  if (client == clients_.end()) {
    logger_->log(spdlog::level::info, "Cannot send public message from " + std::string(msg->hdr.from) + ": client not found");
    return;
  }
  if (!client->second->IsRegistered()) {
    logger_->log(spdlog::level::info, "Cannot send public message from " + std::string(msg->hdr.from) + ": client not registered");
    return;
  }
  clients_mtx_.unlock();

  SendMsgToAll(msg);
  storage_->AddPublicMsg(msg);
}

void PtxChatServer::ParseClientMsg(std::unique_ptr<ChatMsg>&& msg) {
  MsgType t = msg->hdr.type;
  std::shared_ptr<ChatMsg> s_msg(msg.release());
  switch (t) {
    case MsgType::REGISTER:
      ProcessRegMsg(s_msg);
      break;
    case MsgType::UNREGISTER:
      ProcessUnregMsg(s_msg);
      break;
    case MsgType::PRIVATE_DATA:
      ProcessPrivateMsg(s_msg);
      break;
    case MsgType::PUBLIC_DATA:
      ProcessPublicMsg(s_msg);
      break;
    case MsgType::ERR_UNKNOWN:
      break;
    default:
      break;
  }
}

bool PtxChatServer::SendMsgToClient(std::shared_ptr<ChatMsg> msg, std::shared_ptr<Client> client) {
  bool res = Connection::SendMsgToConn(msg, client->GetConnection());
  if (res)
    PushGuiEvent(GuiEvType::PRIVATE_MSG, std::move(msg));
  return res;
}

void PtxChatServer::SendMsgToAll(std::shared_ptr<ChatMsg> msg) {
  std::unique_lock<std::mutex> lc_storage(clients_mtx_);
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    auto client = it->second;
    int c_fd = client->GetSocket();
    ssize_t hdr_bytes_sent = send(c_fd, &msg->hdr, sizeof(ChatMsgHdr), 0);
    if (hdr_bytes_sent == 0) {
      logger_->log(spdlog::level::info, "Cannot send public message from " + std::string(msg->hdr.from) + ": client disconnected");
      continue;
    }
    if (hdr_bytes_sent < 0) {
      if (errno == ECONNRESET) {
        logger_->log(spdlog::level::err, "Cannot send public message from " + std::string(msg->hdr.from) + ": connection reset");
        continue;
      }
      logger_->log(spdlog::level::err, "Cannot send public message from " + std::string(msg->hdr.from) + ": " + strerror(errno));
      continue;
    }

    ssize_t buf_bytes_sent = send(c_fd, msg->buf, msg->hdr.buf_len, 0);
    if (buf_bytes_sent == 0) {
      logger_->log(spdlog::level::info, "Cannot send public message from " + std::string(msg->hdr.from) + ": client disconnected");
      continue;
    }
    if (buf_bytes_sent < 0) {
      if (errno == ECONNRESET) {
        logger_->log(spdlog::level::err, "Cannot send public message from " + std::string(msg->hdr.from) + ": connection reset");
        continue;
      }
      logger_->log(spdlog::level::err, "Cannot send public message from " + std::string(msg->hdr.from) + ": " + strerror(errno));
      continue;
    }
  }

  logger_->log(spdlog::level::info, "Public message from " + std::string(msg->hdr.from) + ": sent");
  PushGuiEvent(GuiEvType::PUBLIC_MSG, msg);
}

void PtxChatServer::Stop() {
  if (!is_running_) {
    logger_->log(spdlog::level::err, "Cannot stop server: server already stopped");
    return;
  }
  is_running_ = false;

  accept_conn_thread_.stop = 1;
  process_msg_thread_.stop = 1;

  connections_.clear();
  clients_.clear();
  client_msgs_->stop(true);

  logger_->log(spdlog::level::info, "Server stopped");
  PushGuiEvent(GuiEvType::SRV_STOP, nullptr);
}

void PtxChatServer::Finalize() {
  Stop();
  if (socket_) {
    shutdown(socket_, SHUT_RDWR);
    close(socket_);
  }
}

void PtxChatServer::CloseConnection(int c) {
  std::unique_lock<std::mutex> lc(conn_mtx_);
  auto conn = connections_.find(c);
  if (conn == connections_.end())
    return;
  shutdown(conn->second->GetSocket(), SHUT_RD);
  close(conn->second->GetSocket());
  connections_.erase(conn);
}

bool PtxChatServer::CheckPortRange(uint16_t port) {
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

void PtxChatServer::InitStorage() {
  storage_ = std::make_unique<ServerStorage>();
}

PtxChatServer::~PtxChatServer() {
  Finalize();
}

}  // namespace ptxchat
