#include "parser.h"

#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    TokenList tokens;
    size_t current;
} Parser;

static Token *peek(Parser *parser) {
    return &parser->tokens.items[parser->current];
}

static bool at_end(Parser *parser) {
    return peek(parser)->type == TOKEN_END;
}

static bool match(Parser *parser, TokenType type) {
    if (peek(parser)->type == type) {
        parser->current++;
        return true;
    }
    return false;
}

static bool consume(Parser *parser, TokenType type, const char *message, char *error, size_t error_size) {
    Token *token = peek(parser);
    if (token->type == type) {
        parser->current++;
        return true;
    }

    snprintf(error, error_size, "%s at line %d, column %d", message, token->line, token->column);
    return false;
}

static bool script_append(SQLScript *script, const Statement *statement) {
    Statement *new_items;

    if (script->count == script->capacity) {
        size_t new_capacity = script->capacity == 0 ? 4 : script->capacity * 2;
        new_items = (Statement *) realloc(script->items, sizeof(Statement) * new_capacity);
        if (new_items == NULL) {
            return false;
        }
        script->items = new_items;
        script->capacity = new_capacity;
    }

    script->items[script->count++] = *statement;
    return true;
}

static bool parse_identifier_list(Parser *parser, StringList *list, char *error, size_t error_size) {
    Token *token;

    memset(list, 0, sizeof(*list));

    do {
        token = peek(parser);
        if (token->type != TOKEN_IDENTIFIER) {
            snprintf(error, error_size, "expected identifier at line %d, column %d", token->line, token->column);
            string_list_free(list);
            return false;
        }

        if (!string_list_append(list, token->lexeme)) {
            snprintf(error, error_size, "out of memory while parsing identifiers");
            string_list_free(list);
            return false;
        }
        parser->current++;
    } while (match(parser, TOKEN_COMMA));

    return true;
}

static bool parse_value_list(Parser *parser, StringList *list, char *error, size_t error_size) {
    Token *token;

    memset(list, 0, sizeof(*list));

    do {
        token = peek(parser);
        if (token->type != TOKEN_STRING && token->type != TOKEN_NUMBER && token->type != TOKEN_IDENTIFIER) {
            snprintf(error, error_size, "expected literal value at line %d, column %d", token->line, token->column);
            string_list_free(list);
            return false;
        }

        if (!string_list_append(list, token->lexeme)) {
            snprintf(error, error_size, "out of memory while parsing values");
            string_list_free(list);
            return false;
        }
        parser->current++;
    } while (match(parser, TOKEN_COMMA));

    return true;
}

static bool parse_qualified_name(Parser *parser, QualifiedName *name, char *error, size_t error_size) {
    Token *first;
    Token *second;
    bool has_schema = false;

    memset(name, 0, sizeof(*name));
    first = peek(parser);
    if (first->type != TOKEN_IDENTIFIER) {
        snprintf(error, error_size, "expected table name at line %d, column %d", first->line, first->column);
        return false;
    }

    parser->current++;
    if (match(parser, TOKEN_DOT)) {
        has_schema = true;
        second = peek(parser);
        if (second->type != TOKEN_IDENTIFIER) {
            snprintf(error, error_size, "expected identifier after '.' at line %d, column %d", second->line, second->column);
            return false;
        }

        name->schema = sql_strdup(first->lexeme);
        name->table = sql_strdup(second->lexeme);
        parser->current++;
    } else {
        name->schema = NULL;
        name->table = sql_strdup(first->lexeme);
    }

    if (name->table == NULL || (has_schema && name->schema == NULL)) {
        free_qualified_name(name);
        snprintf(error, error_size, "out of memory while parsing qualified name");
        return false;
    }

    return true;
}

static bool parse_insert(Parser *parser, Statement *statement, char *error, size_t error_size) {
    InsertStatement insert_statement;

    memset(&insert_statement, 0, sizeof(insert_statement));

    if (!consume(parser, TOKEN_INTO, "expected INTO after INSERT", error, error_size)) {
        return false;
    }

    if (!parse_qualified_name(parser, &insert_statement.target, error, error_size)) {
        return false;
    }

    if (!consume(parser, TOKEN_LPAREN, "expected '(' after table name", error, error_size)) {
        free_qualified_name(&insert_statement.target);
        return false;
    }

    if (!parse_identifier_list(parser, &insert_statement.columns, error, error_size)) {
        free_qualified_name(&insert_statement.target);
        return false;
    }

    if (!consume(parser, TOKEN_RPAREN, "expected ')' after column list", error, error_size)) {
        free_qualified_name(&insert_statement.target);
        string_list_free(&insert_statement.columns);
        return false;
    }

    if (!consume(parser, TOKEN_VALUES, "expected VALUES keyword", error, error_size)) {
        free_qualified_name(&insert_statement.target);
        string_list_free(&insert_statement.columns);
        return false;
    }

    if (!consume(parser, TOKEN_LPAREN, "expected '(' before VALUES list", error, error_size)) {
        free_qualified_name(&insert_statement.target);
        string_list_free(&insert_statement.columns);
        return false;
    }

    if (!parse_value_list(parser, &insert_statement.values, error, error_size)) {
        free_qualified_name(&insert_statement.target);
        string_list_free(&insert_statement.columns);
        return false;
    }

    if (!consume(parser, TOKEN_RPAREN, "expected ')' after VALUES list", error, error_size)) {
        free_qualified_name(&insert_statement.target);
        string_list_free(&insert_statement.columns);
        string_list_free(&insert_statement.values);
        return false;
    }

    statement->type = STATEMENT_INSERT;
    statement->as.insert = insert_statement;
    return true;
}

