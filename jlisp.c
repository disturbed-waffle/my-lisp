#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mpc.h"

#define true 1
#define false 0

#define JLISP_VERSION "0.1"
#define PROMPT ">>"

// wraping fgets as readline substitute for windows
#ifdef _WIN32
static char buffer[2048];

char *readline(const char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0'; // remove newline
  return cpy;
}

void add_history(char* unused) {}
#else 
#include <editline/readline.h>
#endif //_WIN32

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
    lval *err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
    }

#define LASSERT_TYPE(func, args, index, expect) ({ \
    LASSERT(args, args->cell[index]->type == expect, \
            "Function '%s' passed incorrect type for argument %i. " \
            "Got %s, Expected %s.", func, index, ltype_name(args->cell[index]->type), \
            ltype_name(expect)); \
})

#define LASSERT_NUM(func, args, num) ({ \
    LASSERT(args, args->count == num, \
            "Funciton '%s' passed incorrect number of arguments. " \
            "Got %i, Expected %i.", func, args->count, num); \
})

#define LASSERT_NOT_EMPTY(func, args, index) ({ \
    LASSERT(args, args->cell[index]->count != 0, \
            "Funciton '%s' passed {} for argument %i.", \
            func, index); \
})



// forward declaration

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// lval type
enum {
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR,
};

typedef lval *(*lbuiltin)(lenv *, lval *);

struct lval {
    int type;

    long num;
    char *err;
    char *sym;

    // Funciton
    lbuiltin builtin;
    lenv *env;
    lval *formals; // formal arguments
    lval *body; // Qexpression

    // count and array of *lval
    int count;
    struct lval **cell;
};

struct lenv {
    lenv *parent;
    int count;
    char **syms;
    lval **vals;
};

// function definitions
void lval_print(lval *v);
lval *lval_eval(lenv *e, lval *v);
lval *lval_call(lenv *e, lval *f, lval *a);
lenv *lenv_new();
lenv *lenv_copy(lenv *e);
void lenv_del(lenv *e);
lval *lenv_get(lenv *e, lval *k);
void lenv_put(lenv *e, lval *k, lval *v);
void lenv_def(lenv *e, lval *k, lval *v);

long pow_long(long x, long n) {
    if (n <= 0) return 1;

    long res = 1;
    for (int i = 0; i < n; i++) res *= x; 
    return res;
}


// create lval of type num
lval *lval_num(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// create lval of type err
lval *lval_err(char *fmt, ...) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    // create and initialize va list
    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);

    vsnprintf(v->err, 511, fmt, va);

    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(va);
    return v;
}

// create lval of type symbol
lval *lval_sym(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(sizeof(strlen(s)) + 1);
    strcpy(v->sym, s);
    return v;
}

lval *lval_fun(lbuiltin func) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
}

lval *lval_lambda(lval *formals, lval *body) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    // Not builtin
    v->builtin = NULL;

    v->env = lenv_new();

    v->formals = formals;
    v->body = body;
    return v;
}


