#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool token_list_append(
    TokenList *tokens,
    TokenType type,
    const char *start,
    size_t length,
    int line,
    int column
) {
    Token *new_items;
    Token token;

    if (tokens->count == tokens->capacity) {
        size_t new_capacity = tokens->capacity == 0 ? 16 : tokens->capacity * 2;
        new_items = (Token *) realloc(tokens->items, sizeof(Token) * new_capacity);
        if (new_items == NULL) {
            return false;
        }

        tokens->items = new_items;
        tokens->capacity = new_capacity;
    }

    token.type = type;
    token.lexeme = (char *) malloc(length + 1);
    token.line = line;
    token.column = column;
    if (token.lexeme == NULL) {
        return false;
    }

    memcpy(token.lexeme, start, length);
    token.lexeme[length] = '\0';
    tokens->items[tokens->count++] = token;
    return true;
}

static TokenType keyword_type(const char *lexeme) {
    if (sql_stricmp(lexeme, "INSERT") == 0) {
        return TOKEN_INSERT;
    }
    if (sql_stricmp(lexeme, "INTO") == 0) {
        return TOKEN_INTO;
    }
    if (sql_stricmp(lexeme, "VALUES") == 0) {
        return TOKEN_VALUES;
    }
    if (sql_stricmp(lexeme, "SELECT") == 0) {
        return TOKEN_SELECT;
    }
    if (sql_stricmp(lexeme, "FROM") == 0) {
        return TOKEN_FROM;
    }
    if (sql_stricmp(lexeme, "WHERE") == 0) {
        return TOKEN_WHERE;
    }
    return TOKEN_IDENTIFIER;
}

static bool append_simple_token(
    TokenList *tokens,
    TokenType type,
    char ch,
    int line,
    int column,
    char *error,
    size_t error_size
) {
    if (!token_list_append(tokens, type, &ch, 1, line, column)) {
        snprintf(error, error_size, "out of memory while tokenizing");
        return false;
    }
    return true;
}

static bool read_string_token(
    const char *source,
    size_t *index,
    int *line,
    int *column,
    TokenList *tokens,
    char *error,
    size_t error_size
) {
    size_t cursor = *index + 1;
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int start_column = *column;

    while (source[cursor] != '\0') {
        char ch = source[cursor];
        if (ch == '\'') {
            if (source[cursor + 1] == '\'') {
                if (length + 1 >= capacity) {
                    size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
                    char *new_buffer = (char *) realloc(buffer, new_capacity);
                    if (new_buffer == NULL) {
                        free(buffer);
                        snprintf(error, error_size, "out of memory while tokenizing string");
                        return false;
                    }
                    buffer = new_buffer;
                    capacity = new_capacity;
                }
                buffer[length++] = '\'';
                cursor += 2;
                *column += 2;
                continue;
            }
            break;
        }

        if (length + 1 >= capacity) {
            size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
            char *new_buffer = (char *) realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                free(buffer);
                snprintf(error, error_size, "out of memory while tokenizing string");
                return false;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }

        buffer[length++] = ch;
        cursor++;
        (*column)++;
    }

    if (source[cursor] != '\'') {
        free(buffer);
        snprintf(error, error_size, "unterminated string literal at line %d, column %d", *line, start_column);
        return false;
    }

    if (buffer == NULL) {
        buffer = (char *) malloc(1);
        if (buffer == NULL) {
            snprintf(error, error_size, "out of memory while tokenizing string");
            return false;
        }
    }

    buffer[length] = '\0';
    if (tokens->count == tokens->capacity) {
        size_t new_capacity = tokens->capacity == 0 ? 16 : tokens->capacity * 2;
        Token *new_items = (Token *) realloc(tokens->items, sizeof(Token) * new_capacity);
        if (new_items == NULL) {
            free(buffer);
            snprintf(error, error_size, "out of memory while tokenizing");
            return false;
        }
        tokens->items = new_items;
        tokens->capacity = new_capacity;
    }

    tokens->items[tokens->count].type = TOKEN_STRING;
    tokens->items[tokens->count].lexeme = buffer;
    tokens->items[tokens->count].line = *line;
    tokens->items[tokens->count].column = start_column;
    tokens->count++;

    *index = cursor + 1;
    (*column)++;
    return true;
}

