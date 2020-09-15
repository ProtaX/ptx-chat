#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "Gui.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  ptxchat::ServerGUI gui;
  gui.resize(250, 400);
  gui.setWindowTitle("PTX Server");
  gui.show();

  return app.exec();
}
