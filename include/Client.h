#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>
#include <string.h>
#include <string>

namespace ptxchat {

const size_t MAX_NICKNAME_LEN = 64;

class Client {
 public:
  Client() noexcept:
        socket_(0),
        ip_(0),
        port_(0),
        is_registered_(false) {}
  Client(int skt, uint32_t ip, uint16_t port) noexcept:
        socket_(skt),
        ip_(ip),
        port_(port),
        is_registered_(false) {}

  [[nodiscard]] const char* GetNickname() const { return nickname_; }
  [[nodiscard]] int GetSocket() const { return socket_; }
  [[nodiscard]] uint32_t GetIp() const { return ip_; }
  [[nodiscard]] uint16_t GetPort() const { return port_; }
  [[nodiscard]] bool IsRegistered() const { return is_registered_; }

  bool SetNickname(const std::string& nn) {
    size_t n_len = nn.length();
    if (n_len > MAX_NICKNAME_LEN || n_len <= 1)
      return false;
    strcpy(nickname_, nn.c_str());
    return true;
  }

  void Register() { is_registered_ = true; }
  void Unregister() { is_registered_ = false; }

 private:
  int socket_;
  uint32_t ip_;
  uint16_t port_;
  bool is_registered_;
  char nickname_[MAX_NICKNAME_LEN];
};

}  // namespace ptxchat

#endif  // CLIENT_H_
