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
#include <ctype.h>
#include <stdarg.h>

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
int verbose = 0;                  /* -v: muestra enunciados, tabla de símbolos y memoria */
int errorCount = 0;               /* errores detectados durante la compilación */
int statementCount = 0;           /* enunciados leídos del programa Simple */
int currentFileLine = 0;          /* renglón del archivo que se está procesando */
int currentLineNumber = -1;       /* línea de Simple del enunciado actual (-1 si aún no se conoce) */

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

/* ===================== Tabla de símbolos (lista ligada) ===================== */

/* Busca un símbolo por su valor y su tipo; devuelve NULL si no está registrado. */
TableEntry *findSymbol(int symbol, char type) {
    TableEntry *current;

    for (current = symbolTable; current != NULL; current = current->next) {
        if (current->symbol == symbol && current->type == type) return current;
    }
    return NULL;
}

/* Agrega un símbolo al final de la lista y devuelve su nodo. */
TableEntry *insertSymbol(int symbol, char type, int location) {
    TableEntry *entry = malloc(sizeof(TableEntry));
    TableEntry *last;

    if (entry == NULL) {
        fprintf(stderr, "Error: no hay memoria suficiente para la tabla de símbolos.\n");
        exit(EXIT_FAILURE);
    }
    entry->symbol = symbol;
    entry->type = type;
    entry->location = location;
    entry->next = NULL;

    if (symbolTable == NULL) {
        symbolTable = entry;
    } else {
        for (last = symbolTable; last->next != NULL; last = last->next) {
            /* avanza hasta el último nodo */
        }
        last->next = entry;
    }
    return entry;
}

/* Libera todos los nodos de la lista. */
void freeSymbolTable(void) {
    TableEntry *current = symbolTable;
    TableEntry *next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    symbolTable = NULL;
}

/* Muestra la tabla de símbolos (modo -v). */
void printSymbolTable(void) {
    TableEntry *current;

    printf("\nTABLA DE SÍMBOLOS:\n");
    printf(" símbolo  tipo  ubicación\n");
    for (current = symbolTable; current != NULL; current = current->next) {
        printf(" %7d  %4c  %9d", current->symbol, current->type, current->location);
        if (current->type == 'V') printf("   variable '%c'", current->symbol);
        printf("\n");
    }
}

/* ===================== Lectura y análisis léxico ===================== */

/* Convierte un token en entero (con signo opcional). Devuelve 0 si no es un entero válido. */
int parseInteger(const char *token, int *value) {
    const char *p = token;
    int sign = 1;
    long number = 0;

    if (*p == '+' || *p == '-') {
        if (*p == '-') sign = -1;
        p++;
    }
    if (*p == '\0') return 0;
    for (; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
        number = number * 10 + (*p - '0');
        if (number > 999999999L) return 0;
    }
    *value = (int)(sign * number);
    return 1;
}

/* Un número de línea es un entero sin signo. */
int parseLineNumber(const char *token, int *lineNumber) {
    return isdigit((unsigned char)token[0]) && parseInteger(token, lineNumber);
}

/* Una variable es una sola letra. */
int isVariable(const char *token) {
    return isalpha((unsigned char)token[0]) && token[1] == '\0';
}

/* Operadores aritméticos: + - * / % ^ */
int isArithmeticOperator(const char *token) {
    return token[0] != '\0' && token[1] == '\0' && strchr("+-*/%^", token[0]) != NULL;
}

int isParenthesis(const char *token) {
    return (token[0] == '(' || token[0] == ')') && token[1] == '\0';
}

/* Operadores relacionales y de igualdad de las condiciones if. */
int isRelational(const char *token) {
    return strcmp(token, "==") == 0 || strcmp(token, "!=") == 0 ||
           strcmp(token, "<") == 0  || strcmp(token, ">") == 0  ||
           strcmp(token, "<=") == 0 || strcmp(token, ">=") == 0;
}

