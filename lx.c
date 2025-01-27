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

#include "lx.h"

static int lx_isspace(const char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int lx_isdigit(const char c) { return c >= '0' && c <= '9'; }
static int lx_isalpha(const char c) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_'; }
static int lx_isalnum(const char c) { return lx_isdigit(c) || lx_isalpha(c); }
static int lx_strlen(const char* str) { const char* c = str; while (*c) c++; return (int)(c - str); }
static long long lx_align(long long n, long long align) { return (n + align - 1) & -align; }

static int lx_word(const char* str) { const char* word = str; while (lx_isalnum(*word) || *word == '_') word++; return (int)(word - str); }
static double lx_parsenumber(const char* str, const char** end) {
    double val = 0, scale = 1;
    int dot = 0;

    while (lx_isdigit(*str) || (*str == '.' && !dot)) {
        if (dot) {
            scale /= 10.0;
            val = val + (*str - '0') * scale;
        } else {
            if (*str == '.') dot++;
            else val = val * 10.0 + (*str - '0');
        }
        str++;
    }

    if (end) *end = str;
    return val;
}

enum lx_Type { LX_FREE, LX_NIL, LX_NUMBER, LX_STRING, LX_SYMBOL, LX_LIST, LX_ENV, LX_FN, LX_CFN, LX_CALL, LX_EOF };
static const char* formats[] = { "<free>", "<nil>", "<number>", "<string>", "<symbol>", "<list>", "<env>", "<fn>", "<cfn>", "<call>", "<eof>" };

struct lx_Value {
    unsigned char mark : 1;
    unsigned char persist : 1;
    unsigned char type : 6;
    
    union {
        lx_Value* free;
        double number;
        struct {
            const char* start;
            int len;
        } symbol, string;
        struct {
            lx_Value* name;
            lx_Value* value;
            lx_Value* next;
        } env, list;
        struct {
            const char* arg_start;
            const char* body_start;
        } fn;
        struct {
            const char* args;
            lx_Cfn cfn;
        } cfn;
        struct {
            lx_Value* env;
            lx_Value* callable;
            lx_Value* last;
        } call;
    };
};

static lx_Value lx_nil_ = { .type = LX_NIL };
static lx_Value lx_eof = { .type = LX_EOF };
static lx_Value lx_zero = { .type = LX_NUMBER, .number = 0 };
static lx_Value lx_one = { .type = LX_NUMBER, .number = 1 };

struct lx_Ctx {
    lx_Printer printer;

    char* prog_start;
    char* prog_end;
    
    lx_Value* cell_start;
    lx_Value* cell_end;

    lx_Value* free_list;
    lx_Value* current;

    char format_buffer[LX_FORMAT_LEN];
};

static lx_Value* lx_alloc(lx_Ctx* ctx, unsigned char type, unsigned char mark) {
    if (ctx->free_list == 0) if (!lx_gc(ctx)) { return 0; }
    
    lx_Value* item = ctx->free_list;
    item->type = type; item->mark = mark;
    ctx->free_list = ctx->free_list->free;

    return item;
}

static lx_Value* lx_promote(lx_Ctx* ctx, lx_Value val) {
    lx_Value* new_val = lx_alloc(ctx, val.type, 0);
    *new_val = val;
    return new_val;
}

lx_Value* lx_nil(void) { return &lx_nil_; }
int ix_isnil(lx_Value* val) { return val->type == LX_NIL; }

lx_Value* lx_number(lx_Ctx* ctx, double number) { return lx_promote(ctx, (lx_Value) { .type = LX_NUMBER, .number = number }); }
int lx_isnumber(lx_Value* val) { return val->type == LX_NUMBER; }
double lx_getnumber(lx_Value* val) { return lx_isnumber(val) ? val->number : 0.0; }

lx_Value* lx_string(lx_Ctx* ctx, const char* str) { return lx_promote(ctx, (lx_Value) { .type = LX_STRING, .string = { .start = str, .len = lx_strlen(str)  }}); }
int lx_isstring(lx_Value* val) { return val->type == LX_STRING; }
const char* lx_getstring(lx_Value* val, int* length) { if (lx_isstring(val)) { *length = val->string.len; return val->string.start; } *length = 0; return 0; }

int lx_isenv(lx_Value* val) { return val->type == LX_ENV; }

lx_Value* lx_symbol(lx_Ctx* ctx, const char* str, int len) { return lx_promote(ctx, (lx_Value) { .type = LX_SYMBOL, .string = { .start = str, .len = len }}); }
int lx_issymbol(lx_Value* val) { return val->type == LX_SYMBOL; }

lx_Value* lx_fn(lx_Ctx* ctx, const char* args, const char* code) { return lx_promote(ctx, (lx_Value) { .type = LX_FN, .fn = { .arg_start = args, .body_start = code }}); }
int lx_isfn(lx_Value* val) { return val->type == LX_FN; }

lx_Value* lx_cfn(lx_Ctx* ctx, const char* args, lx_Cfn cfn) { return lx_promote(ctx, (lx_Value) { .type = LX_CFN, .cfn = { .args = args, .cfn = cfn } }); }
int lx_iscfn(lx_Value* val) { return val->type == LX_CFN; }

lx_Value* lx_list(lx_Ctx* ctx) { return lx_promote(ctx, (lx_Value) { .type = LX_LIST, .list = { .value = 0, .next = 0 } }); }
int lx_islist(lx_Value* val) { return val->type == LX_LIST; }
lx_Value* lx_getlist(lx_Value* val) { return lx_islist(val) ? val->list.value : &lx_nil_; }
lx_Value* lx_listnext(lx_Value* val) { return lx_islist(val) ? val->list.next : &lx_nil_; }

lx_Value* lx_listappend(lx_Ctx* ctx, lx_Value* list, lx_Value* item) {
    if (!lx_islist(list)) return lx_nil();
    if (!list->list.value) { list->list.value = item; return list; }
    while (list->list.next) list = list->list.next;
    list->list.next = lx_list(ctx);
    list->list.next->list.value = item;
    return list->list.next;
}

lx_Value* lx_listpop(lx_Value* list) {
    if (!lx_islist(list)) return lx_nil();
    lx_Value* prev = list;
    while (list->list.next) { prev = list; list = list->list.next; }
    prev->list.next = 0;
    if (prev == list) list->list.value = 0;
    return list;
}

lx_Ctx* lx_open(void* memory, unsigned long long prog_size, unsigned long long cell_size, lx_Printer printer) {
    for (unsigned long long i = 0; i < prog_size + cell_size; i++) ((char*)memory)[i] = 0;
    
    lx_Ctx* ctx = (lx_Ctx*)memory;
    ctx->printer = printer;

    ctx->prog_start = ((char*)memory + sizeof(lx_Ctx));
    ctx->prog_end = ((char*)memory + prog_size);

    ctx->cell_start = (lx_Value*)lx_align((long long)ctx->prog_end, sizeof(lx_Value));
    ctx->cell_end = (lx_Value*)((char*)memory + prog_size + cell_size);

    for (lx_Value* val = ctx->cell_start; val < ctx->cell_end - 1; val++) {
        val->type = LX_FREE;
        val->free = val + 1;
    }

    ctx->free_list = ctx->cell_start;
    (ctx->cell_start + ((ctx->cell_end - ctx->cell_start) - 1))->free = 0;

    return ctx;
}

int lx_cells(lx_Ctx* ctx) { return (int)(ctx->cell_end - ctx->cell_start); }

const char* lx_format(lx_Ctx* ctx, lx_Value* val) {
    switch (val->type) {
    case LX_NUMBER: {
            int len = 0;
            int int_part = (int)val->number;
            int_part = int_part > 0 ? int_part : -int_part;    

            if (val->number < 0) { ctx->format_buffer[len++] = '-'; }

            if (int_part == 0) ctx->format_buffer[len++] = '0';
            while (int_part > 0) {
                ctx->format_buffer[len++] = '0' + (int_part % 10);
                int_part = int_part / 10;
            }

            for (int i = val->number < 0, j = len - 1; i < j; i++, j--) {
                char c = ctx->format_buffer[i];
                ctx->format_buffer[i] = ctx->format_buffer[j];
                ctx->format_buffer[j] = c;
            }

            double frac_part = val->number - (int)val->number;
            frac_part = frac_part > 0 ? frac_part : -frac_part;    
            if (frac_part > 0.00001) {
                ctx->format_buffer[len++] = '.';

                int decimals = 0;
                while (frac_part > 0 && (decimals++) < 6) {
                    frac_part *= 10;
                    int_part = (int)frac_part;
                    frac_part -= int_part;
                    ctx->format_buffer[len++] = '0' + int_part;
                }
            }
            
            ctx->format_buffer[len] = 0;
            return ctx->format_buffer;
    }
    case LX_STRING: {
            int len = val->string.len > (LX_FORMAT_LEN - 1) ? (LX_FORMAT_LEN - 1) : val->string.len;
            for (int i = 0; i < len; ++i) ctx->format_buffer[i] = val->string.start[i];
            ctx->format_buffer[len] = 0;
            return ctx->format_buffer;
    }
    default: return formats[val->type];
    }
}

static void mark(lx_Value* v) {
    if (!v) return;

    v->mark = 1u;
    switch (v->type) {
    case LX_LIST: mark(v->list.next); mark(v->list.value); break;
    case LX_ENV: mark(v->env.next); mark(v->env.name); mark(v->env.value); break;
    case LX_CALL: mark(v->call.callable); mark(v->call.env); mark(v->call.last); break;
    }
}

int lx_gc(lx_Ctx* ctx) {
    mark(ctx->current);

    for (lx_Value* val = ctx->cell_start; val < ctx->cell_end; val++) if (val->persist) mark(val);

    int n_freed = 0;
    for (lx_Value* val = ctx->cell_start; val < ctx->cell_end; val++) {
        if (val->mark) val->mark = 0;
        else {
            val->type = LX_FREE;
            val->free = ctx->free_list;
            ctx->free_list = val;
            n_freed++;
        }
    }

    return n_freed;
}

static int lx_symbeq(lx_Value* a, lx_Value* b) {
    if (!a || !b) return 0;
    if (a->type != LX_SYMBOL || b->type != LX_SYMBOL) return 0;
    if (a->symbol.len != b->symbol.len) return 0;

    for (int i = 0; i < a->symbol.len; i++) if (a->symbol.start[i] != b->symbol.start[i]) return 0;
    
    return 1;
}

lx_Value* lx_makenv(lx_Ctx* ctx) {
    lx_Value* env = lx_alloc(ctx, LX_ENV, 1);
    env->env.name = 0;
    env->env.value = 0;
    env->env.next = 0;
    return env;
}

void lx_persist(lx_Value* val) { val->persist = 1; }

static lx_Value* lx_marktemp(lx_Value* v) { v->mark = 1; return v; }
static lx_Value* lx_releasetemp(lx_Value* v) { v->mark = 1; return v; }

void lx_setenv(lx_Ctx* ctx, lx_Value* env, lx_Value* name, lx_Value* value) {
    lx_Value* prev = env;
    while (env && env->env.name) {
        if (lx_symbeq(env->env.name, name)) {
            env->env.value = value;
            return;
        }

        prev = env;
        env = env->env.next;
    }

    if (!env) { env = lx_alloc(ctx, LX_ENV, 1); env->env.next = 0; }
    if (env != prev) prev->env.next = env;

    env->env.name = name;
    env->env.value = value;
}

void lx_setenvc(lx_Ctx* ctx, lx_Value* env, const char* name, lx_Value* value) {
    lx_Value* lx_name = lx_promote(ctx, (lx_Value) { .type = LX_SYMBOL, .symbol = { .start = name, .len = lx_strlen(name) } });
    lx_setenv(ctx, env, lx_marktemp(lx_name), lx_marktemp(value));
}


lx_Value* lx_getenv(lx_Value* env, lx_Value* name) {
    if (!env) return &lx_nil_;
    while (env) {
        if (lx_symbeq(env->env.name, name)) return env->env.value;
        env = env->env.next;
    }

    return &lx_nil_;
}

lx_Value* lx_getenvc(lx_Value* env, const char* name) {
    lx_Value lx_name = { .type = LX_SYMBOL, .symbol = { .start = name, .len = lx_strlen(name) } };
    return lx_getenv(env, &lx_name);
}

static lx_Value* lx_getcall(lx_Value* call, lx_Value* name) {
    lx_Value* result = lx_getenv(call->call.env, name);
    if (result->type != LX_NIL) return result;

    if (call->call.last == 0) return &lx_nil_;
    return lx_getcall(call->call.last, name);
}

int lx_truthy(lx_Value* val) {
    if (!val) return 0;
    if (val->type == LX_FREE || val->type == LX_NIL) return 0;
    if (val->type == LX_NUMBER && val->number == 0) return 0;
    return 1;
}



#define WRITE_END (end ? (*end = start, 0) : 0)

#define BUBBLE_EOF(name, expr) \
    lx_Value* ##name = (expr); if(##name->type == LX_EOF) { return &lx_eof; }

#define GET_AB                                                                \
BUBBLE_EOF(a, lx_marktemp(lx_eval(ctx, call, start, &next, 1, side_effects))) \
BUBBLE_EOF(b, lx_eval(ctx, call, next, end, 1, side_effects))                                        

#define ARITH_OP(op)                                                               \
GET_AB                                                                             \
if (a->type != b->type) { return &lx_nil_; }                                       \
if (a->type == LX_NUMBER) {                                                        \
    double result = a->number ##op b->number;                                      \
    return lx_promote(ctx, (lx_Value) { .type = LX_NUMBER, .number = result });    \
}

#define COMP_OP(op)                                         \
GET_AB                                                      \
if (a->type != b->type) { return &lx_zero; }                \
if (a->type == LX_NUMBER) {                                 \
    return a->number ##op b->number ? &lx_one : &lx_zero;   \
}

#define EAT_SPACE(str)                            \
    while (*(str) && lx_isspace(*(str))) (str)++; \
    if(!*(str)) return &lx_eof


#define PARSE_BODY(endchar, _call, afterparse)                              \
EAT_SPACE(start);                                                           \
if(*start == (endchar)) start++;                                            \
else do {                                                                   \
    lx_Value* value = lx_eval(ctx, (_call), start, &next, 1, side_effects); \
    result = value ? value : result;                                        \
    if (result->type == LX_EOF) return &lx_eof;                             \
    start = next;                                                           \
    EAT_SPACE(start);                                                       \
    (afterparse);                                                           \
} while (*start != (endchar)); start++

#define PARSE_ARG                                                                                        \
    EAT_SPACE(args);                                                                                     \
    lx_marktemp(next_call.call.env);                                                                     \
    lx_Value arg_name = { .type = LX_SYMBOL, .symbol = { .start = args, .len = lx_word(args) } };        \
    args += arg_name.symbol.len;                                                                         \
    BUBBLE_EOF(arg_value, lx_eval(ctx, call, start, &next, 1, side_effects))                             \
    lx_setenv(ctx, next_call.call.env, lx_marktemp(lx_promote(ctx, arg_name)), lx_marktemp(arg_value));  \
    start = next                                                                                        


lx_Value* lx_eval(lx_Ctx* ctx, lx_Value* call, const char* start, const char** end, int eval_symbol, int side_effects) {
    EAT_SPACE(start);

    WRITE_END;
    const char* next = start;
    lx_Value* result = &lx_nil_;
    
    switch (*start++) {
    case '~': WRITE_END; return &lx_nil_;
    case '"': {
        result = lx_promote(ctx, (lx_Value) { .type = LX_STRING, .string = { .start = start, .len = 0 } });        
        while (*start && *start != '"') start++;
        if (!*start) return &lx_eof;
        result->string.len = (int)(start - result->string.start); 
        start++; WRITE_END;
        return result;
    }
    case '+': { ARITH_OP(+) return &lx_nil_; }
    case '-': { ARITH_OP(-) return &lx_nil_; }
    case '*': { ARITH_OP(*) return &lx_nil_; }
    case '/': { ARITH_OP(/) return &lx_nil_; }
    case '<': { if (*start == '=') { start++; COMP_OP(<=) } else { COMP_OP(<) } return &lx_nil_; }
    case '>': { if (*start == '=') { start++; COMP_OP(>=) } else { COMP_OP(>) } return &lx_nil_; }
    case '&': { GET_AB return lx_truthy(a) && lx_truthy(b) ? &lx_one : &lx_zero; }
    case '|': { GET_AB return lx_truthy(a) || lx_truthy(b) ? &lx_one : &lx_zero; }
    case '!': { BUBBLE_EOF(a, lx_eval(ctx, call, start, end, 1, side_effects)) return lx_truthy(a) ? &lx_zero : &lx_one; }
    case '_': {
            BUBBLE_EOF(a, lx_eval(ctx, call, start, end, 1, side_effects))
            if (a->type != LX_NUMBER) { return &lx_nil_; }
            return lx_promote(ctx, (lx_Value) {.type = LX_NUMBER, .number = a->number > 0 ? (int)(a->number + 0.5) : (int)(a->number - 0.5) });
    }
    case '(': { PARSE_BODY(')', call, 0); WRITE_END; return result; }
    case '{': {
        lx_Value next_call = { .type = LX_CALL, .call = { .last = call, .env = 0, .callable = 0 } };
        ctx->current = &next_call;
        PARSE_BODY('}', &next_call, 0);
        ctx->current = ctx->current->call.last;
        WRITE_END; return next_call.call.env ? next_call.call.env : &lx_nil_;
    }
    case '[': {
        lx_Value* list_start = lx_list(ctx),* list_current = list_start;
        PARSE_BODY(']', call, list_current = lx_listappend(ctx, list_current, result));
        WRITE_END; return list_start;    
    }
    case '.': {
        BUBBLE_EOF(env, lx_marktemp(lx_eval(ctx, call, start, &next, 1, side_effects)))
        else if (env->type == LX_ENV) {
            BUBBLE_EOF(sym, lx_eval(ctx, call, next, end, 0, side_effects))
            return lx_getenv(lx_releasetemp(env), sym);
        }
        else if (lx_releasetemp(env)->type == LX_LIST) {
            BUBBLE_EOF(sym, lx_eval(ctx, call, next, end, 1, side_effects))
            if (sym->type != LX_NUMBER) { return &lx_nil_; }
            for (int i = 0; i < (int)sym->number; ++i) env = env->list.next ? env->list.next : 0;
            return env ? env->list.value : &lx_nil_;
        } else { BUBBLE_EOF(sym, lx_eval(ctx, call, next, end, 0, side_effects)) }
        return &lx_nil_;
    }
    case ':': {
        BUBBLE_EOF(env, lx_marktemp(lx_eval(ctx, call, start, &next, 1, side_effects)))

        if (side_effects) {
            if (env->type == LX_ENV) {
                BUBBLE_EOF(sym, lx_marktemp(lx_eval(ctx, call, next, &start, 0, side_effects)))
                BUBBLE_EOF(val, lx_marktemp(lx_eval(ctx, call, start, end, 1, side_effects)))
                lx_setenv(ctx, env, sym, val);
            }
            else if (env->type == LX_LIST) {
                BUBBLE_EOF(sym, lx_marktemp(lx_eval(ctx, call, next, &start, 1, side_effects)))
                BUBBLE_EOF(val, lx_marktemp(lx_eval(ctx, call, start, end, 1, side_effects)))
                if (sym->type != LX_NUMBER) { return &lx_nil_; }
                int n = (int)lx_releasetemp(sym)->number;
                for (int i = 0; i < n; ++i) env = env->list.next ? env->list.next : 0;
                if (lx_releasetemp(env)) { env->list.value = lx_releasetemp(val); }
            } else { BUBBLE_EOF(sym, lx_marktemp(lx_eval(ctx, call, next, &start, 0, side_effects))) BUBBLE_EOF(val, lx_eval(ctx, call, start, end, 1, side_effects)) } 
        } else { BUBBLE_EOF(sym, lx_marktemp(lx_eval(ctx, call, next, &start, 0, side_effects))) BUBBLE_EOF(val, lx_eval(ctx, call, start, end, 1, side_effects)) }
        return &lx_nil_;
    }
    case '=': {
        if (*start == '=') {
            start++;
            COMP_OP(==)
            if (a->type == LX_STRING) {
                if (a->string.len != b->string.len) { return &lx_zero; }
                for (int i = 0; i < b->string.len; i++) { if (a->string.start[i] != b->string.start[i]) { return &lx_zero; } }
                return &lx_one;
            }
            if (a == b) return &lx_one;
            return &lx_nil_;
        }
        BUBBLE_EOF(sym, lx_marktemp(lx_eval(ctx, call, start, &next, 0, side_effects)))
        BUBBLE_EOF(val, lx_marktemp(lx_eval(ctx, call, next, end, 1, side_effects)))

        if (side_effects) {
            if (!call->call.env) call->call.env = lx_makenv(ctx);
            lx_setenv(ctx, call->call.env, sym, val);
        }
        return &lx_nil_;    
    }
    case '`': while (*start && *start != '\n') { start++; } WRITE_END; return 0;
    case ',':
        BUBBLE_EOF(v, lx_eval(ctx, call, start, end, 1, side_effects))
        if (side_effects) ctx->printer(lx_format(ctx, v)); return &lx_nil_;
    case ';':
        if (side_effects) ctx->printer("\n"); WRITE_END; return &lx_nil_;
    case '@':
        BUBBLE_EOF(sym, lx_eval(ctx, call, start, end, 0, side_effects))
        return lx_getcall(call, sym);
    case '?': {
        BUBBLE_EOF(cond, lx_marktemp(lx_eval(ctx, call, start, &next, 1, side_effects)))
        BUBBLE_EOF(true_result, lx_marktemp(lx_eval(ctx, call, next, &start, 1, side_effects && lx_truthy(cond))))
        BUBBLE_EOF(false_result, lx_eval(ctx, call, start, end, 1, side_effects && !lx_truthy(cond)))
        return lx_truthy(lx_releasetemp(cond)) ? lx_releasetemp(true_result) : false_result;
    }
    case '#':
        BUBBLE_EOF(list, lx_marktemp(lx_eval(ctx, call, start, &next, 1, side_effects)))
        BUBBLE_EOF(item, lx_marktemp(lx_eval(ctx, call, next, end, 1, side_effects)))
        return side_effects ? lx_listappend(ctx, lx_releasetemp(list), lx_releasetemp(item)) : &lx_nil_;
    case '\\': {
        BUBBLE_EOF(list, lx_eval(ctx, call, start, &next, 1, side_effects))
        WRITE_END; return side_effects ? lx_listpop(list) : &lx_nil_;
    }
    case '%': {
        BUBBLE_EOF(list, lx_marktemp(lx_eval(ctx, call, start, &next, 1, side_effects)))
        BUBBLE_EOF(name, (lx_eval(ctx, call, next, end, 0, side_effects)))

        const char* body_start = *end;
        if (!list->list.value) lx_eval(ctx, call, body_start, end, 0, 0);
        while (list && list->list.value) {
            lx_setenv(ctx, call->call.env, lx_marktemp(name), lx_marktemp(list)->list.value);
            result = lx_eval(ctx, call, body_start, end, 1, side_effects);
            list = lx_listnext(list);
        }
        return result;
    }
    case '^':
        const char* cond_start = start;
        BUBBLE_EOF(cond, lx_marktemp(lx_eval(ctx, call, cond_start, &next, 1, side_effects)))
        const char* body_start = next;
        if (!lx_truthy(cond)) lx_eval(ctx, call, body_start, end, 0, 0);
        while (lx_truthy(cond)) {
            lx_releasetemp(result);
            result = lx_marktemp(lx_eval(ctx, call, body_start, end, 1, side_effects));
            lx_releasetemp(cond);
            cond = lx_marktemp(lx_eval(ctx, call, cond_start, &next, 1, side_effects));
            if (!side_effects) break;
        }
        return result;
    case '$':
        BUBBLE_EOF(val, lx_eval(ctx, call, start, end, 1, side_effects))
        int len = -1;
        if (val->type == LX_STRING) { len = val->string.len; }
        else if (val->type == LX_ENV) { if (val->env.value) { len++; } while (val) { len++; val = val->env.next; } }
        else if (val->type == LX_LIST) { if (val->list.value) { len++; } while (val) { len++; val = val->list.next; } }
        return len == -1 ? &lx_nil_ : lx_promote(ctx, (lx_Value) { .type = LX_NUMBER, .number = len });
    case '\'':
        result = lx_alloc(ctx, LX_FN, 1);
        EAT_SPACE(start);
        result->fn.arg_start = start;
        if (*start == '(') {
            while (*start && *start != ')') start++; start++;
            if (!*start) return &lx_eof;
        } else {
            EAT_SPACE(start);
            start += lx_word(start);
        }
        EAT_SPACE(start);
        result->fn.body_start = start;
        lx_eval(ctx, call, start, end, 0, 0); // dry run to move ptr
        return result;
    default:
        start--;
        if (lx_isdigit(*start)) return lx_promote(ctx, (lx_Value) { .type = LX_NUMBER, .number = lx_parsenumber(start, end) });
        if (lx_isalpha(*start)) {
            lx_Value name = { .type = LX_SYMBOL, .symbol = { .start = start, .len = lx_word(start) } };
            start += name.symbol.len;
            
            lx_Value* result;
            if (eval_symbol) {
                result = lx_getcall(call, &name);
                if (result->type == LX_FN || result->type == LX_CFN) {
                    const char* args = result->type == LX_FN ? result->fn.arg_start : result->cfn.args;

                    lx_Value next_call = { .type = LX_CALL, .call = { .last = call, .env = lx_makenv(ctx), .callable = result } };
                    
                    if (*args == '(') { args++; while (*args && *args != ')') { PARSE_ARG; }
                    } else { PARSE_ARG; }

                    ctx->current = &next_call;
                    if (side_effects && result->type == LX_FN) result = lx_eval(ctx, &next_call, result->fn.body_start, end, 1, side_effects);
                    else if (side_effects) result = result->cfn.cfn(ctx, next_call.call.env); 
                    ctx->current = ctx->current->call.last;
                    lx_releasetemp(next_call.call.env);
                }
            }
            else result = lx_promote(ctx, name);
            WRITE_END; return result;
        }
    }

    return &lx_eof;
}

lx_Value* lx_run(lx_Ctx* ctx, lx_Value* env, const char* code) {
    int len = lx_strlen(code) + 1;

    const char* prog_start = ctx->prog_start;
    const char* prog_current = ctx->prog_start;
    const char* prog_next = ctx->prog_start;
    
    if (ctx->prog_end - ctx->prog_start <= len) return &lx_nil_;
    for (int i = 0; i < len; i++) ctx->prog_start[i] = code[i];
    ctx->prog_start[len] = 0;
    ctx->prog_start += len;

    lx_Value call = {
        .type = LX_CALL, .call = { .last = ctx->current, .callable = 0, .env = env }
    };
    lx_Value* result = &lx_nil_;
    ctx->current = &call;
    while (prog_current < prog_start + len) {
        lx_Value* value = lx_eval(ctx, &call, prog_current, &prog_next, 1, 1);
        if (value && value->type == LX_EOF) break;
        result = value ? value : result;
        prog_current = prog_next;
    }
    ctx->current = call.call.last;

    return result;
}

#ifdef LX_BUILD_CLI

#define _CRT_SECURE_NO_WARNINGS 
#define LX_MEM_SIZE (1024*1024*8)

#include <stdio.h>
#include <stdlib.h>

void lxcli_print(const char* msg) {
    printf("%s", msg);
}

char* lxcli_readfile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    return string;
}

