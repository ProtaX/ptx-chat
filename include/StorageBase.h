#ifndef STORAGE_H_
#define STORAGE_H_

#include <memory>

#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/exception.hpp>

#include "Message.h"

namespace ptxchat {

class StorageBase {
 public:
  StorageBase() {
    instance_ = std::make_unique<mongocxx::instance>();
    try {
      client_ = std::make_unique<mongocxx::client>(mongocxx::uri{});
    } catch (mongocxx::exception& ex) {
      isConnected = false;
      return;
    }
    chat_db_ = std::make_unique<mongocxx::database>(client_->database("ptx-chat"));
    if (!chat_db_->has_collection("messages")) {
      chat_db_->create_collection("messages");
    }
    msg_coll_ = std::make_unique<mongocxx::collection>(chat_db_->collection("messages"));
    isConnected = true;
  }

  virtual ~StorageBase() {

  }

 protected:
  std::unique_ptr<mongocxx::instance> instance_;
  std::unique_ptr<mongocxx::client> client_;
  std::unique_ptr<mongocxx::database> chat_db_;
  std::unique_ptr<mongocxx::collection> msg_coll_;
  bool isConnected;
};

} // namespace ptxchat

#endif // STORAGE_H_