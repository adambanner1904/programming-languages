//
// Created by Developer on 07/03/2026.
//

#ifndef LISPY_LVAL_H
#define LISPY_LVAL_H

#include "lenv.h"

struct lval;

typedef struct lval lval;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    int type;
    union {
        long num;
        char* err;
        char* sym;
    };

    // function
    lbuiltin builtin; // if null then user defined function
    lenv* env;
    lval* formals; // q-expression
    lval* body; // q-expression

    // expression
    int count;
    lval** cell;
};

// type's
enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN};
char* ltype_name(int t);
// constructors
lval* lval_num(long x);
lval* lval_err(char* fmt, ...);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
lval* lval_fun(lbuiltin v);
lval* lval_lambda(lval* formals, lval* body);

// lval manipulation
lval* lval_add(lval* v, lval* x);

void lval_del(lval* v);

lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);

lval* lval_copy(lval* v);

// print lval
void lval_print(lval* v);
void lval_println(lval* v);
void lval_expr_print(lval* v, char open, char close);


#endif //LISPY_LVAL_H