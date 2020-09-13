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
}

PtxChatClient::PtxChatClient(const std::string& ip, uint16_t port) noexcept {
  struct sockaddr_in ip_addr;
  inet_pton(AF_INET, ip.c_str(), &ip_addr);
  server_ip_ = ip_addr.sin_addr.s_addr;
  server_port_ = port;
  nick_ = "";
  socket_ = 0;
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
  SendServerMsg(std::move(msg));

  /* Handle messages async */
  msg_in_thread_.stop = 0;
  msg_in_thread_.thread = std::thread(&PtxChatClient::ReceiveMessages, this);
  msg_in_thread_.thread.detach();

  msg_out_thread_.stop = 0;
  msg_out_thread_.thread = std::thread(&PtxChatClient::SendMessages, this);
  msg_out_thread_.thread.detach();
}

void PtxChatClient::ReceiveMessages() {
  while (!msg_in_thread_.stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint8_t* buf[sizeof(struct ChatMsgHdr)];
    ssize_t bytes_in = recv(socket_, buf, sizeof(buf), 0);
    if (bytes_in < 0) {
      // TODO(me): handle recv errors
      perror("recv");
      continue;
    }

    // TODO(me): возможно, нужно будет сделать очерель событий для GUI, но сейчас сообщения будут просто выводиться в лог
  }
  
}

void PtxChatClient::SendServerMsg(std::unique_ptr<struct ChatMsg>&& msg) {
  const void* buf = &msg->hdr;
  size_t len = sizeof(struct ChatMsgHdr);

  if (send(socket_, buf, len, 0) < 0) {
    perror("SendServerMsg: send");
    return;
  }
}

void PtxChatClient::LogOut() {
  if (!socket_)
    return;

  std::unique_ptr<struct ChatMsg> msg = std::make_unique<struct ChatMsg>();
  strcpy(msg->hdr.from, nick_.c_str());
  msg->hdr.type = MsgType::UNREGISTER;
  msg->hdr.buf_len = 0;

  SendServerMsg(std::move(msg));

  shutdown(socket_, SHUT_RDWR);
  close(socket_);
  socket_ = 0;
}

void PtxChatClient::SendPublicMsg(const std::string& text) {
  struct ChatMsgHdr hdr;
  strcpy(hdr.from, nick_.c_str());
  hdr.type = MsgType::PUBLIC_DATA;
  hdr.buf_len = text.length();
  
  if (send(socket_, &hdr, sizeof(hdr), 0) < 0) {
    perror("SendPublicMsg: send ChatMsgHdr");
    return;
  }

  if (send(socket_, text.c_str(), text.length(), 0) < 0) {
    perror("SendPublicMsg: send buf");
    return;
  }
}

PtxChatClient::~PtxChatClient() {
  LogOut();
}

}  // namespace ptxchat
