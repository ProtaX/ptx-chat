#include "PtxClientGui.h"

namespace ptxchat {

ClientGUI::ClientGUI(QWidget* p) : GUIFrontend(p) {
  QPushButton* login_btn_ = new QPushButton("Log In", this);
  QPushButton* logout_btn_ = new QPushButton("Log Out", this);
  QPushButton* send_btn_ = new QPushButton("Send", this);

  chat_text_ = new QTextEdit();
  chat_text_->setReadOnly(true);
  serv_ip_ = new QLineEdit();
  serv_ip_->setPlaceholderText("127.0.0.1");
  serv_port_ = new QLineEdit();
  serv_port_->setPlaceholderText("1488");
  nick_ = new QLineEdit();

  client_ = std::make_unique<PtxChatClient>();

  QGridLayout* grid = new QGridLayout(this);
  grid->addWidget(login_btn_);
  grid->addWidget(logout_btn_);
  grid->addWidget(chat_text_);
  setLayout(grid);

  connect(login_btn_, &QPushButton::clicked, this, &ptxchat::ClientGUI::LogIn);
  connect(logout_btn_, &QPushButton::clicked, this, &ptxchat::ClientGUI::LogOut);
}

void ClientGUI::LogIn() {
  std::string ip_s = serv_ip_->text().toStdString();
  uint16_t port_s = serv_port_->text().toShort();

  client_->SetIpPort_s(ip_s, port_s);
  client_->LogIn(nick_->text().toStdString());
}

void ClientGUI::LogOut() {
  client_->LogOut();
}

void ClientGUI::ProcessGUIEvents() {
  while (1) {
    char text[MAX_MSG_BUFFER_SIZE + 32];
    std::unique_ptr<struct GuiEvent> e = client_->PopGuiEvent();
    if (e->type == GuiEvType::Q_EMPTY)
      break;
    switch (e->type) {
      case GuiEvType::Q_EMPTY:
        break;
      case GuiEvType::SRV_START:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[SRV] start");
        break;
      case GuiEvType::SRV_STOP:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[SRV] stop");
        break;
      case GuiEvType::CLIENT_REG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[REG] %s", e->msg->hdr.from);
        break;
      case GuiEvType::CLIENT_UNREG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[UNR] %s", e->msg->hdr.from);
        break;
      case GuiEvType::PUBLIC_MSG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[PUB] %s says: %s", e->msg->hdr.from, e->msg->buf);
        break;
      case GuiEvType::PRIVATE_MSG:
        snprintf(text, MAX_MSG_BUFFER_SIZE, "[PRV] %s to %s", e->msg->hdr.from, e->msg->hdr.to);
        break;
    }
    QString qtext(text);
    chat_text_->append(qtext);
  }
}

void ClientGUI::timerEvent(QTimerEvent* e) {
  Q_UNUSED(e);
  ProcessGUIEvents();
}

}  // namespace ptxchat