// create lval of type Sexpr (list of expressions)
lval *lval_sexpr() {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// create lval of type Qexpr 
lval *lval_qexpr() {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

char *ltype_name(int t) {
    switch (t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

void lval_del(lval *v) {

    switch (v->type) {

        case LVAL_NUM: 
            break;
        case LVAL_ERR: free(v->err);
            break;
        case LVAL_SYM: free(v->sym);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
        case LVAL_FUN: 
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
    }
    free(v);
}

lval *lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

// Deep Copy
lval *lval_copy(lval *v) {

    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_FUN: 
            if (v->builtin) {
            x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;
        case LVAL_NUM: x->num = v->num;
            break;

        case LVAL_ERR: 
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;

        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval *) * x->count);
            for (int i = 0; i < x->count; i ++) {
                x->cell[i]= lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10); //strtol sets errno for errors
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_read(mpc_ast_t *t) {
    // Symbol or Number
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    // If root (>) or sexpr create empty list
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
    if (strstr(t->tag, "sexpr")) x = lval_sexpr();
    if (strstr(t->tag, "qexpr")) x = lval_qexpr();


    // fill list with valid expressions
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        if (strcmp(t->children[i]->contents, ")") == 0) continue;
        if (strcmp(t->children[i]->contents, "{") == 0) continue;
        if (strcmp(t->children[i]->contents, "}") == 0) continue;
        if (strcmp(t->children[i]->tag, "regex") == 0) continue;
        
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_expr_print(lval *v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num);
            break;
        case LVAL_ERR: printf("Error: %s", v->err);
            break;
        case LVAL_SYM: printf("%s", v->sym);
            break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}');
            break;
        case LVAL_FUN: 
            if (v->builtin) {
                printf("<function>");
            } else {
                printf(" (\\ ");
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(' ');
            }
            break;
    }
}

void lval_println(lval *v) {
    lval_print(v);
    putc('\n', stdout);
}

lval *lval_pop(lval *v, int i) {

    lval *x = v->cell[i];
    
    // shift memory after i back one
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(lval*) * (v->count - i-1));

    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval *lval_take(lval *v, int i) {
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval *lval_join(lval *x, lval *y) {

    // for each cell in x add it to y
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}


lval *builtin_op(lenv *e, lval *a, char *op) {
    
    // Ensure all numbers
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE(op, a, i, LVAL_NUM);
    }

    //first element
    lval *x = lval_pop(a, 0);

    // unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {

        lval *y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) x->num += y->num;
        if (strcmp(op, "-") == 0) x->num -= y->num;
        if (strcmp(op, "*") == 0) x->num *= y->num;
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero!");
                break;
            }
        }
        if (strcmp(op, "^") == 0) {
            x->num = pow_long(x->num, y->num);
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval *builtin_add(lenv *e, lval *a) {
  return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a) {
  return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a) {
  return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a) {
  return builtin_op(e, a, "/");
}

lval *builtin_pow(lenv *e, lval *a) {
    return builtin_op(e, a, "^");
}

lval *builtin_head(lenv *e, lval *a) {
    // Error conditions
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);
    // Take first element
    lval *v = lval_take(a, 0);
    // Delete all elements that are not head
    while (v->count > 1) lval_del(lval_pop(v, 1));
    return v;
}

lval *builtin_tail(lenv *e, lval *a) {
    // Error conditions
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    // Take first element
    lval *v = lval_take(a, 0);

    // Delete it's first element
    lval_del(lval_pop(v, 0));
    return v;

}

lval *builtin_list(lenv *e, lval *a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval *builtin_eval(lenv *e, lval *a) {
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval *builtin_join(lenv *e, lval *a) {
    // Make sure all are Qexpr
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("joint", a, i, LVAL_QEXPR);
    }

    lval *x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval *builtin_var(lenv *e, lval *a, char *func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    // first argument is symbol list
    lval *syms = a->cell[0];
    // ensure all elements of first list are symbols
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
                "Function '%s' cannot define non-symbol. "
                "Got %s, Expected %s.", func,
                ltype_name(syms->cell[i]->type),
                ltype_name(LVAL_SYM));
    }

    // check correct number of values to symbols
    LASSERT(a, syms->count == a->count - 1,
            "Function '%s' passed too many arguments for symbols. "
            "Got %i, Expected %i.", func, syms->count, a->count-1);

    // assign copies of value to symbols
    for (int i = 0; i < syms->count; i++) {
        // 'def' in global 'put' in local
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }
        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }

    }

    lval_del(a);
    return lval_sexpr();
}

lval *builtin_def(lenv *e, lval *a) {
    return builtin_var(e, a, "def");
}

lval *builtin_put(lenv *e, lval *a) {
    return builtin_var(e, a, "=");
}

lval *builtin_lambda(lenv *e, lval *a) {
    // Check two arguments, each are Q-expr
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    // check that first Q-expression only contains symbols
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
                "Cannot define none-symbol. Got %s, Expected %s.",
                ltype_name(a->cell[0]->cell[i]->type), 
                ltype_name(LVAL_SYM));
    }

    // Pop first two arguments and pass to lval_lambda
    lval *formals = lval_pop(a, 0);
    lval *body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {

    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }
    
    // Check children for errors
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) return lval_take(v, i);
    }

    // Empty expression
    if (v->count == 0) return v;
    // Single expression
    if (v->count == 1) return lval_take(v, 0);

    // Ensure first element is a function after eval
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval *err = lval_err("S-Expression starts with incorrect type. "
        "Got %s, Expected %s.", ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(v);
        lval_del(f);
        return err;
    }

    // call function to get result
    lval *result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv *e, lval *v) {
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr(e, v);

    return v;
}

