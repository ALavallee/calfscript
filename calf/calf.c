/*
    MIT License

    Copyright (c) 2023 Antoine Lavallée

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
 */


#include "calf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

/*
 * EXEC OPCODE
 */

#define CALF_OP_NOP 0
#define CALF_OP_POP 1

#define CALF_OP_CONST_NONE 2
#define CALF_OP_CONST_TRUE 3
#define CALF_OP_CONST_FALSE 4
#define CALF_OP_CONST_INT 5
#define CALF_OP_CONST_FLOAT 6
#define CALF_OP_CONST_STR 7
#define CALF_OP_CURRENT_FUNC 8
#define CALF_OP_CONST_INT_1 9
#define CALF_OP_CONST_INT_2 10


#define CALF_OP_CALL 15

#define CALF_OP_LOAD_ATTR 20
#define CALF_OP_STORE_ATTR 21

#define CALF_OP_LOAD_NAME 40
#define CALF_OP_STORE_NAME 41
#define CALF_OP_LOAD_LOCAL 42
#define CALF_OP_STORE_LOCAL 43

#define CALF_OP_OPERATE 50
#define CALF_OP_SET_ADD 51
#define CALF_OP_SET_SUB 52
#define CALF_OP_SET_MUL 54
#define CALF_OP_SET_DIV 55
#define CALF_OP_SET_MOD 56

#define CALF_OP_SET_EQ 57
#define CALF_OP_SET_LARGER 58
#define CALF_OP_SET_LARGER_EQ 59
#define CALF_OP_SET_SMALLER 60
#define CALF_OP_SET_SMALLER_EQ 61
#define CALF_OP_SET_NOT_EQ 62

#define CALF_OP_SET_AND 65
#define CALF_OP_SET_OR 66

#define CALF_OP_JMP 80
#define CALF_OP_COND_JMP 81

#define CALF_OP_RETURN 90


/*
 * Misc functions
 */


//normal heap allocations
static inline void *calf_alloc(int data_size) {
    return malloc(data_size);
}

static inline void *calf_realloc(void *data, int data_size) {
    return realloc(data, data_size);
}


typedef struct {
    char *memory;
    size_t size;
} ByteWriter;

static int byte_writer_int32(ByteWriter *writer, int obj) {
    const size_t element_size = sizeof(obj);
    size_t newSize = writer->size + element_size;

    writer->memory = calf_realloc(writer->memory, newSize);
    const int result = writer->size;
    int *ptr = (int *) &writer->memory[writer->size];
    *ptr = obj;

    writer->size = newSize;

    return result;
}

static void byte_writer_float(ByteWriter *writer, float obj) {
    const size_t element_size = sizeof(obj);
    size_t newSize = writer->size + element_size;

    writer->memory = calf_realloc(writer->memory, newSize);
    float *ptr = (float *) &writer->memory[writer->size];
    *ptr = obj;

    writer->size = newSize;
}

static char *calf_value_type_to_str(CalfValue value) {
    switch (value.type) {
        case CALF_VALUE_TYPE_BOOL:
            return "bool";

        case CALF_VALUE_TYPE_INT:
            return "int";

        case CALF_VALUE_TYPE_FLOAT:
            return "float";

        case CALF_VALUE_TYPE_STR:
            return "str";

        case CALF_VALUE_TYPE_FUNC:
            return "function";

        case CALF_VALUE_TYPE_C_FUNC:
            return "C func";

        case CALF_VALUE_TYPE_NONE:
            return "none";

        case CALF_VALUE_TYPE_ERROR:
            return "error";

        case CALF_VALUE_TYPE_OBJ:
            return "object";

        case CALF_VALUE_TYPE_USER_OBJ:
            return "user object";

        default:
            return "";
    }
}

#define HASHMAP_DEFAULT_CAPACITY 20
#define HASHMAP_MAX_LOAD 0.75f
#define HASHMAP_RESIZE_FACTOR 2

struct bucket {
    // `next` must be the first struct element.
    // changing the order will break multiple functions
    struct bucket *next;

    // key, key size, key hash, and associated value
    void *key;
    size_t ksize;
    uint32_t hash;
    uintptr_t value;
};


static struct bucket *calf_map_resize_entry(CalfMap *m, struct bucket *old_entry) {
    uint32_t index = old_entry->hash % m->capacity;
    for (;;) {
        struct bucket *entry = &m->buckets[index];

        if (entry->key == NULL) {
            *entry = *old_entry; // copy data from old entry
            return entry;
        }

        index = (index + 1) % m->capacity;
    }
}

static void calf_map_init(CalfMap *m) {
    m->capacity = HASHMAP_DEFAULT_CAPACITY;
    m->count = 0;
    m->buckets = calloc(HASHMAP_DEFAULT_CAPACITY, sizeof(struct bucket));
    m->first = NULL;
    // this prevents branching in hashmap_set.
    // m->first will be treated as the "next" pointer in an imaginary bucket.
    // when the first item is added, m->first will be set to the correct address.
    m->last = (struct bucket *) &m->first;
}

static void calf_map_resize(CalfMap *m) {
    struct bucket *old_buckets = m->buckets;

    m->capacity *= HASHMAP_RESIZE_FACTOR;
    // initializes all bucket fields to null
    m->buckets = calloc(m->capacity, sizeof(struct bucket));

    // same trick; avoids branching
    m->last = (struct bucket *) &m->first;

    // assumes that an empty map won't be resized
    do {

        m->last->next = calf_map_resize_entry(m, m->last->next);
        m->last = m->last->next;
    } while (m->last->next != NULL);

    free(old_buckets);
}

#define HASHMAP_HASH_INIT 2166136261u