/* Informa un error de compilación indicando el renglón y la línea de Simple. */
void compileError(const char *format, ...) {
    va_list args;

    if (currentLineNumber >= 0) {
        fprintf(stderr, "Error (renglón %d, línea %d): ", currentFileLine, currentLineNumber);
    } else {
        fprintf(stderr, "Error (renglón %d): ", currentFileLine);
    }
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    errorCount++;
}

typedef enum { CMD_REM, CMD_INPUT, CMD_LET, CMD_PRINT, CMD_GOTO, CMD_IF, CMD_END } Command;

#define COMMAND_COUNT 7
const char *commandNames[COMMAND_COUNT] = { "rem", "input", "let", "print", "goto", "if", "end" };

/* Un enunciado de Simple separado en tokens. */
typedef struct {
    int lineNumber;           /* número de línea de Simple */
    Command command;          /* comando del enunciado */
    char *args[MAX_TOKENS];   /* variables, constantes y operadores que siguen al comando */
    int argc;                 /* cantidad de elementos en args */
} Statement;

typedef enum { PARSE_BLANK, PARSE_OK, PARSE_ERROR } ParseResult;

/* Separa un renglón en tokens con strtok(): número de línea, comando y argumentos.
 * Modifica el texto de line (strtok escribe '\0' entre los tokens). */
ParseResult parseStatement(char *line, Statement *st) {
    const char *delimiters = " \t\r\n";
    char *token;
    int i, found = 0;

    st->argc = 0;
    token = strtok(line, delimiters);
    if (token == NULL) return PARSE_BLANK;                 /* renglón vacío */

    /* 1. Número de línea (primer token del enunciado) */
    if (!parseLineNumber(token, &st->lineNumber)) {
        compileError("el enunciado debe comenzar con un número de línea (se encontró '%s')", token);
        return PARSE_ERROR;
    }
    currentLineNumber = st->lineNumber;

    /* 2. Comando */
    token = strtok(NULL, delimiters);
    if (token == NULL) {
        compileError("falta el comando después del número de línea");
        return PARSE_ERROR;
    }
    for (i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(token, commandNames[i]) == 0) {
            st->command = (Command)i;
            found = 1;
            break;
        }
    }
    if (!found) {
        compileError("comando desconocido '%s' (use rem, input, let, print, goto, if o end)", token);
        return PARSE_ERROR;
    }
    if (st->command == CMD_REM) return PARSE_OK;           /* el comentario se ignora */

    /* 3. Variables, constantes y operadores */
    while ((token = strtok(NULL, delimiters)) != NULL) {
        if (st->argc == MAX_TOKENS) {
            compileError("el enunciado tiene demasiados elementos (máximo %d)", MAX_TOKENS);
            return PARSE_ERROR;
        }
        st->args[st->argc++] = token;
    }
    return PARSE_OK;
}

/* Muestra un enunciado ya separado en tokens (modo -v). */
void printStatement(const Statement *st) {
    int i;

    printf("renglón %2d | línea %3d | %-5s |", currentFileLine, st->lineNumber, commandNames[st->command]);
    for (i = 0; i < st->argc; i++) printf(" %s", st->args[i]);
    printf("\n");
}

/* ===================== Registro de símbolos ===================== */

int memoryErrorReported = 0;    /* para informar la falta de memoria una sola vez */

/* Informa que el programa no cabe en la memoria (una vez, no en cada enunciado). */
void memoryError(void) {
    if (!memoryErrorReported) {
        compileError("memoria insuficiente: el programa necesita más de %d palabras entre "
                     "instrucciones y datos", MEMORY_SIZE);
        memoryErrorReported = 1;
    }
}

/* Reserva una posición de datos (desde la 99 hacia atrás); devuelve -1 si no hay espacio. */
int allocateData(void) {
    if (dataCounter - 1 < instructionCounter) {
        memoryError();
        return -1;
    }
    return --dataCounter;
}

/* Registra un número de línea con la posición donde comenzará su código SML. */
int registerLine(int lineNumber) {
    if (findSymbol(lineNumber, 'L') != NULL) {
        compileError("el número de línea %d está repetido", lineNumber);
        return 0;
    }
    insertSymbol(lineNumber, 'L', instructionCounter);
    return 1;
}

