#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "PtxChatServer.h"

int main() {
  ptxchat::PtxChatServer serv1;
  serv1.Start();

  ssize_t nread;
  char buf[32];
  const char* ex = "exit";
  while ((nread = read(0, buf, sizeof(buf))) > 0)
    if (memcmp(buf, ex, static_cast<size_t>(nread - 1)) == 0)
      break;
}
