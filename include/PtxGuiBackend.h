#ifndef PTXGUIBACKEND_H_
#define PTXGUIBACKEND_H_

#include <memory>
#include <vector>
#include <functional>

#include "Message.h"
#include "SharedUDeque.h"

namespace ptxchat {

class GUIBackend {
 public:
  GUIBackend() {
    gui_events_ = std::make_unique<SharedUDeque<GuiEvent>>();
  }

  /**
   * \brief Pop the top element of gui events queue
   * \return Top element 
   */
  std::unique_ptr<GuiEvent> PopGuiEvent();

  /**
   * \brief Push gui event to the queue
   * \param t type of event
   * \param msg message that created event or nullptr
   * \return false when queue length is MAX_GUI_EVENTS_Q_SIZE
   */
  bool PushGuiEvent(GuiEvType t, std::shared_ptr<ChatMsg> msg);

 private:
  std::unique_ptr<SharedUDeque<GuiEvent>> gui_events_;
};

}  // namespace ptxchat

#endif  // PTXGUIBACKEND_H_
