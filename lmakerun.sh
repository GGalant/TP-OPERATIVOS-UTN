#!/bin/bash

#=============================================================================#
#────────────────────── VERSION LIGHT DEL SCRIPT DE RUN ──────────────────────#
#=============================================================================#

path_proceso_inicial="PLANI_PRE_0.prc"

# > IMPORTANTE:
# > Para poder ejecutar este script deben de dársele permisos de ejecución, mediante el comando en la terminal:
# > chmod +x makerun.sh


# Este script crea terminales Xterm.


#──── ELIMINO XTERM ANTERIORES────────────────────────────────────────────────────
pkill xterm


#──── BUILDEAMOS CON MAKE──────────────────────────────────────────────────────────

# Make todo los programas "> /dev/null" era un intento de que no loguee el make, pero que loguee ya fue
make -C ./kernel_memory clean > /dev/null
make -C ./kernel_memory > /dev/null

make -C ./kernel_scheduler clean > /dev/null
make -C ./kernel_scheduler > /dev/null

make -C ./memory_stick clean > /dev/null
make -C ./memory_stick > /dev/null

make -C ./cpu clean > /dev/null
make -C ./cpu > /dev/null

make -C ./io clean > /dev/null
make -C ./io > /dev/null

make -C ./swap clean > /dev/null
make -C ./swap > /dev/null


#──── CORREMOS LAS TERMINALES──────────────────────────────────────────────────────────

# Corro cada programa en una terminal Xterm sin estilos
xterm -T "KERNEL MEMORY" -hold \
-e "bash -c './kernel_memory/bin/kernel_memory ./kernel_memory/src/kernelMemory.config'" &

sleep 2
xterm -T "KERNEL SCHEDULER" -hold \
-e "bash -c './kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernelScheduler.config $path_proceso_inicial'" &

sleep 2
xterm -T "MEMORY STICK" -hold \
-e "bash -c './memory_stick/bin/memory_stick ./memory_stick/src/memoryStick.config 10'" &

sleep 2
xterm -T "CPU 100" -hold \
-e "bash -c './cpu/bin/cpu ./cpu/src/cpu.config 100'" &

sleep 2
#xterm -T "IO" -hold \  
#   -e "bash -c './io/bin/io'" &

sleep 2
#xterm -T "SWAP" - hold \     
#   -e "bash -c './swap/bin/swap'" &