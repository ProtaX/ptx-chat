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

  std::vector<std::shared_ptr<ChatMsg>> GetPublicMsgs();

  std::vector<std::shared_ptr<ChatMsg>> GetPrivateMsgs(const std::string& nick);

  ~ClientStorage();

 private:
  std::shared_ptr<ChatMsg> GetMsgFromDoc(bsoncxx::v_noabi::document::view v);
};

} // namespace ptxchat

#endif // CLIENT_STORAGE_H_