// FNV-1a hash function
static inline uint32_t calf_map_hash_data(const unsigned char *data, size_t size) {
    size_t nblocks = size / 8;
    uint64_t hash = HASHMAP_HASH_INIT;
    for (size_t i = 0; i < nblocks; ++i) {
        hash ^= (uint64_t) data[0] << 0 | (uint64_t) data[1] << 8 |
                (uint64_t) data[2] << 16 | (uint64_t) data[3] << 24 |
                (uint64_t) data[4] << 32 | (uint64_t) data[5] << 40 |
                (uint64_t) data[6] << 48 | (uint64_t) data[7] << 56;
        hash *= 0xbf58476d1ce4e5b9;
        data += 8;
    }

    uint64_t last = size & 0xff;
    switch (size % 8) {
        case 7:
            last |= (uint64_t) data[6] << 56; /* fallthrough */
        case 6:
            last |= (uint64_t) data[5] << 48; /* fallthrough */
        case 5:
            last |= (uint64_t) data[4] << 40; /* fallthrough */
        case 4:
            last |= (uint64_t) data[3] << 32; /* fallthrough */
        case 3:
            last |= (uint64_t) data[2] << 24; /* fallthrough */
        case 2:
            last |= (uint64_t) data[1] << 16; /* fallthrough */
        case 1:
            last |= (uint64_t) data[0] << 8;
            hash ^= last;
            hash *= 0xd6e8feb86659fd93;
    }

    // compress to a 32-bit result.
    // also serves as a finalizer.
    return hash ^ hash >> 32;
}

static struct bucket *calf_map_find_entry(CalfMap *m, void *key, size_t ksize, uint32_t hash) {
    uint32_t index = hash % m->capacity;
    for (;;) {
        struct bucket *entry = &m->buckets[index];
        // kind of a thicc condition;
        // I didn't want this to span multiple if statements or functions.
        if (entry->key == NULL ||
            // compare sizes, then hashes, then key data as a last resort.
            (entry->ksize == ksize &&
             entry->hash == hash &&
             memcmp(entry->key, key, ksize) == 0)) {
            // return the entry if a match or an empty bucket is found
            return entry;
        }
        index = (index + 1) % m->capacity;
    }
}

static void calf_map_set(CalfMap *m, void *key, size_t ksize, uintptr_t val) {
    if (m->count + 1 > HASHMAP_MAX_LOAD * m->capacity)
        calf_map_resize(m);

    uint32_t hash = calf_map_hash_data(key, ksize);
    struct bucket *entry = calf_map_find_entry(m, key, ksize, hash);
    if (entry->key == NULL) {
        m->last->next = entry;
        m->last = entry;
        entry->next = NULL;

        ++m->count;

        entry->key = key;
        entry->ksize = ksize;
        entry->hash = hash;
    }
    entry->value = val;
}

static bool calf_map_get(CalfMap *m, void *key, size_t ksize, uintptr_t *out_val) {
    uint32_t hash = calf_map_hash_data(key, ksize);
    struct bucket *entry = calf_map_find_entry(m, key, ksize, hash);
    *out_val = entry->value;
    return entry->key != NULL;
}


static void array_add(CalfArray *array, void *element) {
    if (array->size + 1 > array->capacity) {
        ++array->capacity;
        array->capacity *= 2;
        array->data = (void **) calf_realloc(array->data, array->capacity * (int) sizeof(void *));
    }
    array->data[array->size] = element;
    ++array->size;
}

typedef struct {
    char *text;
    int size;
    int id;
} CalfStr;

static int calf_get_or_add_str_to_array(CalfMap *strings_map, CalfArray *strings_array, char *str) {
    CalfStr *found_str;
    //We found the string. So the string is already indexed and part of the array
    if (calf_map_get(strings_map, str, strlen(str), (uintptr_t *) &found_str)) {
        return found_str->id;
    }

    //not found. Add the string to the map and the array
    found_str = calf_alloc(sizeof(CalfStr));
    found_str->id = strings_array->size;
    found_str->text = str;
    array_add(strings_array, str);
    calf_map_set(strings_map, str, strlen(str), (uintptr_t) found_str);
    return found_str->id;
}

/*
 * Parsing functions
 */

typedef struct {
    int current_char;
    char *code;
    char *current_func_name;

    bool has_error;
    CalfError error;
    ByteWriter writer;
    CalfArray *strings_array;
    CalfMap *strings_map;
    CalfArray locals;
} CalfParser;

//Lexer structure

static void calf_parser_raise_error(char *error, CalfParser *parser) {
    if (!parser->has_error) { //If there is already an error, don't overwrite
        parser->has_error = true;
        parser->error.message = error;
        parser->error.row = parser->current_char;
    }
}

static void calf_lex_skip_comment_line(CalfParser *parser) {
    const char *current_line = parser->code;
    while (current_line[parser->current_char] != '\n') {
        ++parser->current_char;
        if (parser->code[parser->current_char] == 0) {
            break;
        }
    }
}

static void calf_lex_skip_white_lines(CalfParser *parser) {
    char *current_line = parser->code;
    while (current_line[parser->current_char] == ' ' || current_line[parser->current_char] == '\t' ||
           current_line[parser->current_char] == '\n' || current_line[parser->current_char] == '#' ||
           current_line[parser->current_char] == '@') {
        if (current_line[parser->current_char] == '#' || current_line[parser->current_char] == '@') {
            ++parser->current_char;
            calf_lex_skip_comment_line(parser);
        } else {
            ++parser->current_char;
        }
    }
}

static bool calf_lex_at_end(CalfParser *parser) {
    return parser->code[parser->current_char] == 0;
}

static char *calf_lex_id(CalfParser *parser) {
    calf_lex_skip_white_lines(parser);
    const char *current_line = parser->code;
    const int start_index = parser->current_char;
    while (isalpha(current_line[parser->current_char]) ||
           (isdigit(current_line[parser->current_char]) && (parser->current_char - start_index)) ||
           current_line[parser->current_char] == '_') {
        ++parser->current_char;
    }

    if (parser->current_char > start_index) {
        const int buffer_size = parser->current_char - start_index;
        char *str_buffer = calf_alloc(buffer_size + 1); // +1 for the null termination
        memcpy(str_buffer, &current_line[start_index], buffer_size);
        str_buffer[buffer_size] = 0; // at the null termination
        return str_buffer;
    }

    parser->current_char = start_index;
    return NULL;
}

