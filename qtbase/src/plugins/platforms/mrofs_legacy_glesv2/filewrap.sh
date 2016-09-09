#!/bin/sh
#TODO: add to pro
./utils/Filewrap/Linux_x86_32/Filewrap -s -o background_basic_vsh.cpp  background_basic.vsh
./utils/Filewrap/Linux_x86_32/Filewrap -s -o background_basic_fsh.cpp  background_basic.fsh
./utils/Filewrap/Linux_x86_32/Filewrap -s -o backingstore_vsh.cpp  backingstore.vsh
./utils/Filewrap/Linux_x86_32/Filewrap -s -o backingstore_fsh.cpp  backingstore.fsh
./utils/Filewrap/Linux_x86_32/Filewrap -s -o waveform_vsh.cpp  waveform.vsh
./utils/Filewrap/Linux_x86_32/Filewrap -s -o waveform_fsh.cpp  waveform.fsh
echo done!

