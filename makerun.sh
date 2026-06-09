#!/bin/bash

# > Para poder ejecutar este script deben de dársele permisos ejecutando en la terminal:
# > chmod +x makerun.sh

path_proceso_inicial="PLANI_PRE_0.prc"

# Make todo los programas
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

# Corre cada programa en una terminal Xterm
xterm -T "KERNEL MEMORY" -hold \
-e "bash -c './kernel_memory/bin/kernel_memory ./kernel_memory/src/kernelMemory.config'" &

sleep 1
xterm -T "KERNEL SCHEDULER" -hold \
-e "bash -c './kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernelScheduler.config $path_proceso_inicial'" &

sleep 1
xterm -T "MEMORY STICK" -hold \
-e "bash -c './memory_stick/bin/memory_stick ./memory_stick/src/memoryStick.config 10'" &

sleep 1
xterm -T "CPU 100" -hold \
-e "bash -c './cpu/bin/cpu ./cpu/src/cpu.config 100'" &

sleep 1
#xterm -T "IO" -hold \  
#   -e "bash -c './io/bin/io'" &

sleep 1
#xterm -T "SWAP" - hold \     
#   -e "bash -c './swap/bin/swap'" &