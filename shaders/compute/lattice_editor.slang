#include "../lattice_types.slang"
[[vk::binding(0,0)]] StructuredBuffer<uint2> changes;
[[vk::binding(1,0)]] RWStructuredBuffer<Cell> lattice;
struct Parameters { uint count; uint offset; };
[[vk::push_constant]] Parameters parameters;
[shader("compute")]
[numthreads(256,1,1)]
void edit_cells(uint3 id : SV_DispatchThreadID) {
    if(id.x>=parameters.count)return;
    uint2 change=changes[parameters.offset+id.x];
    Cell cell=lattice[change.x];
    cell.type=int(change.y);
    bool liquid=cell.type==FLUID_CELL || cell.type==INTERFACE_CELL;
    cell.velocity=0;
    cell.density=liquid?1.0:0.0;
    cell.pressure=cell.density/3.0;
    cell.mass=cell.type==FLUID_CELL?1.0:cell.type==INTERFACE_CELL?0.5:0.0;
    lattice[change.x]=cell;
}
