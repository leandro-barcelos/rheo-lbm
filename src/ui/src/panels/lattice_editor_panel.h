#ifndef RHEOLBM_UI_LATTICE_EDITOR_PANEL_H
#define RHEOLBM_UI_LATTICE_EDITOR_PANEL_H

#include "rheo/application/application_command.h"
#include "rheo/application/application_state.h"
namespace ui {

class LatticeEditorPanel {
 public:
  static bool Draw(application::ApplicationViewState const& state,
                   application::ICommandSink& commands);

 private:
  static void DiscardEditsPopup(application::ApplicationViewState const& state,
                                application::ICommandSink& commands);
};

}  // namespace ui

#endif  // !RHEOLBM_UI_LATTICE_EDITOR_PANEL_H
