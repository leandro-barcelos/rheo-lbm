#ifndef RHEO_SIMULATION_MODEL_H
#define RHEO_SIMULATION_MODEL_H

#include <cstdint>

namespace rheo {

enum class SimulationModel : uint8_t { kFreeSurface3D, kShallowWater2D };

}

#endif  // !RHEO_SIMULATION_MODEL_H
