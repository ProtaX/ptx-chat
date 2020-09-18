#ifndef PTXGUIBACKEND_H_
#define PTXGUIBACKEND_H_

#include <memory>

#include "Message.h"
#include "SharedUDeque.h"

namespace ptxchat {

class GUIBackend {
 public:
  /**
   * \brief Pop the top element of gui events queue
   * \return Top element 
   */
  std::unique_ptr<struct GuiEvent> PopGuiEvent();

  /**
   * \brief Push gui event to the queue
   * \param t type of event
   * \param msg message that created event or nullptr
   * \return false when queue length is MAX_GUI_EVENTS_Q_SIZE
   */
  bool PushGuiEvent(GuiEvType t, std::unique_ptr<struct ChatMsg>&& msg);

 private:
  SharedUDeque<struct GuiEvent> gui_events_;
};

}  // namespace ptxchat

#endif  // PTXGUIBACKEND_H_