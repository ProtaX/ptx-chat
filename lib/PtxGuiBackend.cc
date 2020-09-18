#include "PtxGuiBackend.h"

#include <memory>

namespace ptxchat {

std::unique_ptr<struct GuiEvent> GUIBackend::PopGuiEvent() {
  return gui_events_.front();
}

bool GUIBackend::PushGuiEvent(GuiEvType t, std::unique_ptr<struct ChatMsg>&& msg) {
  std::unique_ptr<struct GuiEvent> e = std::make_unique<struct GuiEvent>();
  e->type = t;
  e->msg = std::move(msg);

  return gui_events_.push_back(std::move(e));
}

}  // namespace ptxchat
