#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <nanogui/nanogui.h>

#include <thread>
#include <iostream>

#include "PtxChatClient.h"
#include "Message.h"

static ptxchat::PtxChatClient client;

using nanogui::TextBox;
using nanogui::FormHelper;
using nanogui::Window;
using nanogui::Screen;
using nanogui::ref;
using nanogui::GroupLayout;

using Eigen::Vector2i;

using ptxchat::GuiEvType;
using ptxchat::GuiEvent;
using ptxchat::MAX_MSG_BUFFER_SIZE;

typedef std::vector<TextBox*> text_box_t;
static constexpr int chat_lines = 5;

void ProcessChatEvents(text_box_t pub, text_box_t priv) {
  while (1) {
    char text[MAX_MSG_BUFFER_SIZE + 32];
    std::unique_ptr<struct GuiEvent> e = client.PopGuiEvent();
    text_box_t* target;
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
        target = &pub;
        break;
      case GuiEvType::PRIVATE_MSG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[PRV] %s to %s\n", e->msg->hdr.from, e->msg->hdr.to);
        target = &priv;
        break;
    }

    for (size_t i = chat_lines - 1; i > 0; --i)
      (*target)[i]->setValue((*target)[i-1]->value());
    (*target)[0]->setValue(std::string(text));
  }
}

int main(int /* argc */, char** /* argv */) {
  std::string ip_str_ = "127.0.0.1";
  uint16_t port_ = 1488;
  nanogui::init();
  {
    nanogui::Screen* screen = new nanogui::Screen({600, 600}, "PTX Chat");
    FormHelper* gui = new FormHelper(screen);

    /* Server settings window */
    ref<Window> server_window = gui->addWindow({15, 15}, "Server settings");
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

    /* Send window */
    ref<Window> send_window = gui->addWindow({130, 15}, "Send");
    send_window->setLayout(new GroupLayout);
    send_window->setSize({300, 300});
    TextBox* to = new TextBox(send_window, "");
    TextBox* text = new TextBox(send_window, "");
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
    gui->addButton("Send public", [text]{
      if (text->value().length() == 0)
        return;
      client.SendMsg(text->value());
      text->setValue("");
    });

    /* Public chat */
    ref<Window> pub_chat_window = gui->addWindow({100, 200}, "Public chat");
    pub_chat_window->setLayout(new GroupLayout);
    std::vector<TextBox*> pub_chat;
    for (int i = 0; i < chat_lines; ++i) {
      TextBox* t = new TextBox(pub_chat_window, "");
      t->setAlignment(TextBox::Alignment::Left);
      t->setSpinnable(false);
      t->setFixedWidth(80);
      pub_chat.push_back(t);
    }

    /* Private chat */
    ref<Window> prv_chat_window = gui->addWindow({150, 300}, "Private chat");
    prv_chat_window->setLayout(new GroupLayout);
    prv_chat_window->setSize({300, 300});
    std::vector<TextBox*> prv_chat;
    for (int i = 0; i < chat_lines; ++i) {
      TextBox* t = new TextBox(prv_chat_window, "");
      t->setAlignment(TextBox::Alignment::Left);
      t->setSpinnable(false);
      t->setFixedWidth(80);
      prv_chat.push_back(t);
    }

    std::thread chat_thread(ProcessChatEvents, pub_chat, prv_chat);
    chat_thread.detach();
    screen->performLayout();
    screen->setVisible(true);
    nanogui::mainloop();
  }
  nanogui::shutdown();
  return 0;
}
