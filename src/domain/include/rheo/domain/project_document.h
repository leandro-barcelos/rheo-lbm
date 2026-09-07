#ifndef RHEO_DOMAIN_PROJECT_DOCUMENT_H
#define RHEO_DOMAIN_PROJECT_DOCUMENT_H

#include <optional>
#include <string>

#include "rheo/domain/lattice_editing.h"
#include "rheo/domain/simulation_config.h"

namespace domain {

struct ProjectDocument {
  SimulationSettingsDraft simulation;
  std::optional<LatticeEdits> edits;
  std::string terrain_path;
  std::string terrain_texture_path;
};

}  // namespace domain

#endif  // RHEO_DOMAIN_PROJECT_DOCUMENT_H