lx_Value* lxcli_cells(lx_Ctx* ctx, lx_Value* env) {
    return lx_number(ctx, lx_cells(ctx));
}

static char lxcli_loadbuf[1024];
lx_Value* lxcli_load(lx_Ctx* ctx, lx_Value* env) {
    lx_Value* path = lx_getenvc(env, "path");
    if (!lx_isstring(path)) return lx_nil();

    int len;
    const char* cpath = lx_getstring(path, &len);

    for (int i = 0; i < len; i++) lxcli_loadbuf[i] = cpath[i];
    lxcli_loadbuf[len] = 0;
    
    char* source = lxcli_readfile(lxcli_loadbuf);
    if (!source) return lx_nil();
    
    lx_Value* new_env = lx_makenv(ctx);
    lx_persist(new_env);
    lx_run(ctx, new_env, source);
    free(source);

    return new_env;
}

int main(int argc, char** argv) {
    char* lx_memory = malloc(LX_MEM_SIZE);
    lx_Ctx* ctx = lx_open(lx_memory, LX_MEM_SIZE / 2, LX_MEM_SIZE / 2, lxcli_print);
    lx_Value* env = lx_makenv(ctx); lx_persist(env);
    lx_setenvc(ctx, env, "cells", lx_cfn(ctx, "()", lxcli_cells));
    lx_setenvc(ctx, env, "load", lx_cfn(ctx, "path", lxcli_load));

    if (argc == 1) {
        printf("lx " LX_VERSION " (:q to quit)\n");
        printf("Cell count: %d\n", lx_cells(ctx));

        char input_buffer[1024] = { 0 };
        while (input_buffer[0] != ':' || input_buffer[1] != 'q' || input_buffer[2] != 0) {
            printf(">> ");
            int _ = scanf("%1023[^\n]%*c", input_buffer);

            lx_Value* val = lx_run(ctx, env, input_buffer);
            printf("%s\n", lx_format(ctx, val));
        }
    } else {
        char* source = lxcli_readfile(argv[1]);
        if (!source) { printf("Failed to read source file '%s'!\n", argv[1]); return 1; }
        lx_run(ctx, env, source);
        free(source);
    }
    
    return 0;
}

#endif

