#include "PtxChatClient.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <memory>
#include <iostream>

#include "Message.h"

namespace ptxchat {

PtxChatClient::PtxChatClient() noexcept {
  server_ip_ = DEFAULT_SERVER_IP;
  server_port_ = DEFAULT_SERVER_PORT;
  nick_ = "";
  socket_ = 0;
  InitLog();
}

PtxChatClient::PtxChatClient(const std::string& ip, uint16_t port) noexcept {
  struct sockaddr_in ip_addr;
  inet_pton(AF_INET, ip.c_str(), &ip_addr);
  server_ip_ = ip_addr.sin_addr.s_addr;
  server_port_ = port;
  nick_ = "";
  socket_ = 0;
  InitLog();
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
    perror("LogIn: socket");
    return;
  }

  if (connect(skt, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
    perror("LogIn: connect");
    return;
  }

  serv_addr_ = serv_addr;
  socket_ = skt;
  nick_ = nick;
  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::REGISTER;
  msg->hdr.buf_len = 0;

  /* Send sync message */
  SendMsgToServer(std::move(msg));

  /* Handle messages async */
  msg_in_thread_.stop = 0;
  msg_in_thread_.thread = std::thread(&PtxChatClient::ProcessReceiveMessages, this);
  msg_in_thread_.thread.detach();

  msg_out_thread_.stop = 0;
  msg_out_thread_.thread = std::thread(&PtxChatClient::ProcessSendMessages, this);
  msg_out_thread_.thread.detach();
}

void PtxChatClient::SendMsg(const std::string& text) {
  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::PUBLIC_DATA;
  msg->hdr.buf_len = text.length();
  msg->buf = reinterpret_cast<uint8_t*>(malloc(msg->hdr.buf_len));
  memcpy(msg->buf, text.data(), text.length());
  msg_out_.push_front(std::move(msg));
}

void PtxChatClient::SendMsgTo(const std::string& to, const std::string& text) {
  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  strcpy(msg->hdr.to, to.c_str());
  msg->hdr.type = MsgType::PRIVATE_DATA;
  msg->hdr.buf_len = text.length();
  msg->buf = reinterpret_cast<uint8_t*>(malloc(msg->hdr.buf_len));
  memcpy(msg->buf, text.data(), text.length());
  msg_out_.push_front(std::move(msg));
}

void PtxChatClient::ProcessSendMessages() {
  while (!msg_out_thread_.stop) {
    std::unique_ptr<struct ChatMsg> msg = std::move(msg_out_.back());
    if (!msg)
      return;
    SendMsgToServer(std::move(msg));
  }
}

void PtxChatClient::ProcessReceiveMessages() {
  while (!msg_in_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_THREAD_SLEEP));
    uint8_t buf[sizeof(struct ChatMsgHdr) + MAX_MSG_BUFFER_SIZE];
    ssize_t bytes_in = recv(socket_, buf, sizeof(buf), 0);
    if (bytes_in < 0) {
      // TODO(me): handle recv errors
      perror("ProcessReceiveMessages: recv ChatMsgHdr");
      continue;
    }

    std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
    msg->hdr = *reinterpret_cast<struct ChatMsgHdr*>(buf);
    size_t buf_len = msg->hdr.buf_len;
    if (buf_len > MAX_MSG_BUFFER_SIZE)
      continue;
    if (buf_len != 0) {
      msg->buf = reinterpret_cast<uint8_t*>(malloc(buf_len));
      memcpy(msg->buf, buf+sizeof(struct ChatMsgHdr), buf_len);
    }
    std::string text = std::string(reinterpret_cast<char*>(msg->buf));
    Log(text);

    switch (msg->hdr.type) {
      case MsgType::PUBLIC_DATA:
        PushGuiEvent(GuiEvType::PUBLIC_MSG, std::move(msg));
        break;
      case MsgType::PRIVATE_DATA:
        PushGuiEvent(GuiEvType::PRIVATE_MSG, std::move(msg));
        break;
      default:
        break;
    }
  }
}

void PtxChatClient::LogOut() {
  if (!socket_)
    return;

  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::UNREGISTER;
  msg->hdr.buf_len = 0;

  SendMsgToServer(std::move(msg));
}

void PtxChatClient::SendMsgToServer(std::unique_ptr<struct ChatMsg>&& msg) {
  if (send(socket_, &msg->hdr, sizeof(msg->hdr), 0) < 0) {
    perror("SendMsg: send ChatMsgHdr");
    return;
  }
  Log("SendMsg: sent hdr");

  if (!msg->hdr.buf_len)
    return;

  if (send(socket_, msg->buf, msg->hdr.buf_len, 0) < 0) {
    perror("SendMsg: send buf");
    return;
  }
  Log("SendMsg: sent buf");
  Log(reinterpret_cast<char*>(msg->buf));
}

void PtxChatClient::InitLog() {
  chat_log_.open("client.log");
}

void PtxChatClient::Log(const std::string& text) {
  std::lock_guard<std::mutex> lc_log(chat_log_mtx_);
  char time_s[32];
  time_t log_time = time(0);
  struct tm log_local_time;
  localtime_r(&log_time, &log_local_time);
  strftime(time_s, 32, "[%d-%m-%Y %H:%M:%S:] ", &log_local_time);
  std::thread::id t = std::this_thread::get_id();
  chat_log_ << time_s << "[" << t << "] " << text << std::endl;
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

PtxChatClient::~PtxChatClient() {
  msg_in_.stop(true);
  msg_out_.stop(true);

  LogOut();
  msg_out_thread_.stop = 1;
  msg_in_thread_.stop = 1;

  shutdown(socket_, SHUT_RD);
  close(socket_);
  socket_ = 0;
}

}  // namespace ptxchat