bool tokenize_sql(const char *source, TokenList *tokens, char *error, size_t error_size) {
    size_t index = 0;
    int line = 1;
    int column = 1;

    memset(tokens, 0, sizeof(*tokens));

    while (source[index] != '\0') {
        char ch = source[index];

        if (isspace((unsigned char) ch)) {
            if (ch == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            index++;
            continue;
        }

        if (ch == '-' && source[index + 1] == '-') {
            index += 2;
            column += 2;
            while (source[index] != '\0' && source[index] != '\n') {
                index++;
                column++;
            }
            continue;
        }

        if (ch == '/' && source[index + 1] == '*') {
            index += 2;
            column += 2;
            while (source[index] != '\0' && !(source[index] == '*' && source[index + 1] == '/')) {
                if (source[index] == '\n') {
                    line++;
                    column = 1;
                } else {
                    column++;
                }
                index++;
            }

            if (source[index] == '\0') {
                snprintf(error, error_size, "unterminated block comment");
                free_tokens(tokens);
                return false;
            }

            index += 2;
            column += 2;
            continue;
        }

        if (isalpha((unsigned char) ch) || ch == '_') {
            size_t start = index;
            int start_column = column;

            while (isalnum((unsigned char) source[index]) || source[index] == '_') {
                index++;
                column++;
            }

            if (!token_list_append(tokens, TOKEN_IDENTIFIER, source + start, index - start, line, start_column)) {
                snprintf(error, error_size, "out of memory while tokenizing");
                free_tokens(tokens);
                return false;
            }

            tokens->items[tokens->count - 1].type = keyword_type(tokens->items[tokens->count - 1].lexeme);
            continue;
        }

        if (isdigit((unsigned char) ch) || (ch == '-' && isdigit((unsigned char) source[index + 1]))) {
            size_t start = index;
            int start_column = column;

            index++;
            column++;
            while (isdigit((unsigned char) source[index])) {
                index++;
                column++;
            }

            if (!token_list_append(tokens, TOKEN_NUMBER, source + start, index - start, line, start_column)) {
                snprintf(error, error_size, "out of memory while tokenizing");
                free_tokens(tokens);
                return false;
            }
            continue;
        }

        if (ch == '\'') {
            if (!read_string_token(source, &index, &line, &column, tokens, error, error_size)) {
                free_tokens(tokens);
                return false;
            }
            continue;
        }

        switch (ch) {
            case ',':
                if (!append_simple_token(tokens, TOKEN_COMMA, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            case '.':
                if (!append_simple_token(tokens, TOKEN_DOT, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            case '(':
                if (!append_simple_token(tokens, TOKEN_LPAREN, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            case ')':
                if (!append_simple_token(tokens, TOKEN_RPAREN, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            case ';':
                if (!append_simple_token(tokens, TOKEN_SEMICOLON, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            case '*':
                if (!append_simple_token(tokens, TOKEN_STAR, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            case '=':
                if (!append_simple_token(tokens, TOKEN_EQUAL, ch, line, column, error, error_size)) {
                    free_tokens(tokens);
                    return false;
                }
                break;
            default:
                snprintf(error, error_size, "unexpected character '%c' at line %d, column %d", ch, line, column);
                free_tokens(tokens);
                return false;
        }

        index++;
        column++;
    }

    if (!token_list_append(tokens, TOKEN_END, "", 0, line, column)) {
        snprintf(error, error_size, "out of memory while tokenizing");
        free_tokens(tokens);
        return false;
    }

    return true;
}

void free_tokens(TokenList *tokens) {
    size_t index;

    for (index = 0; index < tokens->count; ++index) {
        free(tokens->items[index].lexeme);
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}
