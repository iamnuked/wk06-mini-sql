#include "ast.h"

#include <stdlib.h>

void free_qualified_name(QualifiedName *name) {
    free(name->schema);
    free(name->table);
    name->schema = NULL;
    name->table = NULL;
}

void free_statement(Statement *statement) {
    if (statement->type == STATEMENT_INSERT) {
        free_qualified_name(&statement->as.insert.target);
        string_list_free(&statement->as.insert.columns);
        string_list_free(&statement->as.insert.values);
    } else if (statement->type == STATEMENT_SELECT) {
        free_qualified_name(&statement->as.select.source);
        string_list_free(&statement->as.select.columns);
        free(statement->as.select.where.column);
        free(statement->as.select.where.value);
        statement->as.select.where.column = NULL;
        statement->as.select.where.value = NULL;
    }
}

void free_script(SQLScript *script) {
    size_t index;

    for (index = 0; index < script->count; ++index) {
        free_statement(&script->items[index]);
    }

    free(script->items);
    script->items = NULL;
    script->count = 0;
    script->capacity = 0;
}
