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


#ifndef CALFSCRIPT_CALF_H
#define CALFSCRIPT_CALF_H

//By default, the heap is 128kb. It is totally safe to extend it.
#define CALF_HEAP_SIZE 131072

#include <stdbool.h>

typedef enum {
    CALF_VALUE_TYPE_NOT_INIT = 0,
    CALF_VALUE_TYPE_NONE = 1,
    CALF_VALUE_TYPE_BOOL = 2,
    CALF_VALUE_TYPE_INT = 3,
    CALF_VALUE_TYPE_FLOAT = 4,
    CALF_VALUE_TYPE_STR = 5,
    CAL_VALUE_TYPE_ARRAY = 6,
    CALF_VALUE_TYPE_OBJ = 7,
    CALF_VALUE_TYPE_FUNC = 8,
    CALF_VALUE_TYPE_C_FUNC = 9,
    CALF_VALUE_TYPE_USER_OBJ = 10,
    CALF_VALUE_TYPE_ERROR = 11
} CalfValueType;


typedef struct {
    struct bucket *buckets;
    int capacity;
    int count;

    struct bucket *first;
    struct bucket *last;
} CalfMap;

typedef struct {
    void **data;
    int size;
    int capacity;
} CalfArray;

typedef struct {
    char *message;
    int line;
    int row;
} CalfError;

typedef struct {
    // We keep the strings in two different structure
    CalfMap strings_map; // In a map so we can get their id by value quickly
    CalfArray strings_array; //In an array to get their value by key quickly

    CalfMap globals;

    //heap is allocated during init
    char *heap;
    char *current_heap;
    int heap_size;

    //Error buffer to write the error message
    char error_buffer[500];
} CalfScript;

typedef struct {
    CalfMap globals; //Map containing mostly the functions inside a module (file)
} CalfModule;

typedef struct {
    char *name; //the name of the function defined with script code. NOT UNIQUE
    char **args;
    int args_count;
    int locals_count; //Amount of local variables
    char *exec_code; //byte code
    int exec_code_size; //amount of byte codes
    CalfModule *module; // The module that contains the function
} CalfFunc;

typedef struct {
    int type;
    void *obj;
    void *get_attr;
    void *set_attr;
} CalfUserObject;

typedef struct {
    CalfValueType type;

    union {
        bool bool_value;
        int int_value;
        float float_value;
        char *str_value;
        CalfFunc *func_value;
        char *error;
        CalfUserObject *user_object_value;
    };

} CalfValue;

typedef CalfValue (*CalfGetAttrFunc)(CalfScript *, CalfUserObject *, char *attr_name);

typedef CalfValue (*CalfSetAttrFunc)(CalfScript *, CalfUserObject *, char *, CalfValue);

//First argument is the script, the second is a list of the arguments and third is argument length
typedef CalfValue (*CalfFuncCall)(CalfScript *, CalfValue *, int);


/*
 *
 *  PUBLIC CALF API --------
 *
 */

/*
 *  VALUE API
 */

typedef CalfValue (CalfInterfaceFunc)(CalfScript *, CalfValue *, int);

static CalfValue calf_value_none() {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_NONE;
    value.int_value = 0;
    return value;
}

static CalfValue calf_value_from_bool(bool bool_value) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_BOOL;
    value.bool_value = bool_value;
    return value;
}

static CalfValue calf_value_true() {
    return calf_value_from_bool(true);
}

static CalfValue calf_value_false() {
    return calf_value_from_bool(false);
}

static CalfValue calf_value_from_int(int int_value) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_INT;
    value.int_value = int_value;
    return value;
}

static CalfValue calf_value_from_float(float float_value) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_FLOAT;
    value.float_value = float_value;
    return value;
}

static CalfValue calf_value_from_c_string(char *str) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_STR;
    value.str_value = str;
    return value;
}

static CalfValue calf_value_from_user_value(CalfUserObject *obj) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_USER_OBJ;
    value.user_object_value = obj;
    return value;
}

static CalfValue calf_value_from_interface_function(CalfInterfaceFunc func) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_C_FUNC;
    value.func_value = (CalfFunc *) func;
    return value;
}

static CalfValue calf_value_from_error(char *error) {
    CalfValue value;
    value.type = CALF_VALUE_TYPE_ERROR;
    value.error = error;
    return value;
}

static bool calf_value_to_bool(CalfValue value) {
    switch (value.type) {

        case CALF_VALUE_TYPE_INT:
            return (bool) value.int_value;

        case CALF_VALUE_TYPE_FLOAT:
            return (float) value.float_value;

        case CALF_VALUE_TYPE_BOOL:
            return value.bool_value;

        default:
            return false;

    }
}

static int calf_value_to_int(CalfValue value) {
    switch (value.type) {

        case CALF_VALUE_TYPE_INT:
            return value.int_value;

        case CALF_VALUE_TYPE_FLOAT:
            return (int) value.int_value;

        case CALF_VALUE_TYPE_BOOL:
            return (int) value.bool_value;

        default:
            return -1;
    }
}

static float calf_value_to_float(CalfValue value) {
    switch (value.type) {

        case CALF_VALUE_TYPE_INT:
            return (float) value.int_value;

        case CALF_VALUE_TYPE_FLOAT:
            return value.float_value;

        default:
            return 0.f;

    }
}

static char *calf_value_to_char(CalfValue value) {
    switch (value.type) {

        case CALF_VALUE_TYPE_STR:
            return value.str_value;

        default:
            return "";
    }
}

static bool calf_value_is_none(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_NONE;
}

static bool calf_value_is_bool(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_BOOL;
}

static bool calf_value_is_int(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_INT;
}

static bool calf_value_is_float(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_FLOAT;
}

static bool calf_value_is_string(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_STR;
}

static bool calf_value_is_func(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_FUNC;
}

static bool calf_value_is_interface_func(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_C_FUNC;
}

static bool calf_value_is_user_obj(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_USER_OBJ;
}

static bool calf_value_is_user_obj_with_type(CalfValue value, int id) {
    return calf_value_is_user_obj(value) && value.user_object_value->type == id;
}

static bool calf_value_is_error(CalfValue value) {
    return value.type == CALF_VALUE_TYPE_ERROR;
}


/*
 * Init a CalfScript structure. This called is required before any script can be called.
 */

bool calf_init(CalfScript *script);

/*
 * Allocate temporary memory
 */

static void *calf_script_alloc(CalfScript *script, int size) {
    void *result = script->current_heap;
    script->current_heap += size;
    return result;
}

/*
 * Set a value into the scripts global
 */
void calf_script_set_global(CalfScript *script, char *name, CalfValue value);

/*
 * Get a value from the scripts global
 */
CalfValue calf_script_get_global(CalfScript *script, char *name);

/*
 * Load a module from text.
 */

CalfModule *calf_load_module(CalfScript *script, char *text);

/*
 * Get a global object from a module
 */

CalfValue *calf_module_get_global(CalfModule *module, char *name);

/*
 * Api function to call a script function from C. It's NOT to be called internally by the interpreter
 * since it resets the script stack allocator
 */

CalfValue calf_execute(CalfScript *script, CalfModule *file, char *func_name, CalfValue *args, int args_count);

#endif //CALFSCRIPT_CALF_H
