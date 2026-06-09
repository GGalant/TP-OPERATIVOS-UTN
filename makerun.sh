#!/bin/bash

#=================================================================================#
#────────────────────── VERSION FULL FULL DEL SCRIPT DE RUN ──────────────────────#
#=================================================================================#

# > IMPORTANTE:
# > Para poder ejecutar este script deben de dársele permisos de ejecución, mediante el comando en la terminal:
# > chmod +x makerun.sh

path_proceso_inicial="PLANI_PRE_0.prc"


# Este script crea terminales Xterm


#──── ELIMINO XTERM ANTERIORES────────────────────────────────────────────────────
pkill xterm

# ─── ESTILOS DE TERMINALES Xterm NUEVAS: ────────────────────────────────────────
WIN_COLS=80        # Ancho de ventana en CARACTERES
WIN_ROWS=24        # Alto de ventana en CARACTERES
CASCADE_OFFSET=30   # Desplazamiento en PIXELES entre cada ventana
# Obtiene las dimensiones (PIXELES) de la pantalla del usuario para que no quede desproporcionado
SCREEN_W=$(xdpyinfo | grep dimensions | awk '{print $2}' | cut -d'x' -f1)
SCREEN_H=$(xdpyinfo | grep dimensions | awk '{print $2}' | cut -d'x' -f2)
# Calculo el tamanio de las Xterm en PIXELES ()
WIN_W=$(( WIN_COLS * 10 ))
WIN_H=$(( WIN_ROWS * 20 ))
# Posición inicial en pantalla en PIXELES desde la esquina (intento que sea el medio aprox)
BASE_X=$(( (SCREEN_W - WIN_W) / 2 ))
BASE_Y=$(( (SCREEN_H - WIN_H) / 2 ))
# Contador de cascada (cada Xterm nueva se desplaza un porotito mas)
CASCADE=0

# Espera activa hasta que la ventana con ese título se haya renderizado (para evitar que una ventana anterior quede por encima de una posterior porque se redenrizo luego)
wait_for_window() {
    local title="$1"
    while ! xwininfo -name "$title" &>/dev/null; do
        sleep 0.03
    done
    sleep 0.1
}

#Funcion para abrir Xterm estilizada B)
open_xterm() {
    local title="$1" #Parametro de la funcion (titulo de la ventana)
    local cmd="$2" #Parametro de la funcion (comando a ser ejecutado)
    local x=$(( BASE_X + CASCADE * CASCADE_OFFSET )) #Posicion de la ventana contando cascada
    local y=$(( BASE_Y + CASCADE * CASCADE_OFFSET )) #Posicion de la ventana contando cascada

    # EL CORAZON DE LA FUNCION (crea la ventana)
    xterm -T "$title" -hold \
        -geometry "${WIN_COLS}x${WIN_ROWS}+${x}+${y}" \
        -e "bash -c '$cmd'" &
    #Hold causa que la ventana no se cierre automaticamente al finalizar la tarea
    #geometry permite decidir dimensiones y ubicacion de la ventana
    #-e espera el comando a ser ejecutado: ejecutamos bash (el interprete de comandos) y le pasamos nuestro comando
    wait_for_window "$title"
    #CASCADE++ para la prox ;)
    CASCADE=$(( CASCADE + 1 ))
}

#──── BUILDEAMOS CON MAKE──────────────────────────────────────────────────────────

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

#──── CORREMOS LAS TERMINALES──────────────────────────────────────────────────────────

# Corre cada programa en una terminal Xterm (a traves de la funcion creada mas arriba)

open_xterm "KERNEL MEMORY" \
    "sleep 1; ./kernel_memory/bin/kernel_memory ./kernel_memory/src/kernelMemory.config"

open_xterm "KERNEL SCHEDULER" \
    "sleep 1; ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernelScheduler.config $path_proceso_inicial"

open_xterm "MEMORY STICK" \
    "sleep 1; ./memory_stick/bin/memory_stick ./memory_stick/src/memoryStick.config 10"

open_xterm "CPU 100" \
    "sleep 1; ./cpu/bin/cpu ./cpu/src/cpu.config 100"

# open_xterm "IO" \
#     "sleep 1; ./io/bin/io"

# open_xterm "SWAP" \
#     "sleep 1; ./swap/bin/swap"