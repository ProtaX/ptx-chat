#ifndef CLIENT_STORAGE_H_
#define CLIENT_STORAGE_H_

#include <vector>
#include <memory>

#include "StorageBase.h"
#include "Message.h"

namespace ptxchat {

class ClientStorage: public StorageBase {
 public:
  ClientStorage();

  std::vector<std::shared_ptr<ChatMsg>> LoadPublicMsgs();

  std::vector<std::shared_ptr<ChatMsg>> LoadPrivateMsgs();

  ~ClientStorage();

 private:
  
};

} // namespace ptxchat

#endif // CLIENT_STORAGE_H_
