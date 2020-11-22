#include "connections.h"

#include <fcntl.h>
#include <sys/epoll.h>

#include "Threads.h"

namespace ptxchat {

std::shared_ptr<spdlog::logger> conn_logger_ = spdlog::rotating_logger_mt("Connections",
                                                                          "ptx_server.log",
                                                                          10000000,
                                                                          10);

int Connection::makeNonBlocking(int fd) {
  int flags;
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

int Connection::addEventToEpoll(int epoll_fd, int fd, uint32_t ev) {
  struct epoll_event e;
  e.data.fd = fd;
  e.events = ev;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1)
    return -1;
  return 0;
}

std::unique_ptr<ChatMsg> Connection::RecvMsgFromConn(std::shared_ptr<Connection> conn) {
  int client_fd = conn->socket_;
  if (conn->recv_data_sz_ < sizeof(ChatMsgHdr)) {
    uint8_t raw_hdr[sizeof(ChatMsgHdr)];
    ssize_t rec_bytes_hdr = recv(client_fd, raw_hdr, sizeof(raw_hdr), 0);
    if (rec_bytes_hdr == 0) {
      conn->status_ = ConnStatus::CLOSED;
      conn_logger_->log(spdlog::level::info, "Client " + std::to_string(client_fd) + ": disconnected");
      return nullptr;
    }
    if (rec_bytes_hdr < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return nullptr;

      if (errno == ECONNREFUSED) {
        conn->status_ = ConnStatus::ERROR;
        conn_logger_->log(spdlog::level::err, "Client " + std::to_string(client_fd) + ": connection refused");
        return nullptr;
      }

      conn_logger_->log(spdlog::level::critical, "recv() for client " + std::to_string(client_fd) +
                  " returned errno: " + strerror(errno));
      PtxChatCrash();
    } else if (rec_bytes_hdr < sizeof(ChatMsgHdr)) {
      if (conn->recv_data_sz_ == 0)
        conn->recv_data_ = (uint8_t*)malloc(rec_bytes_hdr);
      else
        conn->recv_data_ = (uint8_t*)realloc(conn->recv_data_, conn->recv_data_sz_ + rec_bytes_hdr);
      memcpy(conn->recv_data_ + conn->recv_data_sz_, raw_hdr, rec_bytes_hdr);
      conn->recv_data_sz_ += rec_bytes_hdr;
      if (conn->recv_data_sz_ != sizeof(ChatMsgHdr))
        return nullptr;
    } else {
      conn->recv_data_ = (uint8_t*)malloc(rec_bytes_hdr);
      memcpy(conn->recv_data_, raw_hdr, sizeof(raw_hdr));
      conn->recv_data_sz_ = sizeof(raw_hdr);
    }
  }
  /* Заголовок полностью в буфере */
  size_t buf_len = reinterpret_cast<ChatMsgHdr*>(conn->recv_data_)->buf_len;
  if (buf_len > MAX_MSG_BUFFER_SIZE) {
    conn->status_ = ConnStatus::ERROR;
    return nullptr;
  }

  if (buf_len != 0) {
    uint8_t* raw_body = (uint8_t*)malloc(buf_len);
    ssize_t rec_bytes_body = recv(client_fd, raw_body, buf_len, 0);
    if (rec_bytes_body == 0) {
      conn->status_ = ConnStatus::CLOSED;
      conn_logger_->log(spdlog::level::info, "Client " + std::to_string(client_fd) + ": disconnected");
      free(raw_body);
      return nullptr;
    }
    if (rec_bytes_body < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        free(raw_body);
        return nullptr;
      }

      if (errno == ECONNREFUSED) {
        conn->status_ = ConnStatus::ERROR;
        conn_logger_->log(spdlog::level::err, "Client " + std::to_string(client_fd) + ": connection refused");
        free(raw_body);
        return nullptr;
      }

      conn_logger_->log(spdlog::level::critical, "recv() for client " + std::to_string(client_fd) +
                " returned errno: " + strerror(errno));
      PtxChatCrash();
    } else if (rec_bytes_body < buf_len) {
      conn->recv_data_ = (uint8_t*)realloc(conn->recv_data_, conn->recv_data_sz_ + rec_bytes_body);
      memcpy(conn->recv_data_ + conn->recv_data_sz_, raw_body, rec_bytes_body);
      conn->recv_data_sz_ += rec_bytes_body;
      if (conn->recv_data_sz_ != sizeof(ChatMsgHdr) + buf_len) {
        free(raw_body);
        return nullptr;
      }
    } else {
      conn->recv_data_ = (uint8_t*)realloc(conn->recv_data_, sizeof(ChatMsgHdr) + buf_len);
      memcpy(conn->recv_data_ + sizeof(ChatMsgHdr), raw_body, buf_len);
      conn->recv_data_sz_ = sizeof(ChatMsgHdr) + buf_len;
    }
    free(raw_body);
  }