static bool calf_lex_specific(CalfParser *parser, char *specific) {
    calf_lex_skip_white_lines(parser);
    const char *current_line = parser->code;
    const int start_index = parser->current_char;
    int spec_size = (int) strlen(specific);
    for (int i = 0; i < spec_size; ++i) {
        char current_char = current_line[parser->current_char];

        if (current_char == 0) { // found an end-of-line before finding all the specifics
            parser->current_char = start_index;
            return false;
        }

        if (current_char == specific[i]) {
            ++parser->current_char;
        } else { //incompatible
            parser->current_char = start_index;
            return false;
        }

    }

    if (parser->current_char > start_index) {
        return true;
    }

    parser->current_char = start_index;
    return false;
}

static bool calf_lex_integer(CalfParser *parser, int *value) {
    calf_lex_skip_white_lines(parser);
    const char *current_line = parser->code;
    const int start_index = parser->current_char;
    while (isdigit(current_line[parser->current_char])) {
        ++parser->current_char;
    }

    if (parser->current_char > start_index) {
        const int buffer_size = parser->current_char - start_index;
        char str_buffer[buffer_size + 1]; // +1 for the null termination
        memcpy(&str_buffer, &current_line[start_index], buffer_size);
        str_buffer[buffer_size] = 0; // at the null termination
        *value = atoi((const char *) &str_buffer);
        return true;
    }

    parser->current_char = start_index;
    return false;
}

static float calf_lex_float(CalfParser *parser, float *value) {
    calf_lex_skip_white_lines(parser);

    const char *current_line = parser->code;
    const int start_index = parser->current_char;
    bool found_dot = false;
    while (isdigit(current_line[parser->current_char]) || current_line[parser->current_char] == '.') {
        if (current_line[parser->current_char] == '.') {
            if (!found_dot) {
                found_dot = true;
            } else {
                calf_parser_raise_error("Multiple '.' found for float decimals", parser);
            }
        }
        ++parser->current_char;
    }

    if (parser->current_char > start_index && found_dot) {
        const int buffer_size = parser->current_char - start_index;
        char str_buffer[buffer_size + 1]; // +1 for the null termination
        memcpy(&str_buffer, &current_line[start_index], buffer_size);
        str_buffer[buffer_size] = 0; // at the null termination
        *value = atof((const char *) &str_buffer);
        return true;
    }

    parser->current_char = start_index;
    return false;
}


static bool calf_lex_str(CalfParser *parser, char **text) {
    calf_lex_skip_white_lines(parser);
    const char *current_line = parser->code;
    const int start_index = parser->current_char;
    if (current_line[parser->current_char] == '"') {
        ++parser->current_char;
        while (current_line[parser->current_char] != '"') {
            ++parser->current_char;
            if (current_line[parser->current_char] == 0) {
                calf_parser_raise_error("\" expected to end a string", parser);
                return false;
            }
        }
        ++parser->current_char;
    }

    if (parser->current_char > start_index) {
        const int str_size = parser->current_char - start_index - 2; // -2 for the two quotes
        char *result = (char *) calf_alloc(str_size + 1); // + 1 for null termination
        memcpy(result, &current_line[start_index + 1], str_size);
        result[str_size] = 0;
        *text = result;
        return true;
    }

    parser->current_char = start_index;
    return false;
}

//Lex eq has his own function because using specific thinks '==' is a valid '=' expr
static bool calf_lex_specific_eq(CalfParser *parser) {
    calf_lex_skip_white_lines(parser);
    const char *current_line = parser->code;
    const int start_index = parser->current_char;
    if (current_line[start_index] == '=' && current_line[start_index + 1] != '=') {
        ++parser->current_char;
        return true;
    }

    return false;
}

/*
 * Code gen functions
 */

static int calf_parse_operator(CalfParser *parser) {
    if (calf_lex_specific(parser, "+")) {
        return CALF_OP_SET_ADD;
    } else if (calf_lex_specific(parser, "-")) {
        return CALF_OP_SET_SUB;
    } else if (calf_lex_specific(parser, "*")) {
        return CALF_OP_SET_MUL;
    } else if (calf_lex_specific(parser, "/")) {
        return CALF_OP_SET_DIV;
    }
    return 0;
}

static bool calf_parse_expr(CalfParser *parser);

static bool calf_parse_expression(CalfParser *parser);

static void calf_parse_func_call(CalfParser *parser) {
    bool parse_func = true;
    int args_count = 0;
    while (parse_func && !parser->has_error) {
        if (!calf_parse_expression(parser)) {
            parse_func = false;
            calf_parser_raise_error("Expression expected for function arguments", parser);
        }

        ++args_count;

        if (calf_lex_specific(parser, ",")) {

        } else if (calf_lex_specific(parser, ")")) {
            parse_func = false;
        } else {
            parse_func = false;
            calf_parser_raise_error("',' or ')' expected after function", parser);
        }
    }

    byte_writer_int32(&parser->writer, CALF_OP_CALL);
    byte_writer_int32(&parser->writer, args_count);
}

