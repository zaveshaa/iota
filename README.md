<p align="center">
  <img src="assets/logo.svg" width="128" height="128" alt="iota">
</p>

<h1 align="center">iota</h1>

<p align="center">a small ray tracer for the terminal</p>

<p align="center">
  <img src="https://img.shields.io/badge/C-C11-blue.svg" alt="C11">
  <img src="https://img.shields.io/badge/dependencies-none-brightgreen.svg" alt="no dependencies">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="MIT license"></a>
</p>

## build

```sh
make
```

## run

```sh
make run
```

Other targets:

```sh
make walk       # walking demo
make edit       # level editor
make install    # install iota
make clean      # remove build files
```

## controls

### walk

- `W A S D` — move
- arrow keys — look
- `space` — jump
- `Esc` — quit

### editor

- mouse — select and build
- `1` … `0` — tools
- `W A S D` — fly
- `space` / `c` — move up / down
- arrow keys — look
- `Ctrl+S` / `Ctrl+O` — save / load
- `Ctrl+Z` / `Ctrl+Y` — undo / redo
- `Esc` — quit

## layout

```text
include/    headers
src/        renderer and engine
demos/      scenes
assets/     logo and sample files
tests/      tests
```

## license

MIT
