//
// Created by Developer on 17/03/2026.
//

#include "mpc.h"
#include <editline/readline.h>

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return Error(err); }

#define LASSERT_NUM(args, num, fname) \
    if (args->count != num) { \
        Value* err = Error("Function '%s' passed too many arguments. Expected %i, got %i. ", \
            fname, num, args->count); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_TYPE(args, index, expected, fname) \
    if (args->cell[index]->type != expected) { \
        lval_del(args); \
        return Error("Function '%s' passed incorrect type. Expected %s, got %s. ", \
            fname, ltype_name(expected), ltype_name(args->cell[index]->type)); \
    }

#define LASSERT_ARGS_NONEMPTY(args, fname) \
    if (args->cell[0]->count == 0) { \
        lval_del(args); \
        return Error("Function '%s' passed {}! ", fname); \
    }

typedef struct Value Value;
typedef struct Environment Environment;
typedef Value*(*Builtin)(Environment*, Value*);

typedef struct Value {
    int type;
    union {
        long num;
        // Error and Symbol types have some string data
        char* err;
        char* sym;
        Builtin fun;
    };
    // Count and Pointer to a list of "lval*"
    int count;
    Value** cell;
} Value;

struct Environment {
    int count;
    char** syms;
    Value** vals;
};

// Enum of lval types
enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN};

char* ltype_name(int t) {
    switch (t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

// Enum of lval error types
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

// type constructors
Value* Number(long x) {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_NUM, .num = x};
    return val;
}

Value* Error(char* m, ...) {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_ERR};

    va_list va;
    va_start(va, m);

    val->err = malloc(512);
    vsnprintf(val->err, 511, m, va);
    val->err = realloc(val->err, strlen(val->err) + 1);
    va_end(va);
    return val;
}

Value* Symbol(char* s) {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_SYM, .sym = strdup(s)};
    return val;
}

Value* Sexpr() {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_SEXPR};
    return val;
}

Value* Qexpr() {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_QEXPR};
    return val;
}

Value* Func(Builtin func) {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_FUN, .fun = func};
    return val;
}

// Value manipulation ///////////////////////////////////////////////////////////////////////////////
void lval_del(Value* val) {
    switch (val->type) {
        case LVAL_NUM:
        case LVAL_FUN: break;

        case LVAL_ERR: free(val->err); break;
        case LVAL_SYM: free(val->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < val->count; i++) {
                lval_del(val->cell[i]);
            }
            free(val->cell);
            break;
    }

    free(val);
}

Value* lval_append(Value* val, Value* x) {
    val->count++;
    val->cell = realloc(val->cell, sizeof(Value*) * val->count);
    val->cell[val->count-1] = x;
    return val;
}

Value* lval_pop(Value* val, int i) {
    Value* x = val->cell[i];

    memmove(&val->cell[i], &val->cell[i+1],
        sizeof(Value*)*(val->count-i-1));

    val->count--;

    val->cell = realloc(val->cell, sizeof(Value*) * val->count);
    return x;
}

Value* lval_take(Value* val, int i) {
    Value* x = lval_pop(val, i);
    lval_del(val);
    return x;
}

