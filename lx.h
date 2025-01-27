/*
MIT License

    Copyright (c) 2025 Beariish

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

#pragma once

#define LX_VERSION "1.0.0"

/* The size of the internal string format buffer, in characters */
#define LX_FORMAT_LEN 64

typedef void (*lx_Printer)(const char*);

typedef struct lx_Ctx lx_Ctx;
typedef struct lx_Value lx_Value;

typedef lx_Value* (*lx_Cfn)(lx_Ctx* ctx, lx_Value* env);

/* Creates a lx context inside the preallocated memory arena. prog_size is rom, cell_size is ram */
lx_Ctx* lx_open(void* memory, unsigned long long prog_size, unsigned long long cell_size, lx_Printer printer);

/* Returns the total number of cells available to the context */
int lx_cells(lx_Ctx* ctx);

/* Evaluate `code` given the environment `env` (or NULL) */
lx_Value* lx_run(lx_Ctx* ctx, lx_Value* env, const char* code);

/* Run a garbage collection cycle on the context's cell memory, returning the number of cells freed */
int lx_gc(lx_Ctx* ctx);

/* Marks a value as persistent, stopping it from being garbage collected even with no live references */
void lx_persist(lx_Value* val);

/* Create a new environment */
lx_Value* lx_makenv(lx_Ctx* ctx);

/* Check if a value is an environment */
int lx_isenv(lx_Value* val);

/* Set key `name` in `env` to `value` */
void lx_setenv(lx_Ctx* ctx, lx_Value* env, lx_Value* name, lx_Value* value);

/* Set key `name` in `env` to `value`. Name is expected to live for the duration of the program */
void lx_setenvc(lx_Ctx* ctx, lx_Value* env, const char* name, lx_Value* value);

/* Get key `name` from `env` */
lx_Value* lx_getenv(lx_Value* env, lx_Value* name);

/* Get key `name` from `env` */
lx_Value* lx_getenvc(lx_Value* env, const char* name);

/* Format `val` into a zero-terminated string, return value is only valid until the next call to format */
const char* lx_format(lx_Ctx* ctx, lx_Value* val);

/* Return whether value `val` is truthy */
int lx_truthy(lx_Value* val);

/* Make a nil value or check if a value is nil */
lx_Value* lx_nil(void);
int ix_isnil(lx_Value* val);

/* Make, check, and retrieve number values */
lx_Value* lx_number(lx_Ctx* ctx, double number);
int lx_isnumber(lx_Value* val);
double lx_getnumber(lx_Value* val);

/* Make, check, and retrieve string values - if a string is made outside of program code, the caller is responsible for the lifetime of the character data */
lx_Value* lx_string(lx_Ctx* ctx, const char* str);
int lx_isstring(lx_Value* val);
const char* lx_getstring(lx_Value* val, int* length);

/* Make symbol values - the source string is expected to outlive the usage */
lx_Value* lx_symbol(lx_Ctx* ctx, const char* str, int len);
int lx_issymbol(lx_Value* val);

/* Make and check function values - arg and code strings should outlive the context */
lx_Value* lx_fn(lx_Ctx* ctx, const char* args, const char* code);
int lx_isfn(lx_Value* val);

/* Make and check native function values - args string should outlive the context */
lx_Value* lx_cfn(lx_Ctx* ctx, const char* args, lx_Cfn cfn);
int lx_iscfn(lx_Value* val);

/* Make an empty list */
lx_Value* lx_list(lx_Ctx* ctx);

/* Check if `val` is a list */
int lx_islist(lx_Value* val);

/* Get the item at the current list location */
lx_Value* lx_getlist(lx_Value* val);

/* Return the next cell in the list, or NULL if at the end */
lx_Value* lx_listnext(lx_Value* val);

/* Append `item` to the end of `list` */
lx_Value* lx_listappend(lx_Ctx* ctx, lx_Value* list, lx_Value* item);

/* Remove the last item from `list` */
lx_Value* lx_listpop(lx_Value* list);