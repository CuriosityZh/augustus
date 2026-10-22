# Planning Mode validation

## Portable automated tests

Run:

```sh
cmake -S tests/planning -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

These tests use AddressSanitizer and UndefinedBehaviorSanitizer by default and check:

- Multi-cell building footprints and secondary fort footprints.
- Whole-object removal on a one-cell overlap; independent road/wall/aqueduct segments.
- Movement, map boundaries, invalid coordinates and duplicate painting.
- SHA-256 known answers for empty input, `abc`, and one million `a` characters.
- Companion roundtrip and copying/renaming alongside an exact game-save copy.
- Foreign save revision and different map dimensions.
- Every truncated prefix and every single-byte corruption of a representative companion.
- Oversized counts, invalid geometry with a recomputed checksum, and trailing garbage.
- Failed atomic replacement, absent game file, empty plans, and preservation of all game-file bytes.

## Real engine regression harness

Build with `-DPLANNING_ENGINE_TESTS=ON` to also compile `planning-engine-tests`. The normal application excludes the harness. It needs your own Caesar III assets and an existing save copied into a disposable directory:

```sh
planning-engine-tests /absolute/path/to/C3 /absolute/path/to/scratch-user /absolute/path/to/copied-fixture.svx
```

Create `scratch-user` first. The harness selects its own scratch user directory, never writes its input fixture, does not advance the simulation, and prepares a vegetation-free test district in memory before taking the baseline. It then compares all serialized state plus live building/figure/scenario structures before/after planning (excluding unwritten padding in upstream serialization), exercises real construction overlap and failure, move/erase/undo, valid/missing/corrupt companion loads, bridge planning, and renders all four map rotations at two zoom levels.

The local test run uses private copies of saves; none of these files or their contents are committed or uploaded. CI builds the harness but cannot run it without the proprietary game data.

## Manual checks before promoting an update

Check single- and multi-cell buildings, warehouses, farms, temples, fort grounds, hippodrome sections, bridge approaches, road/wall/aqueduct turns and intersections. Verify sprite order around tall real buildings at each map orientation and zoom. Exercise Save, Save As, Quick Save, yearly autosaves/backups, copying both files, and loading with only the game file. Confirm that planning while the city runs has no effect on walkers, employment, services or finances.

An automated snapshot test covers simulation state while no tick advances. Upstream serializers leave some padding bytes uninitialized, so raw `.svx` files can differ even between two ordinary saves without gameplay. The test compares live structures for these serializers and only meaningful written bytes of formula/text arrays; it does not modify production serialization. It is not a substitute for extended interactive play, full scenario coverage, or testing every possible decorative sprite combination. Current results and any unresolved failures are recorded in the release validation report.
