#include "client_storage.h"

#include <cstdint>
#include <iostream>
#include <vector>
#include <bsoncxx/json.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

namespace ptxchat {

ClientStorage::ClientStorage() {

}

std::vector<std::shared_ptr<ChatMsg>> ClientStorage::GetPublicMsgs() {
  std::vector<std::shared_ptr<ChatMsg>> res{};
  if (!isConnected)
    return res;
  auto cursor = msg_coll_->find({});
  for (auto doc : cursor) {
    auto msg = GetMsgFromDoc(doc);
    if (msg) {
      if (msg->hdr.type == MsgType::PUBLIC_DATA)
        res.push_back(msg);
    }
  }
  return res;
}

std::vector<std::shared_ptr<ChatMsg>> ClientStorage::GetPrivateMsgs(const std::string& nick) {
  std::vector<std::shared_ptr<ChatMsg>> res{};
  if (!isConnected)
    return res;
  auto cursor = msg_coll_->find({});
  for (auto doc : cursor) {
    auto msg = GetMsgFromDoc(doc);
    if (msg) {
      if (msg->hdr.type == MsgType::PRIVATE_DATA &&
          !strcmp(msg->hdr.to, nick.data())) {
        res.push_back(msg);
      }
    }
  }
  return res;
}

std::shared_ptr<ChatMsg> ClientStorage::GetMsgFromDoc(bsoncxx::v_noabi::document::view v) {
  auto msg = std::make_shared<ChatMsg>();
  auto from = v.find("From");
  if (from == v.end())
    return nullptr;
  strcpy(msg->hdr.from, from->get_string().value.data());

  auto to = v.find("To");
  if (to != v.end())
    strcpy(msg->hdr.to, to->get_string().value.data());

  auto src_ip = v.find("IP");
  if (src_ip == v.end())
    return nullptr;
  msg->hdr.src_ip = src_ip->get_int32();

  auto src_port = v.find("Port");
  if (src_port == v.end())
    return nullptr;
  msg->hdr.src_port = src_port->get_int32();

  auto type = v.find("Type");
  if (type == v.end())
    return nullptr;
  msg->hdr.type = (MsgType)(int)type->get_int32();

  auto data = v.find("Data");
  if (data != v.end()) {
    msg->hdr.buf_len = data->length();
    msg->buf = (uint8_t*)malloc(msg->hdr.buf_len);
    memcpy(msg->buf, data->get_string().value.data(), msg->hdr.buf_len);
  }
  return msg;
}

ClientStorage::~ClientStorage() {
  
}

} // namespace ptxchat
