//
// Created by Developer on 17/03/2026.
//

#include "mpc.h"
#include <editline/readline.h>
typedef struct LispValue LispValue;

typedef struct LispValue {
    int type;
    union {
        long num;
        // Error and Symbol types have some string data
        char* err;
        char* sym;
    };
    // Count and Pointer to a list of "lval*"
    int count;
    LispValue** cell;
} LispValue;

// Enum of lval types
enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR};
// Enum of lval error types
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

// type constructors
LispValue* Number(long x) {
    LispValue* v = malloc(sizeof(LispValue));
    *v = (LispValue){.type = LVAL_NUM, .num = x};
    return v;
}

LispValue* Error(char* m) {
    LispValue* v = malloc(sizeof(LispValue));
    *v = (LispValue){.type = LVAL_ERR, .err = strdup(m)};
    return v;
}

LispValue* Symbol(char* s) {
    LispValue* v = malloc(sizeof(LispValue));
    *v = (LispValue){.type = LVAL_SYM, .sym = strdup(s)};
    return v;
}

LispValue* Sexpr() {
    LispValue* v = malloc(sizeof(LispValue));
    *v = (LispValue){.type = LVAL_SEXPR};
    return v;
}
// LispValue manipulation ///////////////////////////////////////////////////////////////////////////////
void lval_del(LispValue* v) {
    switch (v->type) {
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }

    free(v);
}

LispValue* lval_append(LispValue* v, LispValue* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(LispValue*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

LispValue* lval_pop(LispValue* v, int i) {
    LispValue* x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i+1],
        sizeof(LispValue*)*(v->count-i-1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(LispValue*) * v->count);
    return x;
}

LispValue* lval_take(LispValue* v, int i) {
    LispValue* x = lval_pop(v, i);
    lval_del(v);
    return x;
}


void lval_expr_print(LispValue* v, char open, char close);
void lval_print(LispValue* v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;

        case LVAL_ERR: printf("Err: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

void lval_println(LispValue* v) {
    lval_print(v); putchar('\n');
}

void lval_expr_print(LispValue* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

// parsing /////////////////////////////////////////////////////////////////////////
LispValue* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
    ? Number(x)
    : Error("invalid number");
}

LispValue* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return Symbol(t->contents); }

    LispValue* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = Sexpr();}
    if (strstr(t->tag, "sexpr")) { x = Sexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_append(x, lval_read(t->children[i]));
    }
    return x;
}

// Builtin operations /////////////////////////////////////////////////////////////

LispValue* builtin_op(LispValue* args, char* op) {
    // typecheck all arguments
    for (int i = 0; i < args->count; i++) {
        if (args->cell[i]->type != LVAL_NUM) {
            lval_del(args);
            return Error("Cannot operate on non-number!");
        }
    }

    LispValue* x = lval_pop(args, 0);

    // if only one value and no more args with '-' then negate value
    if (strcmp(op, "-") == 0 && args->count == 0) {
        x->num = -x->num;
    }

    while (args->count > 0) {

        LispValue* y = lval_pop(args, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = Error("Division by zero!"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(args); return x;
}

// eval ///////////////////////////////////////////////////////////////////////////
LispValue* lval_eval(LispValue* v);

LispValue* lval_eval_sexpr(LispValue* v) {

    // Eval all children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return lval_take(v, 0); }

    LispValue* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return Error("S-expression does not start with symbol!");
    }
    LispValue* result = builtin_op(v, f->sym);
    return result;
}

LispValue* lval_eval(LispValue* v) {
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
}

int main(int argc, char *argv[]) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "number   : /-?[0-9]+/ ;    "
        "symbol   : '+' | '-' | '*' | '/' ;"
        "sexpr    : '(' <expr>* ')' ;"
        "expr     : <number> | <symbol> | <sexpr> ;"
        "lispy    : /^/ <expr>* /$/ ;",
        Number, Symbol, Sexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.2");
    puts("Press Ctrl+c to Exit\n");
    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            LispValue* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    mpc_cleanup(4, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}