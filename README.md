iota
====

A 3D level editor that renders inside the terminal.

    make run

`WASD` move, arrows look, `Esc` quit, `Tab` switch between fly and walk.

Fly ignores gravity. Walk keeps the camera at eye height, falls when there
is nothing underfoot, walks around anything taller than a step and steps
onto anything lower.

Editing:

    1   box           select box primitive
    2   sphere        select sphere primitive
    space             place primitive in front of the camera
    x                 delete object under the crosshair
    o                 save level to level.txt
    l                 load level from level.txt

Objects are boxes and spheres. The camera casts one ray per cell and
keeps the nearest hit, so objects occlude the floor and each other.
