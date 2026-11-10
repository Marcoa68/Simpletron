#include <stdio.h>
#include <stdlib.h>

#define MEMORY_SIZE 100
#define SENTINEL 9999
#define MIN_VALUE -9999
#define MAX_VALUE 9999

/* Registros */
int memory[MEMORY_SIZE];
int accumulator = 0;
int instructionCounter = 0;
int instructionRegister = 0;
int operationCode = 0;
int operand = 0;

/* Prototipos */
void loadProgram();
void executeProgram();
void dumpMemory();
void fatalError(const char *message);
int checkOverflow(int value);

int main() {
    printf("*** Bienvenido al simulador Simpletron ***\n");
    loadProgram();
    executeProgram();
    return 0;
}

void loadProgram() {
    int instruction;
    int i = 0;

    printf("*** Cargue su programa SML (9999 para terminar) ***\n");
    while (i < MEMORY_SIZE) {
        printf("%02d ? ", i);
        scanf("%d", &instruction);

        if (instruction == SENTINEL) {
            break;
        }

        if (instruction < MIN_VALUE || instruction > MAX_VALUE) {
            printf("Error: instrucción fuera de rango (%d a %d).\n", MIN_VALUE, MAX_VALUE);
            continue;
        }

        memory[i++] = instruction;
    }
    printf("*** Programa cargado exitosamente ***\n");
}

void executeProgram() {
    printf("*** Comienza la ejecución del programa ***\n");

    while (instructionCounter < MEMORY_SIZE) {
        instructionRegister = memory[instructionCounter];
        operationCode = instructionRegister / 100;
        operand = instructionRegister % 100;

        switch (operationCode) {
            case 10: // READ
                printf("Ingrese un número: ");
                scanf("%d", &memory[operand]);
                break;
            case 11: // WRITE
                printf("Salida: %d\n", memory[operand]);
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
                if (memory[operand] == 0) {
                    fatalError("Error fatal: división entre cero.");
                    return;
                }
                accumulator = checkOverflow(accumulator / memory[operand]);
                break;
            case 33: // MULTIPLY
                accumulator = checkOverflow(accumulator * memory[operand]);
                break;
            case 40: // BRANCH
                instructionCounter = operand;
                continue;
            case 41: // BRANCHNEG
                if (accumulator < 0) {
                    instructionCounter = operand;
                    continue;
                }
                break;
            case 42: // BRANCHZERO
                if (accumulator == 0) {
                    instructionCounter = operand;
                    continue;
                }
                break;
            case 43: // HALT
                printf("*** Programa terminado correctamente ***\n");
                dumpMemory();
                return;
            default:
                fatalError("Error fatal: código de operación inválido.");
                return;
        }
        instructionCounter++;
    }
    fatalError("Error fatal: contador de instrucciones fuera de rango.");
}

int checkOverflow(int value) {
    if (value < MIN_VALUE || value > MAX_VALUE) {
        fatalError("Error fatal: desbordamiento del acumulador.");
        exit(EXIT_FAILURE);
    }
    return value;
}

void dumpMemory() {
    printf("\nREGISTROS:\n");
    printf("accumulator          %+05d\n", accumulator);
    printf("instructionCounter      %02d\n", instructionCounter);
    printf("instructionRegister %+05d\n", instructionRegister);
    printf("operationCode          %02d\n", operationCode);
    printf("operand                %02d\n", operand);

    printf("\nMEMORIA:\n");
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (i % 10 == 0) {
            printf("\n%02d ", i);
        }
        printf("%+05d ", memory[i]);
    }
    printf("\n");
}

void fatalError(const char *message) {
    printf("%s\n", message);
    dumpMemory();
    exit(EXIT_FAILURE);
}