static bool parse_select(Parser *parser, Statement *statement, char *error, size_t error_size) {
    SelectStatement select_statement;

    memset(&select_statement, 0, sizeof(select_statement));

    if (match(parser, TOKEN_STAR)) {
        select_statement.select_all = true;
    } else if (!parse_identifier_list(parser, &select_statement.columns, error, error_size)) {
        return false;
    }

    if (!consume(parser, TOKEN_FROM, "expected FROM after select list", error, error_size)) {
        string_list_free(&select_statement.columns);
        return false;
    }

    if (!parse_qualified_name(parser, &select_statement.source, error, error_size)) {
        string_list_free(&select_statement.columns);
        return false;
    }

    if (match(parser, TOKEN_WHERE)) {
        Token *column = peek(parser);
        Token *value;

        if (column->type != TOKEN_IDENTIFIER) {
            snprintf(error, error_size, "expected column name after WHERE at line %d, column %d", column->line, column->column);
            string_list_free(&select_statement.columns);
            free_qualified_name(&select_statement.source);
            return false;
        }

        select_statement.where.column = sql_strdup(column->lexeme);
        select_statement.where.enabled = true;
        parser->current++;

        if (!consume(parser, TOKEN_EQUAL, "expected '=' inside WHERE clause", error, error_size)) {
            string_list_free(&select_statement.columns);
            free_qualified_name(&select_statement.source);
            free(select_statement.where.column);
            return false;
        }

        value = peek(parser);
        if (value->type != TOKEN_STRING && value->type != TOKEN_NUMBER && value->type != TOKEN_IDENTIFIER) {
            snprintf(error, error_size, "expected literal after '=' at line %d, column %d", value->line, value->column);
            string_list_free(&select_statement.columns);
            free_qualified_name(&select_statement.source);
            free(select_statement.where.column);
            return false;
        }

        select_statement.where.value = sql_strdup(value->lexeme);
        if (select_statement.where.value == NULL) {
            string_list_free(&select_statement.columns);
            free_qualified_name(&select_statement.source);
            free(select_statement.where.column);
            snprintf(error, error_size, "out of memory while parsing WHERE clause");
            return false;
        }
        parser->current++;
    }

    statement->type = STATEMENT_SELECT;
    statement->as.select = select_statement;
    return true;
}

bool parse_sql_script(const char *source, SQLScript *script, char *error, size_t error_size) {
    Parser parser;
    Statement statement;

    memset(script, 0, sizeof(*script));
    memset(&parser, 0, sizeof(parser));

    if (!tokenize_sql(source, &parser.tokens, error, error_size)) {
        return false;
    }

    while (!at_end(&parser)) {
        memset(&statement, 0, sizeof(statement));

        if (match(&parser, TOKEN_SEMICOLON)) {
            continue;
        }

        if (match(&parser, TOKEN_INSERT)) {
            if (!parse_insert(&parser, &statement, error, error_size)) {
                free_tokens(&parser.tokens);
                free_script(script);
                return false;
            }
        } else if (match(&parser, TOKEN_SELECT)) {
            if (!parse_select(&parser, &statement, error, error_size)) {
                free_tokens(&parser.tokens);
                free_script(script);
                return false;
            }
        } else {
            Token *token = peek(&parser);
            snprintf(error, error_size, "unsupported statement at line %d, column %d", token->line, token->column);
            free_tokens(&parser.tokens);
            free_script(script);
            return false;
        }

        if (!script_append(script, &statement)) {
            free_statement(&statement);
            free_tokens(&parser.tokens);
            free_script(script);
            snprintf(error, error_size, "out of memory while building AST");
            return false;
        }

        match(&parser, TOKEN_SEMICOLON);
    }

    free_tokens(&parser.tokens);
    return true;
}
