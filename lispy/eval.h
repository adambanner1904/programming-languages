//
// Created by Developer on 09/03/2026.
//

#ifndef LISPY_EVAL_H
#define LISPY_EVAL_H

#include "lval.h"

lval* lval_eval(lenv* e, lval* v);
void lenv_add_builtins(lenv* e);

#endif //LISPY_EVAL_H