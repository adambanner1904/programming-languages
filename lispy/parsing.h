//
// Created by Developer on 09/03/2026.
//

#ifndef LISPY_PARSING_H
#define LISPY_PARSING_H

#include "lval.h"

void parsers_init(void);

void parsers_cleanup(void);

lval* parse_input(const char* filename, const char* input);

#endif //LISPY_PARSING_H