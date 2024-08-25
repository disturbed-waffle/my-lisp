#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#include <editline/readline.h>

long eval_op(long x, char* op, long y){
    if (strcmp(op, "+")==0) {return x + y;}
    if (strcmp(op, "-")==0) {return x - y;}
    if (strcmp(op, "*")==0) {return x * y;}
    if (strcmp(op, "/")==0) {return x / y;}
    if (strcmp(op, "%")==0) {return x % y;}
    if (strcmp(op, "^")==0) {
        long total = x;
        for (int i=1;i<y;i++) {
            total *= x;
            }
        return total;
    }
    return 0;
}

long eval(mpc_ast_t* t){

    // if tagged as number return it directly 
    if (strstr(t->tag, "number")){
        return atoi(t->contents);
    }

    // the operator is always the second child of expression
    char* op = t->children[1]->contents;

    // store third child in x
    long x = eval(t->children[2]);

    // Iterate remaining children (other expr)
    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

int main(int argc, char** argv){
// Create some parsers
mpc_parser_t* Number = mpc_new("number");
mpc_parser_t* Operator = mpc_new("operator");
mpc_parser_t* Expr = mpc_new("expr");
mpc_parser_t* Lisp = mpc_new("lisp");

// Define the parsers with the following language
mpca_lang(MPCA_LANG_DEFAULT,
"                                                       \
    number   : /-?[0-9]+/;                              \
    operator : '+' | '-' | '*' | '/' | '%' | '^' ;      \
    expr     : <number> | '(' <operator> <expr>+ ')' ;  \
    lisp     : /^/ <operator> <expr>+ /$/ ;             \
",
Number, Operator, Expr, Lisp);

// Version and exit info
puts("JanLisp Version 0.01");
puts("Cntrl+c to Exit!\n>>>");

while (1)
{   
    // prompt and get inpu
    char* input = readline(">>> ");

    // store history
    add_history(input);

    // attempt to parse user input
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lisp, &r)){
        long result = eval(r.output);
        printf("%li\n", result);
        mpc_ast_delete(r.output);
    }else{
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }

    // Free retrieved input
    free(input);
}   

// Undefine and delete parsers
mpc_cleanup(4, Number, Operator, Expr, Lisp);

return 0;

}