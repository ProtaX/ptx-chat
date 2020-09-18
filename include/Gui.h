#ifndef GUI_H_
#define GUI_H_

#include <qt5/QtWidgets/QWidget>
#include <qt5/QtWidgets/QApplication>
#include <qt5/QtWidgets/QPushButton>
#include <qt5/QtWidgets/QGridLayout>
#include <qt5/QtWidgets/QTextEdit>
#include <qt5/QtCore/QTime>
#include <qt5/QtWidgets/QLineEdit>

#include <memory>
#include <thread>
#include <string>

#include "PtxChatServer.h"
#include "PtxChatClient.h"
#include "Message.h"
#include "SharedUDeque.h"

namespace ptxchat {

constexpr int GUI_UPDATE_MS = 100;
constexpr size_t MAX_GUI_EVENTS_Q_SIZE = 1000;

class ServerGUI : public QWidget {
  Q_OBJECT

 public:
  explicit ServerGUI(QWidget* p = nullptr) : QWidget(p) {
    QPushButton* start_btn_ = new QPushButton("Start server", this);
    QPushButton* stop_btn_ = new QPushButton("Stop server", this);

    chat_text_ = new QTextEdit();
    chat_text_->setReadOnly(true);
    ip_line_ = new QLineEdit();
    ip_line_->setPlaceholderText("0.0.0.0");

    port_line_ = new QLineEdit();
    port_line_->setPlaceholderText("1488");

    server_ = std::make_unique<PtxChatServer>();

    QGridLayout* grid = new QGridLayout(this);
    grid->addWidget(ip_line_);
    grid->addWidget(port_line_);
    grid->addWidget(start_btn_);
    grid->addWidget(stop_btn_);
    grid->addWidget(chat_text_);
    setLayout(grid);

    connect(start_btn_, &QPushButton::clicked, this, &ptxchat::ServerGUI::StartServer);
    connect(stop_btn_, &QPushButton::clicked, this, &ptxchat::ServerGUI::StopServer);

    startTimer(GUI_UPDATE_MS);
  }

 private slots:
  void StartServer() {
    std::string ip_s = ip_line_->text().toStdString();
    uint16_t port_s = port_line_->text().toShort();

    server_->SetIpPort_s(ip_s, port_s);
    server_->Start();
  }

  void StopServer() {
    server_->Stop();
  }

  void GetMessages() {
    while (1) {
      char text[MAX_MSG_BUFFER_SIZE + 32];
      std::unique_ptr<GuiEvent> e = server_->PopGuiEvent();
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

  void timerEvent(QTimerEvent* e) {
    Q_UNUSED(e);

    GetMessages();
  }

 private:
  QTextEdit* chat_text_;
  QLineEdit* ip_line_;
  QLineEdit* port_line_;
  std::unique_ptr<PtxChatServer> server_;
};

class ClientGUI : public QWidget {
  Q_OBJECT

 public:
  explicit ClientGUI(QWidget* p = nullptr) : QWidget(p) {
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

    startTimer(GUI_UPDATE_MS);
  }

 private slots:
  void LogIn() {
    std::string ip_s = serv_ip_->text().toStdString();
    uint16_t port_s = serv_port_->text().toShort();

    client_->SetIpPort_s(ip_s, port_s);
    client_->LogIn(nick_->text().toStdString());
  }

  void LogOut() {
    client_->LogOut();
  }

 private slots:
  void GetMessages() {
    while (1) {
      char text[MAX_MSG_BUFFER_SIZE + 32];
      std::unique_ptr<GuiEvent> e = client_->PopGuiEvent();
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

  void timerEvent(QTimerEvent* e) {
    Q_UNUSED(e);

    GetMessages();
  }

 private:
  QTextEdit* chat_text_;
  QLineEdit* serv_ip_;
  QLineEdit* serv_port_;
  QLineEdit* nick_;

  std::unique_ptr<PtxChatClient> client_;
};

class GUIBackend {
 public:
  /**
   * \brief Pop the top element of gui events queue
   */
  std::unique_ptr<struct GuiEvent> PopGuiEvent() {
    return gui_events_.front();
  }

  /**
   * \brief Push gui event to the queue
   * \return false when queue length is MAX_GUI_EVENTS_Q_SIZE
   */
  bool PushGuiEvent(GuiEvType t, std::unique_ptr<struct ChatMsg> msg) {

  }

 protected:
  SharedUDeque<struct ChatMsg> gui_events_;
  std::mutex gui_events_mtx_;
};

}  // namespace ptxchat

#endif  // GUI_H_
