#include "PtxChatClient.h"

int main() {
  ptxchat::PtxChatClient client;
  client.LogIn("[ProtaX");
  client.SendMsg("helo, wordl!");
}
