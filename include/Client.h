#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>
#include <string.h>
#include <string>
#include <functional>
#include <vector>

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

  [[nodiscard]] const std::string& GetNickname() const { return nickname_; }
  [[nodiscard]] int GetSocket() const { return socket_; }
  [[nodiscard]] uint32_t GetIp() const { return ip_; }
  [[nodiscard]] uint16_t GetPort() const { return port_; }
  [[nodiscard]] bool IsRegistered() const { return is_registered_; }

  bool SetNickname(const std::string& nn) {
    size_t n_len = nn.length();
    if (n_len > MAX_NICKNAME_LEN || n_len <= 1)
      return false;
    const std::string old_nn = nn;
    nickname_ = nn;
    for (auto cb : nick_change_cb_)
      cb(*this, old_nn);
    return true;
  }

  void AddCallback(std::function<void(const Client&, const std::string&)> cb) {
    nick_change_cb_.push_back(cb);
  }

  bool Register(const std::string& nn) {
    is_registered_ = true;
    return SetNickname(nn);
  }
  void Unregister() { is_registered_ = false; }

 private:
  int socket_;
  uint32_t ip_;
  uint16_t port_;
  bool is_registered_;
  std::string nickname_;
  std::vector<std::function<void(const Client&, const std::string&)>> nick_change_cb_;
};

}  // namespace ptxchat

#endif  // CLIENT_H_
