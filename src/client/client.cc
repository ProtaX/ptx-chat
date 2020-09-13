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
  SendMsg(std::move(msg));

  /* Handle messages async */
  msg_in_thread_.stop = 0;
  msg_in_thread_.thread = std::thread(&PtxChatClient::ReceiveMessages, this);
  msg_in_thread_.thread.detach();

  msg_out_thread_.stop = 0;
  msg_out_thread_.thread = std::thread(&PtxChatClient::SendMessages, this);
  msg_out_thread_.thread.detach();
}

void PtxChatClient::SendMsg(const std::string& text) {
  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::PUBLIC_DATA;
  msg->hdr.buf_len = text.length();
  msg->buf = reinterpret_cast<uint8_t*>(malloc(msg->hdr.buf_len));
  std::lock_guard<std::mutex> lc_msg_out(msg_out_thread_.mtx);
  msg_out_.push_front(std::move(msg));
}

void PtxChatClient::SendMsgTo(const std::string& to, const std::string& text) {
  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  strcpy(msg->hdr.to, to.c_str());
  msg->hdr.type = MsgType::PRIVATE_DATA;
  msg->hdr.buf_len = text.length();
  msg->buf = reinterpret_cast<uint8_t*>(malloc(msg->hdr.buf_len));
  std::lock_guard<std::mutex> lc_msg_out(msg_out_thread_.mtx);
  msg_out_.push_front(std::move(msg));
}

void PtxChatClient::SendMessages() {
  while (!msg_out_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_THREAD_SLEEP));
    msg_in_thread_.mtx.lock();
    std::unique_ptr<struct ChatMsg> msg = std::move(msg_in_.back());
    msg_in_.pop_back();
    msg_in_thread_.mtx.unlock();

    SendMsg(std::move(msg));
  }
}

void PtxChatClient::ReceiveMessages() {
  while (!msg_in_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_THREAD_SLEEP));
    uint8_t* buf[sizeof(struct ChatMsgHdr)];
    ssize_t bytes_in = recv(socket_, buf, sizeof(buf), 0);
    if (bytes_in < 0) {
      // TODO(me): handle recv errors
      perror("ReceiveMessages: recv ChatMsgHdr");
      continue;
    }

    // TODO(me): возможно, нужно будет сделать очерель событий для GUI, но сейчас сообщения будут просто выводиться в лог
    std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
    msg->hdr = *reinterpret_cast<struct ChatMsgHdr*>(buf);
    size_t buf_len = msg->hdr.buf_len;
    if (buf_len > MAX_MSG_BUFFER_SIZE)
      continue;
    msg->buf = reinterpret_cast<uint8_t*>(malloc(buf_len));
    if (buf_len != 0) {
      if (recv(socket_, msg->buf, buf_len, 0) < 0) {
        perror("ReceiveMessages: recv buf");
        continue;
      }
    }

    std::string text = std::string(reinterpret_cast<char*>(msg->buf));
    Log(text);
  }
}

void PtxChatClient::LogOut() {
  if (!socket_)
    return;

  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::UNREGISTER;
  msg->hdr.buf_len = 0;

  SendMsg(std::move(msg));

  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = 0;
}

void PtxChatClient::SendMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  if (send(socket_, &msg->hdr, sizeof(msg->hdr), 0) < 0) {
    perror("SendMsg: send ChatMsgHdr");
    return;
  }

  if (!msg->hdr.buf_len)
    return;

  if (send(socket_, msg->buf, msg->hdr.buf_len, 0) < 0) {
    perror("SendMsg: send buf");
    return;
  }
}

void PtxChatClient::InitLog() {
  chat_log_.open("client.log");
}

void PtxChatClient::Log(const std::string& text) {
  std::lock_guard<std::mutex> lc_log(chat_log_mtx_);
  chat_log_ << text << std::endl;
}

PtxChatClient::~PtxChatClient() {
  LogOut();
}

}  // namespace ptxchat
