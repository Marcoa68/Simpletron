/*
 * compilador.c
 *
 * Compilador de Simple a SML (Simpletron Machine Language).
 *
 * Primera pasada: lee el programa Simple, arma la tabla de símbolos y genera el
 *                 código SML. Los saltos hacia líneas que todavía no se han
 *                 definido quedan como referencias pendientes (arreglo flags).
 * Segunda pasada: resuelve las referencias pendientes y guarda el programa SML
 *                 en un archivo que el simulador Simpletron puede cargar.
 *
 * Cada instrucción SML es un entero de cuatro dígitos XXYY (XX = código de
 * operación, YY = operando). La memoria tiene 100 palabras: las instrucciones se
 * guardan desde la posición 00 hacia arriba y los datos (variables, constantes y
 * resultados temporales) desde la posición 99 hacia abajo.
 *
 * Compilación: gcc -Wall -Wextra -o compilador compilador.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== Constantes ===================== */

#define MEMORY_SIZE   100      /* palabras de la memoria del Simpletron */
#define NO_PENDING    (-1)     /* valor de un elemento vacío del arreglo flags */
#define MAX_LINE      256      /* caracteres máximos por renglón del programa Simple */
#define MAX_TOKENS    64       /* elementos máximos por enunciado */
#define MIN_WORD      (-9999)  /* palabra mínima del Simpletron */
#define MAX_WORD      9999     /* palabra máxima del Simpletron */

/* Códigos de operación de SML */
#define SML_READ        10
#define SML_WRITE       11
#define SML_LOAD        20
#define SML_STORE       21
#define SML_ADD         30
#define SML_SUBTRACT    31
#define SML_DIVIDE      32
#define SML_MULTIPLY    33
#define SML_MODULO      34
#define SML_EXPONENT    35
#define SML_BRANCH      40
#define SML_BRANCHNEG   41
#define SML_BRANCHZERO  42
#define SML_HALT        43

/* ===================== Estructuras y variables globales ===================== */

/* Nodo de la tabla de símbolos (lista ligada). */
struct tableEntry {
    int symbol;               /* variable: código ASCII de la letra; constante: su valor;
                                 número de línea: el propio número */
    char type;                /* 'C' constante, 'L' número de línea, 'V' variable */
    int location;             /* posición de memoria asociada (00 a 99) */
    struct tableEntry *next;  /* siguiente nodo de la lista */
};
typedef struct tableEntry TableEntry;

TableEntry *symbolTable = NULL;   /* inicio de la lista ligada de símbolos */
int flags[MEMORY_SIZE];           /* referencias pendientes: flags[posición] = línea destino o -1 */
int memory[MEMORY_SIZE];          /* memoria del Simpletron */
int instructionCounter = 0;       /* próxima posición libre para instrucciones (crece hacia arriba) */
int dataCounter = MEMORY_SIZE;    /* posición de datos más baja ya reservada (crece hacia abajo desde 99) */

/* Deja la memoria, flags y contadores en su estado inicial. */
void initCompiler(void) {
    int i;

    for (i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = 0;
        flags[i] = NO_PENDING;
    }
    symbolTable = NULL;
    instructionCounter = 0;
    dataCounter = MEMORY_SIZE;
}

int main(void) {
    printf("*** Compilador de Simple a SML ***\n");
    initCompiler();
    return EXIT_SUCCESS;
}
