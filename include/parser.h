#ifndef MINI_SQL_PARSER_H
#define MINI_SQL_PARSER_H

#include "ast.h"

bool parse_sql_script(const char *source, SQLScript *script, char *error, size_t error_size);

#endif
