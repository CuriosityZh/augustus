# Integration and data ownership

Base: upstream commit `7af2ee66fb816145795192c6648a6720e83d284c` (4.0.0).

`src/planning/model.*` implements a bounded, game-independent collection of logical objects. Each object carries a building type, canonical map position, rotation, variant and footprint. Forts have a second footprint; the hippodrome is one long footprint. Network segments are independent objects. Buildings and bridges disappear as complete objects on any committed overlap.

`src/planning/sha256.*` implements FIPS 180-4 SHA-256 and has standard known-answer tests.

`src/planning/storage.*` implements version 1 of the companion format. It only reads the game save. It writes `<save>.planning` using an exclusive temporary file, flush, fsync and rename on macOS/Linux. Header fields are explicit little-endian values, not dumped C structs. Header: 8-byte magic, version, count, map width/height, game SHA-256, payload/header SHA-256. Each object is thirteen little-endian 32-bit fields. Count, file length, checksum and geometry are validated before replacing the in-memory model. Missing data leaves an empty model; damaged data is rejected as a whole.

`src/planning/planning.*` adapts this model to Augustus input and graphics. It owns no game building, figure, resource, routing or terrain state. It reads property and sprite APIs to expand logical plans into drawing pieces. Farmhouse/crops, warehouse cells, fort/grounds, hippodrome sections and bridge sprites are emitted separately. Pieces use the same isometric footprint/top functions and opacity mechanism as Augustus placement ghosts. Draw-tile indices are recalculated on map rotation. Scaling is performed by the existing renderer. A read-only terrain context helper selects road/wall/aqueduct connection sprites without advancing real terrain's variant counters.

## Small integration hooks

- `widget/city/city.c`: process planning mouse input before regular construction and draw the optional controls; suppress normal price text in Planning Mode.
- `widget/city/building_ghost.c`: render a planning preview instead of running the ordinary preview's terrain marking, animal nudging and roamer-path side effects; disable regular demolition preview while planning.
- `widget/city/draw.c`: inject plan footprints and tops into the existing row draw order.
- `input/hotkey.c`: Alt/Option shortcuts only in the city window.
- `window/city.c`: route Undo to the plan model while planning.
- `building/construction.c`: defensively reject real construction entrypoints while planning. Before normal construction, snapshot occupancy only when plans exist. After successful commit, derive newly occupied cells and remove intersecting plans. Early returns do not remove plans. UI preview writes are excluded by taking the snapshot at construction start.
- `game/file.c`: reset plans on city/scenario change, load companion after normal city initialization, copy companions for rotating yearly backups.
- `game/file_io.c`: save companion only after successful normal serialization and close, covering every caller including mission/autosaves. The save format and serialization table are unchanged.
- SDL2/SDL3 preference names, default preference user path and macOS bundle identity: keep this build independent of the original installation.

## Updating and remaining scope

The integration intentionally avoids adding fields to upstream's `building`, `city_data`, terrain grids, routing grids or save-game pieces. No simulation loop enumerates plans. Upstream merges still require review if construction, rendering, type ids or saving changes. Merge success and successful compilation are necessary checks, not proof of semantic compatibility.

Companion v1 is bounded and corruption-detecting, not an interchange format across arbitrary future type-id renumberings. An upstream update that renumbers building types or alters footprints requires a migration/version bump. Windows companion atomic replacement/Unicode paths and touch input are outside the macOS delivery scope; do not advertise them as tested.
