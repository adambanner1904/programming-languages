//
// Created by Developer on 10/03/2026.
//

#include "lenv.h"

#include <stdlib.h>
#include <string.h>

#include "lval.h"

lenv* lenv_new() {
    lenv* e = malloc(sizeof(lenv));
    *e = (lenv){};
    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {

    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    if (e->parent) {
        return lenv_get(e->parent, k);
    }
    return lval_err("unbound symbol '%s'!", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = strdup(k->sym);

}

void lenv_def(lenv* e, lval* k, lval* v) {
    while (e->parent) { e = e->parent; }
    lenv_put(e, k, v);
}

lenv* lenv_copy(lenv* e) {
    lenv* new = malloc(sizeof(lenv));
    *new = (lenv){
        .parent = e->parent,
        .count = e->count,
        .syms = malloc(sizeof(char*) * e->count),
        .vals = malloc(sizeof(lval*) * e->count)
    };
    for (int i = 0; i<new->count; i++) {
        new->syms[i] = strdup(e->syms[i]);
        new->vals[i] = lval_copy(e->vals[i]);
    }
    return new;
}