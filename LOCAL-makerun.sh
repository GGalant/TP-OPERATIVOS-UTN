#!/bin/bash

#=================================================================================#
#────────────────────── VERSION FULL FULL DEL SCRIPT DE RUN ──────────────────────#
#=================================================================================#

# > IMPORTANTE:
# > Para poder ejecutar este script deben de dársele permisos de ejecución, mediante el comando en la terminal:
# > chmod +x makerun.sh

path_proceso_inicial="MEMORIA_PRE_0.prc"


# Este script crea terminales Xterm


#──── ELIMINO XTERM ANTERIORES────────────────────────────────────────────────────
pkill xterm

# ─── ESTILOS DE TERMINALES Xterm NUEVAS: ────────────────────────────────────────
# MATEMATICAS LOCAS PARA DETERMINAR EL CENTRO DE LA PANTALLA Y UN DESPLAZAMIENTO DIAGONAL 

WIN_COLS=80        # Ancho de ventana Xterm deseado en CARACTERES
WIN_ROWS=24        # Alto de ventana Xterm deseado en CARACTERES
CASCADE_OFFSET=30   # Desplazamiento en PIXELES entre cada ventana al renderizarse (para el efecto cascada)

# Obtiene las dimensiones (PIXELES) de la pantalla del usuario (VM) para que no queden desproporcionales las Xterm:
SCREEN_W=$(xdpyinfo | grep dimensions | awk '{print $2}' | cut -d'x' -f1)
SCREEN_H=$(xdpyinfo | grep dimensions | awk '{print $2}' | cut -d'x' -f2)

WIN_W=$(( WIN_COLS * 10 )) # Tamanio de las Xterm en PIXELES (considerando 1 caracter = 10 pixeles de ancho)
WIN_H=$(( WIN_ROWS * 20 )) # Tamanio de las Xterm en PIXELES (considerando 1 caracter = 20 pixeles de alto)

BASE_X=$(( (SCREEN_W - WIN_W) / 2 )) # Posicion de las Xterm (anchoPantalla - anchoVentana / 2) (divido en 2 para que quede al centro)
BASE_Y=$(( (SCREEN_H - WIN_H) / 2 )) # Posicion de las Xterm (altoPantalla - altoVentana / 2) (divido en 2 para que quede al centro)

CASCADE=0 # Contador de cascada (cada Xterm nueva se desplaza un porotito mas), empieza con desplazamiento 0.

# Espera (sleep) hasta que la ventana con el titulo introducido como parametro se haya renderizado y un poco mas
# (para evitar que una ventana anterior quede por encima de una posterior porque se redenrizo luego)
wait_for_window() {
    local title="$1" # Parametro de la funcion (titulo de la ventana)
    # Mientras no se encuentre la ventana buscada, Sleepea 0.03s
    while ! xwininfo -name "$title" &>/dev/null; do
        sleep 0.03
    done
    # Sleepea 0.1s adicionales para que termine de renderizar
    sleep 0.1
}

#Funcion para abrir Xterm estilizada B)
open_xterm() {
    local title="$1" # Parametro de la funcion (titulo de la ventana)
    local cmd="$2" # Parametro de la funcion (comando a ser ejecutado)
    local x=$(( BASE_X + CASCADE * CASCADE_OFFSET )) # Posicion de la ventana contando cascada
    local y=$(( BASE_Y + CASCADE * CASCADE_OFFSET )) # Posicion de la ventana contando cascada

    # EL CORAZON DE LA FUNCION (crea la ventana Xterm)
    xterm \
        -T "$title" \
        -hold \
        -geometry "${WIN_COLS}x${WIN_ROWS}+${x}+${y}" \
        -e "bash -c '$cmd'" \
        &
    # -T: es el titulo de la ventana (el nombre)
    # -hold: causa que la ventana no se cierre automaticamente al finalizar la tarea (o si da error)
    # -geometry: permite decidir dimensiones y ubicacion de la ventana
    # -e: es el comando a ser ejecutado: ejecutamos "bash" (el interprete de comandos) y le pasamos nuestro comando $cmd
    # El ampersand del final creo que es para lo mismo que el hold, solo que si lo saco no holdea nada
    # Los backslash (\) son para poder escribir el comando en varias lineas aqui en el script, no tiene funcionalidad
    # Escribir \ causa que el parser desestime el siguiente caracter y como luego de este viene un salto de linea (\n), lo ignora y sigue leyendo. 

    # EJECUTO la funcion wait_for_window con title de parametro para esperar que renderice la ventana que acabo de crear.
    wait_for_window "$title"

    #hago CASCADE++ para que la prox ventana se desplace diagonalmente un cachito mas que esta :)
    CASCADE=$(( CASCADE + 1 ))
}