/* Registra una variable (si es nueva) y devuelve su posición; -1 si hay error. */
int registerVariable(int letter) {
    TableEntry *entry = findSymbol(letter, 'V');
    int location;

    if (entry != NULL) return entry->location;
    location = allocateData();
    if (location < 0) return -1;
    insertSymbol(letter, 'V', location);
    return location;
}

/* Registra una constante (si es nueva), guarda su valor en memoria y devuelve su posición. */
int registerConstant(int value) {
    TableEntry *entry = findSymbol(value, 'C');
    int location;

    if (entry != NULL) return entry->location;
    if (value < MIN_WORD || value > MAX_WORD) {
        compileError("la constante %d está fuera del rango permitido (%d a %d)", value, MIN_WORD, MAX_WORD);
        return -1;
    }
    location = allocateData();
    if (location < 0) return -1;
    memory[location] = value;
    insertSymbol(value, 'C', location);
    return location;
}

/* Identifica si un token es variable o constante y lo registra. Devuelve su posición o -1. */
int registerOperand(const char *token) {
    int value;

    if (isVariable(token)) return registerVariable((unsigned char)token[0]);
    if (parseInteger(token, &value)) return registerConstant(value);
    compileError("'%s' no es una variable (una letra) ni una constante entera; "
                 "separe los operadores y operandos con espacios", token);
    return -1;
}

/* Revisa la sintaxis del enunciado y registra sus variables y constantes. */
int identifySymbols(const Statement *st) {
    int target, i;

    switch (st->command) {
        case CMD_REM:
            return 1;

        case CMD_END:
            if (st->argc != 0) {
                compileError("sintaxis: end");
                return 0;
            }
            return 1;

        case CMD_INPUT:
            if (st->argc != 1 || !isVariable(st->args[0])) {
                compileError("sintaxis: input variable");
                return 0;
            }
            return registerOperand(st->args[0]) >= 0;

        case CMD_PRINT:
            if (st->argc != 1) {
                compileError("sintaxis: print variable");
                return 0;
            }
            return registerOperand(st->args[0]) >= 0;

        case CMD_GOTO:
            if (st->argc != 1 || !parseLineNumber(st->args[0], &target)) {
                compileError("sintaxis: goto número_de_línea");
                return 0;
            }
            return 1;

        case CMD_IF:
            if (st->argc != 5 || !isRelational(st->args[1]) ||
                strcmp(st->args[3], "goto") != 0 || !parseLineNumber(st->args[4], &target)) {
                compileError("sintaxis: if operando relación operando goto número_de_línea "
                             "(relación: == != < > <= >=)");
                return 0;
            }
            return registerOperand(st->args[0]) >= 0 && registerOperand(st->args[2]) >= 0;

        case CMD_LET:
            if (st->argc < 3 || !isVariable(st->args[0]) || strcmp(st->args[1], "=") != 0) {
                compileError("sintaxis: let variable = expresión");
                return 0;
            }
            if (registerOperand(st->args[0]) < 0) return 0;
            for (i = 2; i < st->argc; i++) {
                if (isArithmeticOperator(st->args[i]) || isParenthesis(st->args[i])) continue;
                if (registerOperand(st->args[i]) < 0) return 0;
            }
            return 1;
    }
    return 0;
}

/* ===================== Generación de instrucciones SML ===================== */

/* Agrega una instrucción en la siguiente posición libre: opcode * 100 + operando. */
int emit(int opcode, int operand) {
    if (instructionCounter >= dataCounter) {
        memoryError();
        return 0;
    }
    if (operand < 0 || operand >= MEMORY_SIZE) {
        compileError("el operando %d está fuera de la memoria (00 a 99)", operand);
        return 0;
    }
    memory[instructionCounter++] = opcode * 100 + operand;
    return 1;
}

