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
- Water: a smoothed-particle pool sits in the walk range, splashes where the
  body steps through it, and a body in the water drags as it wades.
- A level carries its own springs: any number of `spout` mouths feed the pool,
  so a saved map stirs ripples and lifts steam where walk loads it.
- The sky carries a field of stars and, when a level hangs one, a moon: the
  `sky` line takes the moon's direction, colour and size.
- Light stops at the first surface in its way: each light casts a shadow ray,
  so a pillar or a roof lays its shade on what it stands over.
- Fog pulls distant surfaces toward the sky colour along the view direction,
  so the ground slips into the horizon cleanly, with a smoother ramp in.
- Steam leaves a pool as a drift of small translucent motes that rise and
  fade, thinning toward their rim so what is behind shows through, instead of
  a thin string of single specks or a solid mass. A frame remembers the depth
  of what it drew, so no mote or spring jet shows through a wall standing in
  front of it.
- Water carries heat. A hot pool — the bath, a hot spring — breathes steam
  from across its whole surface, and the motes rise far and live about a
  minute before the top ones thin out; a cold pool, an ocean, steams nowhere.
  Each mote is a few overlapping lobes, so a puff reads as a rounded but
  irregular wisp rather than a perfect disc. The `water` line takes an
  optional heat after its spacing.

### Changed

- The frame is drawn between two simulation steps, so a body moves smoothly
  whatever the frame rate the terminal holds.
- A mesh is solid where it stands: a body walks into the box around it and
  stands on the box's top.
- The jump is a little stronger, so the range's pillars are low enough to hop
  onto and the walk flows over them instead of into a wall.
- The test range hangs a moon, feeds its pit from several springs and starts
  the body at the pool's rim, so the walk opens on the sky, the stars, the
  rising steam and the shadows at once.

### Fixed

- A held direction keeps walking across the terminal's own repeat delay,
  instead of stopping and starting inside it.
- `d` walks to the right and `a` to the left, as the view has them.
- The walking range has the mirror it was missing.
- A jump catches a ledge a step cannot reach, instead of sliding up a face
  that is too tall to get over.
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
