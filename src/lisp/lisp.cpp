/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"

#define TYPE_CHECKING 1

#include "lisp.h"
#include "lisp_gc.h"
#include "funcs.h"

#include "status.h"
#include "specs.h"
#include "cache.h"
#include "dev.h"

/* To bypass the whole garbage collection issue of lisp I am going to have
 * separate spaces where lisp objects can reside.  Compiled code and gloabal
 * variables will reside in permanant space.  Eveything else will reside in
 * tmp space which gets thrown away after completion of eval.  system
 * functions reside in permant space. */
LSpace LSpace::Tmp, LSpace::Perm, LSpace::Gc;

/* Normally set to Tmp, unless compiling or other needs. */
LSpace *LSpace::Current;

bFILE *current_print_file = NULL;

LSymbol *LSymbol::root = NULL;
size_t LSymbol::count = 0;

int print_level = 0, trace_level = 0, trace_print_level = 1000;
int total_user_functions;

void lbreak(char const *format, ...)
{
    char st[300];
    va_list ap;
    va_start(ap, format);
    vsprintf(st, format, ap);
    va_end(ap);
    throw std::runtime_error(st);
}

void need_perm_space(char const *why)
{
    if (LSpace::Current != &LSpace::Perm && LSpace::Current != &LSpace::Gc)
    {
        lbreak("%s : action requires permanant space\n", why);
        exit(EXIT_SUCCESS);
    }
}

void *LSpace::Mark()
{
    return m_free;
}

void LSpace::Restore(void *val)
{
    m_free = (uint8_t *)val;
}

size_t LSpace::GetFree()
{
    size_t used = m_free - m_data;
    return m_size > used ? m_size - used : 0;
}

void *LSpace::Alloc(size_t size)
{
    // Align allocation
    size = (size + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);

    // Collect garbage if necessary
    if (size > GetFree())
    {
        if (this == &LSpace::Perm || this == &LSpace::Tmp)
            Lisp::CollectSpace(this, 0);

        if (size > GetFree())
            Lisp::CollectSpace(this, 1);

        if (size > GetFree())
        {
            lbreak("lisp: cannot find %d bytes in %s\n", size, m_name);
            exit(EXIT_SUCCESS);
        }
    }

    void *ret = m_free;
    m_free += size;
    return ret;
}

void *eval_block(void *list)
{
    PtrRef r1(list);
    void *ret = NULL;
    while (list)
    {
        ret = CAR(list)->Eval();
        list = CDR(list);
    }
    return ret;
}

LArray *LArray::Create(size_t len, void *rest)
{
    PtrRef r11(rest);
    size_t size = sizeof(LArray) + (len - 1) * sizeof(LObject *);
    if (size < sizeof(LRedirect))
        size = sizeof(LRedirect);

    LArray *p = (LArray *)LSpace::Current->Alloc(size);
    p->m_type = L_1D_ARRAY;
    p->m_len = len;
    LObject **data = p->GetData();
    memset(data, 0, len * sizeof(LObject *));
    PtrRef r1(p);

    if (rest)
    {
        LObject *x = CAR(rest)->Eval();
        if (x == colon_initial_contents)
        {
            x = CAR(CDR(rest))->Eval();
            data = p->GetData();
            for (size_t i = 0; i < len; i++, x = CDR(x))
            {
                if (!x)
                {
                    ((LObject *)rest)->Print();
                    lbreak("(make-array) incorrect list length\n");
                    exit(EXIT_SUCCESS);
                }
                data[i] = (LObject *)CAR(x);
            }
            if (x)
            {
                ((LObject *)rest)->Print();
                lbreak("(make-array) incorrect list length\n");
                exit(EXIT_SUCCESS);
            }
        }
        else if (x == colon_initial_element)
        {
            x = CAR(CDR(rest))->Eval();
            data = p->GetData();
            for (size_t i = 0; i < len; i++)
                data[i] = (LObject *)x;
        }
        else
        {
            ((LObject *)x)->Print();
            lbreak("Bad option argument to make-array\n");
            exit(EXIT_SUCCESS);
        }
    }

    return p;
}

LFixedPoint *LFixedPoint::Create(int32_t x)
{
    size_t size = std::max(sizeof(LFixedPoint), sizeof(LRedirect));

    LFixedPoint *p = (LFixedPoint *)LSpace::Current->Alloc(size);
    p->m_type = L_FIXED_POINT;
    p->m_fixed = x;
    return p;
}

LObjectVar *LObjectVar::Create(int index)
{
    size_t size = std::max(sizeof(LObjectVar), sizeof(LRedirect));

    LObjectVar *p = (LObjectVar *)LSpace::Current->Alloc(size);
    p->m_type = L_OBJECT_VAR;
    p->m_index = index;
    return p;
}

LPointer *LPointer::Create(void *addr)
{
    if (addr == NULL)
        return NULL;
    size_t size = std::max(sizeof(LPointer), sizeof(LRedirect));

    LPointer *p = (LPointer *)LSpace::Current->Alloc(size);
    p->m_type = L_POINTER;
    p->m_addr = addr;
    return p;
}

LChar *LChar::Create(uint16_t ch)
{
    size_t size = std::max(sizeof(LChar), sizeof(LRedirect));

    LChar *c = (LChar *)LSpace::Current->Alloc(size);
    c->m_type = L_CHARACTER;
    c->m_ch = ch;
    return c;
}

struct LString *LString::Create(char const *string)
{
    LString *s = Create(strlen(string) + 1);
    strcpy(s->m_str, string);
    return s;
}

struct LString *LString::Create(char const *string, int length)
{
    LString *s = Create(length + 1);
    memcpy(s->m_str, string, length);
    s->m_str[length] = 0;
    return s;
}

struct LString *LString::Create(int length)
{
    size_t size = std::max(sizeof(LString) + length - 1, sizeof(LRedirect));

    LString *s = (LString *)LSpace::Current->Alloc(size);
    s->m_type = L_STRING;
    s->m_str[0] = '\0';
    return s;
}

LUserFunction *new_lisp_user_function(LList *arg_list, LList *block_list)
{
    PtrRef r1(arg_list), r2(block_list);

    size_t size = std::max(sizeof(LUserFunction), sizeof(LRedirect));

    LUserFunction *lu = (LUserFunction *)LSpace::Current->Alloc(size);
    lu->m_type = L_USER_FUNCTION;
    lu->arg_list = arg_list;
    lu->block_list = block_list;
    return lu;
}

LSysFunction *new_lisp_sys_function(int min_args, int max_args, int fun_number)
{
    size_t size = std::max(sizeof(LSysFunction), sizeof(LRedirect));

    // System functions should reside in permanant space
    LSysFunction *ls = LSpace::Current == &LSpace::Gc ? (LSysFunction *)LSpace::Gc.Alloc(size)
                                                      : (LSysFunction *)LSpace::Perm.Alloc(size);
    ls->m_type = L_SYS_FUNCTION;
    ls->min_args = min_args;
    ls->max_args = max_args;
    ls->fun_number = fun_number;
    return ls;
}

LSysFunction *new_lisp_c_function(int min_args, int max_args, int fun_number)
{
    LSysFunction *ls = new_lisp_sys_function(min_args, max_args, fun_number);
    ls->m_type = L_C_FUNCTION;
    return ls;
}

LSysFunction *new_lisp_c_bool(int min_args, int max_args, int fun_number)
{
    LSysFunction *ls = new_lisp_sys_function(min_args, max_args, fun_number);
    ls->m_type = L_C_BOOL;
    return ls;
}

LSysFunction *new_user_lisp_function(int min_args, int max_args, int fun_number)
{
    LSysFunction *ls = new_lisp_sys_function(min_args, max_args, fun_number);
    ls->m_type = L_L_FUNCTION;
    return ls;
}

LNumber *LNumber::Create(long num)
{
    size_t size = std::max(sizeof(LNumber), sizeof(LRedirect));

    LNumber *n = (LNumber *)LSpace::Current->Alloc(size);
    n->m_type = L_NUMBER;
    n->m_num = num;
    return n;
}

LList *LList::Create()
{
    size_t size = std::max(sizeof(LList), sizeof(LRedirect));

    LList *c = (LList *)LSpace::Current->Alloc(size);
    c->m_type = L_CONS_CELL;
    c->m_car = NULL;
    c->m_cdr = NULL;
    return c;
}

char *lerror(char const *loc, char const *cause)
{
    int lines;
    if (loc)
    {
        for (lines = 0; *loc && lines < 10; loc++)
        {
            if (*loc == '\n')
                lines++;
            printf("%c", *loc);
        }
        printf("\nPROGRAM LOCATION : \n");
    }
    if (cause)
        printf("ERROR MESSAGE : %s\n", cause);
    lbreak("");
    exit(EXIT_SUCCESS);
    return NULL;
}

void *nth(int num, void *list)
{
    if (num < 0)
    {
        lbreak("NTH: %d is not a nonnegative fixnum and therefore not a valid index\n", num);
        exit(EXIT_FAILURE);
    }

    while (list && num)
    {
        list = CDR(list);
        num--;
    }
    if (!list)
        return NULL;
    else
        return CAR(list);
}

void *lpointer_value(void *lpointer)
{
    if (!lpointer)
        return NULL;
#ifdef TYPE_CHECKING
    else if (item_type(lpointer) != L_POINTER)
    {
        ((LObject *)lpointer)->Print();
        lbreak(" is not a pointer\n");
        exit(EXIT_SUCCESS);
    }
#endif
    return ((LPointer *)lpointer)->m_addr;
}

int32_t lnumber_value(void *lnumber)
{
    switch (item_type(lnumber))
    {
    case L_NUMBER:
        return ((LNumber *)lnumber)->m_num;
    case L_FIXED_POINT:
        return ((LFixedPoint *)lnumber)->m_fixed >> 16;
    case L_STRING:
        return (uint8_t)*lstring_value(lnumber);
    case L_CHARACTER:
        return ((LChar *)lnumber)->m_ch;
    default:
        ((LObject *)lnumber)->Print();
        lbreak(" is not a number\n");
        exit(EXIT_SUCCESS);
    }
    return 0;
}

char *LString::GetString()
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_STRING)
    {
        Print();
        lbreak(" is not a string\n");
        exit(EXIT_SUCCESS);
    }
#endif
    return m_str;
}

void *lisp_atom(void *i)
{
    if (item_type(i) == (ltype)L_CONS_CELL)
        return NULL;
    else
        return true_symbol;
}

