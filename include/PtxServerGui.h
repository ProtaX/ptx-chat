#ifndef PTXSERVERGUI_H_
#define PTXSERVERGUI_H_

#include <memory>
#include <thread>
#include <string>

#include "PtxChatServer.h"
#include "PtxGuiFrontend.h"

namespace ptxchat {

inline std::unique_ptr<PtxChatServer> server_;

class ServerGUI : public QWidget {
  Q_OBJECT

 public:
  ServerGUI(QWidget* p = nullptr) : QWidget(p) {
    QPushButton* start_btn = new QPushButton("Start server", this);
    //QPushButton* stop_btn = new QPushButton("Stop server", this);
    //chat_text_ = new QTextEdit();
    //chat_text_->setReadOnly(true);

    start_btn->setGeometry(20, 20, 20, 20);
    //stop_btn->setGeometry(20, 20, 40, 20);
    /*
    QGridLayout* grid = new QGridLayout(this);
    grid->addWidget(start_btn);
    grid->addWidget(stop_btn);
    setLayout(grid);
    */
    connect(start_btn, &QPushButton::clicked, qApp, &QApplication::quit);

    server_ = std::make_unique<PtxChatServer>();
  }

 private slots:
  void ProcessGUIEvents()  {

  }
  void timerEvent(QTimerEvent* e)  {
    Q_UNUSED(e);
  }

  //QTextEdit* chat_text_;
 private:
};

}  // namespace ptxchat

#endif  // PTXSERVERGUI_H_