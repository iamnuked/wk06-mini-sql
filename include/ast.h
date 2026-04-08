#ifndef MINI_SQL_AST_H
#define MINI_SQL_AST_H

#include "common.h"

typedef struct {
    char *schema;
    char *table;
} QualifiedName;

typedef struct {
    bool enabled;
    char *column;
    char *value;
} WhereClause;

typedef struct {
    QualifiedName target;
    StringList columns;
    StringList values;
} InsertStatement;

typedef struct {
    QualifiedName source;
    bool select_all;
    StringList columns;
    WhereClause where;
} SelectStatement;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct {
    StatementType type;
    union {
        InsertStatement insert;
        SelectStatement select;
    } as;
} Statement;

typedef struct {
    Statement *items;
    size_t count;
    size_t capacity;
} SQLScript;

void free_qualified_name(QualifiedName *name);
void free_statement(Statement *statement);
void free_script(SQLScript *script);

#endif
