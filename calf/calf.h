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
    CALF_VALUE_TYPE_OBJ = 6,
    CALF_VALUE_TYPE_FUNC = 7,
    CALF_VALUE_TYPE_C_FUNC = 8,
    CALF_VALUE_TYPE_USER_OBJ = 9,
    CALF_VALUE_TYPE_ERROR = 10
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

typedef CalfValue (*CalfGetAttrFunc)(CalfScript *, char *attr_name);

typedef CalfValue (*CalfSetAttrFunc)(CalfScript *, char *attr_name, CalfValue value);

typedef CalfValue (*CalfFuncCall)(CalfScript *, CalfValue *, int);


/*
 *  PUBLIC CALF API
 */

/*
 * Init a CalfScript structure. This called is required before any script can be called.
 */
bool calf_init(CalfScript *script);

/*
 * Set a user object into the scripts global
 */
void
calf_script_set_user_object(CalfScript *script, char *name, void *obj, CalfGetAttrFunc get_attr, CalfSetAttrFunc func);

/*
 * Load a module from text.
 */
CalfModule *calf_load_module(CalfScript *script, char *text);

/*
 * Api function to call a script function from C. It's NOT to be called internally by the interpreter
 * since it resets the script heap allocator
 */
CalfValue calf_execute(CalfScript *script, CalfModule *file, char *func_name, CalfValue *args, int args_count);

#endif //CALFSCRIPT_CALF_H
