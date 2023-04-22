## BL40A1812 - Introduction to Embedded Systems 
This repository contains the source code of my excercise work.

`util.sh` is a AVR utility tool, which allows compilation and uploading +
several other features. `do.sh` is set up to compile this repository by
calling `util.sh`. avr-gcc, avr-binutils ([with avr-binutils-size.patch](
https://github.com/jovaska1337/portage_patches/blob/master/cross-avr/binutils/avr-binutils-size.patch
)), avrdude, bash and other basic unix utilities are required for them to work.

To compile and upload this given your setup is correct, connect Arduino UNO
first (so it becomes /dev/ttyACM0) and then connect Arduino Mega (so it
becomes /dev/ttyACM1). If there are more serial devices `do.sh` will not
autodetect them and you'll have to specify them via environment variables.
Once you've connected both devices, issue `./do.sh compile upload` and the
code is compiled and uploaded for both devices. After this, you can issue
`./do.sh clean` to remove compilation artifacts in `/tmp`.

`io_map.txt` contains IO mappings for both devices. `frontend` contains
the code running on the Arduino Mega and `backend` the code for Arduino UNO,
both of which have `shared` symlinked internally.

The basic architechture for both devices is that we run an event loop in
`main()` which is accessible to all compilation units. Compilation units
may freely dispatch events on the event loop and receive events outside
themselves. `main()` will put the CPU into sleep mode if no events are coming
in, meaning that only event sources are interrupts. Another important part
of the architecture is the 10Hz tick timer which fires TIMER events, on which,
many of the modules rely on to function. All event handlers are prefixed with
`e_` to easily identify them. All event codes and event handlers can be found
in `shared/main/globals.in`. Another trick we use is the .initN sections
supported by avr-gcc. The `INIT()` macro (specified in `shared/utils/init.h`)
is used to put each compilation unit's initialization code in .init7, which
will run before main. The real entry point (`share/main/main.c`) uses .init1
for base initialization and .init8 to complete initialization before calling
`main()`.

Given this architecture,  look in `{front,back}end/program/main.c` for the
control logic, which defines all functionality. Some features like button input,
screen control, alarm/buzzer control and motion detection are put into their
own modules in the `program` directory.
