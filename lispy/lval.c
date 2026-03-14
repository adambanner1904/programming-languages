//
// Created by Developer on 07/03/2026.
//

#include "lval.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* ltype_name(int t) {
    switch (t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_SYM: return "Symbol";
        case LVAL_ERR: return "Error";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

// constructors ///////////////////////////////////////////////////////////////////////////////////////
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){ .type = LVAL_NUM, .num = x };
    return v;
}
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){ .type = LVAL_ERR};

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);
    vsnprintf(v->err, 511, fmt, va);
    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(va);

    return v;
}
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){ .type = LVAL_SYM, .sym = strdup(s) };
    return v;
}
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){ .type = LVAL_SEXPR };
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){ .type = LVAL_QEXPR };
    return v;
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){.type = LVAL_FUN, .builtin = func };
    return v;
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    *v = (lval){.type = LVAL_FUN, .env = lenv_new(), .formals = formals, .body = body };
    return v;
}

// helper functions ///////////////////////////////////////////////////////////////////////////////////

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, v->count * sizeof(lval*));
    v->cell[v->count-1] = x;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_FUN:
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1));

    v->count--;

    v->cell = realloc(v->cell, v->count * sizeof(lval*));
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_NUM: x->num = v->num; break;
        case LVAL_FUN:
            if (v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;

        case LVAL_ERR: x->err = strdup(v->err); break;
        case LVAL_SYM: x->sym = strdup(v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x ->count = v->count;
            x->cell = malloc(v->count * sizeof(lval*));
            for (int i = 0; i < v->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;
    }

    return x;
}

// printing functions //////////////////////////////////////////////////////////////////////////////////
void lval_expr_print(lval* v, char open, char close) {

    putchar(open);

    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != v->count-1) {
            putchar(' ');
        }
    }

    putchar(close);
}
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
        case LVAL_FUN:
            if (v->builtin) {
                printf("<function>");
            } else {
                printf("(\\ "); lval_print(v->formals);
                putchar(' '); lval_print(v->body); putchar(')');
            }
            break;
    }

}
void lval_println(lval* v) { lval_print(v); putchar('\n'); }
///////////////////////////////////////////////////////////////////////////////////////////////////////