static bool calf_parse_followed_expr(CalfParser *parser) {
    char *var_name = calf_lex_id(parser);
    if (var_name) {

        //search if the var can be a local, so we can use the specific opcodes for it
        int local_id = -1;
        for (int i = 0; i < parser->locals.size; ++i) {
            if (strcmp(var_name, parser->locals.data[i]) == 0)
                local_id = i;
        }

        bool var_assign = false;
        if (calf_lex_specific_eq(parser)) {
            var_assign = true;
            if (calf_parse_expression(parser)) {
                byte_writer_int32(&parser->writer, CALF_OP_STORE_LOCAL);
                if (local_id == -1) { //create the local
                    byte_writer_int32(&parser->writer, parser->locals.size);
                    array_add(&parser->locals, var_name);
                } else {
                    byte_writer_int32(&parser->writer, local_id);
                }
            } else {
                calf_parser_raise_error("Expression expected after '='", parser);
            }
        } else {
            if (local_id == -1) {
                if (strcmp(parser->current_func_name, var_name) != 0) {
                    byte_writer_int32(&parser->writer, CALF_OP_LOAD_NAME);
                    byte_writer_int32(&parser->writer,
                                      calf_get_or_add_str_to_array(parser->strings_map, parser->strings_array,
                                                                   var_name));
                } else {
                    byte_writer_int32(&parser->writer, CALF_OP_CURRENT_FUNC);
                }
            } else {
                byte_writer_int32(&parser->writer, CALF_OP_LOAD_LOCAL);
                byte_writer_int32(&parser->writer, local_id);
            }
        }

        if (calf_lex_specific(parser, "(")) { // func call
            calf_parse_func_call(parser);
        }

        bool parse_follow = !var_assign && calf_lex_specific(parser, ".");
        while (parse_follow && !parser->has_error) {
            char *attr_name = calf_lex_id(parser);
            if (attr_name) {

                if (calf_lex_specific_eq(parser)) {
                    if (calf_parse_expr(parser)) {
                        byte_writer_int32(&parser->writer, CALF_OP_STORE_NAME);
                    } else {
                        calf_parser_raise_error("Expression expected after '='", parser);
                    }
                } else {
                    byte_writer_int32(&parser->writer, CALF_OP_LOAD_ATTR);
                }
                byte_writer_int32(&parser->writer,
                                  calf_get_or_add_str_to_array(parser->strings_map, parser->strings_array, attr_name));

                if (calf_lex_specific(parser, "(")) { // func call
                    calf_parse_func_call(parser);
                }

                if (!calf_lex_specific(parser, ".")) {
                    parse_follow = false;
                }
            } else {
                calf_parser_raise_error("Content expected after '.'", parser);
                parse_follow = false;
            }
        }

        return true;
    }

    return false;
}

static bool calf_parse_expr(CalfParser *parser) {
    int int_value;
    float float_value;
    char *str_value;

    if (calf_lex_specific(parser, "true")) {
        byte_writer_int32(&parser->writer, CALF_OP_CONST_TRUE);
    } else if (calf_lex_specific(parser, "false")) {
        byte_writer_int32(&parser->writer, CALF_OP_CONST_FALSE);
    } else if (calf_lex_specific(parser, "none")) {
        byte_writer_int32(&parser->writer, CALF_OP_CONST_NONE);
    } else if (calf_lex_float(parser, &float_value)) {
        byte_writer_int32(&parser->writer, CALF_OP_CONST_FLOAT);
        byte_writer_float(&parser->writer, float_value);
        return true;
    } else if (calf_lex_integer(parser, &int_value)) {
        if (int_value == 1) {
            byte_writer_int32(&parser->writer, CALF_OP_CONST_INT_1);
        } else if (int_value == 2) {
            byte_writer_int32(&parser->writer, CALF_OP_CONST_INT_2);
        } else {
            byte_writer_int32(&parser->writer, CALF_OP_CONST_INT);
            byte_writer_int32(&parser->writer, int_value);
        }
        return true;
    } else if (calf_lex_str(parser, &str_value)) {
        byte_writer_int32(&parser->writer, CALF_OP_CONST_STR);
        const int str_id = calf_get_or_add_str_to_array(parser->strings_map, parser->strings_array, str_value);
        byte_writer_int32(&parser->writer, str_id);
        return true;
    } else {
        return calf_parse_followed_expr(parser);
    }
}

static bool calf_parse_expression(CalfParser *parser) {
    bool parse_expr = true;

    int expr_count = 0;
    int op = 0;
    while (parse_expr && !parser->has_error) {
        if (calf_parse_expr(parser)) {
            if (expr_count >= 1) {
                byte_writer_int32(&parser->writer, op);
                byte_writer_int32(&parser->writer, CALF_OP_OPERATE);
            }
            ++expr_count;
            op = calf_parse_operator(parser);
            if (!op) {
                parse_expr = false;
            }
        } else {
            parse_expr = false;
            calf_parser_raise_error("Expression expected after operator", parser);
        }
    }

    return expr_count > 0;
}

static int calf_parse_cond_operator(CalfParser *parser) {
    //Start picking the longest operator to not pick the wrong one
    if (calf_lex_specific(parser, "==")) {
        return CALF_OP_SET_EQ;
    } else if (calf_lex_specific(parser, "!=")) {
        return CALF_OP_SET_NOT_EQ;
    } else if (calf_lex_specific(parser, ">=")) {
        return CALF_OP_SET_LARGER_EQ;
    } else if (calf_lex_specific(parser, "<=")) {
        return CALF_OP_SET_SMALLER_EQ;
    } else if (calf_lex_specific(parser, "<")) {
        return CALF_OP_SET_SMALLER;
    } else if (calf_lex_specific(parser, ">")) {
        return CALF_OP_SET_LARGER;
    }

    return 0;
}

static bool calf_parse_top_cond_operator(CalfParser *parser) {
    if (calf_lex_specific(parser, "and")) {
        byte_writer_int32(&parser->writer, CALF_OP_SET_AND);
        return true;
    } else if (calf_lex_specific(parser, "or")) {
        byte_writer_int32(&parser->writer, CALF_OP_SET_OR);
        return true;
    }

    return false;
}

static bool calf_parse_cond(CalfParser *parser) {
    bool parse_expr = true;
    int expr_count = 0;
    int op = 0;
    while (parse_expr && !parser->has_error) {
        if (calf_parse_expression(parser)) {
            if (expr_count >= 1) {
                byte_writer_int32(&parser->writer, op);
                byte_writer_int32(&parser->writer, CALF_OP_OPERATE);
            }
            ++expr_count;
            op = calf_parse_cond_operator(parser);
            if (!op) {
                parse_expr = false;
            }
        } else {
            parse_expr = false;
            calf_parser_raise_error("Expression expected after operator", parser);
        }
    }

    return expr_count > 0;
}

static bool calf_parse_top_cond(CalfParser *parser) {
    bool parse_expr = true;
    int expr_count = 0;
    while (parse_expr && !parser->has_error) {
        if (calf_parse_cond(parser)) {
            if (expr_count >= 1) {
                byte_writer_int32(&parser->writer, CALF_OP_OPERATE);
            }
            ++expr_count;
            if (!calf_parse_top_cond_operator(parser)) {
                parse_expr = false;
            }
        } else {
            parse_expr = false;
            calf_parser_raise_error("Expression expected after operator", parser);
        }
    }

    return expr_count > 0;
}

