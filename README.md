# Simulador Simpletron en C

Este proyecto implementa un simulador de la computadora **Simpletron** en C.  
Permite cargar programas escritos en SML, ejecutarlos, mostrar registros y vaciar la memoria completa.  
El simulador detecta errores fatales como división entre cero, acumulador fuera de rango y códigos de operación inválidos.

---

## Compilación

Asegúrese de tener instalado `gcc`.  
En la carpeta del proyecto ejecute:

```bash
gcc simpletron.c -o simpletron

# Para corre el simulador
./simpletron