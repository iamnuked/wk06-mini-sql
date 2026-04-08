#ifndef MINI_SQL_LEXER_H
#define MINI_SQL_LEXER_H

#include "common.h"

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_STAR,
    TOKEN_EQUAL,
    TOKEN_INSERT,
    TOKEN_INTO,
    TOKEN_VALUES,
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_END
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

bool tokenize_sql(const char *source, TokenList *tokens, char *error, size_t error_size);
void free_tokens(TokenList *tokens);

#endif
