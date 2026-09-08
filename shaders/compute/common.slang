#include "../lattice_types.slang"

static const uint DIRECTION_COUNT = 19;
static const float MIN_DENSITY = 1.0e-12;

bool is_obstacle(int type) {
  return type == TERRAIN_OBSTACLE_CELL || type == DAM_OBSTACLE_CELL;
}
int3 direction(int i) {
  switch (i) {
    case 1: return int3(1,0,0); case 2: return int3(-1,0,0);
    case 3: return int3(0,1,0); case 4: return int3(0,-1,0);
    case 5: return int3(0,0,1); case 6: return int3(0,0,-1);
    case 7: return int3(1,1,0); case 8: return int3(1,-1,0);
    case 9: return int3(-1,1,0); case 10: return int3(-1,-1,0);
    case 11: return int3(1,0,1); case 12: return int3(1,0,-1);
    case 13: return int3(-1,0,1); case 14: return int3(-1,0,-1);
    case 15: return int3(0,1,1); case 16: return int3(0,1,-1);
    case 17: return int3(0,-1,1); case 18: return int3(0,-1,-1);
    default: return int3(0,0,0);
  }
}
int opposite(int i) {
  switch (i) {
    case 1:return 2; case 2:return 1; case 3:return 4; case 4:return 3;
    case 5:return 6; case 6:return 5; case 7:return 10; case 8:return 9;
    case 9:return 8; case 10:return 7; case 11:return 14; case 12:return 13;
    case 13:return 12; case 14:return 11; case 15:return 18; case 16:return 17;
    case 17:return 16; case 18:return 15; default:return 0;
  }
}
float weight(int i) { return i == 0 ? 1.0/3.0 : i <= 6 ? 1.0/18.0 : 1.0/36.0; }
uint distribution_index(uint cell_index, int i) { return cell_index * DIRECTION_COUNT + uint(i); }
float equilibrium(int i, float density, float3 velocity) {
  float eu = dot(float3(direction(i)), velocity);
  return weight(i)*density*(1.0+3.0*eu+4.5*eu*eu-1.5*dot(velocity,velocity));
}
float epsilon(Cell cell) {
  if (cell.type == FLUID_CELL || is_obstacle(cell.type) ||
      cell.type == INTERFACE_TO_FLUID_CELL) return 1.0;
  if (cell.type == GAS_CELL || cell.type == INTERFACE_TO_GAS_CELL) return 0.0;
  return cell.density <= MIN_DENSITY ? 0.5 : saturate(cell.mass/cell.density);
}
bool valid(int3 p, uint3 shape) { return all(p >= 0) && all(p < int3(shape)); }
uint cell_index(int3 p, uint3 shape) { return uint(p.x)+uint(p.y)*shape.x+uint(p.z)*shape.x*shape.y; }
float3 surface_normal(int3 p, uint3 shape, RWStructuredBuffer<Cell> lattice) {
  int3 dx=int3(1,0,0),dy=int3(0,1,0),dz=int3(0,0,1);
  float left=valid(p-dx,shape)?epsilon(lattice[cell_index(p-dx,shape)]):1.0;
  float right=valid(p+dx,shape)?epsilon(lattice[cell_index(p+dx,shape)]):1.0;
  float down=valid(p-dy,shape)?epsilon(lattice[cell_index(p-dy,shape)]):1.0;
  float up=valid(p+dy,shape)?epsilon(lattice[cell_index(p+dy,shape)]):1.0;
  float back=valid(p-dz,shape)?epsilon(lattice[cell_index(p-dz,shape)]):1.0;
  float front=valid(p+dz,shape)?epsilon(lattice[cell_index(p+dz,shape)]):1.0;
  return 0.5*float3(left-right,up-down,front-back);
}
