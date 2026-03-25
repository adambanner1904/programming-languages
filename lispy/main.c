//
// Created by Developer on 17/03/2026.
//

#include "mpc.h"
#include <editline/readline.h>

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { Value* err = Error(fmt, ##__VA_ARGS__); lval_del(args); return err; }

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

mpc_parser_t* NumberParser;
mpc_parser_t* SymbolParser;
mpc_parser_t* StringParser;
mpc_parser_t* CommentParser;
mpc_parser_t* SexprParser;
mpc_parser_t* QexprParser;
mpc_parser_t* ExprParser;
mpc_parser_t* LispyParser;

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
        char* str;
    };
    // functions
    Builtin builtin;
    Environment* env;
    Value* formals; //
    Value* body; // Q-expression

    // Count and Pointer to a list of "lval*"
    int count;
    Value** cell;
} Value;

struct Environment {
    Environment* parent;
    int count;
    char** syms;
    Value** vals;
};

// Enum of lval types
enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_STR, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN};

char* ltype_name(int t) {
    switch (t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_STR: return "String";
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

Value* String(char* s) {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_STR, .str = strdup(s)};
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
    *val = (Value){.type = LVAL_FUN, .builtin = func};
    return val;
}

Environment* lenv_new(void);
Environment* lenv_copy(Environment* e);

Value* Lambda(Value* formals, Value* body) {
    Value* val = malloc(sizeof(Value));
    *val = (Value){.type = LVAL_FUN, .formals = formals, .body = body};
    val->env = lenv_new();
    return val;
}

// Value manipulation ///////////////////////////////////////////////////////////////////////////////
void lenv_del(Environment* env);
void lval_del(Value* val) {
    switch (val->type) {
        case LVAL_NUM: break;
        case LVAL_FUN:
            if (!val->builtin) {
                lenv_del(val->env);
                lval_del(val->formals);
                lval_del(val->body);
            }
            break;

        case LVAL_ERR: free(val->err); break;
        case LVAL_SYM: free(val->sym); break;
        case LVAL_STR: free(val->str); break;

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
    for (int i = 0; i < y->count; i++) {
        x = lval_append(x, y->cell[i]);
    }

    /* Delete the empty 'y' and return 'x' */
    free(y->cell);
    free(y);
    return x;
}

Value* lval_copy(Value* val) {
    Value* x = malloc(sizeof(Value));
    *x = (Value){.type = val->type};

    switch (val->type) {
        case LVAL_FUN:
            if (val->builtin) {
                x->builtin = val->builtin;
            } else {
                x->env = lenv_copy(val->env);
                x->formals = lval_copy(val->formals);
                x->body = lval_copy(val->body);
            }
            break;
        case LVAL_NUM: x->num = val->num; break;

        case LVAL_ERR: x->err = strdup(val->err);
        case LVAL_SYM: x->sym = strdup(val->sym); break;
        case LVAL_STR: x->sym = strdup(val->str); break;

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

int lval_eq(Value* x, Value* y) {
    if (x->type != y->type) return 0;

    switch (x->type) {
        case LVAL_NUM: return x->num == y->num;
        case LVAL_ERR: return strcmp(x->err, y->err) == 0;
        case LVAL_SYM: return strcmp(x->sym, y->sym) == 0;
        case LVAL_STR: return strcmp(x->str, y->str) == 0;

        case LVAL_FUN:
            if (x->builtin || y->builtin) {
                return x->builtin == y->builtin;
            }
            return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i = 0; i < x->count; i++) {
                if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }
            return 1;
    }
    return 0;
}

void lval_print_str(Value* val) {
    char* escaped = strdup(val->str);
    escaped = mpcf_escape(escaped);
    printf("\"%s\"", escaped);
    free(escaped);
}

void lval_expr_print(Value* val, char open, char close);
void lval_print(Value* val) {
    switch (val->type) {
        case LVAL_NUM: printf("%li", val->num); break;
        case LVAL_FUN:
            if (val->builtin) {
                printf("<builtin>");
            } else {
                printf("(\\ "); lval_print(val->formals);
                putchar(' '); lval_print(val->body); putchar(')');
            }
            break;

        case LVAL_ERR: printf("Error: %s", val->err); break;
        case LVAL_SYM: printf("%s", val->sym); break;
        case LVAL_STR: lval_print_str(val); break;
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

    if (env->parent) {
        return lenv_get(env->parent, key);
    }
    return Error("Unbound Symbol '%s'", key->sym);
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

void lenv_def(Environment* env, Value* key, Value* val) {
    while (env->parent) { env = env->parent; }
    lenv_put(env, key, val);
}

Environment* lenv_copy(Environment* e) {
    Environment* n = malloc(sizeof(Environment));
    n->parent = e->parent;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(Value*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = strdup(e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

// parsing /////////////////////////////////////////////////////////////////////////
Value* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
    ? Number(x)
    : Error("invalid number");
}

Value* lval_read_str(mpc_ast_t* t) {
    t->contents[strlen(t->contents)-1] = '\0';
    char* unescaped = strdup(t->contents+1);
    unescaped = mpcf_unescape(unescaped);
    Value* str = String(unescaped);
    free(unescaped);
    return str;
}

Value* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return Symbol(t->contents); }
    if (strstr(t->tag, "string")) { return lval_read_str(t); }

    Value* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = Sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = Sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = Qexpr(); }
    if (x == NULL) return Error("Invalid expression!");

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "comment") == 0) { continue; }
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
        Value* y = lval_pop(args, 0);
        x = lval_join(x, y);
    }

    lval_del(args);
    return x;
}

Value* builtin_lambda(Environment* env, Value* args) {
    LASSERT_NUM(args, 2, "\\");
    LASSERT_TYPE(args, 0, LVAL_QEXPR, "\\");
    LASSERT_TYPE(args, 1, LVAL_QEXPR, "\\");

    for (int i = 0; i < args->cell[0]->count; i++) {
        LASSERT(args, args->cell[0]->cell[i]->type == LVAL_SYM,
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(args->cell[0]->cell[i]->type), ltype_name(LVAL_SYM))
    }

    Value* formals = lval_pop(args, 0);
    Value* body = lval_pop(args, 0);
    lval_del(args);
    return Lambda(formals, body);

}

Value* builtin_var(Environment* env, Value* args, char* func) {
    LASSERT_TYPE(args, 0, LVAL_QEXPR, func);

    Value* syms = args->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(args, syms->cell[i]->type == LVAL_SYM,
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.", func,
            ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    LASSERT(args, syms->count == args->count-1,
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, args->count-1);

    for (int i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0) {
            lenv_def(env, syms->cell[i], args->cell[i+1]);
        }
        if (strcmp(func, "=") == 0) {
            lenv_put(env, syms->cell[i], args->cell[i+1]);
        }
    }

    lval_del(args);
    return Sexpr();
}

Value* builtin_def(Environment* env, Value* args) {
    return builtin_var(env, args, "def");
}

Value* builtin_put(Environment* env, Value* args) {
    return builtin_var(env, args, "=");
}

Value* builtin_print(Environment* env, Value* args) {
    for (int i = 0; i < args->count; i++) {
        lval_print(args->cell[i]); putchar(' ');
    }
    putchar('\n');
    lval_del(args);
    return Sexpr();
}

Value* builtin_error(Environment* env, Value* args) {
    LASSERT_NUM(args, 1, "error");
    LASSERT_TYPE(args, 0, LVAL_STR, "error");

    Value* err = Error(args->cell[0]->str);
    lval_del(args);
    return err;
}

// comparison
Value* builtin_ord(Environment* env, Value* args, char* op) {
    LASSERT_NUM(args, 2, op);
    LASSERT_TYPE(args, 0, LVAL_NUM, op);
    LASSERT_TYPE(args, 1, LVAL_NUM, op);

    int r;
    if (strcmp(op, ">") == 0) {
        r = args->cell[0]->num > args->cell[1]->num;
    }
    if (strcmp(op, "<") == 0) {
        r = args->cell[0]->num < args->cell[1]->num;
    }
    if (strcmp(op, ">=") == 0) {
        r = args->cell[0]->num >= args->cell[1]->num;
    }
    if (strcmp(op, "<=") == 0) {
        r = args->cell[0]->num <= args->cell[1]->num;
    }
    lval_del(args);
    return Number(r);
}

Value* builtin_cmp(Environment* env, Value* args, char* op) {
    LASSERT_NUM(args, 2, op);
    int r;
    if (strcmp(op, "==") == 0) {
        r = lval_eq(args->cell[0], args->cell[1]);
    }
    if (strcmp(op, "!=") == 0) {
        r = !lval_eq(args->cell[0], args->cell[1]);
    }
    lval_del(args);
    return Number(r);
}

Value* builtin_gt(Environment* env, Value* args) {
    return builtin_ord(env, args, ">");
}
Value* builtin_lt(Environment* env, Value* args) {
    return builtin_ord(env, args, "<");
}
Value* builtin_ge(Environment* env, Value* args) {
    return builtin_ord(env, args, ">=");
}
Value* builtin_le(Environment* env, Value* args) {
    return builtin_ord(env, args, "<=");
}

Value* builtin_eq(Environment* env, Value* args) {
    return builtin_cmp(env, args, "==");
}

Value* builtin_ne(Environment* env, Value* args) {
    return builtin_cmp(env, args, "!=");
}

Value* builtin_if(Environment* env, Value* args) {
    LASSERT_NUM(args, 3, "if");
    LASSERT_TYPE(args, 0, LVAL_NUM, "if");
    LASSERT_TYPE(args, 1, LVAL_QEXPR, "if");
    LASSERT_TYPE(args, 2, LVAL_QEXPR, "if");

    Value* x;
    args->cell[1]->type = LVAL_SEXPR;
    args->cell[2]->type = LVAL_SEXPR;

    if (args->cell[0]->num) {
        x = lval_eval(env, lval_pop(args, 1));
    } else {
        x = lval_eval(env, lval_pop(args, 2));
    }

    lval_del(args);
    return x;
}

Value* builtin_load(Environment* env, Value* args) {
    LASSERT_NUM(args, 1, "load");
    LASSERT_TYPE(args, 0, LVAL_STR, "load");

    mpc_result_t r;
    if (mpc_parse_contents(args->cell[0]->str, LispyParser, &r)) {
        Value* expr = lval_read(r.output);
        mpc_ast_delete(r.output);

        while (expr->count) {
            Value* x = lval_eval(env, lval_pop(expr, 0));
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }

        lval_del(expr);
        lval_del(args);

        return Sexpr();
    }

    char* err_msg = mpc_err_string(r.error);
    mpc_err_delete(r.error);

    Value* err = Error("Could not load library %s", err_msg);
    free(err_msg);
    lval_del(args);

    return err;
}

void lenv_add_builtin(Environment* env, char* name, Builtin func) {
    Value* key = Symbol(name);
    Value* val = Func(func);
    lenv_put(env, key, val);
    lval_del(key); lval_del(val);
}

void lenv_add_builtins(Environment* env) {
    // Comparison functions
    lenv_add_builtin(env, "if", builtin_if);
    lenv_add_builtin(env, "==", builtin_eq);
    lenv_add_builtin(env, "!=", builtin_ne);
    lenv_add_builtin(env, ">",  builtin_gt);
    lenv_add_builtin(env, "<",  builtin_lt);
    lenv_add_builtin(env, ">=", builtin_ge);
    lenv_add_builtin(env, "<=", builtin_le);
    // Variable functions
    lenv_add_builtin(env, "def", builtin_def);
    lenv_add_builtin(env, "=",   builtin_put);
    lenv_add_builtin(env, "\\",  builtin_lambda);

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

    // syntax
    lenv_add_builtin(env, "load", builtin_load);
    lenv_add_builtin(env, "error", builtin_error);
    lenv_add_builtin(env, "print", builtin_print);

}

// eval ///////////////////////////////////////////////////////////////////////////
Value* lval_call(Environment* env, Value* func, Value* args) {

    if (func->builtin) { return func->builtin(env, args); }

    // record argument counts
    int given = args->count;
    int total = func->formals->count;

    while (args->count) {
        if (func->formals->count == 0) {
            lval_del(args);
            return Error("Function passed too many arguments. "
                         "Got %i, Expected %i.", given, total);
        }

        Value* sym = lval_pop(func->formals, 0);

        // handle where the sym just popped off is '&'
        if (strcmp(sym->sym, "&") == 0) {
            if (func->formals->count !=1 ) {
                lval_del(args);
                return Error("Function format invalid. "
                             "Symbol '&' not followed by single symbol.");
            }

            Value* nsym = lval_pop(func->formals, 0);
            lenv_put(func->env, nsym, builtin_list(env, args));
            lval_del(sym); lval_del(nsym);
            break;
        }

        Value* val = lval_pop(args, 0);

        lenv_put(func->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    // done with arguments as they are now bound in the functions env
    lval_del(args);

    // if '&' remains in formal list bind to empty list
    if (func->formals->count > 0 &&
        strcmp(func->formals->cell[0]->sym, "&") == 0) {

        if (func->formals->count != 2) {
            return Error("Function format invalid. "
                         "Symbol '&' not followed by single symbol");
        }
        // pop and delete '&' symbol
        lval_del(lval_pop(func->formals, 0));

        Value* sym = lval_pop(func->formals, 0);
        Value* val = Qexpr();

        lenv_put(func->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    if (func->formals->count == 0) {
        func->env->parent = env;

        return builtin_eval(func->env, lval_append(Sexpr(), lval_copy(func->body)));
    }

    return lval_copy(func);
}

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

    Value* result = lval_call(env, f, val);
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
    NumberParser = mpc_new("number");
    SymbolParser = mpc_new("symbol");
    StringParser = mpc_new("string");
    CommentParser = mpc_new("comment");
    SexprParser  = mpc_new("sexpr");
    QexprParser  = mpc_new("qexpr");
    ExprParser   = mpc_new("expr");
    LispyParser  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "number   : /-?[0-9]+/ ;    "
        "symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; "
        "string   : /\"(\\\\.|[^\"])*\"/ ;"
        "comment  : /;[^\\r\\n]*/ ;"
        "sexpr    : '(' <expr>* ')' ;"
        "qexpr    : '{' <expr>* '}' ;"
        "expr     : <number> | <symbol> | <string> | <comment> | <sexpr> | <qexpr> ;"
        "lispy    : /^/ <expr>* /$/ ;",
        NumberParser, SymbolParser, StringParser, CommentParser, SexprParser, QexprParser, ExprParser, LispyParser);

    puts("Lispy Version 0.0.0.0.2");
    puts("Press Ctrl+c to Exit\n");

    Environment* env = lenv_new();
    lenv_add_builtins(env);
    builtin_load(env, String("stdlib.lspy"));

    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            Value* args = lval_append(Sexpr(), String(argv[i]));
            Value* x = builtin_load(env, args);
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }
    }
    while (1) {
        char* input = readline("lispy> ");
        add_history(input);


        mpc_result_t r;
        if (mpc_parse("<stdin>", input, LispyParser, &r)) {
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

    mpc_cleanup(8, NumberParser, SymbolParser, StringParser,
        CommentParser, SexprParser, QexprParser, ExprParser, LispyParser);

    return 0;
}