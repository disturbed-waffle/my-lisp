#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mpc.h"

#define true 1
#define false 0

#define JLISP_VERSION 0.1
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

#define LASSERT(args, cond, err) \
if (!(cond)) { lval_del(args); return lval_err(err);} \

// lval type
enum {
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
};

// error
enum {
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM,
};

typedef struct lval{
    int type;
    long num;
    // error and symbol types have string data
    char *err;
    char *sym;

    // count and array of *lval
    int count;
    struct lval **cell;
} lval;

// declaration
void lval_print(lval *v);
lval *lval_eval(lval *v);

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
lval *lval_err(char *m) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(sizeof(strlen(m)) + 1);
    strcpy(v->err, m);
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
    }
    free(v);
}

lval *lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
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

lval *builtin_op(lval *a, char *op) {
    
    // Ensure all numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number");
        }
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

lval *builtin_head(lval *a) {
    // Error conditions
    LASSERT(a, a->count == 1,
        "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'head' passed {}!");

    // Take first element
    lval *v = lval_take(a, 0);
    // Delete all elements that are not head
    while (v->count > 1) lval_del(lval_pop(v, 1));
    return v;
}

lval *builtin_tail(lval *a) {
    // Error conditions
    LASSERT(a, a->count == 1,
        "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'tail' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'tail' passed {}!");

    // Take first element
    lval *v = lval_take(a, 0);

    // Delete it's first element
    lval_del(lval_pop(v, 0));
    return v;

}

lval *lval_eval_sexpr(lval *v) {

    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }
    
    // Check children for errors
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) return lval_take(v, i);
    }

    // Empty expression
    if (v->count == 0) return v;

    // Single expression
    if (v->count == 1) return lval_take(v, 0);

    // Ensure first element is symbol
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }

    // call builtin with the operator
    lval *result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval *v) {
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr(v);

    return v;
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
        "symbol   : \"list\" | \"head\" | \"tail\" | \"join\"  "
        "         | \"eval\" | '+' | '-' | '*' | '/' | '^' ;   "
        "sexpr    : '(' <expr>* ')' ;                          "
        "qexpr    : '{' <expr>* '}' ;                          "
        "expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  "
        "lispy    : /^/ <expr>* /$/ ;                          ",

              number, symbol, sexpr, qexpr, expr, lispy);

    printf("Jlisp Version %f\n", JLISP_VERSION);
    printf("Press Ctrl+c to Exit\n");

    while (true) {
        char *input = readline(PROMPT);
        add_history(input);
        mpc_result_t r;

        if (mpc_parse("<stdin>", input, lispy, &r)) {
            lval *x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(6, number, symbol, sexpr, qexpr, expr, lispy);
    return 0;

}
