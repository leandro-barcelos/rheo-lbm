#include "../lattice_types.slang"
struct Camera { column_major float4x4 model; column_major float4x4 view; column_major float4x4 proj; };
[[vk::binding(0,0)]] ConstantBuffer<Camera> camera;
[[vk::binding(1,0)]] StructuredBuffer<Cell> lattice;
struct Parameters { float4 shape_elevation; float4 center_radius; float4 mode; };
[[vk::push_constant]] Parameters parameters;
struct VertexOutput { float4 position : SV_Position; float4 color : COLOR0; };
uint index_of(int3 p) {uint3 shape=uint3(parameters.shape_elevation.xyz);return p.x+p.y*shape.x+p.z*shape.x*shape.y;}
[shader("vertex")]
VertexOutput vert_main(uint vertex_id : SV_VulkanVertexID, uint instance_id : SV_VulkanInstanceID) {
    VertexOutput output;output.position=float4(2,2,2,1);output.color=0;
    bool preview=parameters.mode.x>0;
    bool dam_line=parameters.mode.x==2;
    uint index=instance_id;
    if(preview) {
      int3 p=int3(parameters.center_radius.xyz);
      if(!dam_line) {
        int radius=int(parameters.center_radius.w),side=2*radius+1;
        p.x+=int(instance_id)%side-radius;p.z+=int(instance_id)/side-radius;
      }
      if(any(p<0)||any(p>=int3(parameters.shape_elevation.xyz)))return output;
      index=index_of(p);
    }
    Cell cell=lattice[index];
    float gray=saturate(cell.position.y/max(parameters.shape_elevation.w,1.0));
    output.color=cell.type==FLUID_CELL?float4(0,0,1,1):cell.type==DAM_OBSTACLE_CELL?float4(.9,.24,.04,1):float4(gray.xxx,1);
    if(!preview && (cell.type==GAS_CELL||cell.type==INTERFACE_CELL))return output;
    if(preview) {
      int mode=int(parameters.mode.y);
      float distance=length(cell.position.xz-parameters.center_radius.xz);
      if(!dam_line && (abs(cell.position.y-parameters.center_radius.y)>.1 || distance>parameters.center_radius.w))return output;
      if(mode==0) {
        if(cell.type==GAS_CELL||cell.position.y<1)return output;
        for(int y=int(cell.position.y)+1;y<int(parameters.shape_elevation.y);++y)
          if(lattice[index_of(int3(int(cell.position.x),y,int(cell.position.z)))].type!=GAS_CELL)return output;
        output.color=float4(1,.15,.05,.85);
      } else if(mode==1) {
        if(cell.type!=GAS_CELL)return output;
        output.color=float4(.35,1,.2,.38);
      } else if(mode==2) {
        if(cell.type==TERRAIN_OBSTACLE_CELL||cell.type==DAM_OBSTACLE_CELL)return output;
        output.color=float4(gray.xxx,.55);
      } else if(mode==3) {
        if(distance>.1||cell.type==GAS_CELL)return output;
        output.color=float4(1,.55,0,.85);
      } else if(mode==4) {
        if(!dam_line && cell.type==GAS_CELL)return output;
        output.color=float4(1,.25,.02,.65);
      } else {
        if(distance>.1||cell.type!=GAS_CELL)return output;
        output.color=float4(0,.55,1,.65);
      }
    }
    static const uint corners[36] = {
        0,4,2, 2,4,6, 1,3,5, 3,7,5,
        0,1,4, 1,5,4, 2,6,3, 3,6,7,
        0,2,1, 1,2,3, 4,5,6, 5,7,6
    };
    uint corner=corners[vertex_id];
    float3 offset=float3(corner&1,(corner>>1)&1,(corner>>2)&1)-.5;
    if(preview)offset*=1.004;
    float3 shape=parameters.shape_elevation.xyz;
    float scale=1.0/max(max(shape.x,shape.y),shape.z);
    float3 position=(cell.position.xyz-.5*(shape-1)+offset)*scale;
    output.position=mul(camera.proj,mul(camera.view,mul(camera.model,float4(position,1))));
    return output;
}
[shader("fragment")]
float4 frag_main(VertexOutput input) : SV_Target { return input.color; }