/* Consulta la tabla de símbolos y devuelve la posición de una variable o constante. */
int lookupOperand(const char *token) {
    TableEntry *entry = NULL;
    int value;

    if (isVariable(token)) {
        entry = findSymbol((unsigned char)token[0], 'V');
    } else if (parseInteger(token, &value)) {
        entry = findSymbol(value, 'C');
    }
    if (entry == NULL) {
        compileError("el símbolo '%s' no está en la tabla de símbolos", token);
        return -1;
    }
    return entry->location;
}

/* Genera un salto hacia un número de línea. Si la línea todavía no está en la tabla de
 * símbolos, deja 00 como operando y anota la línea pendiente en flags[], usando como
 * índice la posición de la instrucción. La segunda pasada completa el operando. */
int emitBranch(int opcode, int lineNumber) {
    TableEntry *entry = findSymbol(lineNumber, 'L');
    int address = instructionCounter;

    if (entry != NULL) return emit(opcode, entry->location);
    if (!emit(opcode, 0)) return 0;
    flags[address] = lineNumber;
    return 1;
}

/* Cuenta las referencias que siguen pendientes. */
int countPending(void) {
    int i, count = 0;

    for (i = 0; i < MEMORY_SIZE; i++) {
        if (flags[i] != NO_PENDING) count++;
    }
    return count;
}

/* Muestra las referencias pendientes (modo -v). */
void printPending(void) {
    int i;

    printf("\nREFERENCIAS PENDIENTES (arreglo flags):\n");
    if (countPending() == 0) printf(" ninguna\n");
    for (i = 0; i < MEMORY_SIZE; i++) {
        if (flags[i] != NO_PENDING) {
            printf(" posición %02d: %+05d -> falta la dirección de la línea %d\n", i, memory[i], flags[i]);
        }
    }
}

/* input variable */
int compileInput(const Statement *st) {
    int location = lookupOperand(st->args[0]);

    return location >= 0 && emit(SML_READ, location);
}

/* print variable */
int compilePrint(const Statement *st) {
    int location = lookupOperand(st->args[0]);

    return location >= 0 && emit(SML_WRITE, location);
}

/* end */
int compileEnd(void) {
    return emit(SML_HALT, 0);
}

/* goto línea */
int compileGoto(const Statement *st) {
    int target;

    parseLineNumber(st->args[0], &target);
    return emitBranch(SML_BRANCH, target);
}

/* ===================== Expresiones aritméticas (let) ===================== */

#define STACK_SIZE 64

/* Pila de enteros: guarda operadores al convertir y direcciones al evaluar. */
typedef struct {
    int items[STACK_SIZE];
    int top;                  /* índice del elemento superior; -1 si la pila está vacía */
} Stack;

void stackInit(Stack *stack) {
    stack->top = -1;
}

int stackIsEmpty(const Stack *stack) {
    return stack->top < 0;
}

int stackSize(const Stack *stack) {
    return stack->top + 1;
}

int stackPush(Stack *stack, int value) {
    if (stack->top >= STACK_SIZE - 1) {
        compileError("la expresión es demasiado larga");
        return 0;
    }
    stack->items[++stack->top] = value;
    return 1;
}

/* El llamador verifica que la pila no esté vacía. */
int stackPop(Stack *stack) {
    return stack->items[stack->top--];
}

int stackPeek(const Stack *stack) {
    return stack->items[stack->top];
}

/* Elemento de una expresión posfija. */
typedef struct {
    char kind;     /* 'V' variable, 'C' constante, 'O' operador */
    int value;     /* letra de la variable, valor de la constante o carácter del operador */
} ExprToken;

/* Prioridad de un operador: ^ > * / % > + - */
int precedence(int op) {
    switch (op) {
        case '^': return 3;
        case '*': case '/': case '%': return 2;
        case '+': case '-': return 1;
    }
    return 0;
}

/* Convierte una expresión infija (tokens separados por espacios) a notación posfija.
 * Los operadores se apilan según su prioridad; ^ se asocia de derecha a izquierda. */
