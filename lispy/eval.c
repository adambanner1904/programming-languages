//
// Created by Developer on 09/03/2026.
//

#include "eval.h"
#include <string.h>

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
    return err; \
}

#define LASSERT_NUMBER_OF_ARGS(args, expected, actual, fname) \
    if (!(expected == actual)) { \
        lval* err = lval_err("Function '%s' passed too many arguments. Got %i, Expected %i.", fname, actual, expected); \
        lval_del(args); \
    return err; \
} \

#define LASSERT_TYPES_MATCH(args, index, expected, fname) \
    if (!(expected == a->cell[index]->type)) { \
        lval* err = lval_err("Function '%s' passed incorrect type at index %i. Got %s, Expected %s.", fname, index, ltype_name(a->cell[index]->type), ltype_name(expected)); \
        lval_del(args); \
    return err; \
} \

// list functions
lval * builtin_head(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 1, a->count, "head");
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "head");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'head' passed {}!");

    lval* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;

}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 1, a->count,
        "tail");
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "tail");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'tail' passed {}!")

    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 1, a->count,
        "eval");
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "eval");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {

    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPES_MATCH(a, i, LVAL_QEXPR, "join");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_cons(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 2, a->count,
        "cons");
    LASSERT(a,
        a->cell[1]->type == LVAL_QEXPR || a->cell[1]->type == LVAL_SEXPR,
        "Function 'cons' passed incorrect type's!"
        )

    lval* head = lval_pop(a, 0);
    lval* tail = lval_pop(a, 0);
    if (tail->type == LVAL_SEXPR) {
        tail = builtin_eval(e, tail);
    }

    lval* q = lval_qexpr();
    q = lval_add(q, head);

    lval_del(a);

    return lval_join(q, tail);
}

lval* builtin_len(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 1, a->count,
        "len");
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "len");
    return lval_num(a->cell[0]->count);
}

lval* builtin_init(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 1, a->count,
        "init");
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "init");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'init' passed {}!");

    lval* v = lval_pop(a, 0);
    lval_del(a);

    lval_del(lval_pop(v, v->count - 1));

    return v;
}


// mathematical operations
static lval* builtin_op(lenv* e, lval* a, const char* op) {

    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPES_MATCH(a, i, LVAL_NUM, op)
    }

    lval* x = lval_pop(a, 0);

    if (strcmp(op, "-") == 0 && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by zero!"); break;
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by zero!"); break;
            }
            x->num %= y->num;
        }

        lval_del(y);
    }

    lval_del(a); return x;
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}
lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}
lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}
lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

// other language syntax
lval* builtin_def(lenv* e, lval* a) {
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "def");

    lval* syms = a->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function 'def' passed incorrect type at index %i of syms expr. Got %s, Expected %s.", i, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    LASSERT(a, syms->count == a->count-1,
        "Function 'def' cannot define incorrect "
        "number of values to symbols");

    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    return lval_sexpr();
}

lval* builtin_lambda(lenv* e, lval* a) {
    LASSERT_NUMBER_OF_ARGS(a, 2, a->count, "lambda");
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, "lambda");
    LASSERT_TYPES_MATCH(a, 1, LVAL_QEXPR, "lambda");

    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, a->cell[0]->cell[i]->type == LVAL_SYM,
            "Cannot define non-symbol. Got %s, expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM))
    }

    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPES_MATCH(a, 0, LVAL_QEXPR, func);
    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function '%s' cannot define non-symbol. "
            "Got %s, expected %s.", func,
            ltype_name(syms->cell[i]->type),
            ltype_name(LVAL_SYM));
    }

    LASSERT(a, syms->count == a->count -1,
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count-1)

    for (int i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i+1]);
        }

        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

// eval
lval* lval_call(lenv* e, lval* f, lval* a) {

    if (f->builtin) { return f->builtin(e, a); }

    int given = a->count;
    int total = f->formals->count;

    while (a->count) {

        if (f->formals->count == 0) {
            lval_del(a); return lval_err(
                "Function passed too many arguments. "
                "Got %i, Expected %i.", given, total);
        }

        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_pop(a, 0);

        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    lval_del(a);

    if (f->formals->count == 0) {
        f->env->parent = e;
        return builtin_eval(f->env,
            lval_add(lval_sexpr(), lval_copy(f->body)));
    }
    return lval_copy(f);
}

static lval* lval_eval_sexpr(lenv* e, lval* v) {

    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i);}
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return lval_take(v, 0); }

    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err("S-Expression starts with incorrect type. "
                             "Got %s, Expected %s.");
        lval_del(f); lval_del(v);
        return err;
    }

    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;

}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}


// Add builtins to env
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    // list functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "init", builtin_init);
    lenv_add_builtin(e, "len", builtin_len);


    // mathematical functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    // language syntax
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "lambda", builtin_lambda);
}
