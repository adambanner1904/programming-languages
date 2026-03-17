//
// Created by Developer on 17/03/2026.
//

#include "mpc.h"
#include <editline/readline.h>

typedef struct {
    int type;
    long num;
    int err;
} LispValue;

// Enum of lval types
enum {LVAL_NUM, LVAL_ERR};
// Enum of lval error types
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

// type constructors
LispValue Number(long x) {
    LispValue v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

LispValue Error(int x) {
    LispValue v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(LispValue v) {
    switch (v.type) {
        case LVAL_NUM: printf("%li", v.num); break;

        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: Division by zero");
            }
            if (v.err == LERR_BAD_OP) {
                printf("Error: Invalid operation");
            }
            if (v.err == LERR_BAD_NUM) {
                printf("Error: Invalid number");
            }
            break;
    }
}

void lval_println(LispValue v) {
    lval_print(v); putchar('\n');
}

LispValue eval_op(LispValue x, char* op, LispValue y) {
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0) { return Number(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return Number(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return Number(x.num * y.num); }
    if (strcmp(op, "/") == 0) {

        return y.num == 0
            ? Error(LERR_DIV_ZERO)
            : Number(x.num / y.num);
    }
    return Error(LERR_BAD_OP);
}

LispValue eval(mpc_ast_t* t) {
    // if number in tag return number: base case
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? Number(x) : Error(LERR_BAD_NUM);
    }

    // operator is always second child
    char* op = t->children[1]->contents;
    // store the third child in x: could be a tree so eval it
    LispValue x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

int main(int argc, char *argv[]) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "number   : /-?[0-9]+/ ;    "
        "operator : '+' | '-' | '*' | '/' ;"
        "expr     : <number> | '(' <operator> <expr>+ ')' ;"
        "lispy     : /^/ <operator> <expr>+ /$/ ;",
        Number, Operator, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.2");
    puts("Press Ctrl+c to Exit\n");
    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            LispValue result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}