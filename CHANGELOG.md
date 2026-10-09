# Changelog

Notable changes to iota are written down here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the version
numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The terminal reports keys coming up where it can (the keyboard protocol),
  and a key held in a scene is a key down until it does.
- Meshes: an OBJ loader, a bounding volume hierarchy over the triangles, and a
  Moller-Trumbore ray test.
- A scene can hold a mesh, so a ray casts against its triangles and the walk
  demo hangs a loaded gem over the range.

### Changed

- The frame is drawn between two simulation steps, so a body moves smoothly
  whatever the frame rate the terminal holds.

### Fixed

- A held direction keeps walking across the terminal's own repeat delay,
  instead of stopping and starting inside it.
- `d` walks to the right and `a` to the left, as the view has them.
- The walking range has the mirror it was missing.
- The walk demo's gem floats clear of a high jump, instead of swallowing the
  eye when it climbs.

## [0.1.0] - 2026-10-09

### Added

- A terminal renderer: the alternate screen, raw mode and a cell grid with
  24-bit colour.
- A frame loop with a fixed simulation step and a per-frame time budget.
- A ray caster for boxes, spheres and planes that keeps the nearest hit.
- A camera and a view with supersampling and a luminance ramp.
- Mirror reflections, two bounces deep, and distance fog.
- Scenes of objects and point lights with ambient shading.
- WASD movement, arrow-key look and `Esc` to quit.
- A golden-frame test, and CI on Linux and macOS with warnings fatal.

[Unreleased]: https://github.com/zaveshaa/iota/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/zaveshaa/iota/releases/tag/v0.1.0