#──── FUNCIÓN DE BUILD (Make con mensajes lindos) ───────────────────────────────────────────────

run_make() {
    local modulo="$1" # Nombre del modulo (solo para fines informativos)
    local dir="$2" # La direccion del archivo Make a correr

    # Me anoto los codigos de colores en variables para colorear los mensajes luego :)
    local DEFAULT='\033[0m' 
    local ROJO='\033[0;31m'
    local VERDE='\033[0;32m'
    local NARANJA='\033[0;33m'

    # Clean silencioso (ignoramos errores: si no hay nada que limpiar, no importa)
    # -C: es la direccion del archivo a limpiar 
    # " > /dev/null 2>&1 ": redirige el output del comando a null (pq no nos importa)
    make -C "$dir" clean > /dev/null 2>&1

    local output
    # Build capturando el output en la variable para informar al usuario cualquier cosa, pero no llenar la ventana de verborragia
    output=$(make -C "$dir" 2>&1)

    # Me anoto el exit code ($? es una variable global de linux que guarda el estado de retorno del ultimo comando o programa ejecutado)
    local exit_code=$?

    # Aclaracion para todos los "echo" de las siguientes lineas:
    # -e: causa que el echo no lea los \ como caracteres, sino como cosas locas:
    # sirve para que el \n salte de linea en lugar de imprimir "\n" (aunque tambien podria ponerse la flag -n para que salte)
    # y, mas importante: para los codigos de colores. (para que cambie de color la terminal en lugar de imprimir literal "\033[0;31m")

    if [ $exit_code -ne 0 ]; then # -ne es NOT EQUALS (es decir "Si exit_code no es igual a 0", que es Success)
        
        # imprimo mensajito de color rojo, al final vuelvo a setear el color de la terminal a DEFAULT, salto e imprimo el output que guardé
        echo -e "${ROJO}EL MAKE DE $modulo DIO ERRORES  :( ${DEFAULT} \n $output"

        # Aborto la ejecucion del script y salgo con exitCode 1 (error)
        exit 1
    fi # jajaja if fi 

    local warnings
    # Me guardo de output solo las lineas que dicen "warning" en la variable.
    warnings=$(echo "$output" | grep -i "warning")

    if [ -n "$warnings" ]; then # Al parecer -n seguido de un string quiere decir "la string no es vacia" (yo pense que era "not", pero no)

        # Imprimo mensajito naranja de que hay warnings, vuelvo a color DEFAULT e imprimo las warnings que discrimine del output
        echo -e "${NARANJA} EL MAKE DE $modulo SE COMPLETO CON WARNINGS  :/  ${DEFAULT}\n $warnings"
    else

        #Imprimo mensajto verde y al final vuelvo a DEFAULT porque sino la terminal queda hecha la pachamama por el resto de su vida
        echo -e "${VERDE} EL MAKE DE $modulo SE COMPLETO SIN ERRORES NI WARNINGS  :)  ${DEFAULT}"
    fi
}

#──── BUILDEAMOS CON MAKE──────────────────────────────────────────────────────────

# Make todo los programas (con la funcion especial que imprime output lindo que defini arriba)
run_make "kernel_memory"    "./kernel_memory"
run_make "kernel_scheduler" "./kernel_scheduler"
run_make "memory_stick"     "./memory_stick"
run_make "cpu"              "./cpu"
run_make "io"   "./io"
run_make "swap" "./swap"


#──── CORREMOS LAS TERMINALES──────────────────────────────────────────────────────────

# Corre cada programa en una terminal Xterm (a traves de la funcion creada mas arriba para mas placer)

open_xterm "KERNEL MEMORY" \
    "sleep 1; ./kernel_memory/bin/kernel_memory ./kernel_memory/src/kernelMemory-LOCAL.config"

open_xterm "KERNEL SCHEDULER" \
    "sleep 1; ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernelScheduler-LOCAL.config $path_proceso_inicial"

open_xterm "MEMORY STICK" \
    "sleep 1; ./memory_stick/bin/memory_stick ./memory_stick/src/memoryStick-LOCAL.config 10"

open_xterm "CPU 100" \
    "sleep 1; ./cpu/bin/cpu ./cpu/src/cpu-LOCAL.config 100"

open_xterm "IO" \
    "sleep 1; ./io/bin/io ./io/src/io-LOCAL.config SLEEP"

open_xterm "SWAP " \
    "sleep 1; ./swap/bin/swap ./swap/src/swap-LOCAL.config"