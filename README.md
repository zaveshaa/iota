<p align="center">
  <img src="assets/logo.svg" width="128" height="128" alt="iota">
</p>

<h1 align="center">iota</h1>

<p align="center">
  A 3D level editor that renders inside the terminal.
</p>

<p align="center">
  MIT · C11 · no dependencies · macOS and Linux
</p>

## Overview

iota is a 3D engine that renders inside the terminal. The terminal, the
renderer, the world, the meshes and the frame loop are the engine's; the rules
are the client's. This repository carries the level editor, and the fishing
game and the shooting range live on branches of their own.

## Build

    make            build iota
    make install    install into $(PREFIX)/bin, /usr/local by default
    make clean      remove the build

The build is warning-clean under `-Wall -Wextra -Wpedantic -Wshadow
-Wstrict-prototypes -Wconversion`; CI builds it with every warning fatal.

## Run

    make run

A frame is drawn only when a key arrived or the window changed size, so an
idle editor costs no cpu. The window is any size the terminal gives it, and
supersampling drops to a single ray per cell when a frame takes longer than
15 ms, which keeps a large one responsive.

## Controls

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | move |
| arrow keys | look |
| `Tab` | switch between fly and walk |
| `=` `-` | zoom |
| `Esc` | quit |

Fly ignores gravity. Walk keeps the camera at eye height, falls when there is
nothing underfoot, walks around anything taller than a step and steps onto
anything lower.

## Editing

| Key | Action |
| --- | --- |
| `1` `2` `3` | select box, sphere or mesh |
| `,` `.` | pick which loaded mesh |
| `space` | place object in front of the camera |
| `x` | delete object under the crosshair |
| `r` `R` | turn the object under the crosshair by 15 degrees |
| `y` | cycle its size through 0.5, 1, 2 and 4 |
| `o` | save level to `level.txt` |
| `l` | load level from `level.txt` |

Turning and sizing act on whatever the crosshair points at, so the three keys
cover every object without picking modes or bringing out a gizmo. Rotation is
around the vertical axis and size scales the object about its own origin. The
status line shows the target with its current angle and size.

Levels written by older versions still load: a line without angle and size
gets no turn and its original size.

## Meshes

Meshes are Wavefront OBJ or binary glTF. Put one path per line in
`meshes.txt` and they load at startup; a level refers to a mesh by its file
name and looks for it in `meshes/` if it is not loaded yet. Rendering hits
the real triangles, walking uses the bounding box of the mesh. Both follow
the turn and the size of the object, because the box is measured in the
object's own space and the mesh shrinks and turns with it.

Triangles go into a BVH at load time, so mesh size is not a problem: a 600k
triangle glb loads in half a second and renders at the same speed as a small
mesh. The glTF reader takes positions, indices and the node transform out of
the file; materials, normals and textures are ignored, and modes other than
triangles are skipped.

## The renderer

Four rays per cell, nearest hit kept, so thin geometry survives and objects
occlude the floor and each other. Surfaces get one directional light plus a
dim fill from the viewer, which keeps characters readable when they face the
camera.

## Layout

    include/    the engine's headers
    src/        the engine
    assets/     the mark

## License

MIT. See [LICENSE](LICENSE).