LObject *lcdr(void *c)
{
    if (!c)
        return NULL;
    else if (item_type(c) == (ltype)L_CONS_CELL)
        return ((LList *)c)->m_cdr;
    else
        return NULL;
}

LObject *lcar(void *c)
{
    if (!c)
        return NULL;
    else if (item_type(c) == (ltype)L_CONS_CELL)
        return ((LList *)c)->m_car;
    else
        return NULL;
}

uint16_t LChar::GetValue()
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_CHARACTER)
    {
        Print();
        lbreak("is not a character\n");
        exit(EXIT_SUCCESS);
    }
#endif
    return m_ch;
}

long lfixed_point_value(void *c)
{
    switch (item_type(c))
    {
    case L_NUMBER:
        return ((LNumber *)c)->m_num << 16;
        break;
    case L_FIXED_POINT:
        return (((LFixedPoint *)c)->m_fixed);
        break;
    default: {
        ((LObject *)c)->Print();
        lbreak(" is not a number\n");
        exit(EXIT_SUCCESS);
    }
    }
    return 0;
}

void *lisp_eq(void *n1, void *n2)
{
    if (!n1 && !n2)
        return true_symbol;
    else if ((n1 && !n2) || (n2 && !n1))
        return NULL;
    {
        int t1 = *((ltype *)n1), t2 = *((ltype *)n2);
        if (t1 != t2)
            return NULL;
        else if (t1 == L_NUMBER)
        {
            if (((LNumber *)n1)->m_num == ((LNumber *)n2)->m_num)
                return true_symbol;
            else
                return NULL;
        }
        else if (t1 == L_CHARACTER)
        {
            if (((LChar *)n1)->m_ch == ((LChar *)n2)->m_ch)
                return true_symbol;
            else
                return NULL;
        }
        else if (n1 == n2)
            return true_symbol;
        else if (t1 == L_POINTER)
            if (n1 == n2)
                return true_symbol;
    }
    return NULL;
}

LObject *LArray::Get(int x)
{
#ifdef TYPE_CHECKING
    if (m_type != L_1D_ARRAY)
    {
        Print();
        lbreak("is not an array\n");
        exit(EXIT_SUCCESS);
    }
#endif
    if (x >= (int)m_len || x < 0)
    {
        lbreak("array reference out of bounds (%d)\n", x);
        exit(EXIT_SUCCESS);
    }
    return m_data[x];
}

void *lisp_equal(void *n1, void *n2)
{
    if (!n1 && !n2) // if both nil, then equal
        return true_symbol;

    if (!n1 || !n2) // one nil, nope
        return NULL;

    int t1 = item_type(n1), t2 = item_type(n2);
    if (t1 != t2)
        return NULL;

    switch (t1)
    {
    case L_STRING:
        if (!strcmp(lstring_value(n1), lstring_value(n2)))
            return true_symbol;
        return NULL;
    case L_CONS_CELL:
        while (n1 && n2) // loop through the list and compare each element
        {
            if (!lisp_equal(CAR(n1), CAR(n2)))
                return NULL;
            n1 = CDR(n1);
            n2 = CDR(n2);
            if (n1 && *((ltype *)n1) != L_CONS_CELL)
                return lisp_equal(n1, n2);
        }
        if (n1 || n2)
            return NULL; // if one is longer than the other
        return true_symbol;
    default:
        return lisp_eq(n1, n2);
    }
}

int32_t lisp_cos(int32_t x)
{
    return lround(cosf(x * (M_PI / 180.0)) * 0xffff);
}

int32_t lisp_sin(int32_t x)
{
    return lround(sinf(x * (M_PI / 180.0)) * 0xffff);
}

int32_t lisp_atan2(int32_t dy, int32_t dx)
{
    return (atan2f(-dy, -dx) + M_PI) * 180.0 / M_PI;
}

LSymbol *LSymbol::Find(char const *name)
{
    LSymbol *p = root;
    while (p)
    {
        int cmp = strcmp(name, p->m_name->GetString());
        if (cmp == 0)
            return p;
        p = (cmp < 0) ? p->m_left : p->m_right;
    }
    return NULL;
}

LSymbol *LSymbol::FindOrCreate(char const *name)
{
    LSymbol *p = root;
    LSymbol **parent = &root;
    while (p)
    {
        int cmp = strcmp(name, p->m_name->GetString());
        if (cmp == 0)
            return p;
        parent = (cmp < 0) ? &p->m_left : &p->m_right;
        p = *parent;
    }

    // Make sure all symbols get defined in permanant space
    LSpace *sp = LSpace::Current;
    if (LSpace::Current != &LSpace::Gc)
        LSpace::Current = &LSpace::Perm;

    // These permanent objects cannot be GCed, so malloc() them
    p = (LSymbol *)malloc(sizeof(LSymbol));
    p->m_type = L_SYMBOL;
    p->m_name = LString::Create(name);

    // If constant, set the value to ourself
    p->m_value = (name[0] == ':') ? p : l_undefined;
    p->m_function = l_undefined;
#ifdef L_PROFILE
    p->time_taken = 0;
#endif
    p->m_left = p->m_right = NULL;
    *parent = p;
    count++;

    LSpace::Current = sp;
    return p;
}

static void DeleteAllSymbols(LSymbol *root)
{
    if (root)
    {
        DeleteAllSymbols(root->m_left);
        DeleteAllSymbols(root->m_right);
        free(root);
    }
}

LList *LList::Assoc(LObject *item)
{
    LList *list = this;
    while (list && item_type(list) == L_CONS_CELL && item_type(CAR(list)) == L_CONS_CELL)
    {
        if (lisp_eq(CAR(CAR(list)), item))
            return (LList *)CAR(list);
        list = (LList *)CDR(list);
    }

    return NULL;
}

size_t LList::GetLength()
{
    size_t ret = 0;

#ifdef TYPE_CHECKING
    if (item_type(this) != (ltype)L_CONS_CELL)
    {
        Print();
        lbreak(" is not a sequence\n");
        exit(EXIT_SUCCESS);
    }
#endif

    for (LObject *p = this; p; p = CDR(p))
        ret++;
    return ret;
}

void *pairlis(void *list1, void *list2, void *list3)
{
    if (item_type(list1) != (ltype)L_CONS_CELL || item_type(list1) != item_type(list2))
        return NULL;

    void *ret = NULL;
    size_t l1 = ((LList *)list1)->GetLength();
    size_t l2 = ((LList *)list2)->GetLength();

    if (l1 != l2)
    {
        ((LObject *)list1)->Print();
        ((LObject *)list2)->Print();
        lbreak("... are not the same length (pairlis)\n");
        exit(EXIT_SUCCESS);
    }
    if (l1 != 0)
    {
        LList *first = NULL, *last = NULL, *cur = NULL;
        LObject *tmp;
        PtrRef r1(first), r2(last), r3(cur);
        while (list1)
        {
            cur = LList::Create();
            if (!first)
                first = cur;
            if (last)
                last->m_cdr = cur;
            last = cur;

            LList *cell = LList::Create();
            tmp = (LObject *)lcar(list1);
            cell->m_car = tmp;
            tmp = (LObject *)lcar(list2);
            cell->m_cdr = tmp;
            cur->m_car = cell;

            list1 = ((LList *)list1)->m_cdr;
            list2 = ((LList *)list2)->m_cdr;
        }
        cur->m_cdr = (LObject *)list3;
        ret = first;
    }
    else
        ret = NULL;
    return ret;
}

void LSymbol::SetFunction(LObject *function)
{
    m_function = function;
}

LSymbol *add_sys_function(char const *name, short min_args, short max_args, SysFunc number)
{
    need_perm_space("add_sys_function");
    LSymbol *s = LSymbol::FindOrCreate(name);
    if (s->m_function != l_undefined)
    {
        lbreak("add_sys_fucntion -> symbol %s already has a function\n", name);
        exit(EXIT_SUCCESS);
    }
    else
        s->m_function = new_lisp_sys_function(min_args, max_args, static_cast<int>(number));
    return s;
}

LSymbol *add_c_object(void *symbol, int index)
{
    need_perm_space("add_c_object");
    LSymbol *s = (LSymbol *)symbol;
    if (s->m_value != l_undefined)
    {
        lbreak("add_c_object -> symbol %s already has a value\n", lstring_value(s->GetName()));
        exit(EXIT_SUCCESS);
    }
    else
        s->m_value = LObjectVar::Create(index);
    return NULL;
}

LSymbol *add_c_function(char const *name, short min_args, short max_args, CFunc number)
{
    total_user_functions++;
    need_perm_space("add_c_function");
    LSymbol *s = LSymbol::FindOrCreate(name);
    if (s->m_function != l_undefined)
    {
        lbreak("add_sys_fucntion -> symbol %s already has a function\n", name);
        exit(EXIT_SUCCESS);
    }
    else
        s->m_function = new_lisp_c_function(min_args, max_args, static_cast<int>(number));
    return s;
}

LSymbol *add_c_bool_fun(char const *name, short min_args, short max_args, CFunc number)
{
    total_user_functions++;
    need_perm_space("add_c_bool_fun");
    LSymbol *s = LSymbol::FindOrCreate(name);
    if (s->m_function != l_undefined)
    {
        lbreak("add_sys_fucntion -> symbol %s already has a function\n", name);
        exit(EXIT_SUCCESS);
    }
    else
        s->m_function = new_lisp_c_bool(min_args, max_args, static_cast<int>(number));
    return s;
}

LSymbol *add_lisp_function(char const *name, short min_args, short max_args, LispFunc number)
{
    total_user_functions++;
    need_perm_space("add_c_bool_fun");
    LSymbol *s = LSymbol::FindOrCreate(name);
    if (s->m_function != l_undefined)
    {
        lbreak("add_sys_fucntion -> symbol %s already has a function\n", name);
        exit(EXIT_SUCCESS);
    }
    else
        s->m_function = new_user_lisp_function(min_args, max_args, static_cast<int>(number));
    return s;
}

void skip_c_comment(char const *&s)
{
    s += 2;
    while (*s && (*s != '*' || *(s + 1) != '/'))
    {
        if (*s == '/' && *(s + 1) == '*')
            skip_c_comment(s);
        else
            s++;
    }
    if (*s)
        s += 2;
}

long str_token_len(char const *st)
{
    long x = 1;
    while (*st && (*st != '"' || st[1] == '"'))
    {
        if (*st == '\\' || *st == '"')
            st++;
        st++;
        x++;
    }
    return x;
}

