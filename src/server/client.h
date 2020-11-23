#ifndef SERVER_CLIENT_H_
#define SERVER_CLIENT_H_

#include <stdint.h>
#include <string.h>
#include <string>
#include <functional>
#include <vector>

#include "connections.h"

namespace ptxchat {

class Client {
 public:
  Client(std::shared_ptr<Connection> c) noexcept:
        conn_(c),
        is_registered_(false) {}

  [[nodiscard]] const std::string& GetNickname() const { return nickname_; }
  [[nodiscard]] int GetSocket() const { return conn_->GetSocket(); }
  [[nodiscard]] uint32_t GetIp() const { return conn_->GetIP(); }
  [[nodiscard]] uint16_t GetPort() const { return conn_->GetPort(); }
  [[nodiscard]] bool IsRegistered() const { return is_registered_; }
  [[nodiscard]] std::shared_ptr<Connection> GetConnection() const { return conn_; }

  bool SetNickname(const std::string& nn) {
    size_t n_len = nn.length();
    if (n_len <= 1)
      return false;
    nickname_ = nn;
    return true;
  }

  bool Register(const std::string& nn) {
    is_registered_ = true;
    return SetNickname(nn);
  }
  void Unregister() { is_registered_ = false; }

 private:
  std::shared_ptr<Connection> conn_;
  bool is_registered_;
  std::string nickname_;
};

}  // namespace ptxchat

#endif // SERVER_CLIENT_H_
