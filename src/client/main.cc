#include "PtxChatClient.h"

int main() {
  ptxchat::PtxChatClient client;
  client.LogIn("assbuster");
  client.SendPublicMsg("hello, wordl!");
}
