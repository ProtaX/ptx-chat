#ifndef PTXCLIENTGUI_H_
#define PTXCLIENTGUI_H_

#include <memory>
#include <thread>
#include <string>

#include "PtxChatClient.h"
#include "PtxGuiFrontend.h"

namespace ptxchat {

class ClientGUI : public GUIFrontend {
 public:
  explicit ClientGUI(QWidget* p = nullptr);

 private slots:
  void LogIn();
  void LogOut();
  void ProcessGUIEvents() final;
  void timerEvent(QTimerEvent* e) final;

 private:
  QTextEdit* chat_text_;
  QLineEdit* serv_ip_;
  QLineEdit* serv_port_;
  QLineEdit* nick_;

  std::unique_ptr<PtxChatClient> client_;
};

}  // namespace ptxchat

#endif  // PTXCLIENTGUI_H_