int read_ltoken(char const *&s, char *buffer)
{
    // skip space
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == 26)
        s++;
    if (*s == ';') // comment
    {
        while (*s && *s != '\n' && *s != '\r' && *s != 26)
            s++;
        return read_ltoken(s, buffer);
    }
    else if (*s == '/' && *(s + 1) == '*') // c style comment
    {
        skip_c_comment(s);
        return read_ltoken(s, buffer);
    }
    else if (*s == 0)
        return 0;
    else if (*s == ')' || *s == '(' || *s == '\'' || *s == '`' || *s == ',' || *s == 26)
    {
        *(buffer++) = *(s++);
        *buffer = 0;
    }
    else if (*s == '"') // string
    {
        *(buffer++) = *(s++); // don't read off the string because it
            // may be to long to fit in the token buffer
            // so just read the '"' so the compiler knows to scan the rest.
        *buffer = 0;
    }
    else if (*s == '#')
    {
        *(buffer++) = *(s++);
        if (*s != '\'')
            *(buffer++) = *(s++);
        *buffer = 0;
    }
    else
    {
        while (*s && *s != ')' && *s != '(' && *s != ' ' && *s != '\n' && *s != '\r' && *s != '\t' && *s != ';' &&
               *s != 26)
            *(buffer++) = *(s++);
        *buffer = 0;
    }
    return 1;
}

char n[MAX_LISP_TOKEN_LEN]; // assume all tokens will be < 200 characters

int end_of_program(char const *s)
{
    return !read_ltoken(s, n);
}

void push_onto_list(void *object, void *&list)
{
    PtrRef r1(object), r2(list);
    LList *c = LList::Create();
    c->m_car = (LObject *)object;
    c->m_cdr = (LObject *)list;
    list = c;
}

void *comp_optimize(void *list);

LObject *LObject::Compile(char const *&code)
{
    LObject *ret = NULL;

    if (!read_ltoken(code, n))
        lerror(NULL, "unexpected end of program");

    if (!strcmp(n, "nil"))
        return NULL;
    else if (toupper(n[0]) == 'T' && !n[1])
        return true_symbol;
    else if (n[0] == '\'') // short hand for quote function
    {
        LObject *cs = LList::Create(), *c2 = NULL, *tmp;
        PtrRef r1(cs), r2(c2);

        ((LList *)cs)->m_car = quote_symbol;
        c2 = LList::Create();
        tmp = Compile(code);
        ((LList *)c2)->m_car = (LObject *)tmp;
        ((LList *)c2)->m_cdr = NULL;
        ((LList *)cs)->m_cdr = (LObject *)c2;
        ret = cs;
    }
    else if (n[0] == '`') // short hand for backquote function
    {
        LObject *cs = LList::Create(), *c2 = NULL, *tmp;
        PtrRef r1(cs), r2(c2);

        ((LList *)cs)->m_car = backquote_symbol;
        c2 = LList::Create();
        tmp = Compile(code);
        ((LList *)c2)->m_car = (LObject *)tmp;
        ((LList *)c2)->m_cdr = NULL;
        ((LList *)cs)->m_cdr = (LObject *)c2;
        ret = cs;
    }
    else if (n[0] == ',') // short hand for comma function
    {
        LObject *cs = LList::Create(), *c2 = NULL, *tmp;
        PtrRef r1(cs), r2(c2);

        ((LList *)cs)->m_car = comma_symbol;
        c2 = LList::Create();
        tmp = Compile(code);
        ((LList *)c2)->m_car = (LObject *)tmp;
        ((LList *)c2)->m_cdr = NULL;
        ((LList *)cs)->m_cdr = (LObject *)c2;
        ret = cs;
    }
    else if (n[0] == '(') // make a list of everything in ()
    {
        void *first = NULL, *cur = NULL, *last = NULL;
        PtrRef r1(first), r2(cur), r3(last);
        int done = 0;
        do
        {
            char const *tmp = code;
            if (!read_ltoken(tmp, n)) // check for the end of the list
                lerror(NULL, "unexpected end of program");
            if (n[0] == ')')
            {
                done = 1;
                read_ltoken(code, n); // read off the ')'
            }
            else
            {
                if (n[0] == '.' && !n[1])
                {
                    if (!first)
                        lerror(code, "token '.' not allowed here\n");
                    else
                    {
                        void *tmp;
                        read_ltoken(code, n); // skip the '.'
                        tmp = Compile(code);
                        ((LList *)last)->m_cdr = (LObject *)tmp; // link the last cdr to
                        last = NULL;
                    }
                }
                else if (!last && first)
                    lerror(code, "illegal end of dotted list\n");
                else
                {
                    void *tmp;
                    cur = LList::Create();
                    PtrRef r1(cur);
                    if (!first)
                        first = cur;
                    tmp = Compile(code);
                    ((LList *)cur)->m_car = (LObject *)tmp;
                    if (last)
                        ((LList *)last)->m_cdr = (LObject *)cur;
                    last = cur;
                }
            }
        } while (!done);
        ret = (LObject *)comp_optimize(first);
    }
    else if (n[0] == ')')
        lerror(code, "mismatched )");
    else if (isdigit(n[0]) || (n[0] == '-' && isdigit(n[1])))
    {
        LNumber *num = LNumber::Create(0);
        sscanf(n, "%ld", &num->m_num);
        ret = num;
    }
    else if (n[0] == '"')
    {
        ret = LString::Create(str_token_len(code));
        char *start = lstring_value(ret);
        for (; *code && (*code != '"' || code[1] == '"'); code++, start++)
        {
            if (*code == '\\')
            {
                code++;
                if (*code == 'n')
                    *start = '\n';
                if (*code == 'r')
                    *start = '\r';
                if (*code == 't')
                    *start = '\t';
                if (*code == '\\')
                    *start = '\\';
            }
            else
                *start = *code;
            if (*code == '"')
                code++;
        }
        *start = 0;
        code++;
    }
    else if (n[0] == '#')
    {
        if (n[1] == '\\')
        {
            read_ltoken(code, n); // read character name
            if (!strcmp(n, "newline"))
                ret = LChar::Create('\n');
            else if (!strcmp(n, "space"))
                ret = LChar::Create(' ');
            else
                ret = LChar::Create(n[0]);
        }
        else if (n[1] == 0) // short hand for function
        {
            LObject *cs = LList::Create(), *c2 = NULL, *tmp;
            PtrRef r4(cs), r5(c2);
            tmp = LSymbol::FindOrCreate("function");
            ((LList *)cs)->m_car = (LObject *)tmp;
            c2 = LList::Create();
            tmp = Compile(code);
            ((LList *)c2)->m_car = (LObject *)tmp;
            ((LList *)cs)->m_cdr = (LObject *)c2;
            ret = cs;
        }
        else
        {
            lbreak("Unknown #\\ notation : %s\n", n);
            exit(EXIT_SUCCESS);
        }
    }
    else
    {
        ret = LSymbol::FindOrCreate(n);
    }
    return ret;
}

static void lprint_string(char const *st)
{
    if (current_print_file)
    {
        for (char const *s = st; *s; s++)
        {
            /*      if (*s=='\\')
      {
    s++;
    if (*s=='n')
      current_print_file->write_uint8('\n');
    else if (*s=='r')
      current_print_file->write_uint8('\r');
    else if (*s=='t')
      current_print_file->write_uint8('\t');
    else if (*s=='\\')
      current_print_file->write_uint8('\\');
      }
      else*/
            current_print_file->write_uint8(*s);
        }
    }
    else
        printf(st);
}

void LObject::Print()
{
    char buf[32];

    print_level++;

    switch (item_type(this))
    {
    case L_CONS_CELL:
        if (this == nullptr)
        {
            lprint_string("nil");
        }
        else
        {
            LList *cs = (LList *)this;
            lprint_string("(");
            for (; cs; cs = (LList *)lcdr(cs))
            {
                if (item_type(cs) == (ltype)L_CONS_CELL)
                {
                    cs->m_car->Print();
                    if (cs->m_cdr)
                        lprint_string(" ");
                }
                else
                {
                    lprint_string(". ");
                    cs->Print();
                    cs = NULL;
                }
            }
            lprint_string(")");
        }
        break;
    case L_NUMBER:
        sprintf(buf, "%ld", ((LNumber *)this)->m_num);
        lprint_string(buf);
        break;
    case L_SYMBOL:
        lprint_string(((LSymbol *)this)->m_name->GetString());
        break;
    case L_USER_FUNCTION:
    case L_SYS_FUNCTION:
        lprint_string("err... function?");
        break;
    case L_C_FUNCTION:
        lprint_string("C function, returns number\n");
        break;
    case L_C_BOOL:
        lprint_string("C boolean function\n");
        break;
    case L_L_FUNCTION:
        lprint_string("External lisp function\n");
        break;
    case L_STRING:
        if (current_print_file)
            lprint_string(lstring_value(this));
        else
            printf("\"%s\"", lstring_value(this));
        break;
    case L_POINTER:
        sprintf(buf, "%p", lpointer_value(this));
        lprint_string(buf);
        break;
    case L_FIXED_POINT:
        sprintf(buf, "%g", (lfixed_point_value(this) >> 16) + ((lfixed_point_value(this) & 0xffff)) / (double)0x10000);
        lprint_string(buf);
        break;
    case L_CHARACTER:
        if (current_print_file)
        {
            uint8_t ch = ((LChar *)this)->m_ch;
            current_print_file->write(&ch, 1);
        }
        else
        {
            uint16_t ch = ((LChar *)this)->m_ch;
            printf("#\\");
            switch (ch)
            {
            case '\n':
                printf("newline");
                break;
            case ' ':
                printf("space");
                break;
            default:
                printf("%c", ch);
                break;
            }
        }
        break;
    case L_OBJECT_VAR:
        l_obj_print(((LObjectVar *)this)->m_index);
        break;
    case L_1D_ARRAY: {
        LArray *a = (LArray *)this;
        LObject **data = a->GetData();
        printf("#(");
        for (size_t j = 0; j < a->m_len; j++)
        {
            data[j]->Print();
            if (j != a->m_len - 1)
                printf(" ");
        }
        printf(")");
    }
    break;
    case L_COLLECTED_OBJECT:
        lprint_string("GC_reference->");
        ((LRedirect *)this)->m_ref->Print();
        break;
    default:
        printf("Shouldn't happen\n");
    }

    print_level--;
    if (!print_level && !current_print_file)
        printf("\n");
}

