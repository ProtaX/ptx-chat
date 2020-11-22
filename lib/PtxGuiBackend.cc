#include "PtxGuiBackend.h"

#include <memory>

namespace ptxchat {

std::unique_ptr<GuiEvent> GUIBackend::PopGuiEvent() {
  return gui_events_->front();
}

bool GUIBackend::PushGuiEvent(GuiEvType t, std::shared_ptr<ChatMsg> msg) {
  std::unique_ptr<GuiEvent> e = std::make_unique<GuiEvent>();
  e->type = t;
  e->msg = msg;
  return gui_events_->push_back(std::move(e));
}

}  // namespace ptxchat
