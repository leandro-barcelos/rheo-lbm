#ifndef RHEO_UI_FREE_SURFACE_PANELS_H
#define RHEO_UI_FREE_SURFACE_PANELS_H

#include "panels/control_panel.h"
#include "panels/parameters_panel.h"
#include "rheo/application/application_command.h"
#include "rheo/application/application_state.h"
namespace ui {

class FreeSurfacePanels {
 public:
  void Draw(application::ApplicationViewState const& state,
            application::ICommandSink& commands);

 private:
  ParametersPanel parameters_;
  ControlPanel control_;
};

}  // namespace ui

#endif  // RHEO_UI_FREE_SURFACE_PANELS_H
