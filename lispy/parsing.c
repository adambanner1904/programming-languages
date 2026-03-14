//
// Created by Developer on 09/03/2026.
//
#include "mpc.h"
#include "parsing.h"
#include <string.h>

static mpc_parser_t* Number;
static mpc_parser_t* Symbol;
static mpc_parser_t* Sexpr;
static mpc_parser_t* Qexpr;
static mpc_parser_t* Expr;
static mpc_parser_t* Lispy;

void parsers_init(void) {
    Number = mpc_new("number");
    Symbol = mpc_new("symbol");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Expr = mpc_new("expr");
    Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "number   : /-?[0-9]+/;"
        "symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;"
        "sexpr    : '(' <expr>* ')';"
        "qexpr    : '{' <expr>* '}';"
        "expr     : <number> | <symbol> | <sexpr> | <qexpr>;"
        "lispy    : /^/ <expr>+ /$/;",
        Number, Symbol, Sexpr, Qexpr, Lispy, Expr);
}

void parsers_cleanup(void) {
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Lispy, Expr);
}

static lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
    ? lval_num(x)
    : lval_err("Invalid number");
}

static lval* lval_read(mpc_ast_t* t) {

    if (strstr(t->tag, "number")) {return lval_read_num(t);}
    if (strstr(t->tag, "symbol")) {return lval_sym(t->contents);}

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) {x = lval_sexpr();}
    if (strstr(t->tag, "sexpr")) {x = lval_sexpr();}
    if (strstr(t->tag, "qexpr")) {x = lval_qexpr();}

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {continue;}
        if (strcmp(t->children[i]->contents, ")") == 0) {continue;}
        if (strcmp(t->children[i]->contents, "}") == 0) {continue;}
        if (strcmp(t->children[i]->contents, "{") == 0) {continue;}
        if (strcmp(t->children[i]->tag, "regex") == 0) {continue;}
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;

}

lval* parse_input(const char* filename, const char* input) {
    mpc_result_t r;

    if (mpc_parse(filename, input, Lispy, &r)) {
        lval* result = lval_read(r.output);
        mpc_ast_delete(r.output);
        return result;
    }

    char* err_str = mpc_err_string(r.error);
    lval* err = lval_err(err_str);
    free(err_str);
    mpc_err_delete(r.error);
    return err;
}
