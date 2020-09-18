#ifndef PTXGUIFRONTEND_H_
#define PTXGUIFRONTEND_H_

#include <qt5/QtWidgets/QApplication>
#include <qt5/QtWidgets/QWidget>
#include <qt5/QtWidgets/QPushButton>
#include <qt5/QtWidgets/QGridLayout>
#include <qt5/QtWidgets/QTextEdit>
#include <qt5/QtCore/QTime>
#include <qt5/QtWidgets/QLineEdit>

namespace ptxchat {

constexpr int GUI_UPDATE_MS = 100;

class GUIFrontend : public QWidget {
  Q_OBJECT

 public:
  GUIFrontend(QWidget* p = nullptr): QWidget(p) {
    startTimer(GUI_UPDATE_MS);
  }

 protected slots:
  virtual void timerEvent(QTimerEvent* e) { Q_UNUSED(e); }
  virtual void ProcessGUIEvents() {}
};

}  // namespace ptxchat

#endif  // PTXGUIFRONTEND_H_