int infixToPostfix(char *const tokens[], int count, ExprToken postfix[], int *postfixCount) {
    Stack operators;
    int expectOperand = 1;    /* 1: toca un operando o '('; 0: toca un operador o ')' */
    int n = 0, value, op, i;

    stackInit(&operators);
    for (i = 0; i < count; i++) {
        const char *token = tokens[i];

        if (strcmp(token, "(") == 0) {
            if (!expectOperand) {
                compileError("falta un operador antes de '('");
                return 0;
            }
            if (!stackPush(&operators, '(')) return 0;

        } else if (strcmp(token, ")") == 0) {
            if (expectOperand) {
                compileError("falta un operando antes de ')'");
                return 0;
            }
            while (!stackIsEmpty(&operators) && stackPeek(&operators) != '(') {
                postfix[n].kind = 'O';
                postfix[n++].value = stackPop(&operators);
            }
            if (stackIsEmpty(&operators)) {
                compileError("hay un ')' sin su '(' correspondiente");
                return 0;
            }
            stackPop(&operators);                           /* descarta el '(' */

        } else if (isArithmeticOperator(token)) {
            op = token[0];
            if (expectOperand) {
                compileError("falta un operando antes del operador '%c'", op);
                return 0;
            }
            while (!stackIsEmpty(&operators) && stackPeek(&operators) != '(' &&
                   (precedence(stackPeek(&operators)) > precedence(op) ||
                    (precedence(stackPeek(&operators)) == precedence(op) && op != '^'))) {
                postfix[n].kind = 'O';
                postfix[n++].value = stackPop(&operators);
            }
            if (!stackPush(&operators, op)) return 0;
            expectOperand = 1;

        } else {                                            /* variable o constante */
            if (!expectOperand) {
                compileError("falta un operador antes de '%s'", token);
                return 0;
            }
            if (isVariable(token)) {
                postfix[n].kind = 'V';
                postfix[n++].value = (unsigned char)token[0];
            } else if (parseInteger(token, &value)) {
                postfix[n].kind = 'C';
                postfix[n++].value = value;
            } else {
                compileError("'%s' no es una variable ni una constante entera", token);
                return 0;
            }
            expectOperand = 0;
        }
    }

    if (expectOperand) {
        compileError("la expresión está incompleta");
        return 0;
    }
    while (!stackIsEmpty(&operators)) {
        op = stackPop(&operators);
        if (op == '(') {
            compileError("hay un '(' sin cerrar");
            return 0;
        }
        postfix[n].kind = 'O';
        postfix[n++].value = op;
    }
    *postfixCount = n;
    return 1;
}

/* Muestra la expresión posfija (modo -v). */
void printPostfix(const ExprToken postfix[], int count) {
    int i;

    printf("    posfija:");
    for (i = 0; i < count; i++) {
        if (postfix[i].kind == 'C') {
            printf(" %d", postfix[i].value);
        } else {
            printf(" %c", postfix[i].value);
        }
    }
    printf("\n");
}

/* Resultados intermedios: posiciones de datos que se reservan al hacer falta y se reutilizan. */
#define MAX_TEMPS 16

int tempCells[MAX_TEMPS];    /* posiciones de memoria usadas como temporales */
int tempInUse[MAX_TEMPS];    /* 1 si la temporal guarda un resultado que todavía se necesita */
int tempCount = 0;           /* temporales reservadas hasta ahora */

/* Libera todas las temporales antes de compilar una expresión nueva. */
void releaseAllTemps(void) {
    int i;

    for (i = 0; i < tempCount; i++) tempInUse[i] = 0;
}

/* Devuelve una temporal libre (reservando una nueva si hace falta); -1 si hay error. */
int acquireTemp(void) {
    int i, location;

    for (i = 0; i < tempCount; i++) {
        if (!tempInUse[i]) {
            tempInUse[i] = 1;
            return tempCells[i];
        }
    }
    if (tempCount == MAX_TEMPS) {
        compileError("la expresión necesita demasiados resultados intermedios");
        return -1;
    }
    location = allocateData();
    if (location < 0) return -1;
    tempCells[tempCount] = location;
    tempInUse[tempCount++] = 1;
    return location;
}

