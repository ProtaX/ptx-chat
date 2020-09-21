#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <nanogui/nanogui.h>

#include <thread>
#include <iostream>

#include "PtxChatClient.h"

static ptxchat::PtxChatClient client;

using nanogui::TextBox;
using nanogui::FormHelper;
using nanogui::Window;
using nanogui::Screen;
using nanogui::ref;
using nanogui::GroupLayout;

int main(int /* argc */, char** /* argv */) {
  std::string ip_str_ = "127.0.0.1";
  uint16_t port_ = 1488;
  nanogui::init();
  {
    nanogui::Screen* screen = new nanogui::Screen(Eigen::Vector2i(400, 400), "Chat");
    FormHelper* gui = new FormHelper(screen);

    /* Server settings window */
    ref<Window> server_window = gui->addWindow(Eigen::Vector2i(35, 15), "Server settings");
    server_window->setLayout(new GroupLayout);
    gui->addGroup("Server address");
    gui->addVariable("IP", ip_str_)->setCallback([](const std::string& ip){
      client.SetIP_s(ip);
    });
    gui->addVariable("Port", port_)->setCallback([](uint16_t port){
      client.SetPort_i(port);
    });

    gui->addGroup("Actions");
    TextBox* nick = new TextBox(server_window, "");
    nick->setEditable(true);
    nick->setPlaceholder("Nickname");
    gui->addWidget("", nick);
    gui->addButton("Log in", [nick]{
      client.LogIn(nick->value());
    });
    gui->addButton("Log out", []{
      client.LogOut();
    });

    /* Chats window */
    ref<Window> chat_window = gui->addWindow(Eigen::Vector2i(100, 15), "Chats");
    chat_window->setLayout(new GroupLayout);
    chat_window->setSize(Eigen::Vector2i(300, 300));
    TextBox* to = new TextBox(chat_window, "");
    TextBox* text = new TextBox(chat_window, "");
    to->setPlaceholder("Send to");
    to->setEditable(true);
    text->setPlaceholder("Enter message...");
    text->setEditable(true);
    gui->addButton("Send private", [to, text]{
      if (text->value().length() == 0)
        return;
      client.SendMsgTo(to->value(), text->value());
      text->setValue("");
    });

    //std::thread chat_thread(ProcessChatEvents, t);
    //chat_thread.detach();
    screen->performLayout();
    screen->setVisible(true);
    nanogui::mainloop();
  }
  nanogui::shutdown();
  return 0;
}
