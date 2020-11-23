#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdint.h>
#include <memory>
#include <utility>

namespace ptxchat {

constexpr size_t MAX_MSG_BUFFER_SIZE = 256;
constexpr size_t MAX_NICKNAME_LEN = 64;

enum class MsgType {
  REGISTER,     REGISTERED,   ERR_REGISTERED,
  UNREGISTER,   UNREGISTERED, ERR_UNREGISTERED,
  PRIVATE_DATA, PUBLIC_DATA,
  ERR_UNKNOWN,
  QUIT,  // TODO: impl
  PING, PONG,  // TODO: impl
};

enum class GuiEvType {
  Q_EMPTY,
  CLIENT_REG,
  CLIENT_UNREG,
  PUBLIC_MSG,
  PRIVATE_MSG,
  SRV_START,
  SRV_STOP,
  CLEAR,
};

struct GuiEvent {
  GuiEvent() noexcept {}
  GuiEvent(const GuiEvent&) = delete;
  GuiEvent(GuiEvent&& r) {
    msg = std::move(r.msg);
    type = r.type;
  }

  GuiEvType type;
  std::shared_ptr<struct ChatMsg> msg;
};

#pragma pack(push, 1)

struct ChatMsgHdr {
  MsgType type;
  uint32_t src_ip;             /**< Filled by server */
  uint16_t src_port;           /**< Filled by server */
  char from[MAX_NICKNAME_LEN];
  char to[MAX_NICKNAME_LEN];
  size_t buf_len;
};

struct ChatMsg {
  ChatMsg() noexcept: buf(nullptr) {}
  ~ChatMsg() { if (buf) free(buf); }

  ChatMsgHdr hdr;
  uint8_t* buf;
};

#pragma pack(push)

} // namespace ptxchat

#endif // MESSAGE_H_
