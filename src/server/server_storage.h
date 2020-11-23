#ifndef SERVER_STORAGE_H_
#define SERVER_STORAGE_H_

#include "StorageBase.h"

namespace ptxchat {

class ServerStorage: public StorageBase {
 public:
  ServerStorage();

  void AddPublicMsg(std::shared_ptr<ChatMsg> msg);

  void AddPrivateMsg(std::shared_ptr<ChatMsg> msg);

  ~ServerStorage();

 private:

};

} // namespace ptxchat

#endif // SERVER_STORAGE_H_