static void calf_parse_return(CalfParser *parser) {
    calf_parse_expression(parser);
    byte_writer_int32(&parser->writer, CALF_OP_RETURN);
}

static void calf_parse_func_content(CalfParser *parser);

static void calf_parse_if(CalfParser *parser) {
    int addr_block_end[500];
    int addr_blocks_count = 0;
    int false_addr = 0;
    if (calf_parse_top_cond(parser)) {
        byte_writer_int32(&parser->writer, CALF_OP_COND_JMP);
        false_addr = byte_writer_int32(&parser->writer, 0);
        calf_parse_func_content(parser);

        byte_writer_int32(&parser->writer, CALF_OP_JMP);
        //create jump that goes after the if - else statement
        addr_block_end[addr_blocks_count++] = byte_writer_int32(&parser->writer, 0);

        parser->writer.memory[false_addr] = parser->writer.size;
        //handle else if chains
        while (calf_lex_specific(parser, "elif")) {
            parser->writer.memory[false_addr] = parser->writer.size;
            byte_writer_int32(&parser->writer, CALF_OP_COND_JMP);
            false_addr = byte_writer_int32(&parser->writer, 0);
            calf_parse_func_content(parser);
            byte_writer_int32(&parser->writer, CALF_OP_JMP);
            //create jump that goes after the if - else statement
            addr_block_end[addr_blocks_count++] = byte_writer_int32(&parser->writer, 0);
        }

        parser->writer.memory[false_addr] = parser->writer.size;

        //handle the else
        if (calf_lex_specific(parser, "else")) {
            calf_parse_func_content(parser);
        }

        for (int i = 0; i < addr_blocks_count; ++i) {
            parser->writer.memory[addr_block_end[i]] = parser->writer.size;
        }
    } else {
        calf_parser_raise_error("Condition expected after if", parser);
    }
}

static void calf_parse_while(CalfParser *parser) {
    int start_cond = parser->writer.size;
    if (calf_parse_cond(parser)) {
        byte_writer_int32(&parser->writer, CALF_OP_COND_JMP);
        int after_else = byte_writer_int32(&parser->writer, 0);
        calf_parse_func_content(parser);
        byte_writer_int32(&parser->writer, CALF_OP_JMP);
        byte_writer_int32(&parser->writer, start_cond);
        parser->writer.memory[after_else] = parser->writer.size;
    } else {
        calf_parser_raise_error("Condition expected after while", parser);
    }
}

static void calf_parse_func_content(CalfParser *parser) {
    bool parse_func = true;

    if (calf_lex_specific(parser, "{")) {
        do {
            if (calf_lex_specific(parser, "pass")) { //pass keyword, we skip
            } else if (calf_lex_specific(parser, "return")) {
                calf_parse_return(parser);
            } else if (calf_lex_specific(parser, "if")) {
                calf_parse_if(parser);
            } else if (calf_lex_specific(parser, "while")) {
                calf_parse_while(parser);
            } else if (calf_lex_specific(parser, "}")) {
                parse_func = false;
            } else {
                if (!calf_parse_expression(parser)) {
                    calf_parser_raise_error("Nothing to do here", parser);
                }
            }
        } while (parse_func && !parser->has_error);
    } else {
        calf_parser_raise_error("'{' expected", parser);
    }
}

static char **calf_parse_func_args(CalfParser *parser, int *args_count) {
    CalfArray args = (CalfArray) {0};
    if (!calf_lex_specific(parser, ")")) {
        bool parse_args = true;
        while (parse_args) {
            char *arg_name = calf_lex_id(parser);
            if (arg_name != NULL) {
                array_add(&parser->locals, arg_name); //args also count has a local
                array_add(&args, arg_name);
                if (calf_lex_specific(parser, ",")) { // another argument
                } else if (calf_lex_specific(parser, ")")) {
                    parse_args = false;
                }
            } else {
                calf_parser_raise_error("Name expected for argument", parser);
                parse_args = false;
            }
        }
    }

    *args_count = args.size;
    return (char **) args.data;
}

CalfFunc calf_parse_func(CalfModule *file, CalfParser *parser, int *found) {
    CalfFunc func = {0};

    calf_lex_skip_white_lines(parser);

    if (!calf_lex_at_end(parser)) {
        if (calf_lex_specific(parser, "fn")) {
            *found = 1;
            func.name = calf_lex_id(parser);
            if (func.name != NULL) {
                parser->current_func_name = func.name;
                if (calf_lex_specific(parser, "(")) {
                    func.args = calf_parse_func_args(parser, &func.args_count);
                    calf_parse_func_content(parser);

                    // if a function ends without return value, it returns null
                    byte_writer_int32(&parser->writer, CALF_OP_CONST_NONE);
                    byte_writer_int32(&parser->writer, CALF_OP_RETURN);

                } else {
                    calf_parser_raise_error("'(' expected for function argument", parser);
                }
            } else {
                calf_parser_raise_error("function name expected after 'fn'", parser);
            }
        } else {
            calf_parser_raise_error("'fn' expected to start a function", parser);
        }
    }

    func.exec_code = parser->writer.memory;
    func.exec_code_size = parser->writer.size;
    func.locals_count = parser->locals.size;
    return func;
}

static void calf_parse_file(CalfScript *script, CalfModule *file, char *text) {
    CalfParser parser = {0};
    parser.code = text;
    parser.strings_array = &script->strings_array;
    parser.strings_map = &script->strings_map;

    int parse_file = true;
    while (parse_file && !parser.has_error) {
        CalfFunc *func = calf_alloc(sizeof(CalfFunc));
        *func = (CalfFunc) {0};
        parser.writer = (ByteWriter) {0};
        parser.locals = (CalfArray) {0};
        *func = calf_parse_func(file, &parser, &parse_file);
        if (parse_file) {
            func->module = file;
            if (func->name != NULL) {
                CalfValue *value = (CalfValue *) calf_alloc(sizeof(CalfValue));
                value->type = CALF_VALUE_TYPE_FUNC;
                value->func_value = func;
                calf_map_set(&file->globals, func->name, strlen(func->name), (uintptr_t) value);
            }
        }

        calf_lex_skip_white_lines(&parser);
        parse_file = !calf_lex_at_end(&parser);
    }

    if (parser.has_error) {
        printf(parser.error.message);
    }
}