/* Si la posición es una temporal, la marca como libre. */
void releaseTemp(int location) {
    int i;

    for (i = 0; i < tempCount; i++) {
        if (tempCells[i] == location) tempInUse[i] = 0;
    }
}

/* Instrucción SML que corresponde a un operador aritmético. */
int operatorOpcode(int op) {
    switch (op) {
        case '+': return SML_ADD;
        case '-': return SML_SUBTRACT;
        case '*': return SML_MULTIPLY;
        case '/': return SML_DIVIDE;
        case '%': return SML_MODULO;
        case '^': return SML_EXPONENT;
    }
    return -1;
}

/* Evalúa la expresión posfija con una pila de direcciones de memoria y genera el código:
 * por cada operador, LOAD izquierdo / operación con el derecho / STORE del resultado.
 * La última operación guarda directamente en la variable destino. */
int generateExpression(const ExprToken postfix[], int count, int destination) {
    Stack operands;
    TableEntry *entry;
    int i, left, right, result;

    stackInit(&operands);
    for (i = 0; i < count; i++) {
        if (postfix[i].kind != 'O') {                       /* operando: se apila su dirección */
            entry = findSymbol(postfix[i].value, postfix[i].kind);
            if (entry == NULL) {
                compileError("un símbolo de la expresión no está en la tabla de símbolos");
                return 0;
            }
            if (!stackPush(&operands, entry->location)) return 0;
            continue;
        }

        if (stackSize(&operands) < 2) {
            compileError("la expresión está incompleta");
            return 0;
        }
        right = stackPop(&operands);
        left = stackPop(&operands);
        releaseTemp(left);
        releaseTemp(right);

        if (i == count - 1) {
            result = destination;
        } else {
            result = acquireTemp();
            if (result < 0) return 0;
        }
        if (!emit(SML_LOAD, left) || !emit(operatorOpcode(postfix[i].value), right) ||
            !emit(SML_STORE, result)) return 0;
        if (i != count - 1 && !stackPush(&operands, result)) return 0;
    }

    if (count == 1) {                                       /* let x = operando */
        return emit(SML_LOAD, stackPop(&operands)) && emit(SML_STORE, destination);
    }
    return 1;
}

/* let variable = expresión */
int compileLet(const Statement *st) {
    ExprToken postfix[MAX_TOKENS];
    int count;
    int destination = lookupOperand(st->args[0]);

    if (destination < 0) return 0;
    if (!infixToPostfix(&st->args[2], st->argc - 2, postfix, &count)) return 0;
    if (verbose) printPostfix(postfix, count);
    releaseAllTemps();
    return generateExpression(postfix, count, destination);
}

/* if operando relación operando goto línea
 * Se resta un operando del otro y se usan BRANCHNEG / BRANCHZERO sobre el acumulador. */
int compileIf(const Statement *st) {
    const char *relation = st->args[1];
    int left = lookupOperand(st->args[0]);
    int right = lookupOperand(st->args[2]);
    int target;

    if (left < 0 || right < 0) return 0;
    parseLineNumber(st->args[4], &target);

    if (strcmp(relation, "==") == 0) {                      /* x - y == 0 */
        return emit(SML_LOAD, left) && emit(SML_SUBTRACT, right) &&
               emitBranch(SML_BRANCHZERO, target);
    }
    if (strcmp(relation, "!=") == 0) {                      /* si x - y == 0 se salta el BRANCH */
        return emit(SML_LOAD, left) && emit(SML_SUBTRACT, right) &&
               emit(SML_BRANCHZERO, instructionCounter + 2) &&
               emitBranch(SML_BRANCH, target);
    }
    if (strcmp(relation, "<") == 0) {                       /* x - y < 0 */
        return emit(SML_LOAD, left) && emit(SML_SUBTRACT, right) &&
               emitBranch(SML_BRANCHNEG, target);
    }
    if (strcmp(relation, ">") == 0) {                       /* y - x < 0 */
        return emit(SML_LOAD, right) && emit(SML_SUBTRACT, left) &&
               emitBranch(SML_BRANCHNEG, target);
    }
    if (strcmp(relation, "<=") == 0) {                      /* x - y < 0  o  x - y == 0 */
        return emit(SML_LOAD, left) && emit(SML_SUBTRACT, right) &&
               emitBranch(SML_BRANCHNEG, target) && emitBranch(SML_BRANCHZERO, target);
    }
    if (strcmp(relation, ">=") == 0) {                      /* y - x < 0  o  y - x == 0 */
        return emit(SML_LOAD, right) && emit(SML_SUBTRACT, left) &&
               emitBranch(SML_BRANCHNEG, target) && emitBranch(SML_BRANCHZERO, target);
    }
    compileError("relación desconocida '%s'", relation);
    return 0;
}

