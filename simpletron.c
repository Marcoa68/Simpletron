#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MEMORY_SIZE 1000
#define SENTINEL 9999
#define MIN_VALUE -9999
#define MAX_VALUE 9999

/* Registros */
double memory[MEMORY_SIZE];
double accumulator = 0;
int instructionCounter = 0;
int instructionRegister = 0;
int operationCode = 0;
int operand = 0;

/* Prototipos */
void loadProgram();
void executeProgram();
void dumpMemory();
void fatalError(const char *message);
double checkOverflow(double value);

int main() {
    printf("*** Bienvenido al simulador Simpletron Mejorado ***\n");
    loadProgram();
    executeProgram();
    return 0;
}

void loadProgram() {
    FILE *fp = fopen("programa.simp", "r");
    int instruction, i = 0;

    if (fp) {
        printf("*** Cargando programa desde archivo programa.simp ***\n");
        while (fscanf(fp, "%d", &instruction) == 1 && i < MEMORY_SIZE) {
            if (instruction == SENTINEL) break;
            if (instruction < MIN_VALUE || instruction > MAX_VALUE) {
                printf("Error: instrucción fuera de rango en archivo.\n");
                exit(EXIT_FAILURE);
            }
            memory[i++] = instruction;
        }
        fclose(fp);
    } else {
        printf("*** Archivo no encontrado, modo interactivo ***\n");
        while (i < MEMORY_SIZE) {
            printf("%03d ? ", i);
            scanf("%d", &instruction);
            if (instruction == SENTINEL) break;
            if (instruction < MIN_VALUE || instruction > MAX_VALUE) {
                printf("Error: instrucción fuera de rango.\n");
                continue;
            }
            memory[i++] = instruction;
        }
    }
    printf("*** Programa cargado exitosamente ***\n");
}

void executeProgram() {
    printf("*** Comienza la ejecucion del programa ***\n");

    while (instructionCounter < MEMORY_SIZE) {
        instructionRegister = (int)memory[instructionCounter];
        operationCode = instructionRegister / 100;
        operand = instructionRegister % 1000;

        switch (operationCode) {
            case 10: // READ (entero)
                printf("Ingrese un número: ");
                scanf("%lf", &memory[operand]);
                break;
            case 11: // WRITE (entero)
                printf("Salida: %.2lf\n", memory[operand]);
                break;
            case 12: // NEWLINE
                printf("\n");
                break;
            case 20: // LOAD
                accumulator = memory[operand];
                break;
            case 21: // STORE
                memory[operand] = accumulator;
                break;
            case 30: // ADD
                accumulator = checkOverflow(accumulator + memory[operand]);
                break;
            case 31: // SUBTRACT
                accumulator = checkOverflow(accumulator - memory[operand]);
                break;
            case 32: // DIVIDE
                if (memory[operand] == 0) fatalError("Error fatal: división entre cero.");
                accumulator = checkOverflow(accumulator / memory[operand]);
                break;
            case 33: // MULTIPLY
                accumulator = checkOverflow(accumulator * memory[operand]);
                break;
            case 34: // MODULO
                if ((int)memory[operand] == 0) fatalError("Error fatal: módulo por cero.");
                accumulator = fmod(accumulator, memory[operand]);
                break;
            case 35: // EXPONENTIATION
                accumulator = pow(accumulator, memory[operand]);
                accumulator = checkOverflow(accumulator);
                break;
            case 40: // BRANCH
                instructionCounter = operand;
                continue;
            case 41: // BRANCHNEG
                if (accumulator < 0) { instructionCounter = operand; continue; }
                break;
            case 42: // BRANCHZERO
                if (accumulator == 0) { instructionCounter = operand; continue; }
                break;
            case 43: // HALT
                printf("*** Programa terminado correctamente ***\n");
                dumpMemory();
                return;
            case 50: { // READ STRING
                char buffer[100];
                printf("Ingrese una cadena: ");
                scanf("%s", buffer);
                int len = strlen(buffer);
                memory[operand] = len; // longitud
                for (int i = 0; i < len; i++) {
                    memory[operand + i + 1] = (int)buffer[i];
                }
                break;
            }
            case 51: { // WRITE STRING
                int len = (int)memory[operand];
                for (int i = 0; i < len; i++) {
                    printf("%c", (char)((int)memory[operand + i + 1]));
                }
                printf("\n");
                break;
            }
            default:
                fatalError("Error fatal: codigo de operación invalido.");
                return;
        }
        instructionCounter++;
    }
    fatalError("Error fatal: contador de instrucciones fuera de rango.");
}

double checkOverflow(double value) {
    if (value < MIN_VALUE || value > MAX_VALUE) {
        fatalError("Error fatal: desbordamiento del acumulador.");
        exit(EXIT_FAILURE);
    }
    return value;
}

void dumpMemory() {
    printf("\nREGISTROS:\n");
    printf("accumulator          %+08.2lf\n", accumulator);
    printf("instructionCounter      %03d\n", instructionCounter);
    printf("instructionRegister %+05d\n", instructionRegister);
    printf("operationCode          %02d\n", operationCode);
    printf("operand                %03d\n", operand);

    printf("\nMEMORIA:\n");
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (i % 20 == 0) printf("\n%03d ", i);
        printf("%+08.2lf ", memory[i]);
    }
    printf("\n");
}

void fatalError(const char *message) {
    printf("%s\n", message);
    dumpMemory();
    exit(EXIT_FAILURE);
}