bool calf_init(CalfScript *script) {
    calf_map_init(&script->strings_map);
    script->strings_array = (CalfArray) {0};
    script->heap = calf_alloc(CALF_HEAP_SIZE);
    script->current_heap = script->heap;
    calf_map_init(&script->globals);
    return true;
}

void calf_script_set_global(CalfScript *script, char *name, CalfValue value) {
    int name_size = strlen(name);
    char *global_name = calf_alloc(name_size + 1);
    strcpy(global_name, name);

    CalfValue *global_value = (CalfValue *) calf_alloc(sizeof(CalfValue));
    *global_value = value;

    calf_map_set(&script->globals, global_name, name_size, (uintptr_t) global_value);
}

CalfValue calf_script_get_global(CalfScript *script, char *name) {
    CalfValue *value;
    calf_map_get(&script->globals, name, strlen(name), (uintptr_t *) &value);
    return *value;
}

CalfModule *calf_load_module(CalfScript *script, char *text) {
    CalfModule *file = (CalfModule *) calf_alloc(sizeof(CalfModule));
    calf_map_init(&file->globals);
    calf_parse_file(script, file, text);
    return file;
}

CalfValue *calf_module_get_global(CalfModule *module, char *name) {
    CalfValue *found_value;
    if (calf_map_get(&module->globals, name, strlen(name), (uintptr_t *) &found_value)) {
        return found_value;
    }

    return NULL;
}

typedef struct {
    char *heap;
    char *max_heap;
} CalfExec;

static CalfValue calf_exec_raise_error(CalfScript *script, char *func_name, char *error_message, char *error_arg) {
    //Simply make sure to not write over the script error buffer
    if (error_arg != NULL) {
        char error_formating[200];
        sprintf((char *) &error_formating, error_message, 100, error_arg);
        sprintf((char *) &script->error_buffer, "Error in function '%.*s' : %.*s", 80, func_name, 300,
                (char *) &error_formating);
    } else {
        sprintf((char *) &script->error_buffer, "Error in function '%.*s' : %.*s", 80, func_name, 300, error_message);
    }

    CalfValue value;
    value.type = CALF_VALUE_TYPE_ERROR;
    value.error = (char *) &script->error_buffer;
    return value;
}

static inline CalfValue calf_int_op(int left, int right, int op) {
    CalfValue result;
    if (op == CALF_OP_SET_ADD) {
        result.type = CALF_VALUE_TYPE_INT;
        result.int_value = left + right;
    } else if (op == CALF_OP_SET_SUB) {
        result.type = CALF_VALUE_TYPE_INT;
        result.int_value = left - right;
    } else if (op == CALF_OP_SET_MUL) {
        result.type = CALF_VALUE_TYPE_INT;
        result.int_value = left * right;
    } else if (op == CALF_OP_SET_DIV) {
        result.type = CALF_VALUE_TYPE_INT;
        if (right == 0) //to avoid a crash
            result.int_value = 0;
        result.int_value = left / right;
    } else if (op == CALF_OP_SET_AND) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left && right;
    } else if (op == CALF_OP_SET_OR) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left && right;
    } else if (op == CALF_OP_SET_EQ) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left == right;
    } else if (op == CALF_OP_SET_NOT_EQ) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left != right;
    } else if (op == CALF_OP_SET_SMALLER) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left < right;
    } else if (op == CALF_OP_SET_SMALLER_EQ) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left <= right;
    } else if (op == CALF_OP_SET_LARGER) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left > right;
    } else { //larger eq
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left >= right;
    }

    return result;
}

static inline CalfValue calf_bool_op(bool left, bool right, int op) {
    CalfValue result = calf_int_op((int) left, (int) right, op);
    return result;
}

static inline CalfValue calf_float_op(float left, float right, int op) {
    CalfValue result;
    if (op == CALF_OP_SET_ADD) {
        result.type = CALF_VALUE_TYPE_FLOAT;
        result.float_value = left + right;
    } else if (op == CALF_OP_SET_SUB) {
        result.type = CALF_VALUE_TYPE_FLOAT;
        result.float_value = left - right;
    } else if (op == CALF_OP_SET_MUL) {
        result.type = CALF_VALUE_TYPE_FLOAT;
        result.float_value = left * right;
    } else if (op == CALF_OP_SET_DIV) {
        result.type = CALF_VALUE_TYPE_FLOAT;
        if (right == 0) //to avoid a crash
            result.float_value = 0;
        result.float_value = left / right;
    } else if (op == CALF_OP_SET_EQ) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left == right;
    } else if (op == CALF_OP_SET_NOT_EQ) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left != right;
    } else if (op == CALF_OP_SET_SMALLER) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left < right;
    } else if (op == CALF_OP_SET_SMALLER_EQ) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left <= right;
    } else if (op == CALF_OP_SET_LARGER) {
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left > right;
    } else { //larger eq
        result.type = CALF_VALUE_TYPE_BOOL;
        result.bool_value = left >= right;
    }

    return result;
}

//execution linear heap allocations
static inline void *calf_exec_alloc(CalfExec *exec, int data_size) {
    char *result = exec->heap;
    exec->heap += data_size;
    return result;
}

static CalfValue calf_execute_func(CalfScript *script, CalfMap *map, CalfFunc *func, CalfValue *args, int args_count);

