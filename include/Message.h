#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdint.h>
#include <memory>
#include <utility>

#include "Client.h"

namespace ptxchat {

constexpr size_t MAX_MSG_BUFFER_SIZE = 256;

enum class MsgType {
  REGISTER,
  UNREGISTER,
  PRIVATE_DATA,
  PUBLIC_DATA,
  ERR_UNREGISTERED,
  ERR_REGISTERED,
  ERR_UNKNOWN,
};

enum class GuiMsgType {
  Q_EMPTY,
  CLIENT_REG,
  CLIENT_UNREG,
  PUBLIC_MSG,
  PRIVATE_MSG,
  SRV_START,
  SRV_STOP,
};

struct GuiMsg {
  GuiMsg() noexcept {}
  GuiMsg(const GuiMsg&) = delete;
  GuiMsg(GuiMsg&& r) {
    msg = std::move(r.msg);
    type = r.type;
  }

  GuiMsgType type;
  std::unique_ptr<struct ChatMsg> msg;
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

// TODO(me): pool for message buffers
struct ChatMsg {
  ChatMsg() noexcept: buf(nullptr) {}
  ~ChatMsg() { if (buf) free(buf); }

  ChatMsgHdr hdr;
  uint8_t* buf;
};

#pragma pack(push)

}  // namespace ptxchat

#endif  // MESSAGE_H_
