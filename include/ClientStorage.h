#ifndef CLIENTSTORAGE_H_
#define CLIENTSTORAGE_H_

#include <unordered_map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>

#include "Client.h"

namespace ptxchat {

typedef std::unordered_map<const char*, std::unique_ptr<Client>>::iterator ClientStorageIter;
typedef std::unordered_map<const char*, std::unique_ptr<Client>> ClientStorageMap;

class ClientStorage {
 public:
  ClientStorage() noexcept {}
  ~ClientStorage() {}

  /**
   * \brief Add a client to the storage
   * \param cl client
   * \return false if client was not added
   */
  bool add(std::unique_ptr<Client>&& cl);

  /**
   * \brief Delete client by nick name
   * \param nick nick name of client
   * \return false if no client with given nick name
   */
  bool del(const char* nick);

  /**
   * \brief Perform a const operation for every client
   * \param func functor
   * If func returns false, the iteration stops
   */
  void for_each(std::function<bool(Client&)> func);

  /**
   * \brief Perform a non-const operation for every client
   * \param func functor
   * Params of @func:
   *   m reference to storage, to delete elements from storage
   *   it reference to storage itereator
   * Return of @func:
   *   iterator that will be passed to @func on next iteration
   * If func returns m.end(), iteration stops
   */
  void for_each(std::function<ClientStorageIter(ClientStorageMap& m, ClientStorageIter& it)> func);

  /**
   * \brief Check if client is in storage
   * \param nick nick of client
   * \return pair with bool result of search and result if found
   */
  std::pair<bool, const std::unique_ptr<Client>&> exists(const char* client);

 private:
  std::mutex mtx_;
  std::unordered_map<const char*, std::unique_ptr<Client>> clients_;
};

}  // namespace ptxchat

#endif  // CLIENTSTORAGE_H_
