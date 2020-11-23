#include "client.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <memory>
#include <iostream>
#include <string.h>

#include "Message.h"
#include "log.h"

namespace ptxchat {

PtxChatClient::PtxChatClient() noexcept {
  server_ip_ = DEFAULT_SERVER_IP;
  server_port_ = DEFAULT_SERVER_PORT;
  nick_ = "";
  socket_ = 0;
  msg_in_ = std::make_unique<SharedUDeque<ChatMsg>>();
  msg_out_ = std::make_unique<SharedUDeque<ChatMsg>>();
  registered_ = false;
  InitRotatingLogger("PTX Client");
  InitStorage();
}

PtxChatClient::PtxChatClient(const std::string& ip, uint16_t port) noexcept {
  struct sockaddr_in ip_addr;
  inet_pton(AF_INET, ip.c_str(), &ip_addr);
  server_ip_ = ip_addr.sin_addr.s_addr;
  server_port_ = port;
  nick_ = "";
  socket_ = 0;
  msg_in_ = std::make_unique<SharedUDeque<ChatMsg>>();
  msg_out_ = std::make_unique<SharedUDeque<ChatMsg>>();
  registered_ = false;
  InitRotatingLogger("PTX Client");
  InitStorage();
}

void PtxChatClient::LogIn(const std::string& nick) {
  struct sockaddr_in serv_addr = sockaddr_in{
    AF_INET,
    htons(server_port_),
    in_addr{htonl(server_ip_)},
    {0}
  };

  int skt = socket(AF_INET, SOCK_STREAM, 0);
  if (skt < 0) {
    logger_->log(spdlog::level::critical, "LogIn: socket() " + std::string(strerror(errno)));
    return;
  }

  if (connect(skt, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
    logger_->log(spdlog::level::critical, "LogIn: connect() " + std::string(strerror(errno)));
    return;
  }

  serv_addr_ = serv_addr;
  socket_ = skt;
  nick_ = nick;
  auto msg = std::make_unique<ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::REGISTER;
  msg->hdr.buf_len = 0;

  /* Send sync message */
  SendMsgToServer(std::move(msg));

  /* Load messages */
  for (auto msg_p : storage_->GetPrivateMsgs(nick))
    ProcessIncomingPrivateMsg(msg_p);
  for (auto msg_p : storage_->GetPublicMsgs())
    ProcessIncomingPublicMsg(msg_p);

  /* Handle messages async */
  msg_in_thread_.stop = 0;
  msg_in_thread_.thread = std::thread(&PtxChatClient::ReceiveMessagesTask, this);
  msg_in_thread_.thread.detach();

  msg_out_thread_.stop = 0;
  msg_out_thread_.thread = std::thread(&PtxChatClient::SendMessagesTask, this);
  msg_out_thread_.thread.detach();
}

void PtxChatClient::SendMsg(const std::string& text) {
  auto msg = std::make_unique<ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::PUBLIC_DATA;
  msg->hdr.buf_len = text.length();
  msg->buf = reinterpret_cast<uint8_t*>(malloc(msg->hdr.buf_len));
  memcpy(msg->buf, text.data(), text.length());
  msg_out_->push_front(std::move(msg));
}

void PtxChatClient::SendMsgTo(const std::string& to, const std::string& text) {
  auto msg = std::make_unique<ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  strcpy(msg->hdr.to, to.c_str());
  msg->hdr.type = MsgType::PRIVATE_DATA;
  msg->hdr.buf_len = text.length();
  msg->buf = reinterpret_cast<uint8_t*>(malloc(msg->hdr.buf_len));
  memcpy(msg->buf, text.data(), text.length());
  msg_out_->push_front(std::move(msg));
}

void PtxChatClient::SendMessagesTask() {
  while (!msg_out_thread_.stop) {
    std::unique_ptr<ChatMsg> msg = std::move(msg_out_->back());
    if (!msg)
      return;
    std::shared_ptr<ChatMsg> s_msg(msg.release());
    SendMsgToServer(s_msg);
  }
}

void PtxChatClient::ReceiveMessagesTask() {
  while (!msg_in_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_THREAD_SLEEP));
    uint8_t buf[sizeof(struct ChatMsgHdr) + MAX_MSG_BUFFER_SIZE];
    ssize_t bytes_in = recv(socket_, buf, sizeof(buf), 0);
    if (bytes_in < 0) {
      logger_->log(spdlog::level::err, "ReceiveMessagesTask: recv() " + std::string(strerror(errno)));
      continue;
    }
    if (bytes_in == 0) {
      Stop();
      logger_->log(spdlog::level::critical, "ReceiveMessagesTask: server disconnected");
      return; // TODO: reconnect
    }

    auto msg = std::make_shared<ChatMsg>();
    msg->hdr = *reinterpret_cast<ChatMsgHdr*>(buf);
    size_t buf_len = msg->hdr.buf_len;
    if (buf_len > MAX_MSG_BUFFER_SIZE)
      continue;
    if (buf_len != 0) {
      msg->buf = reinterpret_cast<uint8_t*>(malloc(buf_len));
      memcpy(msg->buf, buf+sizeof(ChatMsgHdr), buf_len);
      std::string text = std::string(reinterpret_cast<char*>(msg->buf));
      logger_->log(spdlog::level::debug, "ReceiveMessagesTask: received message from " + std::string(msg->hdr.from) + ": " + text);
    }

    switch (msg->hdr.type) {
      case MsgType::PUBLIC_DATA:
        ProcessIncomingPublicMsg(msg);
        break;
      case MsgType::PRIVATE_DATA:
        ProcessIncomingPrivateMsg(msg);
        break;
      case MsgType::REGISTERED:
        ProcessRegisteredMsg(msg);
        break;
      case MsgType::UNREGISTERED:
        ProcessUnregisteredMsg(msg);
        break;
      default:
        ProcessErrorMsg(msg);
        break;
    }
  }
}

