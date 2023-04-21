## BL40A1812 - Introduction to Embedded Systems 
This repository contains the source code of my excercise work.

`util.sh` is a AVR utility tool, which allows compilation and uploading +
several other features. `do.sh` is set up to compile this repository by
calling `util.sh`.

`io_map.txt` contains IO mappings for both devices. `frontend` contains
the code running on the Arduino Mega and `backend` the code for Arduino UNO,
both of which have `shared` symlinked internally.

This program is event/interrupt driven so the only thing `main()` does that
it runs the global event loop. `shared/main/{globals,main}.c` contain setup
code for the event loop and the actual entry point for the program.
`shared/main/globals.in` contains event code/handler mappings which allow the
rest of the program to work.

Look in `{front,back}end/program/main.c` for the actual control logic.