  ChatMsgHdr* hdr = reinterpret_cast<ChatMsgHdr*>(conn->recv_data_);
  auto msg = std::make_unique<ChatMsg>();
  sockaddr_in cl_addr;
  socklen_t sock_len = sizeof(cl_addr);
  getpeername(client_fd, reinterpret_cast<sockaddr*>(&cl_addr), &sock_len);
  memcpy(&msg->hdr, hdr, sizeof(ChatMsgHdr));
  msg->hdr.src_ip = cl_addr.sin_addr.s_addr;
  msg->hdr.src_port = cl_addr.sin_port;
  msg->buf = (uint8_t*)malloc(msg->hdr.buf_len);
  memcpy(msg->buf, conn->recv_data_ + sizeof(ChatMsgHdr), msg->hdr.buf_len);
  free(conn->recv_data_);
  conn->recv_data_sz_ = 0;

  conn_logger_->log(spdlog::level::debug, "Recv message buf from client " + std::to_string(client_fd));
  return msg;
}

bool Connection::SendMsgToConn(std::shared_ptr<ChatMsg> msg, std::shared_ptr<Connection> conn) {
    ssize_t bytes_sent = 0;
    while (bytes_sent != sizeof(ChatMsgHdr)) {
      ssize_t sz = send(conn->socket_, &msg->hdr + bytes_sent, sizeof(ChatMsgHdr) - bytes_sent, 0);

      if (sz == 0) {
        conn->status_ = ConnStatus::CLOSED;
        conn_logger_->log(spdlog::level::info, "Cannot send private message to " + std::string(msg->hdr.to) + ": client disconnected");
        return false;
      }
      if (sz < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          continue;
        if (errno == ECONNRESET) {
          conn->status_ = ConnStatus::ERROR;
          conn_logger_->log(spdlog::level::err, "Cannot send private message to " + std::string(msg->hdr.to) + ": connection reset");
          return false;
        }
        conn_logger_->log(spdlog::level::err, "Cannot send private message to " + std::string(msg->hdr.to) + ": " + strerror(errno));
        PtxChatCrash();
      }
      bytes_sent += sz;
    }

    bytes_sent = 0;
    while (bytes_sent != msg->hdr.buf_len) {
      ssize_t sz = send(conn->socket_, msg->buf + bytes_sent, msg->hdr.buf_len - bytes_sent, 0);

      if (sz == 0) {
        conn->status_ = ConnStatus::CLOSED;
        conn_logger_->log(spdlog::level::info, "Cannot send private message to " + std::string(msg->hdr.to) + ": client disconnected");
        return false;
      }
      if (sz < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          continue;
        if (errno == ECONNRESET) {
          conn->status_ = ConnStatus::ERROR;
          conn_logger_->log(spdlog::level::err, "Cannot send private message to " + std::string(msg->hdr.to) + ": connection reset");
          return false;
        }
        conn_logger_->log(spdlog::level::err, "Cannot send private message to " + std::string(msg->hdr.to) + ": " + strerror(errno));
        PtxChatCrash();
      }
      bytes_sent += sz;
    }

    conn_logger_->log(spdlog::level::debug, "Message from " + std::string(msg->hdr.from) + " sent to " + std::string(msg->hdr.to));
    return true;
  }

} // namespace ptxchat