static CalfValue calf_execute_op(CalfScript *script, CalfExec *exec, CalfFunc *func, CalfValue *args, int args_count) {

    typedef struct {
        CalfValue stacks[30];
        int stack_index;
        int operation_flag;
        const int opcode_min;
        int *opcode;
    } CallStack;

    CalfValue stacks[30];
    int operation_flag = 0;
    int call_stack_index = 0;
    int stack_index = 0;
    const int *opcode_min = (int *) func->exec_code;
    int *opcode = (int *) func->exec_code;

    //set the arguments into the local array
    const int locals_count = func->locals_count;
    CalfValue locals[locals_count];
    for (int i = 0; i < args_count; ++i) {
        locals[i] = args[i];
    }
    for (int i = args_count; i < locals_count; ++i) {
        locals[i].type = CALF_VALUE_TYPE_NOT_INIT;
    }

#define PUSH(a) stacks[stack_index] = a;++stack_index

#define POP()  --stack_index

#define GET() stacks[stack_index - 1]

#define POP_GET() stacks[--stack_index]

    while (1) {

        switch (*opcode) {

            case CALF_OP_POP: {
                POP();
            }
                break;

            case CALF_OP_CONST_NONE: {
                CalfValue value;
                value.type = CALF_VALUE_TYPE_NONE;
                value.int_value = 0;
                PUSH(value);
            }
                break;

            case CALF_OP_CONST_INT: {
                CalfValue value;
                value.type = CALF_VALUE_TYPE_INT;
                value.int_value = *(++opcode);
                PUSH(value);
            }
                break;

            case CALF_OP_CONST_FLOAT: {
                CalfValue value;
                value.type = CALF_VALUE_TYPE_FLOAT;
                value.float_value = *((float *) (++opcode));
                PUSH(value);
            }
                break;

            case CALF_OP_CONST_STR: {
                const int str_id = *(++opcode);
                char *str = script->strings_array.data[str_id];
                CalfValue value;
                value.type = CALF_VALUE_TYPE_STR;
                value.str_value = str;
                PUSH(value);
            }
                break;

            case CALF_OP_CURRENT_FUNC: {
                CalfValue value;
                value.type = CALF_VALUE_TYPE_FUNC;
                value.func_value = func;
                PUSH(value);
            }
                break;

            case CALF_OP_CONST_INT_1: {
                CalfValue value;
                value.type = CALF_VALUE_TYPE_INT;
                value.int_value = 1;
                PUSH(value);
            }
                break;

            case CALF_OP_CONST_INT_2: {
                CalfValue value;
                value.type = CALF_VALUE_TYPE_INT;
                value.int_value = 2;
                PUSH(value);
            }
                break;

            case CALF_OP_CALL: {
                const int args_count_call = *(++opcode);
                CalfValue all_args[args_count_call];
                for (int i = 0; i < args_count_call; ++i) {
                    all_args[args_count_call - 1 - i] = GET();
                    POP();
                }

                CalfValue func_to_call = POP_GET();

                CalfValue func_result;

                switch (func_to_call.type) {

                    case CALF_VALUE_TYPE_FUNC: {
                        func_result = calf_execute_func(script, &func->module->globals, func_to_call.func_value,
                                                        (CalfValue *) &all_args,
                                                        args_count_call);
                    }
                        break;

                    case CALF_VALUE_TYPE_C_FUNC: {
                        func_result = ((CalfFuncCall) func_to_call.func_value)(script, (CalfValue *) &all_args,
                                                                               args_count_call);
                    }
                        break;

                    default: {
                        return calf_exec_raise_error(script, func->name, "Type '%.*s' can't be called",
                                                     calf_value_type_to_str(func_to_call));
                    }

                }

                PUSH(func_result);

                if (func_result.type == CALF_VALUE_TYPE_ERROR) {
                    return func_result;
                }

            }
                break;

            case CALF_OP_LOAD_ATTR: {
                char *attr_name_to_load = script->strings_array.data[*(++opcode)];

                CalfValue value = POP_GET();

                switch (value.type) {

                    case CALF_VALUE_TYPE_USER_OBJ: {
                        CalfUserObject *user_object = value.user_object_value;
                        CalfGetAttrFunc get_attr_func = user_object->get_attr;
                        if (get_attr_func != NULL) {
                            CalfValue get_attr_value = get_attr_func(script, user_object, attr_name_to_load);
                            if (calf_value_is_error(get_attr_value)) {
                                return get_attr_value;
                            } else {
                                PUSH(get_attr_value);
                            }
                        } else {
                            return calf_value_from_error("Use object doesn't support get_attr");
                        }
                    }
                        break;

                    case CALF_VALUE_TYPE_OBJ: {
                    }
                        break;


                    default:
                        return calf_exec_raise_error(script, func->name, "Can't get attribute on type '%.*s'",
                                                     calf_value_type_to_str(value));
                }
            }
                break;

            case CALF_OP_STORE_ATTR: {
                char *attr_name_to_load = script->strings_array.data[*(++opcode)];

                CalfValue value_to_store = POP_GET();

                CalfValue value = POP_GET();

                if (value.type == CALF_VALUE_TYPE_USER_OBJ) {
                    CalfUserObject *user_object = value.user_object_value;
                    ((CalfSetAttrFunc) user_object->set_attr)(script, user_object, attr_name_to_load, value);
                } else if (value.type == CALF_VALUE_TYPE_OBJ) {

                } else {
                    calf_exec_raise_error(script, func->name, "Can't set attribute on type '%.*s'",
                                          calf_value_type_to_str(value));
                }

            }
                break;

            case CALF_OP_LOAD_NAME: {
                char *name_to_load = script->strings_array.data[*(++opcode)];
                CalfValue *load_value;
                //check in the module first
                if (calf_map_get(&func->module->globals, name_to_load, strlen(name_to_load),
                                 (uintptr_t *) &load_value)) {
                } else if (calf_map_get(&script->globals, name_to_load, strlen(name_to_load),
                                        (uintptr_t *) &load_value)) {
                } else {
                    return calf_exec_raise_error(script, func->name, "Name '%.*s' not found", name_to_load);
                }

                PUSH(*load_value);
            }
                break;

            case CALF_OP_STORE_NAME: {
                /*
                char *name_to_load = script->strings_array.data[*((int *) &opcode[op_index + 1])];
                op_index += sizeof(int);
                CalfValue store_value = GET();
                CalfValue *store_ptr = calf_alloc(sizeof(CalfValue));
                *store_ptr = store_value;
                POP();
                calf_map_set(&exec->variables, name_to_load, strlen(name_to_load), (uintptr_t) store_ptr);
                 */
            }
                break;

            case CALF_OP_LOAD_LOCAL: {
                const int local_id = *(++opcode);
                CalfValue local_value = locals[local_id];
                if (local_value.type != CALF_VALUE_TYPE_NOT_INIT) {
                    PUSH(locals[local_id]);
                } else {
                    return calf_exec_raise_error(script, func->name, "Variable '%.*s' not found", "Var name");
                }
            }
                break;

            case CALF_OP_STORE_LOCAL: {
                const int local_id = *(++opcode);
                locals[local_id] = POP_GET();
            }
                break;

            case CALF_OP_OPERATE: {
                const CalfValue right_value = POP_GET();
                const CalfValue left_value = POP_GET();

                CalfValue op_result;

                if (left_value.type == CALF_VALUE_TYPE_INT) {
                    if (right_value.type == CALF_VALUE_TYPE_INT) {
                        op_result = calf_int_op(left_value.int_value, right_value.int_value, operation_flag);
                    } else if (right_value.type == CALF_VALUE_TYPE_FLOAT) {
                        op_result = calf_float_op((float) left_value.int_value,
                                                  right_value.float_value, operation_flag);
                    } else {
                        return calf_exec_raise_error(script, func->name, "'int' can't operate on '%.*s'",
                                                     calf_value_type_to_str(right_value));
                    }
                } else if (left_value.type == CALF_VALUE_TYPE_BOOL) {
                    op_result = calf_bool_op(left_value.bool_value, right_value.bool_value, operation_flag);
                } else if (left_value.type == CALF_VALUE_TYPE_FLOAT) {
                    if (right_value.type == CALF_VALUE_TYPE_INT) {
                        op_result = calf_float_op(left_value.float_value,
                                                  (float) right_value.int_value, operation_flag);
                    } else if (right_value.type == CALF_VALUE_TYPE_FLOAT) {
                        op_result = calf_float_op(left_value.float_value,
                                                  right_value.float_value, operation_flag);
                    } else {
                        return calf_exec_raise_error(script, func->name, "'float' can't operate on '%.*s'",
                                                     calf_value_type_to_str(right_value));
                    }
                } else if (left_value.type == CALF_VALUE_TYPE_STR) {
                    if (right_value.type == CALF_VALUE_TYPE_STR) {
                        const int left_size = strlen(left_value.str_value);
                        const int right_size = strlen(right_value.str_value);
                        const int result_size = left_size + right_size;
                        op_result.type = CALF_VALUE_TYPE_STR;
                        //+1 for null terminating string
                        char *new_str = calf_exec_alloc(exec, result_size + 1);
                        memcpy(new_str, left_value.str_value, left_size);
                        memcpy(new_str + left_size, right_value.str_value, right_size);
                        new_str[result_size] = 0;
                        op_result.str_value = new_str;
                    } else {
                        return calf_exec_raise_error(script, func->name, "'str' can't operate on '%.*s'",
                                                     calf_value_type_to_str(right_value));
                    }
                } else if (left_value.type == CALF_VALUE_TYPE_NONE || left_value.type == CALF_VALUE_TYPE_BOOL) {
                    op_result = calf_int_op(left_value.int_value, right_value.int_value, operation_flag);
                } else if (left_value.type == CALF_VALUE_TYPE_USER_OBJ && right_value.type == CALF_VALUE_TYPE_NONE) {
                    op_result = calf_int_op(left_value.int_value, right_value.int_value, operation_flag);
                } else {
                    return calf_exec_raise_error(script, func->name, "'%.*s' can't be used with an operator",
                                                 calf_value_type_to_str(left_value));
                }

                PUSH(op_result);
            }
                break;

            case CALF_OP_SET_ADD:
            case CALF_OP_SET_SUB:
            case CALF_OP_SET_MUL:
            case CALF_OP_SET_DIV:
            case CALF_OP_SET_MOD:
            case CALF_OP_SET_EQ:
            case CALF_OP_SET_NOT_EQ:
            case CALF_OP_SET_LARGER:
            case CALF_OP_SET_LARGER_EQ:
            case CALF_OP_SET_SMALLER:
            case CALF_OP_SET_SMALLER_EQ:
            case CALF_OP_SET_AND:
            case CALF_OP_SET_OR: {
                operation_flag = *opcode;
            }
                break;

            case CALF_OP_JMP:
            op_jump:
            {
                const int index_to_jump = *(++opcode);
                opcode = ((int *) ((char *) opcode_min + index_to_jump)) - 1;
            }
                break;

            case CALF_OP_COND_JMP: {
                CalfValue value = POP_GET();
                if (!value.bool_value) {
                    goto op_jump;
                } else
                    ++opcode;
            }
                break;


            case CALF_OP_RETURN:
                if (call_stack_index == 0) {
                    return GET();
                } else {

                }
                break;


            default: {
                char wrong_opcode_buffer[10];
                sprintf((char *) &wrong_opcode_buffer, "%d", *opcode);
                return calf_exec_raise_error(script, func->name, "Unknown opcode '%.*s'",
                                             (char *) &wrong_opcode_buffer);
            }
                break;

        }

        ++opcode;
    }

    //If the function reach the end of execution  without ever returning, it returns None
    CalfValue result;
    result.type = CALF_VALUE_TYPE_NONE;
    return result;
}

static CalfValue calf_execute_func(CalfScript *script, CalfMap *map, CalfFunc *func, CalfValue *args, int args_count) {
    CalfExec exec = {0};
    exec.heap = script->heap;
    return calf_execute_op(script, &exec, func, args, args_count);
}

CalfValue calf_execute(CalfScript *script, CalfModule *file, char *func_name, CalfValue *args, int args_count) {
    CalfValue *found_value;
    if (calf_map_get(&file->globals, func_name, strlen(func_name), (uintptr_t *) &found_value)) {
        CalfFunc *func = found_value->func_value;
        return calf_execute_func(script, &file->globals, func, args, args_count);
    }

    return calf_exec_raise_error(script, "", "No function named '%.*s' found", func_name);
}