/* PtrRef check: OK */
LObject *LSymbol::EvalFunction(void *arg_list)
{
#ifdef TYPE_CHECKING
    int args, req_min, req_max;
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("EVAL: is not a function name (not symbol either)");
        exit(EXIT_SUCCESS);
    }
#endif

    LObject *fun = m_function;
    PtrRef ref2(fun);
    PtrRef ref3(arg_list);

    // make sure the arguments given to the function are the correct number
    ltype t = item_type(fun);

#ifdef TYPE_CHECKING
    switch (t)
    {
    case L_SYS_FUNCTION:
    case L_C_FUNCTION:
    case L_C_BOOL:
    case L_L_FUNCTION:
        req_min = ((LSysFunction *)fun)->min_args;
        req_max = ((LSysFunction *)fun)->max_args;
        break;
    case L_USER_FUNCTION:
        return EvalUserFunction((LList *)arg_list);
    default:
        Print();
        lbreak(" is not a function name");
        exit(EXIT_FAILURE);
        break;
    }

    if (req_min != -1)
    {
        void *a = arg_list;
        for (args = 0; a; a = CDR(a))
            args++; // count number of parameters

        if (args < req_min)
        {
            ((LObject *)arg_list)->Print();
            printf("Function %s expected a minimum of %d parameter(s) but received %d.\n", m_name->GetString(), req_min,
                   args);
            exit(EXIT_FAILURE);
        }
        else if (req_max != -1 && args > req_max)
        {
            ((LObject *)arg_list)->Print();
            printf("Function %s expected a maximum of %d parameter(s) but received %d.\n", m_name->GetString(), req_max,
                   args);
            exit(EXIT_FAILURE);
        }
    }
#endif

#ifdef L_PROFILE
    time_marker start;
#endif

    LObject *ret = NULL;

    switch (t)
    {
    case L_SYS_FUNCTION:
        ret = ((LSysFunction *)fun)->EvalFunction((LList *)arg_list);
        break;
    case L_L_FUNCTION:
        ret = (LObject *)l_caller(static_cast<LispFunc>(((LSysFunction *)fun)->fun_number), arg_list);
        break;
    case L_USER_FUNCTION:
        return EvalUserFunction((LList *)arg_list);
    case L_C_FUNCTION:
    case L_C_BOOL: {
        LList *first = NULL, *cur = NULL;
        PtrRef r1(first), r2(cur), r3(arg_list);
        while (arg_list)
        {
            LList *tmp = LList::Create();
            if (first)
                cur->m_cdr = tmp;
            else
                first = tmp;
            cur = tmp;

            LObject *val = CAR(arg_list)->Eval();
            ((LList *)cur)->m_car = val;
            arg_list = lcdr(arg_list);
        }
        if (t == L_C_FUNCTION)
            ret = LNumber::Create(c_caller(static_cast<CFunc>(((LSysFunction *)fun)->fun_number), first));
        else if (c_caller(static_cast<CFunc>(((LSysFunction *)fun)->fun_number), first))
            ret = true_symbol;
        else
            ret = NULL;
        break;
    }
    default:
        fprintf(stderr, "not a fun, shouldn't happen\n");
    }

#ifdef L_PROFILE
    time_marker end;
    time_taken += end.diff_time(&start);
#endif

    return ret;
}

#ifdef L_PROFILE
void pro_print(bFILE *out, LSymbol *p)
{
    if (p)
    {
        pro_print(out, p->m_right);
        {
            char st[100];
            sprintf(st, "%20s %f\n", lstring_value(p->GetName()), p->time_taken);
            out->write(st, strlen(st));
        }
        pro_print(out, p->m_left);
    }
}

void preport(char *fn)
{
    bFILE *fp = open_file("preport.out", "wb");
    pro_print(fp, LSymbol::root);
    delete fp;
}
#endif

void *mapcar(void *arg_list)
{
    PtrRef ref1(arg_list);
    LObject *sym = CAR(arg_list)->Eval();
    switch ((short)item_type(sym))
    {
    case L_SYS_FUNCTION:
    case L_USER_FUNCTION:
    case L_SYMBOL:
        break;
    default: {
        sym->Print();
        lbreak(" is not a function\n");
        exit(EXIT_SUCCESS);
    }
    }
    int i, stop = 0, num_args = ((LList *)CDR(arg_list))->GetLength();
    if (!num_args)
        return 0;

    void **arg_on = (void **)malloc(sizeof(void *) * num_args);
    LList *list_on = (LList *)CDR(arg_list);
    long old_ptr_son = PtrRef::stack.m_size;

    for (i = 0; i < num_args; i++)
    {
        arg_on[i] = (LList *)CAR(list_on)->Eval();
        PtrRef::stack.push(&arg_on[i]);

        list_on = (LList *)CDR(list_on);
        if (!arg_on[i])
            stop = 1;
    }

    if (stop)
    {
        free(arg_on);
        return NULL;
    }

    LList *na_list = NULL, *return_list = NULL, *last_return = NULL;

    do
    {
        na_list = NULL; // create a cons list with all of the parameters for the function

        LList *first = NULL; // save the start of the list
        for (i = 0; !stop && i < num_args; i++)
        {
            if (!na_list)
                first = na_list = LList::Create();
            else
            {
                na_list->m_cdr = (LObject *)LList::Create();
                na_list = (LList *)CDR(na_list);
            }

            if (arg_on[i])
            {
                na_list->m_car = (LObject *)CAR(arg_on[i]);
                arg_on[i] = (LList *)CDR(arg_on[i]);
            }
            else
                stop = 1;
        }
        if (!stop)
        {
            LList *c = LList::Create();
            c->m_car = ((LSymbol *)sym)->EvalFunction(first);
            if (return_list)
                last_return->m_cdr = c;
            else
                return_list = c;
            last_return = c;
        }
    } while (!stop);
    PtrRef::stack.m_size = old_ptr_son;

    free(arg_on);
    return return_list;
}

void *concatenate(void *prog_list)
{
    void *el_list = CDR(prog_list);
    PtrRef ref1(prog_list), ref2(el_list);
    void *ret = NULL;
    void *rtype = CAR(prog_list)->Eval();

    long len = 0; // determin the length of the resulting string
    if (rtype == string_symbol)
    {
        int elements = ((LList *)el_list)->GetLength(); // see how many things we need to concat
        if (!elements)
            ret = LString::Create("");
        else
        {
            void **str_eval = (void **)malloc(elements * sizeof(void *));
            int i, old_ptr_stack_start = PtrRef::stack.m_size;

            // evalaute all the strings and count their lengths
            for (i = 0; i < elements; i++, el_list = CDR(el_list))
            {
                str_eval[i] = CAR(el_list)->Eval();
                PtrRef::stack.push(&str_eval[i]);

                switch ((short)item_type(str_eval[i]))
                {
                case L_CONS_CELL: {
                    LList *char_list = (LList *)str_eval[i];
                    while (char_list)
                    {
                        if (item_type(CAR(char_list)) == (ltype)L_CHARACTER)
                            len++;
                        else
                        {
                            ((LObject *)str_eval[i])->Print();
                            lbreak(" is not a character\n");
                            exit(EXIT_SUCCESS);
                        }
                        char_list = (LList *)CDR(char_list);
                    }
                }
                break;
                case L_STRING:
                    len += strlen(lstring_value(str_eval[i]));
                    break;
                default:
                    ((LObject *)prog_list)->Print();
                    lbreak("type not supported\n");
                    exit(EXIT_SUCCESS);
                    break;
                }
            }
            LString *st = LString::Create(len + 1);
            char *s = lstring_value(st);

            // now add the string up into the new string
            for (i = 0; i < elements; i++)
            {
                switch ((short)item_type(str_eval[i]))
                {
                case L_CONS_CELL: {
                    LList *char_list = (LList *)str_eval[i];
                    while (char_list)
                    {
                        if (item_type(CAR(char_list)) == L_CHARACTER)
                            *(s++) = ((LChar *)CAR(char_list))->m_ch;
                        char_list = (LList *)CDR(char_list);
                    }
                }
                break;
                case L_STRING: {
                    memcpy(s, lstring_value(str_eval[i]), strlen(lstring_value(str_eval[i])));
                    s += strlen(lstring_value(str_eval[i]));
                }
                break;
                default:; // already checked for, but make compiler happy
                }
            }
            free(str_eval);
            PtrRef::stack.m_size = old_ptr_stack_start; // restore pointer GC stack
            *s = 0;
            ret = st;
        }
    }
    else
    {
        ((LObject *)prog_list)->Print();
        lbreak("concat operation not supported, try 'string\n");
        exit(EXIT_SUCCESS);
    }
    return ret;
}

void *backquote_eval(void *args)
{
    if (item_type(args) != L_CONS_CELL)
        return args;
    else if (args == NULL)
        return NULL;
    else if ((LSymbol *)(((LList *)args)->m_car) == comma_symbol)
        return CAR(CDR(args))->Eval();
    else
    {
        void *first = NULL, *last = NULL, *cur = NULL, *tmp;
        PtrRef ref1(first), ref2(last), ref3(cur), ref4(args);
        while (args)
        {
            if (item_type(args) == L_CONS_CELL)
            {
                if (CAR(args) == comma_symbol) // dot list with a comma?
                {
                    tmp = CAR(CDR(args))->Eval();
                    ((LList *)last)->m_cdr = (LObject *)tmp;
                    args = NULL;
                }
                else
                {
                    cur = LList::Create();
                    if (first)
                        ((LList *)last)->m_cdr = (LObject *)cur;
                    else
                        first = cur;
                    last = cur;
                    tmp = backquote_eval(CAR(args));
                    ((LList *)cur)->m_car = (LObject *)tmp;
                    args = CDR(args);
                }
            }
            else
            {
                tmp = backquote_eval(args);
                ((LList *)last)->m_cdr = (LObject *)tmp;
                args = NULL;
            }
        }
        return (void *)first;
    }
    return NULL; // for stupid compiler messages
}

