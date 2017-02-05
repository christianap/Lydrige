/* Lydrige - A simple interpreted programming language inspired by Lisp.
 * Copyright (c) 2016, Christian Seibold
 * Under MIT License
 *
 * You can find latest source code at:
 * https://github.com/krixano/Lydrige
 *
 * -----------------------------------------------------------------------
 * MIT License
 *
 * Copyright (c) 2016 Christian Seibold
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <colors.h>

#include <mpc.h>
#include <structure.h>
#include <builtin.h>

// STB and Single File Header Libs
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
//#define GB_IMPLEMENTATION
//#include <gb.h>

#ifdef _WIN32
#include <windows.h>

static char buffer[2048];

char* readline() {
    fgets(buffer, 2048, stdin);
    char *cpy = (char *) malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    
    return cpy;
}

#else
#include <linenoise.h>
#endif

typedef struct dval_or_darray {
    bool isArray;
    dval *result;
} dval_or_darray;

internal dval *read_eval_expr(denv *e, mpc_ast_t *t);

// Returns int of amount of args
internal int 
arg_amt(mpc_ast_t *t, bool isQExpr)
{
    unsigned int argc = 0;
    for (unsigned int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        else if (strcmp(t->children[i]->contents, ")") == 0) continue;
        else if (strcmp(t->children[i]->contents, "'(") == 0) continue;
        else if (strcmp(t->children[i]->contents, "[") == 0) continue;
        else if (strcmp(t->children[i]->contents, "]") == 0) continue;
        else if (strcmp(t->children[i]->contents, ",") == 0) continue;
        else if (strcmp(t->children[i]->contents, ";") == 0) continue;
        else if (strcmp(t->children[i]->contents, "{") == 0) continue;
        else if (strcmp(t->children[i]->contents, "}") == 0) continue;
        else if (strcmp(t->children[i]->contents, "'") == 0) continue;
        else if (strcmp(t->children[i]->tag, "regex") == 0) continue;
        if (strstr(t->children[i]->tag, "ident") && !strstr(t->children[i]->tag, "value") && !isQExpr) continue;
        
        argc++;
    }
    
    return argc;
}

// Returns array of dvals
internal dval_or_darray
eval_args(int argc, mpc_ast_t *t, char **ident, denv *e, bool isQExpr)
{
    dval *args = (dval*) calloc(argc, sizeof(dval));
    
    // Check for NULL, and return an error
    if (args == NULL) {
        return((dval_or_darray) { false, dval_error("Unable to allocate memory for arguments.") });
    }
    
    unsigned int currentArgPos = 0;
    for (unsigned int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        else if (strcmp(t->children[i]->contents, ")") == 0) continue;
        else if (strcmp(t->children[i]->contents, "'(") == 0) continue;
        else if (strcmp(t->children[i]->contents, "[") == 0) continue;
        else if (strcmp(t->children[i]->contents, "]") == 0) continue;
        else if (strcmp(t->children[i]->contents, ",") == 0) continue;
        else if (strcmp(t->children[i]->contents, ";") == 0) continue;
        else if (strcmp(t->children[i]->contents, "{") == 0) continue;
        else if (strcmp(t->children[i]->contents, "}") == 0) continue;
        else if (strcmp(t->children[i]->contents, "'") == 0) continue;
        else if (strcmp(t->children[i]->tag, "regex") == 0) continue;
        if (strstr(t->children[i]->tag, "ident") && !strstr(t->children[i]->tag, "value") && isQExpr) {
            args[currentArgPos] = (dval) { DVAL_IDENT, 0, { .str = t->children[i]->contents} };
            
            currentArgPos++;
            continue;
        }
        if (strstr(t->children[i]->tag, "ident") && !strstr(t->children[i]->tag, "value") && ident != NULL) {
            *ident = t->children[i]->contents;
            continue;
        }
        
        if (strstr(t->children[i]->tag, "expression")) {
            if (isQExpr) {
                unsigned int largc = arg_amt(t->children[i], true);
                dval_or_darray elements = eval_args(largc, t->children[i], NULL, e, true);
                if (!elements.isArray) { // If not an array, then it returned an error
                    return((dval_or_darray) { false, elements.result });
                }
                
                args[currentArgPos] = (dval) { DVAL_EXPR, 0, { .elements = elements.result }, largc };
                currentArgPos++;
            } else {
                dval *d = read_eval_expr(e, t->children[i]);
                
                args[currentArgPos] = *d; // TODO: This gets coppied over. Is there a better way?
                if (d->type == DVAL_ERROR) {
                    free(args);
                    return((dval_or_darray) { false, d });
                } else {
                    dval_del(d);
                }
                currentArgPos++;
            }
        } else if (strstr(t->children[i]->tag, "qexpr")) {
            unsigned int largc = arg_amt(t->children[i], true);
            dval_or_darray elements = eval_args(largc, t->children[i], NULL, e, true);
            if (!elements.isArray) { // If not an array, then it returned an error
                return((dval_or_darray) { false, elements.result });
            }
            
            args[currentArgPos] = (dval) { DVAL_QEXPR, 0, { .elements = elements.result }, largc };
            currentArgPos++;
        } else if (strstr(t->children[i]->tag, "list")) { // TODO
            unsigned int largc = arg_amt(t->children[i], isQExpr);
            dval_or_darray elements = eval_args(largc, t->children[i], NULL, e, isQExpr);
            if (!elements.isArray) { // If not an array, then it returned an error
                return((dval_or_darray) { false, elements.result });
            }
            
            args[currentArgPos] = (dval) { DVAL_LIST, 0, { .elements = elements.result }, largc };
            currentArgPos++;
        } else if (strstr(t->children[i]->tag, "statement")) {
            free(args);
            return((dval_or_darray) { false, read_eval_expr(e, t->children[i])}); // TODO(BUG): This will result in lines returning only its first statement's value
        } else if (strstr(t->children[i]->tag, "command")) { // REPL Only
            if (strcmp(t->children[i]->children[1]->contents, "exit") == 0) {
                running = false;
                
                free(args);
                return((dval_or_darray) { false, dval_info("Program Exited with Result: 1\n"
                                                           "(User Interruption)\n") });
            } else if (strcmp(t->children[i]->children[1]->contents, "version") == 0) { // TODO: Make global version string
                printf("Lydrige Version v0.6.0a\n");
                printf("Copyright (c) 2016-2017, Christian Seibold All Rights Reserved\n");
                printf("Under MIT License\n");
                printf("\n");
                printf("Uses MPC Library under the BSD-2-Clause License\n");
                printf("Copyright (c) 2013, Daniel Holden All Rights Reserved\n");
                printf("https://github.com/orangeduck/mpc/\n");
                printf("\n");
                printf("Uses Linenoise Library under the BSD-2-Clause License\n");
                printf("Copyright (c) 2010-2014, Salvatore Sanfilippo <antirez at gmail dot com>\n");
                printf("Copyright (c) 2010-1013, Pieter Noordhuis <pcnoordhuis at gmail dot com>\n");
                printf("https://github.com/antirez/linenoise/\n");
                
                free(args);
                return((dval_or_darray) { false, 0 });
            } else if (strcmp(t->children[i]->children[1]->contents, "clear") == 0) {
                // TODO: Use builtin_clear(null, null, 0) instead?
                
#ifdef _WIN32
                system("cls");
#else
                linenoiseClearScreen();
#endif
                
                free(args);
                return((dval_or_darray) { false, 0 });
            } else if (strcmp(t->children[i]->children[1]->contents, "builtins") == 0) {
                colors_printf(COLOR_YELLOW, "basic operators  "); colors_printf(COLOR_CYAN, "[+, -, *, /, mod, ^]"); printf("            returns result of respective operation\n");
                colors_printf(COLOR_YELLOW, "succ   :num      "); colors_printf(COLOR_MAGENTA, "[n  :num]                    "); printf("returns succession of number [n] (num + 1)\n");
                colors_printf(COLOR_YELLOW, "list   :array    "); colors_printf(COLOR_MAGENTA, "[&v :any]                    "); printf("returns list with given values [&v] as its elements\n");
                colors_printf(COLOR_YELLOW, "len    :int      "); colors_printf(COLOR_MAGENTA, "[l  :array]                  "); printf("returns length of given list [l] as an integer\n");
                colors_printf(COLOR_YELLOW, "get    :any      "); colors_printf(COLOR_MAGENTA, "[i  :int,  l :array]         "); printf("returns element at given index [i] from given list [l]\n");
                colors_printf(COLOR_YELLOW, "set    :any      "); colors_printf(COLOR_MAGENTA, "[i  :int,  v: any,  l :array]"); printf("returns copy of given list [l] with the element at given index [i] replaced with given value [v]\n");
                colors_printf(COLOR_YELLOW, "first  :any      "); colors_printf(COLOR_MAGENTA, "[l  :array]                  "); printf("returns first element of given list [l]\n");
                colors_printf(COLOR_YELLOW, "last   :any      "); colors_printf(COLOR_MAGENTA, "[l  :array]                  "); printf("returns last element of given list [l]\n");
                colors_printf(COLOR_YELLOW, "head   :array    "); colors_printf(COLOR_MAGENTA, "[l  :array]                  "); printf("returns list of all but last element of given list [l]\n");
                colors_printf(COLOR_YELLOW, "tail   :array    "); colors_printf(COLOR_MAGENTA, "[l  :array]                  "); printf("returns list of all but first element of given list [l]\n");
                colors_printf(COLOR_YELLOW, "join   :array    "); colors_printf(COLOR_MAGENTA, "[&l :array]                  "); printf("returns list of given lists [&l] joined together\n");
                colors_printf(COLOR_YELLOW, "typeof :type     "); colors_printf(COLOR_MAGENTA, "[v: any]                     "); printf("returns type of given value [v]\n");
                colors_printf(COLOR_YELLOW, "print  :int      "); colors_printf(COLOR_MAGENTA, "[&v :any]                    "); printf("prints out given values [&v], returns 1\n");
                colors_printf(COLOR_YELLOW, "read   :string   "); colors_printf(COLOR_MAGENTA, "[prompt :string]             "); printf("returns input from the user, will print out given string [prompt]\n");
                
                free(args);
                return((dval_or_darray) { false, 0 });
            } else if (strcmp(t->children[i]->children[1]->contents, "commands") == 0) {
                colors_printf(COLOR_YELLOW, "version  "); printf("version and copyright info\n");
                colors_printf(COLOR_YELLOW, "clear    "); printf("clears the screen\n");
                colors_printf(COLOR_YELLOW, "builtins "); printf("list all builtin functions\n");
                colors_printf(COLOR_YELLOW, "commands "); printf("list all REPL command, each should be prefaced with ':'\n");
                colors_printf(COLOR_YELLOW, "exit     "); printf("exit the REPL\n");
                
                free(args);
                return((dval_or_darray) { false, 0 });
            } else {
                free(args);
                return((dval_or_darray) { false, dval_error("Command doesn't exist.") });
            }
        } else if (strstr(t->children[i]->tag, "value")) {
            if (strstr(t->children[i]->tag, "qident")) {
                args[currentArgPos] = (dval) { DVAL_IDENT, 0, { .str=t->children[i]->children[1]->contents } };
            } else if (strstr(t->children[i]->tag, "ident")) {
                // TODO(FUTURE): Handle unary operators here
                // Evaluate identifier here, and add result to args
                
                if (isQExpr) {
                    args[currentArgPos] = (dval) { DVAL_IDENT, 0, { .str=t->children[i]->contents } };
                } else {
                    dval *v = denv_get(e, t->children[i]->contents);
                    
                    if (v->type == DVAL_ERROR) {
                        free(args);
                        return((dval_or_darray) { false, v });
                    } else {
                        args[currentArgPos] = *v; // TODO: This gets copied
                    }
                }
            } else if (strstr(t->children[i]->tag, "integer")) {
                args[currentArgPos] = (dval) { DVAL_INT, 0, {strtol(t->children[i]->contents, NULL, 10)} };
            } else if (strstr(t->children[i]->tag, "double")) {
                args[currentArgPos] = (dval) { DVAL_DOUBLE, 0, {.doub=strtod(t->children[i]->contents, NULL)} };
            } else if (strstr(t->children[i]->tag, "character")) {
                args[currentArgPos] = (dval) { DVAL_CHARACTER, 0, {.character=t->children[i]->contents[1]} };
            } else if (strstr(t->children[i]->tag, "string")) {
                char *substring;
                int substrlen = strlen(t->children[i]->contents) - 1;
                substring = malloc(substrlen * sizeof(char));
                memcpy(substring, &t->children[i]->contents[1], substrlen);
                substring[substrlen-1] = '\0';
                
                args[currentArgPos] = (dval) { DVAL_STRING, 0, { .nstr=substring }, 30 };
            }
            
            currentArgPos++;
        } else {
            free(args);
            return((dval_or_darray) { false, dval_error("[Interpreter] A value type was added to the parser but its evaluation is not handled. [%s]", t->children[i]->tag) });
        }
    }
    
    return((dval_or_darray) { true, args });
}

internal dval
*read_eval_expr(denv *e, mpc_ast_t *t)
{
    char *ident = ""; // TODO(Future): Eventually allow lambdas for function calls (also evaluate identifiers to be builtin functions or lambdas)
    
    unsigned int argc = arg_amt(t, false);
    dval_or_darray args = eval_args(argc, t, &ident, e, false);
    if (!args.isArray) {
        return(args.result);
    }
    
    if (strcmp(t->tag, ">") == 0) {
        free(args.result);
        return(NULL);
    }
    
    // Function/Lambda call
    dval *func = denv_get(e, ident);
    if (func->type == DVAL_FUNC) {
        dval *v = func->func(e, args.result, argc);
        
        free(args.result);
        free(func);
        return(v);
    } else if (func->type == DVAL_ERROR || func->type == DVAL_INFO) {
        free(args.result);
        return(func);
    } else {
        free(args.result);
        free(func);
        return(dval_error("'%s' is not a function or lambda.", ident)); // TODO: Error?
    }
}

void printStart(void)
{
    colors_printf(COLOR_GREEN, "Lydrige REPL - v0.6.0a\n\n");
    
    setColor(COLOR_CYAN);
    printf("Type ':exit' to Exit the REPL\n");
    printf("Type ':builtins' to get a list of builtin functions\n");
    printf("\n");
    resetColor();
}

void win32PrintPrompt(void)
{
    colors_printf(COLOR_GREEN, "Lydrige> ");
}

void printREPLResult(dval *result)
{
    printf("\n");
    
    if (result->type == DVAL_ERROR) {
        colors_printf(COLOR_RED, "Error: %s\n", result->str);
    } else if (result->type == DVAL_INFO) {
        colors_printf(COLOR_CYAN, "Info: %s\n", result->str);
    } else {
        printf(" -> ");
        
        bool known = print_elem(*result, false);
        
        printf("\n");
        
        if (!known) {
            colors_printf(COLOR_RED, "Error: Cannot print value of type Unknown or Any!\n");
        }
    }
}

void unixSetupLinenoise()
{
#ifndef _WIN32
    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(20);
#endif
}

char *getInput()
{
    char *input;
#ifdef _WIN32
    win32PrintPrompt();
    input = readline();
#else
    input = linenoise("Lydrige> ");
    linenoiseHistoryAdd(input);
#endif
    
    return input;
}

void freeInput(char *input)
{
#ifdef _WIN32
    free(input);
#else
    linenoiseFree(input);
#endif
}

int main(int argc, char** argv) // TODO: Possible memory leak from not calling bdestroy for all bstrings!
{
#ifdef _WIN32
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    
    Line = mpc_new("line");
    Command = mpc_new("command");
    Statement = mpc_new("statement");
    Expression = mpc_new("expression");
    Value = mpc_new("value");
    Integer = mpc_new("integer");
    Double = mpc_new("double");
    Character = mpc_new("character");
    String = mpc_new("string");
    Identifier = mpc_new("ident");
    QIdentifier = mpc_new("qident");
    List = mpc_new("list");
    Qexpression = mpc_new("qexpr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
              "line : /^/ <command> /$/ | /^/ <statement>* /$/ ;"
              "command : ':' <ident> ;"
              "statement : <ident> <value>* ';' ;"
              "expression : '(' <ident> <value>* ')' ;"
              "value : <double> | <integer> | <character> | <string> | <expression> | <ident> | <qident> | <list> | <qexpr> ;"
              "double : /-?[0-9]+\\.[0-9]+/ ;"
              "integer : /-?[0-9]+/ ;"
              "character : /\'(\\\\.|[^\"])\'/ ;"
              "string : /\"(\\\\.|[^\"])*\"/ ;"
              "ident : /[a-zA-Z0-9_\\-*\\/\\\\=<>!^%]+/ | '&' | '+' ;"
              "qident : '\\'' /[a-zA-Z0-9_\\-*\\/\\\\=<>!^%]+/ ;"
              "list : '[' (<value> (',' <value>)*)? ']' ;"
              "qexpr : '{' <ident> <value>* '}' ;",
              Line, Command, Statement, Expression, Value, Double, Integer, Character, String, Identifier, QIdentifier, List, Qexpression);
    
    if (argc == 1) {
        // REPL
        printStart();
        
        denv *e = denv_new();
        denv_add_builtins(e);
        unixSetupLinenoise();
        
        while (running) {
            char *input = getInput();
            
            mpc_result_t r;
            if (mpc_parse("<stdin>", input, Line, &r)) {
                //mpc_ast_print((mpc_ast_t*) r.output); puts("");
                dval *result = read_eval_expr(e, (mpc_ast_t *) r.output);
                if (!result) {
                    printf("\n");
                    mpc_ast_delete((mpc_ast_t *) r.output);
                    freeInput(input);
                    continue;
                }
                
                printREPLResult(result);
                
                dval_del(result);
                mpc_ast_delete((mpc_ast_t *) r.output);
            } else {
                mpc_err_print(r.error);
                mpc_err_delete(r.error);
            }
            
            freeInput(input);
        }
        
        denv_del(e);
    } else if (argc == 2) {
        // Read file and evaluate each line here!
    }
    
    mpc_cleanup(12, Line, Command, Statement, Expression, Value, Double, Integer, Character, String, Identifier, QIdentifier, List, Qexpression);
    return(0);
}