Value* lval_join(Value* x, Value* y) {

    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_append(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

Value* lval_copy(Value* val) {
    Value* x = malloc(sizeof(Value));
    *x = (Value){.type = val->type};

    switch (val->type) {
        case LVAL_FUN: x->fun = val->fun; break;
        case LVAL_NUM: x->num = val->num; break;

        case LVAL_ERR: x->err = strdup(val->err);
        case LVAL_SYM: x->sym = strdup(val->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = val->count;
            x->cell = malloc(sizeof(Value*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(val->cell[i]);
            }
            break;
    }
    return x;
}

void lval_expr_print(Value* val, char open, char close);
void lval_print(Value* val) {
    switch (val->type) {
        case LVAL_NUM: printf("%li", val->num); break;
        case LVAL_FUN: printf("<function>"); break;

        case LVAL_ERR: printf("Err: %s", val->err); break;
        case LVAL_SYM: printf("%s", val->sym); break;
        case LVAL_SEXPR: lval_expr_print(val, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(val, '{', '}'); break;
    }
}

void lval_println(Value* val) {
    lval_print(val); putchar('\n');
}

void lval_expr_print(Value* val, char open, char close) {
    putchar(open);
    for (int i = 0; i < val->count; i++) {
        lval_print(val->cell[i]);
        if (i != (val->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

// Environment functions//////////////////////////////////////////////////////////////////////////////////////////
Environment* lenv_new(void) {
    Environment* env = malloc(sizeof(Environment));
    *env = (Environment){};
    return env;
}

void lenv_del(Environment* env) {
    for (int i = 0; i < env->count; i++) {
        free(env->syms[i]);
        lval_del(env->vals[i]);
    }
    free(env->syms);
    free(env->vals);
    free(env);
}

Value* lenv_get(Environment* env, Value* key) {

    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->syms[i], key->sym) == 0) {
            return lval_copy(env->vals[i]);
        }
    }

    return Error("unbound symbol '%s'!", key->sym);
}

void lenv_put(Environment* env, Value* key, Value* val) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->syms[i], key->sym) == 0) {
            lval_del(env->vals[i]);
            env->vals[i] = lval_copy(val);
            return;
        }
    }

    // If no element found allocate space for a new entry.
    env->count++;
    env->vals = realloc(env->vals, sizeof(Value*) * env->count);
    env->syms = realloc(env->syms, sizeof(char*) * env->count);

    // Copy new info into a new position
    env->vals[env->count-1] = lval_copy(val);
    env->syms[env->count-1] = strdup(key->sym);
}

// parsing /////////////////////////////////////////////////////////////////////////
Value* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
    ? Number(x)
    : Error("invalid number");
}

Value* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return Symbol(t->contents); }

    Value* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = Sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = Sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = Qexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_append(x, lval_read(t->children[i]));
    }
    return x;
}

// Builtin operations /////////////////////////////////////////////////////////////
Value* lval_eval(Environment* env, Value* val);

