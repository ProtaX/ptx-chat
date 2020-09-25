#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <nanogui/nanogui.h>

#include <thread>
#include <iostream>
#include <vector>

#include "PtxChatServer.h"
#include "Message.h"

static ptxchat::PtxChatServer server;

using nanogui::TextBox;
using nanogui::FormHelper;
using nanogui::Window;
using nanogui::Screen;
using nanogui::ref;
using nanogui::GroupLayout;
using nanogui::Widget;
using nanogui::VScrollPanel;
using nanogui::GridLayout;
using nanogui::Label;
using nanogui::Button;
using nanogui::BoxLayout;

using Eigen::Vector2i;

using ptxchat::GuiEvType;
using ptxchat::GuiEvent;
using ptxchat::MAX_MSG_BUFFER_SIZE;

static constexpr int h = 250;
static constexpr int w = 300;
static constexpr int box_w = w/2;
static constexpr int box_add = 10;
static constexpr int log_add = 50;
static constexpr int log_lines = 5;

void ProcessChatEvents(std::vector<TextBox*> log) {
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

    for (size_t i = log_lines - 1; i > 0; --i)
      log[i]->setValue(log[i-1]->value());
    log[0]->setValue(std::string(text));
  }
}

int main(int /* argc */, char** /* argv */) {
  nanogui::init();
  {
    Screen* screen = new Screen({w, h}, "PTX Server", false);
    Window *window = new Window(screen, "PTX Server Control Panel");
    window->setPosition({0, 0});
    window->setFixedSize({w, h});
    window->setLayout(new BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 0));
    auto set_wrapper = new Widget(window);
    set_wrapper->setFixedSize({box_w, h});
    set_wrapper->setLayout(new GroupLayout());

    new Label(set_wrapper, "IP");

    TextBox* ip_text = new TextBox(set_wrapper, "0.0.0.0");
    ip_text->setFixedWidth(box_w/2 + box_add);
    ip_text->setEditable(true);
    ip_text->setCallback([](const std::string& ip){
      return server.SetIP_s(ip);
    });

    new Label(set_wrapper, "Port");
    TextBox* port_text = new TextBox(set_wrapper, "1488");
    port_text->setFixedWidth(box_w/2 + box_add);
    port_text->setEditable(true);
    port_text->setCallback([](const std::string& port){
      return server.SetPort_s(port);
    });

    Button* start_btn = new Button(set_wrapper, "Start server");
    start_btn->setFixedWidth(box_w/2 + box_add);
    start_btn->setCallback([]{
      server.Start();
    });
    Button* stop_btn = new Button(set_wrapper, "Stop server");
    stop_btn->setFixedWidth(box_w/2 + box_add);
    stop_btn->setCallback([]{
      server.Stop();
    });

    Widget* log_wrapper = new Widget(window);
    auto v = new VScrollPanel(window);
    v->setFixedSize({w/2, h});
    log_wrapper->setFixedSize({w/2, h});
    log_wrapper->setLayout(new GroupLayout());

    new Label(log_wrapper, "Server Log");
    std::vector<TextBox*> log;
    for (int i = 0; i < log_lines; ++i) {
      TextBox* t = new TextBox(log_wrapper, "");
      t->setEnabled(true);
      t->setAlignment(TextBox::Alignment::Left);
      t->setSpinnable(false);
      t->setFixedWidth(box_w/2 + 30);
      log.push_back(t);
    }
    std::thread chat_thread(ProcessChatEvents, log);
    chat_thread.detach();

    screen->performLayout();
    screen->setVisible(true);
    nanogui::mainloop();
  }
  nanogui::shutdown();
  return 0;
}
