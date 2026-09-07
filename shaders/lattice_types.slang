// Matches simulation::Cell (48 bytes) and simulation::CellType.
static const int FLUID_CELL = 0;
static const int INTERFACE_CELL = 1;
static const int GAS_CELL = 2;
static const int TERRAIN_OBSTACLE_CELL = 3;
static const int DAM_OBSTACLE_CELL = 4;
struct Cell {
    float4 position;
    float4 velocity;
    int type;
    float density;
    float pressure;
    float mass;
};
