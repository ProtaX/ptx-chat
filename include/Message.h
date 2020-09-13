#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdint.h>

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
