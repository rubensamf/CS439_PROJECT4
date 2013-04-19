!/bin/bash

make clean
make
cd build
pintos --filesys-size=2 -p ../../examples/echo -a echo -- -f -q run 'echo a'
