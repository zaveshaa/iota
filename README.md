iota
====

A 3D level editor that renders inside the terminal.

    make run

`WASD` move, arrows look, `Esc` quit, `Tab` switch between fly and walk,
`=` and `-` zoom.

Fly ignores gravity. Walk keeps the camera at eye height, falls when there
is nothing underfoot, walks around anything taller than a step and steps
onto anything lower.

Editing:

    1              select box primitive
    2              select sphere primitive
    3              select mesh
    , .            pick which loaded mesh
    space          place object in front of the camera
    x              delete object under the crosshair
    o              save level to level.txt
    l              load level from level.txt

Meshes are Wavefront OBJ or binary glTF. Put one path per line in
`meshes.txt` and they load at startup; a level refers to a mesh by its file
name and looks for it in `meshes/` if it is not loaded yet. Rendering hits
the real triangles, walking uses the bounding box of the mesh. Triangles go
into a BVH at load time, so mesh size is not a problem: a 600k triangle glb
loads in half a second and renders at the same speed as a small mesh.

The glTF reader takes positions, indices and the node transform out of the
file. Materials, normals and textures are ignored, and modes other than
triangles are skipped.

The camera casts four rays per cell and keeps the nearest hit, so thin
geometry survives and objects occlude the floor and each other. Surfaces get
one directional light plus a dim fill from the viewer, which keeps characters
readable when they face the camera.

A frame is drawn only when a key arrived or the window changed size, and the
frame rate is capped, so an idle editor costs no cpu.
