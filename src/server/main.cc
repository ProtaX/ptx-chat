#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <nanogui/nanogui.h>

#include <thread>
#include <iostream>

#include "PtxChatServer.h"
#include "Message.h"

static ptxchat::PtxChatServer server;

using nanogui::TextBox;
using nanogui::FormHelper;
using nanogui::Window;
using nanogui::Screen;
using nanogui::ref;

using Eigen::Vector2i;

using ptxchat::GuiEvType;
using ptxchat::GuiEvent;
using ptxchat::MAX_MSG_BUFFER_SIZE;

void ProcessChatEvents(TextBox* t) {
  while (1) {
    char text[MAX_MSG_BUFFER_SIZE + 32];
    std::unique_ptr<struct GuiEvent> e = server.PopGuiEvent();
    // if (e->type == GuiEvType::Q_EMPTY)
    //   break;
    switch (e->type) {
      case GuiEvType::Q_EMPTY:
        break;
      case GuiEvType::SRV_START:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[SRV] start\n");
        break;
      case GuiEvType::SRV_STOP:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[SRV] stop\n");
        break;
      case GuiEvType::CLIENT_REG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[REG] %s\n", e->msg->hdr.from);
        break;
      case GuiEvType::CLIENT_UNREG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[UNR] %s\n", e->msg->hdr.from);
        break;
      case GuiEvType::PUBLIC_MSG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[PUB] %s says: %s\n", e->msg->hdr.from, e->msg->buf);
        break;
      case GuiEvType::PRIVATE_MSG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[PRV] %s to %s\n", e->msg->hdr.from, e->msg->hdr.to);
        break;
    }
    t->setValue(t->value() + std::string(text));
  }
}

int main(int /* argc */, char** /* argv */) {
  std::string ip_str_ = "0.0.0.0";
  uint16_t port_ = 1488;

  nanogui::init();
  {
    Screen* screen_ = new Screen(Vector2i(300, 600), "PTX Server");
    FormHelper* gui = new FormHelper(screen_);
    ref<Window> window = gui->addWindow(Vector2i(35, 15), "Chat window");

    TextBox* t = new TextBox(window, "");
    t->setAlignment(TextBox::Alignment::Right);
    t->setSize(Vector2i(300, 300));
    t->setSpinnable(true);

    gui->addGroup("Server IP and port");
    gui->addVariable("IP", ip_str_)->setCallback([](const std::string& ip){
      server.SetIP_s(ip);
    });
    gui->addVariable("Port", port_)->setCallback([](uint16_t port){
      server.SetPort_i(port);
    });
    gui->addButton("Start server", []{
      server.Start();
    });
    gui->addButton("Stop server", []{
      server.Stop();
    });
    gui->addWidget("", t);

    std::thread chat_thread(ProcessChatEvents, t);
    chat_thread.detach();
    window->center();
    screen_->performLayout();
    screen_->setVisible(true);
    nanogui::mainloop();
  }
  nanogui::shutdown();
  return 0;
}
