#ifndef GUI_H_
#define GUI_H_

#include <qt5/QtWidgets/QWidget>
#include <qt5/QtWidgets/QApplication>
#include <qt5/QtWidgets/QPushButton>
#include <qt5/QtWidgets/QGridLayout>
#include <qt5/QtWidgets/QTextEdit>

#include <memory>
#include <thread>

#include "PtxChatServer.h"
#include "Message.h"

namespace ptxchat {

class ServerGUI : public QWidget {
  Q_OBJECT

 public:
  explicit ServerGUI(QWidget* p = nullptr) : QWidget(p) {
    QPushButton* start_btn_ = new QPushButton("Start server", this);
    QPushButton* stop_btn_ = new QPushButton("Stop server", this);
    QPushButton* refresh_btn_ = new QPushButton("Refresh", this);
    text_ = new QTextEdit();

    server_ = std::make_unique<PtxChatServer>();

    QGridLayout* grid = new QGridLayout(this);
    grid->addWidget(start_btn_);
    grid->addWidget(stop_btn_);
    grid->addWidget(refresh_btn_);
    grid->addWidget(text_);
    setLayout(grid);

    connect(start_btn_, &QPushButton::clicked, this, &ptxchat::ServerGUI::StartServer);
    connect(stop_btn_, &QPushButton::clicked, this, &ptxchat::ServerGUI::StopServer);
    connect(refresh_btn_, &QPushButton::clicked, this, &ptxchat::ServerGUI::GetMessages);
  }

 private slots:
  void StartServer() {
    server_->Start();
  }

  void StopServer() {
    server_->Stop();
  }

  void GetMessages() {
    while (1) {
      char text[MAX_MSG_BUFFER_SIZE + 32];
      std::unique_ptr<GuiMsg> e = server_->PopGuiEvent();
      if (e->type == GuiMsgType::Q_EMPTY)
        break;
      switch (e->type) {
        case GuiMsgType::Q_EMPTY:
          break;
        case GuiMsgType::SRV_START:
          snprintf(text, MAX_MSG_BUFFER_SIZE, "[SRV] start\n");
          break;
        case GuiMsgType::SRV_STOP:
          snprintf(text, MAX_MSG_BUFFER_SIZE, "[SRV] stop\n");
          break;
        case GuiMsgType::CLIENT_REG:
          snprintf(text, MAX_MSG_BUFFER_SIZE, "[CLIENT_REG] %s\n", e->msg->hdr.from);
          break;
        case GuiMsgType::CLIENT_UNREG:
          snprintf(text, MAX_MSG_BUFFER_SIZE, "[CLIENT_UNREG] %s\n", e->msg->hdr.from);
          break;
        case GuiMsgType::PUBLIC_MSG:
          snprintf(text, MAX_MSG_BUFFER_SIZE, "[MSG] %s says: %s\n", e->msg->hdr.from, e->msg->buf);
          break;
        case GuiMsgType::PRIVATE_MSG:
          snprintf(text, MAX_MSG_BUFFER_SIZE, "[PRIV] %s to %s\n", e->msg->hdr.from, e->msg->hdr.to);
          break;
      }
      QString qtext(text);
      text_->append(qtext);
    }
  }

 private:
  QTextEdit* text_;
  std::unique_ptr<PtxChatServer> server_;
};

}  // namespace ptxchat

#endif  // GUI_H_