void PtxChatClient::ProcessIncomingPublicMsg(std::shared_ptr<ChatMsg> msg) {
  PushGuiEvent(GuiEvType::PUBLIC_MSG, msg);
}

void PtxChatClient::ProcessIncomingPrivateMsg(std::shared_ptr<ChatMsg> msg) {
  PushGuiEvent(GuiEvType::PRIVATE_MSG, msg);
}

void PtxChatClient::ProcessRegisteredMsg(std::shared_ptr<ChatMsg> msg) {
  if (strcmp(msg->hdr.from, "Server"))
    return;
  registered_ = true;
  logger_->log(spdlog::level::info, "ProcessRegisteredMsg: client registered on server");
  // TODO: push to GUI
}

void PtxChatClient::ProcessUnregisteredMsg(std::shared_ptr<ChatMsg> msg) {
  if (strcmp(msg->hdr.from, "Server"))
    return;
  registered_ = false;
  logger_->log(spdlog::level::info, "ProcessUnregisteredMsg: client unregistered on server");
  // TODO: push to GUI
}

void PtxChatClient::ProcessErrorMsg(std::shared_ptr<ChatMsg> msg) {
  logger_->log(spdlog::level::err, "ProcessErrorMsg: some error occured");
  // TODO: push to GUI
}

void PtxChatClient::LogOut() {
  if (!socket_)
    return;
  
  auto msg = std::make_shared<ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::UNREGISTER;
  msg->hdr.buf_len = 0;
  SendMsgToServer(msg);
  PushGuiEvent(GuiEvType::CLEAR, nullptr);
  shutdown(socket_, SHUT_RD);
  close(socket_);
  socket_ = 0;
}

void PtxChatClient::SendMsgToServer(std::shared_ptr<ChatMsg> msg) {
  if (send(socket_, &msg->hdr, sizeof(msg->hdr), 0) < 0) {
    logger_->log(spdlog::level::err, "SendMsgToServer: send(hdr) " + std::string(strerror(errno)));
    return;
  }

  if (!msg->hdr.buf_len)
    return;

  if (send(socket_, msg->buf, msg->hdr.buf_len, 0) < 0) {
    logger_->log(spdlog::level::err, "SendMsgToServer: send(buf) " + std::string(strerror(errno)));
    return;
  }
  std::string text = std::string(reinterpret_cast<char*>(msg->buf));
  logger_->log(spdlog::level::debug, "SendMsgToServer: send(buf) " + text);
}

bool PtxChatClient::SetIP_i(uint32_t ip) {
  server_ip_ = ip;
  return true;
}

bool PtxChatClient::SetIP_s(const std::string& ip) {
  struct in_addr ip_addr;
  if (inet_pton(AF_INET, ip.c_str(), &ip_addr) <= 0)
    return false;
  server_ip_ = ip_addr.s_addr;
  return true;
}

bool PtxChatClient::SetPort_i(uint16_t port) {
  server_port_ = port;
  return true;
}

bool PtxChatClient::SetPort_s(const std::string& port) {
  uint16_t port_i = atoi(port.c_str());
  if (!port_i)
    return false;
  server_port_ = port_i;
  return true;
}

void PtxChatClient::Stop() {
  msg_in_->stop(true);
  msg_out_->stop(true);

  msg_out_thread_.stop = 1;
  msg_in_thread_.stop = 1;
}

void PtxChatClient::InitStorage() {
  storage_ = std::make_unique<ClientStorage>();
}

PtxChatClient::~PtxChatClient() {
  Stop();
  LogOut();
}

} // namespace ptxchat
