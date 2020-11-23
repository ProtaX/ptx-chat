#include "server_storage.h"

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

ServerStorage::ServerStorage() {

}

void ServerStorage::AddPrivateMsg(std::shared_ptr<ChatMsg> msg) {
  auto builder = document{};
  bsoncxx::document::value doc_val = builder
  << "From" << (const char*)msg->hdr.from
  << "IP"   << (int)msg->hdr.src_ip
  << "Port" << (int)msg->hdr.src_port
  << "To"   << (const char*)msg->hdr.to
  << "Type" << (int)msg->hdr.type
  << "Text" << (const char*)msg->buf
  << bsoncxx::builder::stream::finalize;
  msg_coll_->insert_one(doc_val.view());
}

void ServerStorage::AddPublicMsg(std::shared_ptr<ChatMsg> msg) {
  auto builder = document{};
  bsoncxx::document::value doc_val = builder
  << "From" << (const char*)msg->hdr.from
  << "IP"   << (int)msg->hdr.src_ip
  << "Port" << (int)msg->hdr.src_port
  << "Type" << (int)msg->hdr.type
  << "Text" << (const char*)msg->buf
  << bsoncxx::builder::stream::finalize;
  msg_coll_->insert_one(doc_val.view());
}

ServerStorage::~ServerStorage() {
  
}

} // namespace ptxchat