lval *lval_call(lenv *e, lval *f, lval *a) {

    if (f->builtin) return f->builtin(e, a);

    // Argument counts
    int given = a->count;
    int total = f->formals->count;

    // while args still remain
    while (a->count) {
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function passed too many arguments. "
            "Got %i, Expected %i.", given, total);
        }

        // Pop first symbol from formals
        lval *sym = lval_pop(f->formals, 0);

        // Special case for '&'
        if (strcmp(sym->sym, "&") == 0) {
            // ensure & is followed by symbol
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. " 
                "Symbol '&' not followed by single symbol.");
            }
            // next formal is bound to remaining arguments
            lval *nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        // next arg from list
        lval *val = lval_pop(a, 0);
        lenv_put(f->env, sym, val);

        lval_del(sym);
        lval_del(val);
    }
    // arguments now bound
    lval_del(a);

    // if '&' remains in formal list bind to empty list
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {

        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                            "Symbol '&' not followed by single symbol.");
        }

        // pop and delete '&'
        lval_del(lval_pop(f->formals, 0));

        // pop next symbol and create empty list
        lval *sym = lval_pop(f->formals, 0);
        lval *val = lval_qexpr();

        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }

    // evaluate if formals are all bound
    if (f->formals->count == 0) {
        // set parent
        f->env->parent = e;

        // eval the body
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        // otherwise return partial function
        return lval_copy(f);
    }
}


lenv *lenv_new() {
    lenv *e = malloc(sizeof(lval));
    e->parent = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]); // syms are strings
        lval_del(e->vals[i]);

    }
}

lenv *lenv_copy(lenv *e) {
    lenv *n = malloc(sizeof(lenv));
    n->parent = e->parent;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

// get lval from enviroment with given key (sym -> fun)
lval *lenv_get(lenv *e, lval *k) {
    
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    // if no symbol check in parent environment
    if (e->parent) {
        return lenv_get(e->parent, k);
    } else {
    return lval_err("Unbound symbol '%s'", k->sym);
    }

}

void lenv_put(lenv *e, lval *k, lval *v) {
    // check if already exists
    // and replace with v
    for (int i = 0; i < e->count; i++) {
        if(strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // allocate for new entry
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval *) * e->count);
    e->syms = realloc(e->syms, sizeof(char *) * e->count);


    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

// global variable definition
void lenv_def(lenv *e, lval *k, lval *v) {
    while (e->parent) e = e->parent;

    lenv_put(e, k, v);
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);

    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv *e) {
    // List Functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    // Math functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "^", builtin_pow);

    // Variable Functions
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
}

int main(int argc, char *argv[]) {
    // create parsers
    mpc_parser_t *number = mpc_new("number");
    mpc_parser_t *symbol = mpc_new("symbol");
    mpc_parser_t *sexpr = mpc_new("sexpr");
    mpc_parser_t *qexpr = mpc_new("qexpr");
    mpc_parser_t *expr = mpc_new("expr");
    mpc_parser_t *lispy = mpc_new("lispy");

    // parser language definitions
    mpca_lang(MPCA_LANG_DEFAULT,

        "number   : /-?[0-9]+/ ;                               "
        "symbol   : /[a-zA-Z0-9_+\\-*^\\/\\\\=<>!&]+/ ;         "
        "sexpr    : '(' <expr>* ')' ;                          "
        "qexpr    : '{' <expr>* '}' ;                          "
        "expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  "
        "lispy    : /^/ <expr>* /$/ ;                          ",

              number, symbol, sexpr, qexpr, expr, lispy);

    printf("Jlisp Version %s\n", JLISP_VERSION);
    printf("Press Ctrl+c to Exit\n");

    // Environment
    lenv *e = lenv_new();
    lenv_add_builtins(e);

    while (true) {
        char *input = readline(PROMPT);
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, lispy, &r)) {
            lval *x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    lenv_del(e);
    mpc_cleanup(6, number, symbol, sexpr, qexpr, expr, lispy);
    return 0;

}
