# RheoLBM

> ⚠️ **Status:** Active development.


### Interactive lattice editor

The **Lattice editor** panel provides Erase, Terrain, Elevation, Eyedropper,
Dam and Water bucket. Left click applies a tool; dragging applies new samples
only when the cursor moves. Right/middle drag or Space+left pans, and the wheel
zooms. North remains at the top and east at the right. Ctrl+Z/Ctrl+Y undo/redo;
Escape cancels an unfinished dam. Dam points also have Finish, Remove point
and Cancel buttons.

Terrain is gray, dams are orange and fluid is blue. Interfaces remain hidden
except during editing highlights, matching the Unity reference. The translucent
preview is a separate depth-tested pass with no depth writes. Moving it issues
no lattice uploads or compute dispatches.

The base layer is protected. Elevation only raises terrain; the eyedropper
ignores water and dams. A dam follows Bresenham segments with circular thickness
and the lower of its endpoint elevations. The bucket fills enclosed layers,
stopping before a spill and leaving the top grid layer unfilled.

Each stroke, completed dam or bucket is one history operation (100 operations,
64 MiB maximum). Restore terrain returns to the GPU-voxelized DEM and clears
history. Changing subdivisions or margin with existing edits asks before
rebuilding. Projects use YAML version 2, retain version 1 compatibility and store
sparse cell differences plus a SHA-256 of normalized DEM data and the lattice
definition. Loading validates these before replacing the scene. Saving uses a
temporary file and atomic rename; previews, history and pending dam points are
not persisted. OpenSSL Crypto is a direct build dependency.

The editor initializes water in lattice units (density 1, pressure 1/3, zero
velocity, mass 1 for fluid or 0.5 for interface). LBM dynamics and population
buffers remain a subsequent stage.

Run `ctest --test-dir build --output-on-failure` for domain, input, project and
camera tests. Configure `-DRHEO_GPU_TESTS=ON` for GPU readback checks and the
optional visual smoke executable. From `build`, run
`VK_LAYER_VALIDATE_SYNC=1 ./tests/terrain_render_smoke basin` to exercise a
closed basin, previews, editing, undo/redo, save/load and resize with Vulkan
validation (Debug build). A GeoTIFF path can be supplied instead of `basin`.
