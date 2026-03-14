#include <stdlib.h>

#include <editline/readline.h>

#include "eval.h"
#include "lval.h"
#include "parsing.h"

int main(int argc, char** argv) {

    parsers_init();
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit");


    while (1) {

        char* input = readline("lispy> ");
        if (!input) break;
        add_history(input);

        lval* parsed_input = parse_input("<stdin>", input);
        lval* evaluated_input = lval_eval(e, parsed_input);

        lval_println(evaluated_input);

        lval_del(evaluated_input);

        free(input);
    }
    lenv_del(e);
    parsers_cleanup();
    return 0;
}
