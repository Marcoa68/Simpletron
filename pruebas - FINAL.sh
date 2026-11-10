#!/bin/bash
# pruebas.sh - Compila el simulador y el compilador y prueba los programas de ejemplo.
#
# Uso:  bash pruebas.sh
#
# Cada programa en Simple se compila a SML, el resultado se ejecuta en el simulador
# Simpletron con unas entradas fijas y lo que imprime se compara con lo esperado.
# Los programas de ejemplos/errores deben ser rechazados por el compilador.

cd "$(dirname "$0")" || exit 1
RAIZ=$(pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

gcc -Wall -Wextra -o "$TMP/simpletron" simpletron.c -lm || exit 1
gcc -Wall -Wextra -o "$TMP/compilador" compilador.c || exit 1

total=0
fallos=0

# ruta <archivo>: ruta absoluta (si ya es absoluta se deja igual)
ruta() {
    case "$1" in
        /*) echo "$1" ;;
        *)  echo "$RAIZ/$1" ;;
    esac
}

# probar <programa.simple> "<entradas>" "<salida esperada>"
probar() {
    local fuente=$1 entradas=$2 esperado=$3 obtenido
    total=$((total + 1))
    rm -f "$TMP/programa.simp"
    if ! "$TMP/compilador" "$(ruta "$fuente")" "$TMP/programa.simp" > /dev/null 2> "$TMP/errores.txt"; then
        echo "FALLO  $fuente: no compiló"
        cat "$TMP/errores.txt"
        fallos=$((fallos + 1))
        return
    fi
    obtenido=$(cd "$TMP" && printf '%s\n' $entradas | ./simpletron 2>&1 \
        | grep -a -o 'Salida: [-0-9.]*' | sed 's/Salida: //; s/\.00$//' | tr '\n' ' ')
    obtenido=${obtenido% }
    if [ "$obtenido" = "$esperado" ]; then
        echo "ok     $fuente  entradas [$entradas]  ->  $obtenido"
    else
        echo "FALLO  $fuente  entradas [$entradas]: se esperaba '$esperado' y se obtuvo '$obtenido'"
        fallos=$((fallos + 1))
    fi
}

# probar_error <programa.simple>: el compilador debe rechazarlo y no dejar archivo de salida
probar_error() {
    local fuente=$1
    total=$((total + 1))
    rm -f "$TMP/programa.simp"
    if "$TMP/compilador" "$(ruta "$fuente")" "$TMP/programa.simp" > /dev/null 2>&1; then
        echo "FALLO  $fuente: debía rechazarse y se compiló"
        fallos=$((fallos + 1))
    elif [ -e "$TMP/programa.simp" ]; then
        echo "FALLO  $fuente: se rechazó pero dejó un archivo de salida"
        fallos=$((fallos + 1))
    else
        echo "ok     $fuente  (rechazado)"
    fi
}

echo "--- Programas correctos"
probar ejemplos/eco.simple "4 8" "4 8"
probar ejemplos/salto_adelante.simple "7" "7"
probar ejemplos/suma.simple "3 4" "7"
probar ejemplos/suma.simple "-5 12" "7"
probar ejemplos/mayor.simple "3 8" "8"
probar ejemplos/mayor.simple "9 2" "9"
probar ejemplos/suma_hasta_cero.simple "5 10 20 0" "35"
probar ejemplos/suma_hasta_cero.simple "0" "0"
probar ejemplos/factorial.simple "5" "120"
probar ejemplos/factorial.simple "7" "5040"
probar ejemplos/factorial.simple "0" "1"
probar ejemplos/expresiones.simple "7 3" "13 20 1 8 20 12 512"
probar ejemplos/condiciones.simple "3 5" "0 1 1 0 1 0"
probar ejemplos/condiciones.simple "5 5" "1 0 0 0 1 1"
probar ejemplos/condiciones.simple "7 2" "0 1 0 1 0 1"
probar ejemplos/cuenta_regresiva.simple "3" "3 2 1"

echo
echo "--- Programas con errores (el compilador debe rechazarlos)"
for f in ejemplos/errores/*.simple; do
    probar_error "$f"
done

# Más enunciados de los que caben en las 100 palabras de memoria
{
    echo "10 input a"
    for n in $(seq 20 10 1500); do echo "$n print a"; done
    echo "1510 end"
} > "$TMP/grande.simple"
probar_error "$TMP/grande.simple"

echo
echo "$((total - fallos)) de $total pruebas correctas"
[ "$fallos" -eq 0 ]