/* PtrRef check: OK */
LObject *LSysFunction::EvalFunction(LList *arg_list)
{
    LObject *ret = NULL;

    PtrRef ref1(arg_list);

    switch (static_cast<SysFunc>(fun_number))
    {
    case SysFunc::Print:
        while (arg_list)
        {
            ret = CAR(arg_list)->Eval();
            arg_list = (LList *)CDR(arg_list);
            ret->Print();
        }
        break;
    case SysFunc::Car:
        ret = lcar(CAR(arg_list)->Eval());
        break;
    case SysFunc::Cdr:
        ret = lcdr(CAR(arg_list)->Eval());
        break;
    case SysFunc::Length: {
        LObject *v = CAR(arg_list)->Eval();
        switch (item_type(v))
        {
        case L_STRING:
            ret = LNumber::Create(strlen(lstring_value(v)));
            break;
        case L_CONS_CELL:
            ret = LNumber::Create(((LList *)v)->GetLength());
            break;
        default:
            v->Print();
            lbreak("length : type not supported\n");
            break;
        }
        break;
    }
    case SysFunc::List: {
        LList *cur = NULL, *last = NULL, *first = NULL;
        PtrRef r1(cur), r2(first), r3(last);
        while (arg_list)
        {
            cur = LList::Create();
            LObject *val = CAR(arg_list)->Eval();
            cur->m_car = val;
            if (last)
                last->m_cdr = cur;
            else
                first = cur;
            last = cur;
            arg_list = (LList *)CDR(arg_list);
        }
        ret = first;
        break;
    }
    case SysFunc::Cons: {
        LList *c = LList::Create();
        PtrRef r1(c);
        LObject *val = CAR(arg_list)->Eval();
        c->m_car = val;
        val = CAR(CDR(arg_list))->Eval();
        c->m_cdr = val;
        ret = c;
        break;
    }
    case SysFunc::Quote:
        ret = CAR(arg_list);
        break;
    case SysFunc::Eq:
        l_user_stack.push(CAR(arg_list)->Eval());
        l_user_stack.push(CAR(CDR(arg_list))->Eval());
        ret = (LObject *)lisp_eq(l_user_stack.pop(1), l_user_stack.pop(1));
        break;
    case SysFunc::Equal:
        l_user_stack.push(CAR(arg_list)->Eval());
        l_user_stack.push(CAR(CDR(arg_list))->Eval());
        ret = (LObject *)lisp_equal(l_user_stack.pop(1), l_user_stack.pop(1));
        break;
    case SysFunc::Plus: {
        int32_t sum = 0;
        while (arg_list)
        {
            sum += lnumber_value(CAR(arg_list)->Eval());
            arg_list = (LList *)CDR(arg_list);
        }
        ret = LNumber::Create(sum);
        break;
    }
    case SysFunc::Times: {
        int32_t prod;
        LObject *first = CAR(arg_list)->Eval();
        PtrRef r1(first);
        if (arg_list && item_type(first) == L_FIXED_POINT)
        {
            prod = 1 << 16;
            do
            {
                prod = (prod >> 8) * (lfixed_point_value(first) >> 8);
                arg_list = (LList *)CDR(arg_list);
                if (arg_list)
                    first = CAR(arg_list)->Eval();
            } while (arg_list);
            ret = LFixedPoint::Create(prod);
        }
        else
        {
            prod = 1;
            do
            {
                prod *= lnumber_value(CAR(arg_list)->Eval());
                arg_list = (LList *)CDR(arg_list);
                if (arg_list)
                    first = CAR(arg_list)->Eval();
            } while (arg_list);
            ret = LNumber::Create(prod);
        }
        break;
    }
    case SysFunc::Slash: {
        int32_t quot = 0, first = 1;
        while (arg_list)
        {
            LObject *i = CAR(arg_list)->Eval();
            if (item_type(i) != L_NUMBER)
            {
                i->Print();
                lbreak("/ only defined for numbers, cannot divide ");
                exit(EXIT_SUCCESS);
            }
            else if (first)
            {
                quot = ((LNumber *)i)->m_num;
                first = 0;
            }
            else
                quot /= ((LNumber *)i)->m_num;
            arg_list = (LList *)CDR(arg_list);
        }
        ret = LNumber::Create(quot);
        break;
    }
    case SysFunc::Minus: {
        int32_t sub = lnumber_value(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        while (arg_list)
        {
            sub -= lnumber_value(CAR(arg_list)->Eval());
            arg_list = (LList *)CDR(arg_list);
        }
        ret = LNumber::Create(sub);
        break;
    }
    case SysFunc::If:
        if (CAR(arg_list)->Eval())
            ret = CAR(CDR(arg_list))->Eval();
        else
        {
            arg_list = (LList *)CDR(CDR(arg_list)); // check for a else part
            if (arg_list)
                ret = CAR(arg_list)->Eval();
            else
                ret = NULL;
        }
        break;
    case SysFunc::Setq:
    case SysFunc::Setf: {
        LObject *set_to = CAR(CDR(arg_list))->Eval(), *i = NULL;
        PtrRef r1(set_to), r2(i);
        i = CAR(arg_list);

        ltype x = item_type(set_to);
        switch (item_type(i))
        {
        case L_SYMBOL:
            switch (item_type(((LSymbol *)i)->m_value))
            {
            case L_NUMBER:
                if (x == L_NUMBER && ((LSymbol *)i)->m_value != l_undefined)
                    ((LSymbol *)i)->SetNumber(lnumber_value(set_to));
                else
                    ((LSymbol *)i)->SetValue((LNumber *)set_to);
                break;
            case L_OBJECT_VAR:
                l_obj_set(((LObjectVar *)(((LSymbol *)i)->m_value))->m_index, set_to);
                break;
            default:
                ((LSymbol *)i)->SetValue((LObject *)set_to);
            }
            ret = ((LSymbol *)i)->m_value;
            break;
        case L_CONS_CELL: // this better be an 'aref'
        {
#ifdef TYPE_CHECKING
            LObject *car = ((LList *)i)->m_car;
            if (car == car_symbol)
            {
                car = CAR(CDR(i))->Eval();
                if (!car || item_type(car) != L_CONS_CELL)
                {
                    car->Print();
                    lbreak("setq car : evaled object is not a cons cell\n");
                    exit(EXIT_SUCCESS);
                }
                ((LList *)car)->m_car = set_to;
            }
            else if (car == cdr_symbol)
            {
                car = CAR(CDR(i))->Eval();
                if (!car || item_type(car) != L_CONS_CELL)
                {
                    car->Print();
                    lbreak("setq cdr : evaled object is not a cons cell\n");
                    exit(EXIT_SUCCESS);
                }
                ((LList *)car)->m_cdr = set_to;
            }
            else if (car != aref_symbol)
            {
                lbreak("expected (aref, car, cdr, or symbol) in setq\n");
                exit(EXIT_SUCCESS);
            }
            else
            {
#endif
                LArray *a = (LArray *)CAR(CDR(i))->Eval();
                PtrRef r1(a);
#ifdef TYPE_CHECKING
                if (item_type(a) != L_1D_ARRAY)
                {
                    a->Print();
                    lbreak("is not an array (aref)\n");
                    exit(EXIT_SUCCESS);
                }
#endif
                int num = lnumber_value(CAR(CDR(CDR(i)))->Eval());
#ifdef TYPE_CHECKING
                if (num >= (int)a->m_len || num < 0)
                {
                    lbreak("aref : value of bounds (%d)\n", num);
                    exit(EXIT_SUCCESS);
                }
#endif
                a->GetData()[num] = set_to;
#ifdef TYPE_CHECKING
            }
#endif
            ret = set_to;
            break;
        }
        default:
            i->Print();
            lbreak("setq/setf only defined for symbols and arrays now..\n");
            exit(EXIT_SUCCESS);
            break;
        }
        break;
    }
    case SysFunc::SymbolList:
        ret = NULL;
        break;
    case SysFunc::Assoc: {
        LObject *item = CAR(arg_list)->Eval();
        PtrRef r1(item);
        LList *list = (LList *)CAR(CDR(arg_list))->Eval();
        PtrRef r2(list);
        ret = list->Assoc(item);
        break;
    }
    case SysFunc::Not:
    case SysFunc::Null:
        if (CAR(arg_list)->Eval() == NULL)
            ret = true_symbol;
        else
            ret = NULL;
        break;
    case SysFunc::Acons: {
        LObject *i1 = CAR(arg_list)->Eval();
        PtrRef r1(i1);
        LObject *i2 = CAR(CDR(arg_list))->Eval();
        PtrRef r2(i2);
        LList *cs = LList::Create();
        cs->m_car = i1;
        cs->m_cdr = i2;
        ret = cs;
        break;
    }
    case SysFunc::Pairlis: {
        l_user_stack.push(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        l_user_stack.push(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        LObject *n3 = CAR(arg_list)->Eval();
        LObject *n2 = (LObject *)l_user_stack.pop(1);
        LObject *n1 = (LObject *)l_user_stack.pop(1);
        ret = (LObject *)pairlis(n1, n2, n3);
        break;
    }
    case SysFunc::Let: {
        // make an a-list of new variable names and new values
        LObject *var_list = CAR(arg_list);
        LObject *block_list = CDR(arg_list);
        PtrRef r1(block_list), r2(var_list);
        long stack_start = l_user_stack.m_size;

        while (var_list)
        {
            LObject *var_name = CAR(CAR(var_list)), *tmp;
#ifdef TYPE_CHECKING
            if (item_type(var_name) != L_SYMBOL)
            {
                var_name->Print();
                lbreak("should be a symbol (let)\n");
                exit(EXIT_SUCCESS);
            }
#endif

            l_user_stack.push(((LSymbol *)var_name)->m_value);
            tmp = CAR(CDR(CAR(var_list)))->Eval();
            ((LSymbol *)var_name)->SetValue(tmp);
            var_list = CDR(var_list);
        }

        // now evaluate each of the blocks with the new environment and
        // return value from the last block
        while (block_list)
        {
            ret = CAR(block_list)->Eval();
            block_list = CDR(block_list);
        }

        long cur_stack = stack_start;
        var_list = CAR(arg_list); // now restore the old symbol values
        while (var_list)
        {
            LObject *var_name = CAR(CAR(var_list));
            ((LSymbol *)var_name)->SetValue((LObject *)l_user_stack.sdata[cur_stack++]);
            var_list = CDR(var_list);
        }
        l_user_stack.m_size = stack_start; // restore the stack
        break;
    }
    case SysFunc::Defun: {
        LSymbol *symbol = (LSymbol *)CAR(arg_list);
        PtrRef r1(symbol);
#ifdef TYPE_CHECKING
        if (item_type(symbol) != L_SYMBOL)
        {
            symbol->Print();
            lbreak(" is not a symbol! (DEFUN)\n");
            exit(EXIT_SUCCESS);
        }

        if (item_type(arg_list) != L_CONS_CELL)
        {
            arg_list->Print();
            lbreak("is not a lambda list (DEFUN)\n");
            exit(EXIT_SUCCESS);
        }
#endif
        LObject *block_list = CDR(CDR(arg_list));

        LUserFunction *ufun = new_lisp_user_function((LList *)lcar(lcdr(arg_list)), (LList *)block_list);
        symbol->SetFunction(ufun);
        ret = symbol;
        break;
    }
    case SysFunc::Atom:
        ret = (LObject *)lisp_atom(CAR(arg_list)->Eval());
        break;
    case SysFunc::And: {
        LObject *l = arg_list;
        PtrRef r1(l);
        ret = true_symbol;
        while (l)
        {
            if (!CAR(l)->Eval())
            {
                ret = NULL;
                l = NULL; // short-circuit
            }
            else
                l = CDR(l);
        }
        break;
    }
    case SysFunc::Or: {
        LObject *l = arg_list;
        PtrRef r1(l);
        ret = NULL;
        while (l)
        {
            if (CAR(l)->Eval())
            {
                ret = true_symbol;
                l = NULL; // short-circuit
            }
            else
                l = CDR(l);
        }
        break;
    }
    case SysFunc::Progn:
        ret = (LObject *)eval_block(arg_list);
        break;
    case SysFunc::Concatenate:
        ret = (LObject *)concatenate(arg_list);
        break;
    case SysFunc::CharCode: {
        LObject *i = CAR(arg_list)->Eval();
        PtrRef r1(i);
        ret = NULL;
        switch (item_type(i))
        {
        case L_CHARACTER:
            ret = LNumber::Create(((LChar *)i)->m_ch);
            break;
        case L_STRING:
            ret = LNumber::Create(*lstring_value(i));
            break;
        default:
            i->Print();
            lbreak(" is not character type\n");
            exit(EXIT_SUCCESS);
            break;
        }
        break;
    }
    case SysFunc::CodeChar: {
        LObject *i = CAR(arg_list)->Eval();
        PtrRef r1(i);
        if (item_type(i) != L_NUMBER)
        {
            i->Print();
            lbreak(" is not number type\n");
            exit(EXIT_SUCCESS);
        }
        ret = LChar::Create(((LNumber *)i)->m_num);
        break;
    }
    case SysFunc::Cond: {
        LList *block_list = (LList *)CAR(arg_list);
        PtrRef r1(block_list);
        ret = NULL;
        PtrRef r2(ret); // Required to protect from the last Eval call
        while (block_list)
        {
            if (lcar(CAR(block_list))->Eval())
                ret = CAR(CDR(CAR(block_list)))->Eval();
            block_list = (LList *)CDR(block_list);
        }
        break;
    }
    case SysFunc::Select: {
        LObject *selector = CAR(arg_list)->Eval();
        LObject *sel = CDR(arg_list);
        PtrRef r1(selector), r2(sel);
        ret = NULL;
        PtrRef r3(ret); // Required to protect from the last Eval call
        while (sel)
        {
            if (lisp_equal(selector, CAR(CAR(sel))->Eval()))
            {
                sel = CDR(CAR(sel));
                while (sel)
                {
                    ret = CAR(sel)->Eval();
                    sel = CDR(sel);
                }
            }
            else
                sel = CDR(sel);
        }
        break;
    }
    case SysFunc::Function:
        ret = ((LSymbol *)CAR(arg_list)->Eval())->GetFunction();
        break;
    case SysFunc::Mapcar:
        ret = (LObject *)mapcar(arg_list);
        break;
    case SysFunc::Funcall: {
        LSymbol *n1 = (LSymbol *)CAR(arg_list)->Eval();
        ret = n1->EvalFunction(CDR(arg_list));
        break;
    }
    case SysFunc::GreaterThan: {
        int32_t n1 = lnumber_value(CAR(arg_list)->Eval());
        int32_t n2 = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = n1 > n2 ? true_symbol : NULL;
        break;
    }
    case SysFunc::LessThan: {
        int32_t n1 = lnumber_value(CAR(arg_list)->Eval());
        int32_t n2 = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = n1 < n2 ? true_symbol : NULL;
        break;
    }
    case SysFunc::GreaterOrEqual: {
        int32_t n1 = lnumber_value(CAR(arg_list)->Eval());
        int32_t n2 = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = n1 >= n2 ? true_symbol : NULL;
        break;
    }
    case SysFunc::LessOrEqual: {
        int32_t n1 = lnumber_value(CAR(arg_list)->Eval());
        int32_t n2 = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = n1 <= n2 ? true_symbol : NULL;
        break;
    }
    case SysFunc::TmpSpace:
        tmp_space();
        ret = true_symbol;
        break;
    case SysFunc::PermSpace:
        perm_space();
        ret = true_symbol;
        break;
    case SysFunc::SymbolName: {
        LSymbol *symb = (LSymbol *)CAR(arg_list)->Eval();
#ifdef TYPE_CHECKING
        if (item_type(symb) != L_SYMBOL)
        {
            symb->Print();
            lbreak(" is not a symbol (symbol-name)\n");
            exit(EXIT_SUCCESS);
        }
#endif
        ret = symb->m_name;
        break;
    }
    case SysFunc::Trace:
        trace_level++;
        if (arg_list)
            trace_print_level = lnumber_value(CAR(arg_list)->Eval());
        ret = true_symbol;
        break;
    case SysFunc::Untrace:
        if (trace_level > 0)
        {
            trace_level--;
            ret = true_symbol;
        }
        else
            ret = NULL;
        break;
    case SysFunc::Digstr: {
        char tmp[50], *tp;
        int32_t num = lnumber_value(CAR(arg_list)->Eval());
        int32_t dig = lnumber_value(CAR(CDR(arg_list))->Eval());
        tp = tmp + 49;
        *(tp--) = 0;
        while (num)
        {
            *(tp--) = '0' + (num % 10);
            num /= 10;
            dig--;
        }
        while (dig--)
            *(tp--) = '0';
        ret = LString::Create(tp + 1);
        break;
    }
    case SysFunc::LocalLoad:
    case SysFunc::Load:
    case SysFunc::CompileFile: {
        LObject *fn = CAR(arg_list)->Eval();
        PtrRef r1(fn);
        char *st = lstring_value(fn);
        bFILE *fp;
        if (static_cast<SysFunc>(fun_number) == SysFunc::LocalLoad)
        {
            // A special test for gamma.lsp
            if (strcmp(st, "gamma.lsp") == 0)
            {
                char *gammapath;
                gammapath = (char *)malloc(strlen(get_save_filename_prefix()) + 9 + 1);
                sprintf(gammapath, "%sgamma.lsp", get_save_filename_prefix());
                fp = new jFILE(gammapath, "rb");
                free(gammapath);
            }
            else
                fp = new jFILE(st, "rb");
        }
        else
            fp = open_file(st, "rb");

        if (fp->open_failure())
        {
            delete fp;
            if (DEFINEDP(((LSymbol *)load_warning)->GetValue()) && ((LSymbol *)load_warning)->GetValue())
                printf("Warning : file %s does not exist\n", st);
            ret = NULL;
        }
        else
        {
            size_t l = fp->file_size();
            char *s = (char *)malloc(l + 1);
            if (!s)
            {
                printf("Malloc error in load_script\n");
                exit(EXIT_SUCCESS);
            }

            fp->read(s, l);
            s[l] = 0;
            delete fp;
            char const *cs = s;
#ifndef NO_LIBS
            char msg[100];
            sprintf(msg, "(load \"%s\")", st);
            if (stat_man)
                stat_man->push(msg, NULL);
            crc_manager.get_filenumber(st); // make sure this file gets crc'ed
#endif
            LObject *compiled_form = NULL;
            PtrRef r11(compiled_form);
            while (!end_of_program(cs)) // see if there is anything left to compile and run
            {
#ifndef NO_LIBS
                if (stat_man)
                    stat_man->update((cs - s) * 100 / l);
#endif
                void *m = LSpace::Tmp.Mark();
                compiled_form = LObject::Compile(cs);
                compiled_form->Eval();
                compiled_form = NULL;
                LSpace::Tmp.Restore(m);
            }
#ifndef NO_LIBS
            if (stat_man)
            {
                stat_man->update(100);
                stat_man->pop();
            }
#endif
            free(s);
            ret = fn;
        }
        break;
    }
    case SysFunc::Abs:
        ret = LNumber::Create(abs(lnumber_value(CAR(arg_list)->Eval())));
        break;
    case SysFunc::Min: {
        int32_t x = lnumber_value(CAR(arg_list)->Eval());
        int32_t y = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = LNumber::Create(x < y ? x : y);
        break;
    }
    case SysFunc::Max: {
        int32_t x = lnumber_value(CAR(arg_list)->Eval());
        int32_t y = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = LNumber::Create(x > y ? x : y);
        break;
    }
    case SysFunc::Backquote:
        ret = (LObject *)backquote_eval(CAR(arg_list));
        break;
    case SysFunc::Comma:
        arg_list->Print();
        lbreak("comma is illegal outside of backquote\n");
        exit(EXIT_SUCCESS);
        break;
    case SysFunc::Nth: {
        int32_t x = lnumber_value(CAR(arg_list)->Eval());
        ret = (LObject *)nth(x, CAR(CDR(arg_list))->Eval());
        break;
    }
    case SysFunc::ResizeTmp:
        // Deprecated and useless
        break;
    case SysFunc::ResizePerm:
        // Deprecated and useless
        break;
    case SysFunc::Cos:
        ret = LFixedPoint::Create(lisp_cos(lnumber_value(CAR(arg_list)->Eval())));
        break;
    case SysFunc::Sin:
        ret = LFixedPoint::Create(lisp_sin(lnumber_value(CAR(arg_list)->Eval())));
        break;
    case SysFunc::Atan2: {
        int32_t y = (lnumber_value(CAR(arg_list)->Eval()));
        int32_t x = (lnumber_value(CAR(CDR(arg_list))->Eval()));
        ret = LNumber::Create(lisp_atan2(y, x));
        break;
    }
    case SysFunc::Enum: {
        LSpace *sp = LSpace::Current;
        LSpace::Current = &LSpace::Perm;
        int32_t x = 0;
        while (arg_list)
        {
            LObject *sym = CAR(arg_list)->Eval();
            PtrRef r1(sym);
            switch (item_type(sym))
            {
            case L_SYMBOL: {
                LObject *tmp = LNumber::Create(x);
                ((LSymbol *)sym)->m_value = tmp;
                break;
            }
            case L_CONS_CELL: {
                LObject *s = CAR(sym)->Eval();
                PtrRef r1(s);
#ifdef TYPE_CHECKING
                if (item_type(s) != L_SYMBOL)
                {
                    arg_list->Print();
                    lbreak("expecting (symbol value) for enum\n");
                    exit(EXIT_SUCCESS);
                }
#endif
                x = lnumber_value(CAR(CDR(sym))->Eval());
                LObject *tmp = LNumber::Create(x);
                ((LSymbol *)sym)->m_value = tmp;
                break;
            }
            default:
                arg_list->Print();
                lbreak("expecting symbol or (symbol value) in enum\n");
                exit(EXIT_SUCCESS);
            }
            arg_list = (LList *)CDR(arg_list);
            x++;
        }
        LSpace::Current = sp;
        break;
    }
    case SysFunc::Quit:
        exit(EXIT_SUCCESS);
        break;
    case SysFunc::Eval:
        ret = CAR(arg_list)->Eval()->Eval();
        break;
    case SysFunc::Break:
        lbreak("User break");
        break;
    case SysFunc::Mod: {
        int32_t x = lnumber_value(CAR(arg_list)->Eval());
        int32_t y = lnumber_value(CAR(CDR(arg_list))->Eval());
        if (y == 0)
        {
            lbreak("mod: division by zero\n");
            y = 1;
        }
        ret = LNumber::Create(x % y);
        break;
    }
#if 0
    case sys_func_index::WRITE_PROFILE:
    {
        char *fn = lstring_value(CAR(arg_list)->Eval());
        FILE *fp = fopen(fn, "wb");
        if (!fp)
            lbreak("could not open %s for writing", fn);
        else
        {
            for (void *s = symbol_list; s; s = CDR(s))
                fprintf(fp, "%8d  %s\n", ((LSymbol *)(CAR(s)))->call_counter,
                        lstring_value(((LSymbol *)(CAR(s)))->m_name));
            fclose(fp);
        }
        break;
    }
#endif
    case SysFunc::For: {
        LSymbol *bind_var = (LSymbol *)CAR(arg_list);
        PtrRef r1(bind_var);
        if (item_type(bind_var) != L_SYMBOL)
        {
            lbreak("expecting for iterator to be a symbol\n");
            exit(EXIT_FAILURE);
        }
        arg_list = (LList *)CDR(arg_list);

        if (CAR(arg_list) != in_symbol)
        {
            lbreak("expecting in after 'for iterator'\n");
            exit(EXIT_FAILURE);
        }
        arg_list = (LList *)CDR(arg_list);

        LObject *ilist = CAR(arg_list)->Eval();
        PtrRef r2(ilist);
        arg_list = (LList *)CDR(arg_list);

        if (CAR(arg_list) != do_symbol)
        {
            lbreak("expecting do after 'for iterator in list'\n");
            exit(EXIT_FAILURE);
        }
        arg_list = (LList *)CDR(arg_list);

        LObject *block = NULL;
        PtrRef r3(block);
        PtrRef r4(ret); // Required to protect from the last SetValue call
        l_user_stack.push(bind_var->GetValue()); // save old symbol value
        while (ilist)
        {
            bind_var->SetValue((LObject *)CAR(ilist));
            for (block = arg_list; block; block = CDR(block))
                ret = CAR(block)->Eval();
            ilist = CDR(ilist);
        }
        bind_var->SetValue((LObject *)l_user_stack.pop(1)); // restore value
        break;
    }
    case SysFunc::OpenFile: {
        LObject *str1 = CAR(arg_list)->Eval();
        PtrRef r1(str1);
        LObject *str2 = CAR(CDR(arg_list))->Eval();

        bFILE *old_file = current_print_file;
        current_print_file = open_file(lstring_value(str1), lstring_value(str2));

        if (!current_print_file->open_failure())
        {
            while (arg_list)
            {
                ret = CAR(arg_list)->Eval();
                arg_list = (LList *)CDR(arg_list);
            }
        }
        delete current_print_file;
        current_print_file = old_file;
        break;
    }
    case SysFunc::BitAnd: {
        int32_t first = lnumber_value(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        while (arg_list)
        {
            first &= lnumber_value(CAR(arg_list)->Eval());
            arg_list = (LList *)CDR(arg_list);
        }
        ret = LNumber::Create(first);
        break;
    }
    case SysFunc::BitOr: {
        int32_t first = lnumber_value(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        while (arg_list)
        {
            first |= lnumber_value(CAR(arg_list)->Eval());
            arg_list = (LList *)CDR(arg_list);
        }
        ret = LNumber::Create(first);
        break;
    }
    case SysFunc::BitXor: {
        int32_t first = lnumber_value(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        while (arg_list)
        {
            first ^= lnumber_value(CAR(arg_list)->Eval());
            arg_list = (LList *)CDR(arg_list);
        }
        ret = LNumber::Create(first);
        break;
    }
    case SysFunc::MakeArray: {
        int32_t l = lnumber_value(CAR(arg_list)->Eval());
        if (l >= (2 << 16) || l <= 0)
        {
            lbreak("bad array size %d\n", l);
            exit(EXIT_SUCCESS);
        }
        ret = LArray::Create(l, CDR(arg_list));
        break;
    }
    case SysFunc::Aref: {
        int32_t x = lnumber_value(CAR(CDR(arg_list))->Eval());
        ret = ((LArray *)CAR(arg_list)->Eval())->Get(x);
        break;
    }
    case SysFunc::If1Progn:
        if (CAR(arg_list)->Eval())
            ret = (LObject *)eval_block(CAR(CDR(arg_list)));
        else
            ret = CAR(CDR(CDR(arg_list)))->Eval();
        break;
    case SysFunc::If2Progn:
        if (CAR(arg_list)->Eval())
            ret = CAR(CDR(arg_list))->Eval();
        else
            ret = (LObject *)eval_block(CAR(CDR(CDR(arg_list))));

        break;
    case SysFunc::If12Progn:
        if (CAR(arg_list)->Eval())
            ret = (LObject *)eval_block(CAR(CDR(arg_list)));
        else
            ret = (LObject *)eval_block(CAR(CDR(CDR(arg_list))));
        break;
    case SysFunc::Eq0: {
        LObject *v = CAR(arg_list)->Eval();
        if (item_type(v) != L_NUMBER || (((LNumber *)v)->m_num != 0))
            ret = NULL;
        else
            ret = true_symbol;
        break;
    }
    case SysFunc::Preport: {
#ifdef L_PROFILE
        char *s = lstring_value(CAR(arg_list)->Eval());
        preport(s);
#endif
        break;
    }
    case SysFunc::Search: {
        LObject *arg1 = CAR(arg_list)->Eval();
        PtrRef r1(arg1); // protect this reference
        arg_list = (LList *)CDR(arg_list);
        char *haystack = lstring_value(CAR(arg_list)->Eval());
        char *needle = lstring_value(arg1);

        char *find = strstr(haystack, needle);
        ret = find ? LNumber::Create(find - haystack) : NULL;
        break;
    }
    case SysFunc::Elt: {
        LObject *arg1 = CAR(arg_list)->Eval();
        PtrRef r1(arg1); // protect this reference
        arg_list = (LList *)CDR(arg_list);
        int32_t x = lnumber_value(CAR(arg_list)->Eval());
        char *st = lstring_value(arg1);
        if (x < 0 || x >= (int32_t)strlen(st))
        {
            lbreak("elt: out of range of string\n");
            ret = NULL;
        }
        else
            ret = LChar::Create(st[x]);
        break;
    }
    case SysFunc::Listp: {
        LObject *tmp = CAR(arg_list)->Eval();
        ltype t = item_type(tmp);
        ret = (t == L_CONS_CELL) ? true_symbol : NULL;
        break;
    }
    case SysFunc::Numberp: {
        LObject *tmp = CAR(arg_list)->Eval();
        ltype t = item_type(tmp);
        ret = (t == L_NUMBER || t == L_FIXED_POINT) ? true_symbol : NULL;
        break;
    }
    case SysFunc::Do: {
        LObject *init_var = CAR(arg_list);
        PtrRef r1(init_var);
        int ustack_start = l_user_stack.m_size; // restore stack at end
        LSymbol *sym = NULL;
        PtrRef r2(sym);

        // check to make sure iter vars are symbol and push old values
        for (init_var = CAR(arg_list); init_var; init_var = CDR(init_var))
        {
            sym = (LSymbol *)CAR(CAR(init_var));
            if (item_type(sym) != L_SYMBOL)
            {
                lbreak("expecting symbol name for iteration var\n");
                exit(EXIT_SUCCESS);
            }
            l_user_stack.push(sym->GetValue());
        }

        void **do_evaled = l_user_stack.sdata + l_user_stack.m_size;
        // push all of the init forms, so we can set the symbol
        for (init_var = CAR(arg_list); init_var; init_var = CDR(init_var))
            l_user_stack.push(CAR(CDR(CAR((init_var))))->Eval());

        // now set all the symbols
        for (init_var = CAR(arg_list); init_var; init_var = CDR(init_var))
        {
            sym = (LSymbol *)CAR(CAR(init_var));
            sym->SetValue((LObject *)*do_evaled);
            do_evaled++;
        }

        for (int i = 0; !i;) // set i to 1 when terminate conditions are met
        {
            i = CAR(CAR(CDR(arg_list)))->Eval() != NULL;
            if (!i)
            {
                eval_block(CDR(CDR(arg_list)));
                for (init_var = CAR(arg_list); init_var; init_var = CDR(init_var))
                    CAR(CDR(CDR(CAR(init_var))))->Eval();
            }
        }

        ret = CAR(CDR(CAR(CDR(arg_list))))->Eval();

        // restore old values for symbols
        do_evaled = l_user_stack.sdata + ustack_start;
        for (init_var = CAR(arg_list); init_var; init_var = CDR(init_var))
        {
            sym = (LSymbol *)CAR(CAR(init_var));
            sym->SetValue((LObject *)*do_evaled);
            do_evaled++;
        }

        l_user_stack.m_size = ustack_start;
        break;
    }
    case SysFunc::GarbageCollect:
        Lisp::CollectSpace(LSpace::Current, 0);
        break;
    case SysFunc::Schar: {
        char *s = lstring_value(CAR(arg_list)->Eval());
        arg_list = (LList *)CDR(arg_list);
        int32_t x = lnumber_value(CAR(arg_list)->Eval());

        if (x < 0 || x >= (int32_t)strlen(s))
        {
            lbreak("SCHAR: index %d out of bounds\n", x);
            exit(EXIT_SUCCESS);
        }
        ret = LChar::Create(s[x]);
        break;
    }
    case SysFunc::Symbolp: {
        LObject *tmp = CAR(arg_list)->Eval();
        ret = (item_type(tmp) == L_SYMBOL) ? true_symbol : NULL;
        break;
    }
    case SysFunc::Num2Str: {
        char str[20];
        sprintf(str, "%ld", (long int)lnumber_value(CAR(arg_list)->Eval()));
        ret = LString::Create(str);
        break;
    }
    case SysFunc::Nconc: {
        LObject *l1 = CAR(arg_list)->Eval();
        PtrRef r1(l1);
        arg_list = (LList *)CDR(arg_list);
        LObject *first = l1, *next;
        PtrRef r2(first);

        if (!l1)
        {
            l1 = first = CAR(arg_list)->Eval();
            arg_list = (LList *)CDR(arg_list);
        }

        if (item_type(l1) != L_CONS_CELL)
        {
            l1->Print();
            lbreak("first arg should be a list\n");
        }

        do
        {
            next = l1;
            while (next)
            {
                l1 = next;
                next = lcdr(next);
            }
            LObject *tmp = CAR(arg_list)->Eval();
            ((LList *)l1)->m_cdr = tmp;
            arg_list = (LList *)CDR(arg_list);
        } while (arg_list);
        ret = first;
        break;
    }
    case SysFunc::First:
        ret = CAR(CAR(arg_list)->Eval());
        break;
    case SysFunc::Second:
        ret = CAR(CDR(CAR(arg_list)->Eval()));
        break;
    case SysFunc::Third:
        ret = CAR(CDR(CDR(CAR(arg_list)->Eval())));
        break;
    case SysFunc::Fourth:
        ret = CAR(CDR(CDR(CDR(CAR(arg_list)->Eval()))));
        break;
    case SysFunc::Fifth:
        ret = CAR(CDR(CDR(CDR(CDR(CAR(arg_list)->Eval())))));
        break;
    case SysFunc::Sixth:
        ret = CAR(CDR(CDR(CDR(CDR(CDR(CAR(arg_list)->Eval()))))));
        break;
    case SysFunc::Seventh:
        ret = CAR(CDR(CDR(CDR(CDR(CDR(CDR(CAR(arg_list)->Eval())))))));
        break;
    case SysFunc::Eighth:
        ret = CAR(CDR(CDR(CDR(CDR(CDR(CDR(CDR(CAR(arg_list)->Eval()))))))));
        break;
    case SysFunc::Ninth:
        ret = CAR(CDR(CDR(CDR(CDR(CDR(CDR(CDR(CDR(CAR(arg_list)->Eval())))))))));
        break;
    case SysFunc::Tenth:
        ret = CAR(CDR(CDR(CDR(CDR(CDR(CDR(CDR(CDR(CDR(CAR(arg_list)->Eval()))))))))));
        break;
    case SysFunc::Substr: {
        int32_t x1 = lnumber_value(CAR(arg_list)->Eval());
        int32_t x2 = lnumber_value(CAR(CDR(arg_list))->Eval());
        LObject *st = CAR(CAR(CDR(arg_list)))->Eval();
        PtrRef r1(st);

        if (x1 < 0 || x1 > x2 || x2 >= (int32_t)strlen(lstring_value(st)))
            lbreak("substr: bad x1 or x2 value");

        LString *s = LString::Create(x2 - x1 + 2);
        if (x2 - x1)
            memcpy(lstring_value(s), lstring_value(st) + x1, x2 - x1 + 1);

        lstring_value(s)[x2 - x1 + 1] = 0;
        ret = s;
        break;
    }
    default:
        printf("Undefined system function number %d\n", fun_number);
        break;
    }

    return ret;
}

void tmp_space()
{
    LSpace::Current = &LSpace::Tmp;
}

void perm_space()
{
    LSpace::Current = &LSpace::Perm;
}

/* PtrRef check: OK */
LObject *LSymbol::EvalUserFunction(LList *arg_list)
{
    LObject *ret = NULL;
    PtrRef ref1(ret);

#ifdef TYPE_CHECKING
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("EVAL : is not a function name (not symbol either)");
        exit(EXIT_SUCCESS);
    }
#endif
#ifdef L_PROFILE
    time_marker start;
#endif

    LUserFunction *fun = (LUserFunction *)m_function;

#ifdef TYPE_CHECKING
    if (item_type(fun) != L_USER_FUNCTION)
    {
        Print();
        lbreak("is not a user defined function\n");
    }
#endif

    LList *fun_arg_list = fun->arg_list;
    LList *block_list = fun->block_list;
    PtrRef r9(block_list), r10(fun_arg_list);

    // mark the start start, so we can restore when done
    long stack_start = l_user_stack.m_size;

    // first push all of the old symbol values
    LObject *f_arg = NULL;
    PtrRef r18(f_arg);
    PtrRef r19(arg_list);

    for (f_arg = fun_arg_list; f_arg; f_arg = CDR(f_arg))
    {
        LSymbol *s = (LSymbol *)CAR(f_arg);
        l_user_stack.push(s->m_value);
    }

    // open block so that local vars aren't saved on the stack
    {
        int new_start = l_user_stack.m_size;
        int i = new_start;
        // now push all the values we wish to gather
        for (f_arg = fun_arg_list; f_arg; f_arg = CDR(f_arg))
        {
            if (!arg_list)
            {
                Print();
                lbreak("too few parameter to function\n");
                exit(EXIT_SUCCESS);
            }
            l_user_stack.push(CAR(arg_list)->Eval());
            arg_list = (LList *)CDR(arg_list);
        }

        // now store all the values and put them into the symbols
        for (f_arg = fun_arg_list; f_arg; f_arg = CDR(f_arg))
            ((LSymbol *)CAR(f_arg))->SetValue((LObject *)l_user_stack.sdata[i++]);

        l_user_stack.m_size = new_start;
    }

    if (f_arg)
    {
        Print();
        lbreak("too many parameter to function\n");
        exit(EXIT_SUCCESS);
    }

    // now evaluate the function block
    while (block_list)
    {
        ret = CAR(block_list)->Eval();
        block_list = (LList *)CDR(block_list);
    }

    long cur_stack = stack_start;
    for (f_arg = fun_arg_list; f_arg; f_arg = CDR(f_arg))
        ((LSymbol *)CAR(f_arg))->SetValue((LObject *)l_user_stack.sdata[cur_stack++]);

    l_user_stack.m_size = stack_start;

#ifdef L_PROFILE
    time_marker end;
    sym->time_taken += end.diff_time(&start);
#endif

    return ret;
}

/* PtrRef check: OK */
LObject *LObject::Eval()
{
    PtrRef ref1(this);

    int tstart = trace_level;

    if (trace_level)
    {
        if (trace_level <= trace_print_level)
        {
            printf("%d (%d, %d, %d) TRACE : ", trace_level, LSpace::Perm.GetFree(), LSpace::Tmp.GetFree(),
                   PtrRef::stack.m_size);
            Print();
            printf("\n");
        }
        trace_level++;
    }

    LObject *ret = NULL;

    if (this != nullptr)
    {
        switch (item_type(this))
        {
        case L_BAD_CELL:
            lbreak("error: eval on a bad cell\n");
            exit(EXIT_SUCCESS);
            break;
        case L_CHARACTER:
        case L_STRING:
        case L_NUMBER:
        case L_POINTER:
        case L_FIXED_POINT:
            ret = this;
            break;
        case L_SYMBOL:
            if (this == true_symbol)
                ret = this;
            else
            {
                ret = ((LSymbol *)this)->GetValue();
                if (item_type(ret) == L_OBJECT_VAR)
                    ret = (LObject *)l_obj_get(((LObjectVar *)ret)->m_index);
            }
            break;
        case L_CONS_CELL:
            ret = ((LSymbol *)CAR(this))->EvalFunction(CDR(this));
            break;
        default:
            this->Print();
            fprintf(stderr, "Unexpected LObject type %d\n", this->m_type);
            break;
        }
    }

    if (tstart)
    {
        trace_level--;
        if (trace_level <= trace_print_level)
            printf("%d (%d, %d, %d) TRACE ==> ", trace_level, LSpace::Perm.GetFree(), LSpace::Tmp.GetFree(),
                   PtrRef::stack.m_size);
        ret->Print();
        printf("\n");
    }

    return ret;
}

void Lisp::Init()
{
    LSymbol::root = NULL;
    total_user_functions = 0;

    LSpace::Tmp.m_free = LSpace::Tmp.m_data = (uint8_t *)malloc(0x10000);
    LSpace::Tmp.m_size = 0x10000;
    LSpace::Tmp.m_name = "temporary space";

    LSpace::Perm.m_free = LSpace::Perm.m_data = (uint8_t *)malloc(0x1000);
    LSpace::Perm.m_size = 0x1000;
    LSpace::Perm.m_name = "permanent space";

    LSpace::Gc.m_name = "garbage space";

    LSpace::Current = &LSpace::Perm;

    InitConstants();
    initSysFuncs();

    clisp_init();
    LSpace::Current = &LSpace::Tmp;
    printf("Lisp: defined %zu symbols, %d functions\n", LSymbol::count, total_user_functions);
}

void Lisp::Uninit()
{
    free(LSpace::Tmp.m_data);
    free(LSpace::Perm.m_data);
    DeleteAllSymbols(LSymbol::root);
    LSymbol::root = NULL;
    LSymbol::count = 0;
}

void LSpace::Clear()
{
    m_free = m_data;
}

LString *LSymbol::GetName()
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("is not a symbol\n");
        exit(EXIT_SUCCESS);
    }
#endif
    return m_name;
}

void LSymbol::SetNumber(long num)
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("is not a symbol\n");
        exit(EXIT_SUCCESS);
    }
#endif
    if (m_value != l_undefined && item_type(m_value) == L_NUMBER)
        ((LNumber *)m_value)->m_num = num;
    else
        m_value = LNumber::Create(num);
}

void LSymbol::SetValue(LObject *val)
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("is not a symbol\n");
        exit(EXIT_SUCCESS);
    }
#endif
    m_value = val;
}

LObject *LSymbol::GetFunction()
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("is not a symbol\n");
        exit(EXIT_SUCCESS);
    }
#endif
    return m_function;
}

LObject *LSymbol::GetValue()
{
#ifdef TYPE_CHECKING
    if (item_type(this) != L_SYMBOL)
    {
        Print();
        lbreak("is not a symbol\n");
        exit(EXIT_SUCCESS);
    }
#endif
    return m_value;
}