Value* builtin_op(Environment* env, Value* args, char* op) {
    // typecheck all arguments
    for (int i = 0; i < args->count; i++) {
        if (args->cell[i]->type != LVAL_NUM) {
            lval_del(args);
            return Error("Cannot operate on non-number!");
        }
    }

    Value* x = lval_pop(args, 0);

    // if only one value and no more args with '-' then negate value
    if (strcmp(op, "-") == 0 && args->count == 0) {
        x->num = -x->num;
    }

    while (args->count > 0) {

        Value* y = lval_pop(args, 0);

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

Value* builtin_add(Environment* env, Value* args) { return builtin_op(env, args, "+"); }
Value* builtin_sub(Environment* env, Value* args) { return builtin_op(env, args, "-"); }
Value* builtin_mul(Environment* env, Value* args) { return builtin_op(env, args, "*"); }
Value* builtin_div(Environment* env, Value* args) { return builtin_op(env, args, "/"); }

Value* builtin_head(Environment* env, Value* args) {
    LASSERT_NUM(args, 1, "head");
    LASSERT_TYPE(args, 0, LVAL_QEXPR, "head");
    LASSERT_ARGS_NONEMPTY(args, "head");

    Value* val = lval_take(args, 0);
    while (val->count > 1) {lval_del(lval_pop(val, 1));}
    return val;
}

Value* builtin_tail(Environment* env, Value* args) {
    LASSERT_NUM(args, 1, "tail");
    LASSERT_TYPE(args, 0, LVAL_QEXPR, "tail");
    LASSERT_ARGS_NONEMPTY(args, "tail");

    Value* val = lval_take(args, 0);
    lval_del(lval_pop(val, 0));
    return val;
}

Value* builtin_list(Environment* env, Value* args) {
    args->type = LVAL_QEXPR;
    return args;
}

Value* builtin_eval(Environment* env, Value* args) {
    LASSERT_NUM(args, 1, "eval");
    LASSERT_TYPE(args, 0, LVAL_QEXPR, "eval");

    Value* x = lval_take(args, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(env, x);
}

Value* builtin_join(Environment* env, Value* args) {

    for (int i = 0; i < args->count; i++) {
        LASSERT_TYPE(args, i, LVAL_QEXPR, "join");
    }

    Value* x = lval_pop(args, 0);

    while (args->count) {
        x = lval_join(x, lval_pop(args, 0));
    }

    lval_del(args);
    return x;
}

Value* builtin_def(Environment* env, Value* args) {
    // def {a b} {1 2}
    LASSERT_TYPE(args, 0, LVAL_QEXPR, "def");

    Value* syms = args->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(args, syms->cell[i]->type == LVAL_SYM, "Function 'def' cannot define non-symbol");
    }

    LASSERT(args, syms->count == args->count-1,
        "Function 'def' cannot define incorrect "
        "number of values to symbols");

    for (int i = 0; i < syms->count; i++) {
        lenv_put(env, syms->cell[i], args->cell[i+1]);
    }

    lval_del(args);
    return Sexpr();

}

void lenv_add_builtin(Environment* env, char* name, Builtin func) {
    Value* key = Symbol(name);
    Value* val = Func(func);
    lenv_put(env, key, val);
    lval_del(key); lval_del(val);
}

void lenv_add_builtins(Environment* env) {
    // Variable functions
    lenv_add_builtin(env, "def",  builtin_def);

    // List functions
    lenv_add_builtin(env, "list", builtin_list);
    lenv_add_builtin(env, "head", builtin_head);
    lenv_add_builtin(env, "tail", builtin_tail);
    lenv_add_builtin(env, "eval", builtin_eval);
    lenv_add_builtin(env, "join", builtin_join);

    // Mathematical functions
    lenv_add_builtin(env, "+", builtin_add);
    lenv_add_builtin(env, "-", builtin_sub);
    lenv_add_builtin(env, "*", builtin_mul);
    lenv_add_builtin(env, "/", builtin_div);

}

// eval ///////////////////////////////////////////////////////////////////////////

Value* lval_eval_sexpr(Environment* env, Value* val) {

    // Eval all children
    for (int i = 0; i < val->count; i++) {
        val->cell[i] = lval_eval(env, val->cell[i]);
    }

    // Error checking
    for (int i = 0; i < val->count; i++) {
        if (val->cell[i]->type == LVAL_ERR) { return lval_take(val, i); }
    }

    if (val->count == 0) { return val; }
    if (val->count == 1) { return lval_take(val, 0); }

    Value* f = lval_pop(val, 0);
    if (f->type != LVAL_FUN) {
        Value* err = Error("S-expression starts with incorrect type. "
                           "Got %s, Expected %s.", ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f); lval_del(val);
        return err;
    }

    Value* result = f->fun(env, val);
    lval_del(f);
    return result;
}

Value* lval_eval(Environment* env, Value* val) {
    if (val->type == LVAL_SYM) {
        Value* x = lenv_get(env, val);
        lval_del(val);
        return x;
    }
    
    if (val->type == LVAL_SEXPR) { return lval_eval_sexpr(env, val); }
    return val;
}

int main(int argc, char *argv[]) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "number   : /-?[0-9]+/ ;    "
        "symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; "
        "sexpr    : '(' <expr>* ')' ;"
        "qexpr    : '{' <expr>* '}' ;"
        "expr     : <number> | <symbol> | <sexpr> | <qexpr> ;"
        "lispy    : /^/ <expr>* /$/ ;",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.2");
    puts("Press Ctrl+c to Exit\n");

    Environment* env = lenv_new();
    lenv_add_builtins(env);

    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            Value* x = lval_eval(env, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    lenv_del(env);

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}