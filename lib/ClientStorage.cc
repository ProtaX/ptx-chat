#include "ClientStorage.h"

namespace ptxchat {

bool ClientStorage::add(std::unique_ptr<Client>&& cl) {
  std::unique_lock<std::mutex> lc_storage(mtx_);
  auto res = clients_.find(cl->GetNickname());
  if (res != clients_.end())
    return false;
  clients_[cl->GetNickname()] = std::move(cl);
  return true;
}

bool ClientStorage::del(const char* nick) {
  std::unique_lock<std::mutex> lc_storage(mtx_);
  return (clients_.erase(nick) == 1);
}

void ClientStorage::for_each(std::function<bool(Client&)> func) {
  for (const auto& it : clients_)
    if (!func(*it.second.get()))
      break;
}

void ClientStorage::for_each(std::function<ClientStorageIter(ClientStorageMap&, ClientStorageIter&)> func) {
  for (auto it = clients_.begin(); it != clients_.end(); )
    it = func(clients_, it);
}

std::pair<bool, const std::unique_ptr<Client>&> ClientStorage::exists(const char* client) {
  auto res = clients_.find(client);
  if (res != clients_.end())
    return {true, res->second};
  return {false, nullptr};
}

}  // namespace ptxchat