/* ===================== Primera pasada ===================== */

/* Traduce un enunciado: registra su número de línea y sus símbolos y genera su código SML. */
void compileStatement(const Statement *st) {
    if (!registerLine(st->lineNumber)) return;
    if (!identifySymbols(st)) return;

    switch (st->command) {
        case CMD_REM:   break;                              /* los comentarios no generan código */
        case CMD_INPUT: compileInput(st); break;
        case CMD_PRINT: compilePrint(st); break;
        case CMD_GOTO:  compileGoto(st);  break;
        case CMD_END:   compileEnd();     break;
        case CMD_LET:   compileLet(st);   break;
        case CMD_IF:    compileIf(st);    break;
    }
}

/* Primera pasada: lee el programa Simple enunciado por enunciado. */
void firstPass(FILE *fp) {
    char line[MAX_LINE];
    Statement st;
    ParseResult result;
    int c;

    currentFileLine = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        currentFileLine++;
        currentLineNumber = -1;

        if (strchr(line, '\n') == NULL && !feof(fp)) {      /* el renglón no cabe en el buffer */
            compileError("el renglón excede %d caracteres", MAX_LINE - 1);
            while ((c = fgetc(fp)) != '\n' && c != EOF) {
                /* descarta el resto del renglón */
            }
            continue;
        }

        result = parseStatement(line, &st);
        if (result == PARSE_BLANK) continue;
        statementCount++;
        if (result == PARSE_ERROR) continue;

        if (verbose) printStatement(&st);
        compileStatement(&st);
    }
    currentLineNumber = -1;
}

/* Muestra la memoria del Simpletron en una tabla de 10 x 10 (modo -v). */
void printMemory(const char *title) {
    int i;

    printf("\n%s\n    ", title);
    for (i = 0; i < 10; i++) printf("%6d", i);
    for (i = 0; i < MEMORY_SIZE; i++) {
        if (i % 10 == 0) printf("\n%3d ", i);
        printf(" %+05d", memory[i]);
    }
    printf("\n");
}

void usage(const char *program) {
    fprintf(stderr, "Uso: %s [-v] programa.simple\n", program);
    fprintf(stderr, "  -v  muestra los enunciados, la tabla de símbolos y la memoria\n");
}

int main(int argc, char *argv[]) {
    const char *inputName = NULL;
    FILE *fp;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (inputName == NULL) {
            inputName = argv[i];
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (inputName == NULL) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    initCompiler();
    fp = fopen(inputName, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: no se pudo abrir '%s'.\n", inputName);
        return EXIT_FAILURE;
    }

    printf("*** Compilador de Simple a SML ***\n");

    /* Primera pasada: tabla de símbolos y código SML */
    firstPass(fp);
    fclose(fp);
    if (statementCount == 0 && errorCount == 0) {
        fprintf(stderr, "Error: el programa no contiene enunciados.\n");
        errorCount++;
    }
    if (verbose) {
        printSymbolTable();
        printMemory("MEMORIA DESPUÉS DE LA PRIMERA PASADA:");
        printPending();
    }

    if (errorCount > 0) {
        fprintf(stderr, "\nCompilación fallida: %d error(es).\n", errorCount);
        freeSymbolTable();
        return EXIT_FAILURE;
    }
    printf("\nPrimera pasada terminada sin errores.\n");

    freeSymbolTable();
    return EXIT_SUCCESS;
}
