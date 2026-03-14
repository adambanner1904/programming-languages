//
// Created by Developer on 10/03/2026.
//

#ifndef LISPY_LENV_H
#define LISPY_LENV_H

struct lenv;
typedef struct lenv lenv;

struct lval;
typedef struct lval lval;

struct lenv {
    lenv* parent;
    int count;
    char** syms;
    lval** vals;
};

lval* lenv_get(lenv* e, lval* k);
void lenv_put(lenv* e, lval* k, lval* v);
void lenv_def(lenv* e, lval* k, lval* v);
lenv* lenv_new();
void lenv_del(lenv* e);
lenv* lenv_copy(lenv* e);

#endif //LISPY_LENV_H