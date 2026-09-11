# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1175/1354 lines (86.78%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "ph7int.h"` |
|          - |    7 | `/*` |
|          - |    8 | ` * This file implement a hand-coded, thread-safe, full-reentrant and highly-efficient` |
|          - |    9 | ` * expression parser for the PH7 engine.` |
|          - |   10 | ` * Besides from the one introudced by PHP (Over 60), the PH7 engine have introduced three new` |
|          - |   11 | ` * operators. These are 'eq', 'ne' and the comma operator ','.` |
|          - |   12 | ` * The eq and ne operators are borrowed from the Perl world. They are used for strict` |
|          - |   13 | ` * string comparison. The reason why they have been implemented in the PH7 engine` |
|          - |   14 | ` * and introduced as an extension to the PHP programming language is due to the confusion` |
|          - |   15 | ` * introduced by the standard PHP comparison operators ('==' or '===') especially if you` |
|          - |   16 | ` * are comparing strings with numbers.` |
|          - |   17 | ` * Take the following example:` |
|          - |   18 | ` * var_dump( 0xFF == '255' ); // bool(true) ???` |
|          - |   19 | ` * // use the type equal operator by adding a single space to one of the operand` |
|          - |   20 | ` * var_dump( '255  ' === '255' ); //bool(true) depending on the PHP version` |
|          - |   21 | ` * That is, if one of the operand looks like a number (either integer or float) then PHP` |
|          - |   22 | ` * will internally convert the two operands to numbers and then a numeric comparison is performed.` |
|          - |   23 | ` * This is what the PHP language reference manual says:` |
|          - |   24 | ` * If you compare a number with a string or the comparison involves numerical strings, then each` |
|          - |   25 | ` * string is converted to a number and the comparison performed numerically.` |
|          - |   26 | ` * Bummer, if you ask me,this is broken, badly broken. I mean,the programmer cannot dictate` |
|          - |   27 | ` * it's comparison rule, it's the underlying engine who decides in it's place and perform` |
|          - |   28 | ` * the internal conversion. In most cases,PHP developers wants simple string comparison and they` |
|          - |   29 | ` * are stuck to use the ugly and inefficient strcmp() function and it's variants instead.` |
|          - |   30 | ` * This is the big reason why we have introduced these two operators.` |
|          - |   31 | ` * The eq operator is used to compare two strings byte per byte. If you came from the C/C++ world` |
|          - |   32 | ` * think of this operator as a barebone implementation of the memcmp() C standard library function.` |
|          - |   33 | ` * Keep in mind that if you are comparing two ASCII strings then the capital letters and their lowercase` |
|          - |   34 | ` * letters are completely different and so this example will output false.` |
|          - |   35 | ` * var_dump('allo' eq 'Allo'); //bool(FALSE)` |
|          - |   36 | ` * The ne operator perform the opposite operation of the eq operator and is used to test for string` |
|          - |   37 | ` * inequality. This example will output true` |
|          - |   38 | ` * var_dump('allo' ne 'Allo'); //bool(TRUE) unequal strings` |
|          - |   39 | ` * The eq operator return a Boolean true if and only if the two strings are identical while the` |
|          - |   40 | ` * ne operator return a Boolean true if and only if the two strings are different. Otherwise` |
|          - |   41 | ` * a Boolean false is returned (equal strings).` |
|          - |   42 | ` * Note that the comparison is performed only if the two strings are of the same length.` |
|          - |   43 | ` * Otherwise the eq and ne operators return a Boolean false without performing any comparison` |
|          - |   44 | ` * and avoid us wasting CPU time for nothing.` |
|          - |   45 | ` * Again remember that we talk about a low level byte per byte comparison and nothing else.` |
|          - |   46 | ` * Also remember that zero length strings are always equal.` |
|          - |   47 | ` *` |
|          - |   48 | ` * Again, another powerful mechanism borrowed from the C/C++ world and introduced as an extension` |
|          - |   49 | ` * to the PHP programming language.` |
|          - |   50 | ` * A comma expression contains two operands of any type separated by a comma and has left-to-right` |
|          - |   51 | ` * associativity. The left operand is fully evaluated, possibly producing side effects, and its` |
|          - |   52 | ` * value, if there is one, is discarded. The right operand is then evaluated. The type and value` |
|          - |   53 | ` * of the result of a comma expression are those of its right operand, after the usual unary conversions.` |
|          - |   54 | ` * Any number of expressions separated by commas can form a single expression because the comma operator` |
|          - |   55 | ` * is associative. The use of the comma operator guarantees that the sub-expressions will be evaluated` |
|          - |   56 | ` * in left-to-right order, and the value of the last becomes the value of the entire expression.` |
|          - |   57 | ` * The following example assign the value 25 to the variable $a, multiply the value of $a with 2` |
|          - |   58 | ` * and assign the result to variable $b and finally we call a test function to output the value` |
|          - |   59 | ` * of $a and $b. Keep-in mind that all theses operations are done in a single expression using` |
|          - |   60 | ` * the comma operator to create side effect.` |
|          - |   61 | ` * $a = 25,$b = $a << 1 ,test();` |
|          - |   62 | ` * //Output the value of $a and $b` |
|          - |   63 | ` * function test(){` |
|          - |   64 | ` *	 global $a,$b;` |
|          - |   65 | ` *	 echo "\$a = $a \$b= $b\n"; // You should see: $a = 25 $b = 50` |
|          - |   66 | ` * }` |
|          - |   67 | ` *` |
|          - |   68 | ` * For a full discussions on these extensions, please refer to  offical` |
|          - |   69 | ` * documentation(http://ph7.symisc.net/features.html) or visit the offical forums` |
|          - |   70 | ` * (http://forums.symisc.net/) if you want to share your point of view.` |
|          - |   71 | ` *` |
|          - |   72 | ` * Exprressions: According to the PHP language reference manual` |
|          - |   73 | ` *` |
|          - |   74 | ` * Expressions are the most important building stones of PHP. In PHP, almost anything you write is an expression.` |
|          - |   75 | ` * The simplest yet most accurate way to define an expression is "anything that has a value".` |
|          - |   76 | ` * The most basic forms of expressions are constants and variables. When you type "$a = 5", you're assigning` |
|          - |   77 | ` * '5' into $a. '5', obviously, has the value 5, or in other words '5' is an expression with the value of 5` |
|          - |   78 | ` * (in this case, '5' is an integer constant).` |
|          - |   79 | ` * After this assignment, you'd expect $a's value to be 5 as well, so if you wrote $b = $a, you'd expect` |
|          - |   80 | ` * it to behave just as if you wrote $b = 5. In other words, $a is an expression with the value of 5 as well.` |
|          - |   81 | ` * If everything works right, this is exactly what will happen.` |
|          - |   82 | ` * Slightly more complex examples for expressions are functions. For instance, consider the following function:` |
|          - |   83 | ` * <?php` |
|          - |   84 | ` * function foo ()` |
|          - |   85 | ` * {` |
|          - |   86 | ` *   return 5;` |
|          - |   87 | ` * }` |
|          - |   88 | ` * ?>` |
|          - |   89 | ` * Assuming you're familiar with the concept of functions (if you're not, take a look at the chapter about functions)` |
|          - |   90 | ` * you'd assume that typing $c = foo() is essentially just like writing $c = 5, and you're right.` |
|          - |   91 | ` * Functions are expressions with the value of their return value. Since foo() returns 5, the value of the expression` |
|          - |   92 | ` * 'foo()' is 5. Usually functions don't just return a static value but compute something.` |
|          - |   93 | ` * Of course, values in PHP don't have to be integers, and very often they aren't.` |
|          - |   94 | ` * PHP supports four scalar value types: integer values, floating point values (float), string values and boolean values` |
|          - |   95 | ` * (scalar values are values that you can't 'break' into smaller pieces, unlike arrays, for instance).` |
|          - |   96 | ` * PHP also supports two composite (non-scalar) types: arrays and objects. Each of these value types can be assigned` |
|          - |   97 | ` * into variables or returned from functions.` |
|          - |   98 | ` * PHP takes expressions much further, in the same way many other languages do. PHP is an expression-oriented language` |
|          - |   99 | ` * in the sense that almost everything is an expression. Consider the example we've already dealt with, '$a = 5'.` |
|          - |  100 | ` * It's easy to see that there are two values involved here, the value of the integer constant '5', and the value` |
|          - |  101 | ` * of $a which is being updated to 5 as well. But the truth is that there's one additional value involved here` |
|          - |  102 | ` * and that's the value of the assignment itself. The assignment itself evaluates to the assigned value, in this case 5.` |
|          - |  103 | ` * In practice, it means that '$a = 5', regardless of what it does, is an expression with the value 5. Thus, writing` |
|          - |  104 | ` * something like '$b = ($a = 5)' is like writing '$a = 5; $b = 5;' (a semicolon marks the end of a statement).` |
|          - |  105 | ` * Since assignments are parsed in a right to left order, you can also write '$b = $a = 5'.` |
|          - |  106 | ` * Another good example of expression orientation is pre- and post-increment and decrement.` |
|          - |  107 | ` * Users of PHP and many other languages may be familiar with the notation of variable++ and variable--.` |
|          - |  108 | ` * These are increment and decrement operators. In PHP, like in C, there are two types of increment - pre-increment` |
|          - |  109 | ` * and post-increment. Both pre-increment and post-increment essentially increment the variable, and the effect` |
|          - |  110 | ` * on the variable is identical. The difference is with the value of the increment expression. Pre-increment, which is written` |
|          - |  111 | ` * '++$variable', evaluates to the incremented value (PHP increments the variable before reading its value, thus the name 'pre-increment').` |
|          - |  112 | ` * Post-increment, which is written '$variable++' evaluates to the original value of $variable, before it was incremented` |
|          - |  113 | ` * (PHP increments the variable after reading its value, thus the name 'post-increment').` |
|          - |  114 | ` * A very common type of expressions are comparison expressions. These expressions evaluate to either FALSE or TRUE.` |
|          - |  115 | ` * PHP supports > (bigger than), >= (bigger than or equal to), == (equal), != (not equal), < (smaller than) and <= (smaller than or equal to).` |
|          - |  116 | ` * The language also supports a set of strict equivalence operators: === (equal to and same type) and !== (not equal to or not same type).` |
|          - |  117 | ` * These expressions are most commonly used inside conditional execution, such as if statements.` |
|          - |  118 | ` * The last example of expressions we'll deal with here is combined operator-assignment expressions.` |
|          - |  119 | ` * You already know that if you want to increment $a by 1, you can simply write '$a++' or '++$a'.` |
|          - |  120 | ` * But what if you want to add more than one to it, for instance 3? You could write '$a++' multiple times, but this is obviously not a very` |
|          - |  121 | ` * efficient or comfortable way. A much more common practice is to write '$a = $a + 3'. '$a + 3' evaluates to the value of $a plus 3` |
|          - |  122 | ` * and is assigned back into $a, which results in incrementing $a by 3. In PHP, as in several other languages like C, you can write` |
|          - |  123 | ` * this in a shorter way, which with time would become clearer and quicker to understand as well. Adding 3 to the current value of $a` |
|          - |  124 | ` * can be written '$a += 3'. This means exactly "take the value of $a, add 3 to it, and assign it back into $a".` |
|          - |  125 | ` * In addition to being shorter and clearer, this also results in faster execution. The value of '$a += 3', like the value of a regular` |
|          - |  126 | ` * assignment, is the assigned value. Notice that it is NOT 3, but the combined value of $a plus 3 (this is the value that's assigned into $a).` |
|          - |  127 | ` * Any two-place operator can be used in this operator-assignment mode, for example '$a -= 5' (subtract 5 from the value of $a), '$b *= 7'` |
|          - |  128 | ` * (multiply the value of $b by 7), etc.` |
|          - |  129 | ` * There is one more expression that may seem odd if you haven't seen it in other languages, the ternary conditional operator:` |
|          - |  130 | ` * <?php` |
|          - |  131 | ` * $first ? $second : $third` |
|          - |  132 | ` * ?>` |
|          - |  133 | ` * If the value of the first subexpression is TRUE (non-zero), then the second subexpression is evaluated, and that is the result` |
|          - |  134 | ` * of the conditional expression. Otherwise, the third subexpression is evaluated, and that is the value.` |
|          - |  135 | ` */` |
|          - |  136 | `/* Operators associativity */` |
|          - |  137 | `#define EXPR_OP_ASSOC_LEFT   0x01 /* Left associative operator */` |
|          - |  138 | `#define EXPR_OP_ASSOC_RIGHT  0x02 /* Right associative operator */` |
|          - |  139 | `#define EXPR_OP_NON_ASSOC    0x04 /* Non-associative operator */` |
|          - |  140 | `/*` |
|          - |  141 | ` * Operators table` |
|          - |  142 | ` * This table is sorted by operators priority (highest to lowest) according` |
|          - |  143 | ` * the PHP language reference manual.` |
|          - |  144 | ` * PH7 implements all the 60 PHP operators and have introduced the eq and ne operators.` |
|          - |  145 | ` * The operators precedence table have been improved dramatically so that you can do same` |
|          - |  146 | ` * amazing things now such as array dereferencing,on the fly function call,anonymous function` |
|          - |  147 | ` * as array values,class member access on instantiation and so on.` |
|          - |  148 | ` * Refer to the following page for a full discussion on these improvements:` |
|          - |  149 | ` * http://ph7.symisc.net/features.html#improved_precedence` |
|          - |  150 | ` */` |
|          - |  151 | `static const ph7_expr_op aOpTable[] = {` |
|          - |  152 | `	/* Precedence 1: non-associative */` |
|          - |  153 | `	{ {"new",sizeof("new")-1},     EXPR_OP_NEW,   1, EXPR_OP_NON_ASSOC, PH7_OP_NEW  },` |
|          - |  154 | `	{ {"clone",sizeof("clone")-1}, EXPR_OP_CLONE, 1, EXPR_OP_NON_ASSOC, PH7_OP_CLONE},` |
|          - |  155 | `	                              /* Postfix operators */` |
|          - |  156 | `	/* Precedence 2(Highest),left-associative */` |
|          - |  157 | `	{ {"->",sizeof(char)*2}, EXPR_OP_ARROW,     2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|          - |  158 | `	{ {"?->",sizeof(char)*3},EXPR_OP_NULLSAFE_ARROW, 2, EXPR_OP_ASSOC_LEFT, PH7_OP_MEMBER},` |
|          - |  159 | `	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|          - |  160 | `	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},` |
|          - |  161 | `	/* Precedence 3,non-associative  */` |
|          - |  162 | `	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},` |
|          - |  163 | `	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},` |
|          - |  164 | `	                              /* Unary operators */` |
|          - |  165 | `	/* Precedence 4,right-associative  */` |
|          - |  166 | `	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },` |
|          - |  167 | `	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },` |
|          - |  168 | `	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },` |
|          - |  169 | `	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },` |
|          - |  170 | `	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},` |
|          - |  171 | `	                             /* Cast operators */` |
|          - |  172 | `	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },` |
|          - |  173 | `	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },` |
|          - |  174 | `	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },` |
|          - |  175 | `	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|          - |  176 | `	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},` |
|          - |  177 | `	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },` |
|          - |  178 | `	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },` |
|          - |  179 | `	                           /* Binary operators */` |
|          - |  180 | `	/* Precedence 5,right-associative: exponentiation (PHP 5.6) */` |
|          - |  181 | `	{ {"**",sizeof(char)*2}, EXPR_OP_POW, 5, EXPR_OP_ASSOC_RIGHT, PH7_OP_POW},` |
|          - |  182 | `	/* Precedence 7,left-associative */` |
|          - |  183 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|          - |  184 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|          - |  185 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|          - |  186 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|          - |  187 | `	/* Precedence 8,left-associative */` |
|          - |  188 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|          - |  189 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|          - |  190 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|          - |  191 | `	/* Precedence 9,left-associative */` |
|          - |  192 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|          - |  193 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|          - |  194 | ``	/* PHP 8.5 pipe operator: `$x \|> f(...)` desugars to `f($x)`. It binds`` |
|          - |  195 | `	 * looser than shift/arithmetic and tighter than comparison — PHP places it` |
|          - |  196 | `	 * between precedence 9 and 10. We share level 9 (left-associative) so the` |
|          - |  197 | `	 * generic binary tree-builder links it correctly; the actual codegen is` |
|          - |  198 | `	 * custom (a one-argument call of the RHS callable), handled in` |
|          - |  199 | `	 * GenStateEmitExprCode. iVmOp is 0 like the other codegen-only operators. */` |
|          - |  200 | `	{ {"\|>",sizeof(char)*2}, EXPR_OP_PIPE, 9, EXPR_OP_ASSOC_LEFT, 0},` |
|          - |  201 | `	/* Precedence 10,non-associative */` |
|          - |  202 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|          - |  203 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|          - |  204 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|          - |  205 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|          - |  206 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|          - |  207 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|          - |  208 | `	/* Precedence 11,non-associative */` |
|          - |  209 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|          - |  210 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|          - |  211 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|          - |  212 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|          - |  213 | `	/* Precedence 12,left-associative */` |
|          - |  214 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|          - |  215 | `	/* Precedence 12,left-associative */` |
|          - |  216 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|          - |  217 | `	                         /* Binary operators */` |
|          - |  218 | `	/* Precedence 13,left-associative */` |
|          - |  219 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|          - |  220 | `	/* Precedence 14,left-associative */` |
|          - |  221 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|          - |  222 | `	/* Precedence 15,left-associative */` |
|          - |  223 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|          - |  224 | `	/* Precedence 16,left-associative */` |
|          - |  225 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|          - |  226 | `	                      /* Null coalescing operator */` |
|          - |  227 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|          - |  228 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|          - |  229 | `	                      /* Ternary operator */` |
|          - |  230 | `	/* Precedence 17,left-associative */` |
|          - |  231 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|          - |  232 | `	                     /* Combined binary operators */` |
|          - |  233 | `	/* Precedence 18,right-associative */` |
|          - |  234 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|          - |  235 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|          - |  236 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|          - |  237 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|          - |  238 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|          - |  239 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|          - |  240 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|          - |  241 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|          - |  242 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|          - |  243 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|          - |  244 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|          - |  245 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|          - |  246 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|          - |  247 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|          - |  248 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|          - |  249 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|          - |  250 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|          - |  251 | `	 * in this file: keep one of the question marks escaped. */` |
|          - |  252 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|          - |  253 | `	/* Precedence 19,left-associative */` |
|          - |  254 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|          - |  255 | `	/* Precedence 20,left-associative */` |
|          - |  256 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|          - |  257 | `	/* Precedence 21,left-associative */` |
|          - |  258 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|          - |  259 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|          - |  260 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|          - |  261 | `};` |
|          - |  262 | `/* Function call operator need special handling */` |
|          - |  263 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|          - |  264 | `/*` |
|          - |  265 | ` * Check if the given token is a potential operator or not.` |
|          - |  266 | ` * This function is called by the lexer each time it extract a token that may` |
|          - |  267 | ` * look like an operator.` |
|          - |  268 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|          - |  269 | ` * Otherwise NULL.` |
|          - |  270 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|          - |  271 | ` * a binary minus or unary minus.]` |
|          - |  272 | ` */` |
|   25734000 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   25734005 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  407278386 |  278 | `	for(;;){` |
|  814556777 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  814556777 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   77372219 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   38686112 |  285 | `		}else{` |
|  737184563 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  814556777 |  288 | `		if( rc == 0 ){` |
|   26056167 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   25633953 |  291 | `				return &aOpTable[n];` |
|          - |  292 | `			}` |
|          - |  293 | `			/* Handle ambiguity */` |
|     422219 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|          - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      31093 |  296 | `				return &aOpTable[n];` |
|          - |  297 | `			}` |
|     391131 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      68977 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|          - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      68977 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|          - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      68969 |  303 | `					return &aOpTable[n];` |
|          - |  304 | `				}` |
|          - |  305 |  |
|          4 |  306 | `			}` |
|     161081 |  307 | `		}` |
|  788822777 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   12867005 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    6990820 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    6990825 |  320 | `	SyToken *pCur = pIn;` |
|    6990825 |  321 | `	sxi32 iNest = 1;` |
|   73509246 |  322 | `	for(;;){` |
|  147018497 |  323 | `		if( pCur >= pEnd ){` |
|      15819 |  324 | `			break;` |
|          - |  325 | `		}` |
|  147002683 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    5706073 |  328 | `			iNest++;` |
|  144149649 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   12681079 |  331 | `			iNest--;` |
|   12681079 |  332 | `			if( iNest <= 0 ){` |
|    6975011 |  333 | `				break;` |
|          - |  334 | `			}` |
|    2853034 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  140027677 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    6990825 |  340 | `	*ppEnd = pCur;` |
|    6990825 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     456372 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     456372 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     456309 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        137 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     456245 |  358 | `	if( bCheckFunc ){` |
|      38672 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      38661 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      38638 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|         59 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      19309 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     456191 |  366 | `	return FALSE;` |
|     228191 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   15238284 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   15238289 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7685 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7685 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3840 |  383 | `	}` |
|   15238289 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|   95147421 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   79909173 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     216461 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   79692717 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    6621645 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     376042 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    6179665 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    6179665 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    6179665 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    6179665 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3089830 |  401 | `					}` |
|    3089830 |  402 | `			}` |
|    6621645 |  403 | `			iParen++;` |
|   76381897 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    6621645 |  405 | `			if( iParen <= 0 ){` |
|         15 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         15 |  407 | `				if( rc != SXERR_ABORT ){` |
|         15 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         15 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    6621633 |  412 | `			iParen--;` |
|   69760251 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    2875229 |  414 | `			iSquare++;` |
|   65011825 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    2875233 |  416 | `			if( iSquare <= 0 ){` |
|          8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          8 |  418 | `				if( rc != SXERR_ABORT ){` |
|          8 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          8 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    2875227 |  423 | `			iSquare--;` |
|   62136596 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       3839 |  425 | `			iBraces++;` |
|       3839 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|          - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|          - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|          - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|          3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|          3 |  431 | `				if( rc != SXERR_ABORT ){` |
|          3 |  432 | `					rc = SXERR_SYNTAX;` |
|          1 |  433 | `				}` |
|          3 |  434 | `				return rc;` |
|          5 |  435 | `			}` |
|   60697067 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3849 |  437 | `			if( iBraces <= 0 ){` |
|         15 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         15 |  439 | `				if( rc != SXERR_ABORT ){` |
|         15 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         15 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3837 |  444 | `			iBraces--;` |
|   60693223 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     458721 |  446 | `			if( iQuesty > 0 ){` |
|     458431 |  447 | `				iQuesty--;` |
|     229508 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   60461947 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   19738425 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   19738425 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     458433 |  461 | `				iQuesty++;` |
|   19509211 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      80859 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|          9 |  464 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|          9 |  465 | `					sxu32 n = 0;` |
|          9 |  466 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|          5 |  467 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|          2 |  468 | `					}` |
|          - |  469 | `					/*` |
|          - |  470 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|          - |  471 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|          - |  472 | `					 */` |
|        213 |  473 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|        205 |  474 | `						++n;` |
|          1 |  475 | `					}` |
|          9 |  476 | `					pOp = &aOpTable[n];` |
|          - |  477 | `					/* Mark as binary '+' or '-',not an unary */` |
|          9 |  478 | `					apNode[i]->pOp = pOp;` |
|          9 |  479 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|          4 |  480 | `				}` |
|      40427 |  481 | `			}` |
|    9869210 |  482 | `		}` |
|   39846343 |  483 | `	}` |
|   15238253 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         19 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         19 |  486 | `		if( rc != SXERR_ABORT ){` |
|         19 |  487 | `			rc = SXERR_SYNTAX;` |
|          8 |  488 | `		}` |
|         19 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   15238237 |  491 | `	return SXRET_OK;` |
|    7619147 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   11955852 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   11955857 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   11955857 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   11951963 |  502 | `		pIn++;` |
|    5975979 |  503 | `	}` |
|    5979911 |  504 | `	for(;;){` |
|   11959827 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       3975 |  506 | `			pIn++;` |
|       3975 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3973 |  508 | `				pIn++;` |
|       1984 |  509 | `			}` |
|       1990 |  510 | `		}else{` |
|    5977931 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   11955857 |  515 | `	*ppCur = pIn;` |
|   11955857 |  516 | `}` |
|          - |  517 | `/*` |
|          - |  518 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|          - |  519 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  520 | ` * Note on annonymous functions.` |
|          - |  521 | ` *  According to the PHP language reference manual:` |
|          - |  522 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|          - |  523 | ` *  which have no specified name. They are most useful as the value of callback` |
|          - |  524 | ` *  parameters, but they have many other uses.` |
|          - |  525 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|          - |  526 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|          - |  527 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|          - |  528 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|          - |  529 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|          - |  530 | ` *` |
|          - |  531 | ` * Some example:` |
|          - |  532 | ` *  $greet = function($name)` |
|          - |  533 | ` * {` |
|          - |  534 | ` *   printf("Hello %s\r\n", $name);` |
|          - |  535 | ` * };` |
|          - |  536 | ` *  $greet('World');` |
|          - |  537 | ` *  $greet('PHP');` |
|          - |  538 | ` *` |
|          - |  539 | ` * $double = function($a) {` |
|          - |  540 | ` *   return $a * 2;` |
|          - |  541 | ` * };` |
|          - |  542 | ` * // This is our range of numbers` |
|          - |  543 | ` * $numbers = range(1, 5);` |
|          - |  544 | ` * // Use the Annonymous function as a callback here to` |
|          - |  545 | ` * // double the size of each element in our` |
|          - |  546 | ` * // range` |
|          - |  547 | ` * $new_numbers = array_map($double, $numbers);` |
|          - |  548 | ` * print implode(' ', $new_numbers);` |
|          - |  549 | ` */` |
|          - |  550 | `/*` |
|          - |  551 | ` * Skip an optional return-type declaration at *ppIn:` |
|          - |  552 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|          - |  553 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|          - |  554 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|          - |  555 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|          - |  556 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|          - |  557 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|          - |  558 | ` * authoritative type parse, so this must accept every shape it does` |
|          - |  559 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|          - |  560 | ` */` |
|       1170 |  561 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|          5 |  562 | `{` |
|       1175 |  563 | `	SyToken *pIn = *ppIn;` |
|       1175 |  564 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|         27 |  565 | `		pIn++; /* Skip ':' */` |
|         12 |  566 | `		for(;;){` |
|          - |  567 | `			/* Optional '?' nullable prefix */` |
|         31 |  568 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|          6 |  569 | `				pIn++;` |
|          2 |  570 | `			}` |
|         31 |  571 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|          - |  572 | `				/* Parenthesized DNF group '(A&B)' */` |
|        ! 0 |  573 | `				pIn++;` |
|        ! 0 |  574 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        ! 0 |  575 | `				if( pIn < pEnd ){` |
|        ! 0 |  576 | `					pIn++; /* ')' */` |
|        ! 0 |  577 | `				}` |
|         28 |  578 | `			}else if( pIn < pEnd` |
|         31 |  579 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|          - |  580 | `				/* ['\']Name('\'Name)* */` |
|         31 |  581 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|         31 |  582 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|         31 |  583 | `					pIn++;` |
|         31 |  584 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|        ! 0 |  585 | `						pIn += 2;` |
|        ! 0 |  586 | `					}` |
|         14 |  587 | `				}` |
|         17 |  588 | `			}else{` |
|          - |  589 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|        ! 0 |  590 | `				break;` |
|          - |  591 | `			}` |
|          - |  592 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|         28 |  593 | `			if( pIn < pEnd` |
|         31 |  594 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|         28 |  595 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|          5 |  596 | `				pIn++;` |
|          5 |  597 | `				continue;` |
|          - |  598 | `			}` |
|         27 |  599 | `			break;` |
|        ! 0 |  600 | `		}` |
|         12 |  601 | `	}` |
|       1175 |  602 | `	*ppIn = pIn;` |
|       1175 |  603 | `}` |
|        602 |  604 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  605 | `{` |
|        607 |  606 | `	SyToken *pIn = *ppCur;` |
|          - |  607 | `	sxi32 rc;` |
|          - |  608 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|          - |  609 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|          - |  610 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|          - |  611 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|          - |  612 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|        607 |  613 | `	pIn++;` |
|        602 |  614 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|        312 |  615 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|         11 |  616 | `		pIn++;` |
|          5 |  617 | `	}` |
|        607 |  618 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  619 | `		/* Syntax error */` |
|          6 |  620 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  621 | `		if( rc != SXERR_ABORT ){` |
|          6 |  622 | `			rc = SXERR_SYNTAX;` |
|          2 |  623 | `		}` |
|          6 |  624 | `		goto Synchronize;` |
|          - |  625 | `	}` |
|        603 |  626 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|        603 |  627 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        603 |  628 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|          - |  629 | `		/* Nothing follows the parameter list inside our slice: the body is missing.` |
|          - |  630 | `		 * php names the token that actually comes next (it lives just past the` |
|          - |  631 | `		 * expression slice, still in the raw stream) and says it wanted the '{'. */` |
|          6 |  632 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|          6 |  633 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|          6 |  634 | `		if( rc != SXERR_ABORT ){` |
|          6 |  635 | `			rc = SXERR_SYNTAX;` |
|          2 |  636 | `		}` |
|          6 |  637 | `		goto Synchronize;` |
|          - |  638 | `	}` |
|        599 |  639 | `	pIn++; /* Jump the trailing parenthesis */` |
|          - |  640 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|        599 |  641 | `	ExprSkipReturnType(&pIn,pEnd);` |
|        599 |  642 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|        109 |  643 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|          - |  644 | `		/* Check if we are dealing with a closure */` |
|        109 |  645 | `		if( nKey == PH7_TKWRD_USE ){` |
|        101 |  646 | `			pIn++; /* Jump the 'use' keyword */` |
|        101 |  647 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  648 | `				/* Syntax error */` |
|          6 |  649 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  650 | `				if( rc != SXERR_ABORT ){` |
|          6 |  651 | `					rc = SXERR_SYNTAX;` |
|          2 |  652 | `				}` |
|          6 |  653 | `				goto Synchronize;` |
|          - |  654 | `			}` |
|         97 |  655 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|         97 |  656 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         97 |  657 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|          - |  658 | `				/* Syntax error */` |
|          6 |  659 | `				rc = PH7_GenSyntaxError(&(*pGen),0 /* ran off the end */,0);` |
|          6 |  660 | `				if( rc != SXERR_ABORT ){` |
|          6 |  661 | `					rc = SXERR_SYNTAX;` |
|          2 |  662 | `				}` |
|          6 |  663 | `				goto Synchronize;` |
|          - |  664 | `			}` |
|         93 |  665 | `			pIn++;` |
|          - |  666 | `			/* php 7.1+: the return type may also follow the use clause —` |
|          - |  667 | ``			 * `function (...) use (...) : int {` */`` |
|         93 |  668 | `			ExprSkipReturnType(&pIn,pEnd);` |
|         49 |  669 | `		}else{` |
|          - |  670 | `			/* Syntax error */` |
|         11 |  671 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|         11 |  672 | `			if( rc != SXERR_ABORT ){` |
|         11 |  673 | `				rc = SXERR_SYNTAX;` |
|          4 |  674 | `			}` |
|         11 |  675 | `			goto Synchronize;` |
|          - |  676 | `		}` |
|         44 |  677 | `	}` |
|          - |  678 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|          - |  679 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|          - |  680 | `	 * the type), and pEnd is one past the last token. */` |
|        583 |  681 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|        583 |  682 | `		pIn++; /* Jump the leading curly '{' */` |
|        583 |  683 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        583 |  684 | `		if( pIn < pEnd ){` |
|        583 |  685 | `			pIn++;` |
|        289 |  686 | `		}` |
|        294 |  687 | `	}else{` |
|          - |  688 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|          - |  689 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|          - |  690 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|          - |  691 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|        ! 0 |  692 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|        ! 0 |  693 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|        ! 0 |  694 | `		if( rc == SXERR_ABORT ){` |
|        ! 0 |  695 | `			return SXERR_ABORT;` |
|          - |  696 | `		}` |
|          - |  697 | `	}` |
|        583 |  698 | `	rc = SXRET_OK;` |
|        301 |  699 | `Synchronize:` |
|          - |  700 | `	/* Synchronize pointers */` |
|        607 |  701 | `	*ppCur = pIn;` |
|        607 |  702 | `	return rc;` |
|        306 |  703 | `}` |
|          - |  704 | `/*` |
|          - |  705 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|          - |  706 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|          - |  707 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|          - |  708 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|          - |  709 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|          - |  710 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|          - |  711 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|          - |  712 | ` */` |
|         28 |  713 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          4 |  714 | `{` |
|         32 |  715 | `	SyToken *pIn = *ppCur;` |
|         32 |  716 | `	sxu32 nLine = pIn->nLine;` |
|          - |  717 | `	sxi32 rc;` |
|         32 |  718 | `	pIn++; /* Jump the 'class' keyword */` |
|          - |  719 | `	/* Optional constructor argument list */` |
|         32 |  720 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|          7 |  721 | `		pIn++; /* Jump '(' */` |
|          7 |  722 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|          7 |  723 | `		if( pIn < pEnd ){` |
|          7 |  724 | `			pIn++; /* Jump ')' */` |
|          3 |  725 | `		}` |
|          3 |  726 | `	}` |
|          - |  727 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|          - |  728 | `	 * (no braces appear between ')' and the class body). */` |
|         60 |  729 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|         32 |  730 | `		pIn++;` |
|          4 |  731 | `	}` |
|         32 |  732 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|          - |  733 | `		/* Syntax error: missing class body */` |
|        ! 0 |  734 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|          - |  735 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|        ! 0 |  736 | `		if( rc != SXERR_ABORT ){` |
|        ! 0 |  737 | `			rc = SXERR_SYNTAX;` |
|        ! 0 |  738 | `		}` |
|        ! 0 |  739 | `		*ppCur = pIn;` |
|        ! 0 |  740 | `		return rc;` |
|          - |  741 | `	}` |
|         32 |  742 | `	pIn++; /* Jump the leading '{' */` |
|         32 |  743 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|         32 |  744 | `	if( pIn < pEnd ){` |
|         32 |  745 | `		pIn++; /* Jump the trailing '}' */` |
|         14 |  746 | `	}` |
|         32 |  747 | `	*ppCur = pIn;` |
|         32 |  748 | `	return SXRET_OK;` |
|         18 |  749 | `}` |
|          - |  750 | `/*` |
|          - |  751 | ` * Assemble a PHP 7.4 arrow function token range:` |
|          - |  752 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|          - |  753 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|          - |  754 | ` * past the body expression — the body ends at the first top-level comma,` |
|          - |  755 | ` * semicolon, or unbalanced closing delimiter.` |
|          - |  756 | ` */` |
|        488 |  757 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  758 | `{` |
|        493 |  759 | `	SyToken *pIn = *ppCur;` |
|          - |  760 | `	sxu32 nLine;` |
|          - |  761 | `	sxi32 rc;` |
|          - |  762 | `	int iNest;` |
|        493 |  763 | `	nLine = pIn->nLine;` |
|          - |  764 | `	/* Optional 'static' prefix */` |
|        488 |  765 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|        493 |  766 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|          9 |  767 | `		pIn++;` |
|          4 |  768 | `	}` |
|          - |  769 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|        488 |  770 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        493 |  771 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|        ! 0 |  772 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  773 | `		goto Synchronize;` |
|          - |  774 | `	}` |
|        493 |  775 | `	pIn++; /* Jump 'fn' */` |
|        244 |  776 | `	SXUNUSED(nLine);` |
|        244 |  777 | `	SXUNUSED(pGen);` |
|          - |  778 | `	/* Optional '&' for return-by-reference */` |
|        493 |  779 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        ! 0 |  780 | `		pIn++;` |
|        ! 0 |  781 | `	}` |
|          - |  782 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|          - |  783 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|          - |  784 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|          - |  785 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|        493 |  786 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        491 |  787 | `		pIn++; /* '(' */` |
|        491 |  788 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        491 |  789 | `		if( pIn < pEnd ){` |
|        488 |  790 | `			pIn++; /* ')' */` |
|        242 |  791 | `		}` |
|        243 |  792 | `	}` |
|          - |  793 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|        493 |  794 | `	ExprSkipReturnType(&pIn,pEnd);` |
|          - |  795 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|        493 |  796 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        486 |  797 | `		pIn++;` |
|        241 |  798 | `	}` |
|          - |  799 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|        493 |  800 | `	iNest = 0;` |
|       3761 |  801 | `	while( pIn < pEnd ){` |
|       3628 |  802 | `		if( iNest == 0 && (pIn->nType &` |
|          - |  803 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        359 |  804 | `			break;` |
|          - |  805 | `		}` |
|       3272 |  806 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        476 |  807 | `			iNest++;` |
|       3036 |  808 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        476 |  809 | `			iNest--;` |
|        236 |  810 | `		}` |
|       3272 |  811 | `		pIn++;` |
|          4 |  812 | `	}` |
|        493 |  813 | `	rc = SXRET_OK;` |
|        244 |  814 | `Synchronize:` |
|        493 |  815 | `	*ppCur = pIn;` |
|        493 |  816 | `	return rc;` |
|          5 |  817 | `}` |
|          - |  818 | `/*` |
|          - |  819 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|          - |  820 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|          - |  821 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|          - |  822 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|          - |  823 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|          - |  824 | ` */` |
|         72 |  825 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  826 | `{` |
|         77 |  827 | `	SyToken *pIn = *ppCur;` |
|          - |  828 | `	sxi32 rc;` |
|         36 |  829 | `	SXUNUSED(pGen);` |
|          - |  830 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|         72 |  831 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|         77 |  832 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|        ! 0 |  833 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  834 | `		goto Synchronize;` |
|          - |  835 | `	}` |
|         77 |  836 | `	pIn++; /* Jump 'match' */` |
|          - |  837 | `	/* Optional '(' subject ')' */` |
|         77 |  838 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         77 |  839 | `		pIn++;` |
|         77 |  840 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|         77 |  841 | `		if( pIn < pEnd ){` |
|         77 |  842 | `			pIn++; /* ')' */` |
|         36 |  843 | `		}` |
|         36 |  844 | `	}` |
|          - |  845 | `	/* Optional '{' arms '}' */` |
|         77 |  846 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|         77 |  847 | `		pIn++;` |
|         77 |  848 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|         77 |  849 | `		if( pIn < pEnd ){` |
|         77 |  850 | `			pIn++; /* '}' */` |
|         36 |  851 | `		}` |
|         36 |  852 | `	}` |
|         77 |  853 | `	rc = SXRET_OK;` |
|         36 |  854 | `Synchronize:` |
|         77 |  855 | `	*ppCur = pIn;` |
|         77 |  856 | `	return rc;` |
|          5 |  857 | `}` |
|          - |  858 | `/*` |
|          - |  859 | ` * Extract a single expression node from the input.` |
|          - |  860 | ` * On success store the freshly extractd node in ppNode.` |
|          - |  861 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  862 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|          - |  863 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|          - |  864 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|          - |  865 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|          - |  866 | ` */` |
|   79913410 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  868 | `{` |
|          - |  869 | `	ph7_expr_node *pNode;` |
|          - |  870 | `	SyToken *pCur;` |
|          - |  871 | `	sxi32 rc;` |
|          - |  872 | `	/* Allocate a new node */` |
|   79913415 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   79913415 |  874 | `	if( pNode == 0 ){` |
|          - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  877 | `		 */` |
|        ! 0 |  878 | `		return SXERR_MEM;` |
|          - |  879 | `	}` |
|          - |  880 | `	/* Zero the structure */` |
|   79913415 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   79913415 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  883 | `	/* Point to the head of the token stream */` |
|   79913415 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  885 | `	/* Start collecting tokens */` |
|   79913415 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4173 |  887 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|          - |  888 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|          - |  889 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|          - |  890 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|          - |  891 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|         81 |  892 | `			pNode->pEnd = pCur;` |
|         81 |  893 | `			pCur++;` |
|         81 |  894 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|         81 |  895 | `			pNode->xCode = PH7_CompileFccMarker;` |
|         81 |  896 | `			pGen->pIn = pCur;` |
|         81 |  897 | `			*ppNode = pNode;` |
|         81 |  898 | `			return SXRET_OK;` |
|          - |  899 | `		}` |
|          - |  900 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|          - |  901 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       4093 |  902 | `		pCur++;` |
|       4093 |  903 | `		pGen->pIn = pCur;` |
|       4093 |  904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4093 |  905 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4093 |  906 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4093 |  907 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2044 |  908 | `		}` |
|       4093 |  909 | `		return rc;` |
|          - |  910 | `	}` |
|   79909247 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - |  912 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - |  913 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - |  914 | `		 */` |
|     216463 |  915 | `		pCur++; /* Skip the opening '[' */` |
|     216463 |  916 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     216463 |  917 | `		if( pCur < pGen->pEnd ){` |
|     216463 |  918 | `			pCur++; /* Skip past the closing ']' */` |
|     108234 |  919 | `		}else{` |
|        ! 0 |  920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - |  921 | `				"Short array: Missing closing bracket ']'");` |
|        ! 0 |  922 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 |  923 | `				rc = SXERR_SYNTAX;` |
|        ! 0 |  924 | `			}` |
|        ! 0 |  925 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 |  926 | `			return rc;` |
|          - |  927 | `		}` |
|          - |  928 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|          - |  929 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|          - |  930 | `		 */` |
|     216650 |  931 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        378 |  932 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        378 |  933 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|         56 |  934 | `				pNode->xCode = PH7_CompileShortList;` |
|         29 |  935 | `			}else{` |
|        324 |  936 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - |  937 | `			}` |
|        191 |  938 | `		}else{` |
|     216089 |  939 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 |  940 | `		}` |
|   79801018 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|          - |  942 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|          - |  943 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|          - |  944 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|          - |  945 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|          - |  946 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|          - |  947 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|          - |  948 | `		 * call, not the clone() intrinsic. */` |
|         17 |  949 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         17 |  950 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|         17 |  951 | `		pNode->xCode = PH7_CompileLiteral;` |
|   79692777 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   22613700 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   11335566 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|          - |  955 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|          - |  956 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|          - |  957 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|          - |  958 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|          - |  959 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|          - |  960 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|          - |  961 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|          - |  962 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|          - |  963 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|          - |  964 | `		 * operator (which would dereference the NULL pOp). */` |
|         24 |  965 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         24 |  966 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|         24 |  967 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         24 |  968 | `		if( pCur < pGen->pEnd ){` |
|         24 |  969 | `			pCur++; /* skip the closing ')' */` |
|         13 |  970 | `		}else{` |
|        ! 0 |  971 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - |  972 | `				"clone: Missing closing parenthesis ')'");` |
|        ! 0 |  973 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 |  974 | `				rc = SXERR_SYNTAX;` |
|        ! 0 |  975 | `			}` |
|        ! 0 |  976 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 |  977 | `			return rc;` |
|          - |  978 | `		}` |
|         24 |  979 | `		pNode->xCode = PH7_CompileCloneCall;` |
|   79692762 |  980 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - |  981 | `		/* Point to the instance that describe this operator */` |
|   22613683 |  982 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - |  983 | `		/* Advance the stream cursor */` |
|   22613683 |  984 | `		pCur++;` |
|   68385912 |  985 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - |  986 | `		/* Isolate variable */` |
|   38669693 |  987 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   19334855 |  988 | `			pCur++; /* Variable variable */` |
|          5 |  989 | `		}` |
|   19334843 |  990 | `		if( pCur < pGen->pEnd ){` |
|   19334843 |  991 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - |  992 | `				/* Variable name */` |
|   19334815 |  993 | `				pCur++;` |
|    9667438 |  994 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|         25 |  995 | `				pCur++;` |
|          - |  996 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|         25 |  997 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|         25 |  998 | `				if( pCur < pGen->pEnd ){` |
|         19 |  999 | `					pCur++;` |
|         11 | 1000 | `				}else{` |
|          6 | 1001 | `					rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          6 | 1002 | `					if( rc != SXERR_ABORT ){` |
|          6 | 1003 | `						rc = SXERR_SYNTAX;` |
|          2 | 1004 | `					}` |
|          6 | 1005 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          6 | 1006 | `					return rc;` |
|          - | 1007 | `				}` |
|          8 | 1008 | `			}` |
|    9667417 | 1009 | `		}` |
|   19334839 | 1010 | `		pNode->xCode = PH7_CompileVariable;` |
|   47411652 | 1011 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|     960555 | 1012 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     960555 | 1013 | `		 if( bAfterMemberOp ){` |
|          - | 1014 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1015 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1016 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1017 | `			  * as a plain literal like an ordinary identifier member name. */` |
|     126415 | 1018 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     126415 | 1019 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     897350 | 1020 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1021 | `			 /* List/Array node */` |
|     399591 | 1022 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1023 | `				 /* Assume a literal */` |
|        ! 0 | 1024 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1025 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1026 | `			 }else{` |
|     399591 | 1027 | `				 pCur += 2;` |
|          - | 1028 | `				 /* Collect array/list tokens */` |
|     399591 | 1029 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     399591 | 1030 | `				 if( pCur < pGen->pEnd ){` |
|     399589 | 1031 | `					 pCur++;` |
|     199797 | 1032 | `				 }else{` |
|          - | 1033 | `					 /* Syntax error */` |
|          4 | 1034 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          1 | 1035 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|          3 | 1036 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1037 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1038 | `					 }` |
|          3 | 1039 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1040 | `					 return rc;` |
|          - | 1041 | `				 }` |
|     399589 | 1042 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     399589 | 1043 | `				 if( pNode->xCode == PH7_CompileList ){` |
|         39 | 1044 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|         39 | 1045 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|          - | 1046 | `						 /* Syntax error */` |
|          3 | 1047 | `						 rc = PH7_GenSyntaxError(pGen,pNode->pStart,"\"=\"");` |
|          3 | 1048 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1049 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1050 | `						 }` |
|          3 | 1051 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1052 | `						 return rc;` |
|          - | 1053 | `					 }` |
|         16 | 1054 | `				 }` |
|          5 | 1055 | `			 }` |
|     634350 | 1056 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1057 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15687 | 1058 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15687 | 1059 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1060 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1061 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15687 | 1062 | `			 pNode->xCode = PH7_CompileYield;` |
|     426718 | 1063 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     418590 | 1064 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|         58 | 1065 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|         37 | 1066 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1067 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|        607 | 1068 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1069 | `				 /* Assume a literal */` |
|        ! 0 | 1070 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1071 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1072 | `			 }else{` |
|          - | 1073 | `				 /* Assemble annonymous functions body */` |
|        607 | 1074 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|        607 | 1075 | `				 if( rc != SXRET_OK ){` |
|         28 | 1076 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1077 | `					 return rc;` |
|          - | 1078 | `				 }` |
|        583 | 1079 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1080 | `			  }` |
|     418564 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         41 | 1082 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         23 | 1083 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         12 | 1084 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|          9 | 1085 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1086 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1087 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1088 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1089 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         32 | 1090 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         32 | 1091 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1092 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1093 | `				 return rc;` |
|          - | 1094 | `			 }` |
|         32 | 1095 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     418260 | 1096 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     418011 | 1097 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|         48 | 1098 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|         27 | 1099 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1100 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        493 | 1101 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        493 | 1102 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1103 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1104 | `				 return rc;` |
|          - | 1105 | `			 }` |
|        493 | 1106 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     418003 | 1107 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1108 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         77 | 1109 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         77 | 1110 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1111 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1112 | `				 return rc;` |
|          - | 1113 | `			 }` |
|         77 | 1114 | `			 pNode->xCode = PH7_CompileMatch;` |
|     417723 | 1115 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1116 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1117 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1118 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1119 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1120 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1121 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1122 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1123 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     417669 | 1124 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1125 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         79 | 1126 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         79 | 1127 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         42 | 1128 | `		 }else{` |
|          - | 1129 | `			 /* Assume a literal */` |
|     417577 | 1130 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     417577 | 1131 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1132 | `		 }` |
|   37263946 | 1133 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1134 | `		 /* Constants,function name,namespace path,class name... */` |
|   11411859 | 1135 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   11411859 | 1136 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    5705932 | 1137 | `	 }else{` |
|   25371831 | 1138 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1139 | `			 /* Point to the code generator routine */` |
|    8786905 | 1140 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    8786905 | 1141 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1142 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1143 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1144 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1145 | `				 }` |
|          3 | 1146 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1147 | `				 return rc;` |
|          - | 1148 | `			 }` |
|    4393449 | 1149 | `		 }` |
|          - | 1150 | `		/* Advance the stream cursor */` |
|   25371829 | 1151 | `		pCur++;` |
|          - | 1152 | `	 }` |
|          - | 1153 | `	/* Point to the end of the token stream */` |
|   79909213 | 1154 | `	pNode->pEnd = pCur;` |
|          - | 1155 | `	/* Save the node for later processing */` |
|   79909213 | 1156 | `	*ppNode = pNode;` |
|          - | 1157 | `	/* Synchronize cursors */` |
|   79909213 | 1158 | `	pGen->pIn = pCur;` |
|   79909213 | 1159 | `	return SXRET_OK;` |
|   39956710 | 1160 | `}` |
|          - | 1161 | `/*` |
|          - | 1162 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1163 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1164 | ` * level is zero.` |
|          - | 1165 | ` */` |
|    1788232 | 1166 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1167 | `{` |
|    1788237 | 1168 | `	SyToken *pCur = pStart;` |
|    1788237 | 1169 | `	sxi32 iNest = 0;` |
|    1788237 | 1170 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1171 | `		/* Last expression */` |
|     660077 | 1172 | `		return SXERR_EOF;` |
|          - | 1173 | `	}` |
|    4273805 | 1174 | `	while( pCur < pEnd ){` |
|    3999449 | 1175 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     853809 | 1176 | `			break;` |
|          - | 1177 | `		}` |
|    3145645 | 1178 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     214693 | 1179 | `			iNest++;` |
|    3038301 | 1180 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     214695 | 1181 | `			iNest--;` |
|     107345 | 1182 | `		}` |
|    3145645 | 1183 | `		pCur++;` |
|          5 | 1184 | `	}` |
|    1128165 | 1185 | `	*ppNext = pCur;` |
|    1128165 | 1186 | `	return SXRET_OK;` |
|     894121 | 1187 | `}` |
|          - | 1188 | `/*` |
|          - | 1189 | ` * Free an expression tree.` |
|          - | 1190 | ` */` |
|   68351960 | 1191 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1192 | `{` |
|   68351965 | 1193 | `	if( pNode->pLeft ){` |
|          - | 1194 | `		/* Release the left tree */` |
|   26745259 | 1195 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   13372627 | 1196 | `	}` |
|   68351965 | 1197 | `	if( pNode->pRight ){` |
|          - | 1198 | `		/* Release the right tree */` |
|   15551135 | 1199 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    7775565 | 1200 | `	}` |
|   68351965 | 1201 | `	if( pNode->pCond ){` |
|          - | 1202 | `		/* Release the conditional tree used by the ternary operator */` |
|     458429 | 1203 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     229212 | 1204 | `	}` |
|   68351965 | 1205 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1206 | `		ph7_expr_node **apArg;` |
|          - | 1207 | `		sxu32 n;` |
|          - | 1208 | `		/* Release node arguments */` |
|    7422335 | 1209 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   16888661 | 1210 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|    9466331 | 1211 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    4733168 | 1212 | `		}` |
|    7422335 | 1213 | `		SySetRelease(&pNode->aNodeArgs);` |
|    3711165 | 1214 | `	}` |
|          - | 1215 | `	/* Finally,release this node */` |
|   68351965 | 1216 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   68351965 | 1217 | `}` |
|          - | 1218 | `/*` |
|          - | 1219 | ` * Free an expression tree.` |
|          - | 1220 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1221 | ` */` |
|   15238314 | 1222 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1223 | `{` |
|          - | 1224 | `	ph7_expr_node **apNode;` |
|          - | 1225 | `	sxu32 n;` |
|   15238319 | 1226 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|   95147577 | 1227 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   79909263 | 1228 | `		if( apNode[n] ){` |
|   15238659 | 1229 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    7619327 | 1230 | `		}` |
|   39954634 | 1231 | `	}` |
|   15238319 | 1232 | `	return SXRET_OK;` |
|          5 | 1233 | `}` |
|          - | 1234 | `/*` |
|          - | 1235 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1236 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1237 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1238 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1239 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1240 | ` */` |
|   19453916 | 1241 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1242 | `{` |
|   19453921 | 1243 | `	if( pNode == 0 ){` |
|   12146165 | 1244 | `		return 0;` |
|          - | 1245 | `	}` |
|    7307761 | 1246 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1247 | `		return 1;` |
|          - | 1248 | `	}` |
|    7307749 | 1249 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1250 | `		return 1;` |
|          - | 1251 | `	}` |
|    7307745 | 1252 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1253 | `		return 1;` |
|          - | 1254 | `	}` |
|    7307745 | 1255 | `	return 0;` |
|    9726963 | 1256 | `}` |
|          - | 1257 | `/*` |
|          - | 1258 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1259 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1260 | ` */` |
|    4815404 | 1261 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1262 | `{` |
|          - | 1263 | `	sxi32 iExprOp;` |
|    4815409 | 1264 | `	if( pNode->pOp == 0 ){` |
|    3482963 | 1265 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1266 | `	}` |
|    1332451 | 1267 | `	iExprOp = pNode->pOp->iOp;` |
|    1332451 | 1268 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     819121 | 1269 | `			return TRUE;` |
|          - | 1270 | `	}` |
|     513335 | 1271 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     513321 | 1272 | `		if( pNode->pLeft->pOp ) {` |
|     118626 | 1273 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      49748 | 1274 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1275 | `				return FALSE;` |
|          5 | 1276 | `			}` |
|     454008 | 1277 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1278 | `			return FALSE;` |
|          - | 1279 | `		}` |
|     513321 | 1280 | `		return TRUE;` |
|          - | 1281 | `	}` |
|         16 | 1282 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1283 | `		return TRUE;` |
|          - | 1284 | `	}` |
|          - | 1285 | `	/* Not a modifiable l or r-value */` |
|          9 | 1286 | `	return FALSE;` |
|    2407707 | 1287 | `}` |
|          - | 1288 | `/* Forward declaration */` |
|          - | 1289 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|          - | 1290 | `/* Macro to check if the given node is a terminal.` |
|          - | 1291 | ` * A node is a term if it has no operator, or has already been linked into an` |
|          - | 1292 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|          - | 1293 | ` * linked ternary/elvis node). */` |
|          - | 1294 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|          - | 1295 | `/*` |
|          - | 1296 | ` * Buid an expression tree for each given function argument.` |
|          - | 1297 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1298 | ` */` |
|    4769302 | 1299 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1300 | `{` |
|          - | 1301 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1302 | `	sxi32 rc;` |
|          - | 1303 | `	/* Process function arguments from left to right */` |
|    4769307 | 1304 | `	iCur = 0;` |
|    5791288 | 1305 | `	for(;;){` |
|   11582581 | 1306 | `		if( iCur >= nToken ){` |
|          - | 1307 | `			/* No more arguments to process */` |
|    4769281 | 1308 | `			break;` |
|          - | 1309 | `		}` |
|    6813305 | 1310 | `		iNode = iCur;` |
|    6813305 | 1311 | `		iNest = 0;` |
|   21910661 | 1312 | `		while( iCur < nToken ){` |
|   17141383 | 1313 | `			if( apNode[iCur] ){` |
|   17095317 | 1314 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1022016 | 1315 | `					break;` |
|   15051290 | 1316 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    8107178 | 1317 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1160557 | 1318 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1319 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1320 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1321 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1322 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1323 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1324 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    1158043 | 1325 | `					iNest++;` |
|   14472276 | 1326 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    1158043 | 1327 | `					iNest--;` |
|     579019 | 1328 | `				}` |
|    7525645 | 1329 | `			}` |
|   15097361 | 1330 | `			iCur++;` |
|          5 | 1331 | `		}` |
|    6813305 | 1332 | `		if( iCur > iNode ){` |
|    6813299 | 1333 | `			SyString sArgName = {0, 0};` |
|          - | 1334 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1335 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1336 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    6813294 | 1337 | `			if( (iCur - iNode) >= 2` |
|    4594427 | 1338 | `				&& apNode[iNode]` |
|    2375546 | 1339 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1277108 | 1340 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     178416 | 1341 | `				&& apNode[iNode+1]` |
|     178153 | 1342 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1343 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        291 | 1344 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        291 | 1345 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        291 | 1346 | `				apNode[iNode] = 0;` |
|        291 | 1347 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        291 | 1348 | `				apNode[iNode+1] = 0;` |
|        291 | 1349 | `				iNode += 2;` |
|          - | 1350 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1351 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        291 | 1352 | `				if( iNode >= iCur ){` |
|          4 | 1353 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1354 | `						pOp->pStart->nLine,` |
|          - | 1355 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1356 | `						&sArgName);` |
|          3 | 1357 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1358 | `						rc = SXERR_SYNTAX;` |
|          1 | 1359 | `					}` |
|          3 | 1360 | `					return rc;` |
|          - | 1361 | `				}` |
|        142 | 1362 | `			}` |
|    6813292 | 1363 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1364 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1365 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1366 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1367 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1368 | `					apNode[iNode] = 0;` |
|        ! 0 | 1369 | `			}` |
|          - | 1370 | `			{` |
|          - | 1371 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1372 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1373 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1374 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1375 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1376 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1377 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1378 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1379 | `				 * NULL slots ahead of the flagged subtree. */` |
|    6813297 | 1380 | `				int bSpreadArg = 0;` |
|          - | 1381 | `				sxi32 iScan;` |
|    6813325 | 1382 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    6813325 | 1383 | `					if( apNode[iScan] ){` |
|    6813297 | 1384 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    6813297 | 1385 | `						break;` |
|          - | 1386 | `					}` |
|         15 | 1387 | `				}` |
|    6813297 | 1388 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    6813297 | 1389 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4023 | 1390 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2009 | 1391 | `				}` |
|          - | 1392 | `			}` |
|    6813297 | 1393 | `			if( apNode[iNode] ){` |
|    6813297 | 1394 | `				if( sArgName.nByte > 0 ){` |
|        289 | 1395 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        289 | 1396 | `					apNode[iNode]->sArgName = sArgName;` |
|        142 | 1397 | `				}` |
|          - | 1398 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    6813297 | 1399 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    3406651 | 1400 | `			}else{` |
|          - | 1401 | `				/* No expression before comma */` |
|        ! 0 | 1402 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1403 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1404 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1405 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1406 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1407 | `				}` |
|        ! 0 | 1408 | `				return rc;` |
|          - | 1409 | `			}` |
|    3406651 | 1410 | `		}else{` |
|          - | 1411 | `			/* Comma with no preceding argument */` |
|          8 | 1412 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1413 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1414 | `				rc = SXERR_SYNTAX;` |
|          3 | 1415 | `			}` |
|          8 | 1416 | `			return rc;` |
|          - | 1417 | `		}` |
|          - | 1418 | `		/* Jump trailing comma */` |
|    6813297 | 1419 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2044021 | 1420 | `			iCur++;` |
|    2044021 | 1421 | `			if( iCur >= nToken ){` |
|          - | 1422 | `				/* Trailing comma after last argument */` |
|         19 | 1423 | `				break;` |
|          - | 1424 | `			}` |
|    1021999 | 1425 | `		}` |
|          5 | 1426 | `	}` |
|    4769299 | 1427 | `	return SXRET_OK;` |
|    2384656 | 1428 | `}` |
|          - | 1429 | ` /*` |
|          - | 1430 | `  * Create an expression tree from an array of tokens.` |
|          - | 1431 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1432 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1433 | `  */` |
|   26228146 | 1434 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1435 | ` {` |
|          - | 1436 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1437 | `	 ph7_expr_node *pNode;` |
|          - | 1438 | `	 ph7_expr_node *pSuppress;` |
|          - | 1439 | `	 sxi32 iCur;` |
|          - | 1440 | `	 sxi32 rc;` |
|   26228151 | 1441 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1442 | `		 /* TICKET 1433-17: self evaluating node */` |
|   11763191 | 1443 | `		 return SXRET_OK;` |
|          - | 1444 | `	 }` |
|          - | 1445 | `	 /* Process expressions enclosed in parenthesis first */` |
|  103303185 | 1446 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1447 | `		 sxi32 iNest;` |
|          - | 1448 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1449 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1450 | `		  */` |
|   88838227 | 1451 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|   88396257 | 1452 | `			 continue;` |
|          - | 1453 | `		 }` |
|     441975 | 1454 | `		 iNest = 1;` |
|     441975 | 1455 | `		 iLeft = iCur;` |
|          - | 1456 | `		 /* Find the closing parenthesis */` |
|     441975 | 1457 | `		 iCur++;` |
|    3847621 | 1458 | `		 while( iCur < nToken ){` |
|    3847621 | 1459 | `			 if( apNode[iCur] ){` |
|    3847621 | 1460 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1461 | `					 /* Decrement nesting level */` |
|     642087 | 1462 | `					 iNest--;` |
|     642087 | 1463 | `					 if( iNest <= 0 ){` |
|     441975 | 1464 | `						 break;` |
|          5 | 1465 | `					 }` |
|    3305595 | 1466 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1467 | `					 /* Increment nesting level */` |
|     200117 | 1468 | `					 iNest++;` |
|     100056 | 1469 | `				 }` |
|    1702823 | 1470 | `			 }` |
|    3405651 | 1471 | `			 iCur++;` |
|          5 | 1472 | `		 }` |
|     441975 | 1473 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1474 | `			 sxi32 j;` |
|          - | 1475 | `			 /* Recurse and process this expression */` |
|     441975 | 1476 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     441975 | 1477 | `			 if( rc != SXRET_OK ){` |
|          3 | 1478 | `				 return rc;` |
|          - | 1479 | `			 }` |
|          - | 1480 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1481 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1482 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1483 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1484 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1485 | `			  * group's free below silently drops the unpacking. */` |
|     441973 | 1486 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     441973 | 1487 | `				 if( apNode[j] ){` |
|     441973 | 1488 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     441968 | 1489 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     441973 | 1490 | `					 break;` |
|          - | 1491 | `				 }` |
|        ! 0 | 1492 | `			 }` |
|     220984 | 1493 | `		 }` |
|          - | 1494 | `		 /* Free the left and right nodes */` |
|     441973 | 1495 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     441973 | 1496 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     441973 | 1497 | `		 apNode[iLeft] = 0;` |
|     441973 | 1498 | `		 apNode[iCur] = 0;` |
|     220989 | 1499 | `	 }` |
|          - | 1500 | `	  /* Process expressions enclosed in braces */` |
|  106813463 | 1501 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1502 | `		 sxi32 iNest;` |
|          - | 1503 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1504 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1505 | `		  */` |
|   92678169 | 1506 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|   92674337 | 1507 | `			 continue;` |
|          - | 1508 | `		 }` |
|       3837 | 1509 | `		 iNest = 1;` |
|       3837 | 1510 | `		 iLeft = iCur;` |
|          - | 1511 | `		 /* Find the closing parenthesis */` |
|       3837 | 1512 | `		 iCur++;` |
|       7667 | 1513 | `		 while( iCur < nToken ){` |
|       7667 | 1514 | `			 if( apNode[iCur] ){` |
|       7667 | 1515 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1516 | `					 /* Decrement nesting level */` |
|       3837 | 1517 | `					 iNest--;` |
|       3837 | 1518 | `					 if( iNest <= 0 ){` |
|       3837 | 1519 | `						 break;` |
|        ! 0 | 1520 | `					 }` |
|       3835 | 1521 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1522 | `					 /* Increment nesting level */` |
|        ! 0 | 1523 | `					 iNest++;` |
|        ! 0 | 1524 | `				 }` |
|       1915 | 1525 | `			 }` |
|       3835 | 1526 | `			 iCur++;` |
|          5 | 1527 | `		 }` |
|       3837 | 1528 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1529 | `			 /* Recurse and process this expression */` |
|       3835 | 1530 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3835 | 1531 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1532 | `				 return rc;` |
|          - | 1533 | `			 }` |
|       1915 | 1534 | `		 }` |
|          - | 1535 | `		 /* Free the left and right nodes */` |
|       3837 | 1536 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3837 | 1537 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3837 | 1538 | `		 apNode[iLeft] = 0;` |
|       3837 | 1539 | `		 apNode[iCur] = 0;` |
|       1921 | 1540 | `	 }` |
|          - | 1541 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   14135299 | 1542 | `	 iLeft = -1;` |
|  106821089 | 1543 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92685807 | 1544 | `		 if( apNode[iCur] == 0 ){` |
|   40124043 | 1545 | `			 continue;` |
|          - | 1546 | `		 }` |
|   52561769 | 1547 | `		 pNode = apNode[iCur];` |
|   52561769 | 1548 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   14006665 | 1549 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1550 | `				 /* Collect function arguments */` |
|    6179661 | 1551 | `				 sxi32 iPtr = 0;` |
|    6179661 | 1552 | `				 sxi32 nFuncTok = 0;` |
|   29500697 | 1553 | `				 while( nFuncTok + iCur < nToken ){` |
|   29500697 | 1554 | `					 if( apNode[nFuncTok+iCur] ){` |
|   29454631 | 1555 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    6545577 | 1556 | `							 iPtr++;` |
|   26181845 | 1557 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    6545577 | 1558 | `							 iPtr--;` |
|    6545577 | 1559 | `							 if( iPtr <= 0 ){` |
|    6179661 | 1560 | `								 break;` |
|          - | 1561 | `							 }` |
|     182958 | 1562 | `						 }` |
|   11637485 | 1563 | `					 }` |
|   23321041 | 1564 | `					 nFuncTok++;` |
|          5 | 1565 | `				 }` |
|    6179661 | 1566 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1567 | `					 /* Syntax error */` |
|        ! 0 | 1568 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1569 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1570 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1571 | `					 }` |
|        ! 0 | 1572 | `					 return rc;` |
|          - | 1573 | `				 }` |
|    6179661 | 1574 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1575 | `					 /* Syntax error */` |
|        ! 0 | 1576 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1577 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1578 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1579 | `					 }` |
|        ! 0 | 1580 | `					 return rc;` |
|          - | 1581 | `				 }` |
|    6179661 | 1582 | `				 if( nFuncTok > 1 ){` |
|          - | 1583 | `					 /* Process function arguments */` |
|    4769307 | 1584 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    4769307 | 1585 | `					 if( rc != SXRET_OK ){` |
|         11 | 1586 | `						 return rc;` |
|          - | 1587 | `					 }` |
|    2384647 | 1588 | `				 }` |
|          - | 1589 | `				 /* Link the node to the tree */` |
|    6179653 | 1590 | `				 pNode->pLeft = apNode[iLeft];` |
|    6179653 | 1591 | `				 apNode[iLeft] = 0;` |
|   29500665 | 1592 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   23321017 | 1593 | `					 apNode[iCur+iPtr] = 0;` |
|   11660511 | 1594 | `				 }` |
|          - | 1595 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1596 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1597 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1598 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1599 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1600 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1601 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1602 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1603 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1604 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1605 | `				 {` |
|    6179653 | 1606 | `					 sxi32 iNew = iLeft - 1;` |
|    8025973 | 1607 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    1846325 | 1608 | `						 iNew--;` |
|          5 | 1609 | `					 }` |
|    6179648 | 1610 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3681735 | 1611 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2210282 | 1612 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     752223 | 1613 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     752223 | 1614 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     752223 | 1615 | `						 apNode[iNew] = 0;` |
|     752223 | 1616 | `						 pNode = apNode[iCur];` |
|     376114 | 1617 | `					 }` |
|          - | 1618 | `				 }` |
|   10916833 | 1619 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1620 | `				 /* Subscripting */` |
|    2875227 | 1621 | `				 sxi32 iArrTok = iCur + 1;` |
|    2875227 | 1622 | `				 sxi32 iNest = 1;` |
|    2875222 | 1623 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         18 | 1624 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         14 | 1625 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         14 | 1626 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    2875222 | 1627 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1628 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1629 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     300451 | 1630 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1631 | `						 /* Syntax error */` |
|        ! 0 | 1632 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1633 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1634 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1635 | `						 }` |
|        ! 0 | 1636 | `						 return rc;` |
|          - | 1637 | `				 }` |
|          - | 1638 | `				 /* Collect index tokens */` |
|    6056143 | 1639 | `				 while( iArrTok < nToken ){` |
|    6056143 | 1640 | `					 if( apNode[iArrTok] ){` |
|    6056111 | 1641 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1642 | `							 /* Increment nesting level */` |
|      26773 | 1643 | `							 iNest++;` |
|    6042727 | 1644 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1645 | `							 /* Decrement nesting level */` |
|    2901995 | 1646 | `							 iNest--;` |
|    2901995 | 1647 | `							 if( iNest <= 0 ){` |
|    2875227 | 1648 | `								 break;` |
|          - | 1649 | `							 }` |
|      13384 | 1650 | `						 }` |
|    1590442 | 1651 | `					 }` |
|    3180921 | 1652 | `					 ++iArrTok;` |
|          5 | 1653 | `				 }` |
|    2875227 | 1654 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1655 | `					 /* Recurse and process this expression */` |
|    2653039 | 1656 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2653039 | 1657 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1658 | `						 return rc;` |
|          - | 1659 | `					 }` |
|          - | 1660 | `					 /* Link the node to it's index */` |
|    2653039 | 1661 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1326517 | 1662 | `				 }` |
|          - | 1663 | `				 /* Link the node to the tree */` |
|    2875227 | 1664 | `				 pNode->pLeft = apNode[iLeft];` |
|    2875227 | 1665 | `				 pNode->pRight = 0;` |
|    2875227 | 1666 | `				 apNode[iLeft] = 0;` |
|    8931365 | 1667 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6056143 | 1668 | `					 apNode[iNest] = 0;` |
|    3028074 | 1669 | `				 }` |
|    1437616 | 1670 | `			 }else{` |
|          - | 1671 | `				 /* Member access operators [i.e: '->','::'] */` |
|    4951787 | 1672 | `				  iRight = iCur + 1;` |
|    4955617 | 1673 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3835 | 1674 | `					 iRight++;` |
|          5 | 1675 | `				 }` |
|    4951787 | 1676 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1677 | `					 /* Syntax error */` |
|          5 | 1678 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1679 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1680 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1681 | `					 }` |
|          5 | 1682 | `					 return rc;` |
|          - | 1683 | `				 }` |
|          - | 1684 | `				 /* Link the node to the tree */` |
|    4951783 | 1685 | `				 pNode->pLeft = apNode[iLeft];` |
|    4951778 | 1686 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    4767742 | 1687 | `					 && pNode->pLeft->pOp == 0 &&` |
|    4506617 | 1688 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1689 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1690 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1691 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1692 | `					  * accepts. */` |
|          4 | 1693 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1694 | `						 /* Syntax error */` |
|        ! 0 | 1695 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1696 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1697 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1698 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1699 | `						 }` |
|        ! 0 | 1700 | `						 return rc;` |
|          - | 1701 | `				 }` |
|    4951783 | 1702 | `				 pNode->pRight = apNode[iRight];` |
|    4951783 | 1703 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1704 | `			 }` |
|    7003324 | 1705 | `		 }` |
|   52561757 | 1706 | `		 iLeft = iCur;` |
|   26280881 | 1707 | `	 }` |
|          - | 1708 | `	 /* Handle left associative (new, clone) operators */` |
|  106821057 | 1709 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92685775 | 1710 | `		 if( apNode[iCur] == 0 ){` |
|   54940649 | 1711 | `			 continue;` |
|          - | 1712 | `		 }` |
|   37745131 | 1713 | `		 pNode = apNode[iCur];` |
|   37745131 | 1714 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1715 | `			 SyToken *pToken;` |
|          - | 1716 | `			 /* Get the left node */` |
|      57745 | 1717 | `			 iLeft = iCur + 1;` |
|      57753 | 1718 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1719 | `				 iLeft++;` |
|          1 | 1720 | `			 }` |
|      57745 | 1721 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1722 | `				  /* Syntax error */` |
|        ! 0 | 1723 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1724 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1725 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1726 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1727 | `				 }` |
|        ! 0 | 1728 | `				 return rc;` |
|          - | 1729 | `			 }` |
|          - | 1730 | `			 /* Make sure the operand are of a valid type */` |
|      57745 | 1731 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1732 | `				 /* Clone:` |
|          - | 1733 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1734 | `				  *  ++ function call (including annonymous)` |
|          - | 1735 | `				  *  ++ array member` |
|          - | 1736 | `				  *  ++ 'new' operator` |
|          - | 1737 | `				  * Example:` |
|          - | 1738 | `				  *   clone $pObj;` |
|          - | 1739 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1740 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1741 | `				  */` |
|      57405 | 1742 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      57399 | 1743 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1744 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1745 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1746 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1747 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1748 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1749 | `						 }` |
|        ! 0 | 1750 | `						 return rc;` |
|          - | 1751 | `					 }` |
|      28697 | 1752 | `				 }` |
|      28705 | 1753 | `			 }else{` |
|          - | 1754 | `				 /* New */` |
|        340 | 1755 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1756 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1757 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1758 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1759 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1760 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1761 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1762 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1763 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1764 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1765 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1766 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1767 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1768 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1769 | `					 }` |
|        ! 0 | 1770 | `					 return rc;` |
|          - | 1771 | `				 }` |
|        345 | 1772 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|        345 | 1773 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|        340 | 1774 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         33 | 1775 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1776 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1777 | `						 /* Syntax error */` |
|        ! 0 | 1778 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1779 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1780 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1781 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1782 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1783 | `						 }` |
|        ! 0 | 1784 | `						 return rc;` |
|          - | 1785 | `					 }` |
|        170 | 1786 | `				 }` |
|          - | 1787 | `			 }` |
|          - | 1788 | `			  /* Link the node to the tree */` |
|      57745 | 1789 | `			 pNode->pLeft = apNode[iLeft];` |
|      57745 | 1790 | `			 apNode[iLeft] = 0;` |
|      57745 | 1791 | `			 pNode->pRight = 0; /* Paranoid */` |
|      28870 | 1792 | `		 }` |
|   18872568 | 1793 | `	 }` |
|          - | 1794 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   14135287 | 1795 | `	 iLeft = -1;` |
|  106985889 | 1796 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92685775 | 1797 | `		 if( apNode[iCur] == 0 ){` |
|   54940649 | 1798 | `			 continue;` |
|          - | 1799 | `		 }` |
|   37745131 | 1800 | `		 pNode = apNode[iCur];` |
|   37745131 | 1801 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     195533 | 1802 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     172531 | 1803 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1804 | `					 /* Link the node to the tree */` |
|     180205 | 1805 | `					 pNode->pLeft = apNode[iLeft];` |
|     180205 | 1806 | `					 apNode[iLeft] = 0;` |
|      90100 | 1807 | `			 }` |
|     262596 | 1808 | `		  }` |
|   37909963 | 1809 | `		 iLeft = iCur;` |
|   19037400 | 1810 | `	  }` |
|   14300119 | 1811 | `	 iLeft = -1;` |
|  106985889 | 1812 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|   92685775 | 1813 | `		 if( apNode[iCur] == 0 ){` |
|   55120849 | 1814 | `			 continue;` |
|          - | 1815 | `		 }` |
|   37564931 | 1816 | `		 pNode = apNode[iCur];` |
|   37564931 | 1817 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15328 | 1818 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15333 | 1819 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1820 | `					 /* Syntax error */` |
|        ! 0 | 1821 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1822 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1823 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1824 | `					 }` |
|        ! 0 | 1825 | `					 return rc;` |
|          - | 1826 | `			 }` |
|          - | 1827 | `			 /* Link the node to the tree */` |
|      15333 | 1828 | `			 pNode->pLeft = apNode[iLeft];` |
|      15333 | 1829 | `			 apNode[iLeft] = 0;` |
|          - | 1830 | `			 /* Mark as pre-increment/decrement node */` |
|      15333 | 1831 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7664 | 1832 | `		  }` |
|   37564931 | 1833 | `		 iLeft = iCur;` |
|   18782468 | 1834 | `	 }` |
|          - | 1835 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1836 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1837 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1838 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1839 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1840 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1841 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1842 | `	  * pass below skips it (pLeft != 0). */` |
|   14300119 | 1843 | `	 iLeft = -1;` |
|  106985889 | 1844 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92685775 | 1845 | `		 if( apNode[iCur] == 0 ){` |
|   55212937 | 1846 | `			 continue;` |
|          - | 1847 | `		 }` |
|   37472843 | 1848 | `		 pNode = apNode[iCur];` |
|   37472843 | 1849 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      76765 | 1850 | `			 iRight = iCur + 1;` |
|      76765 | 1851 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1852 | `				 iRight++;` |
|        ! 0 | 1853 | `			 }` |
|      76765 | 1854 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1855 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1856 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1857 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1858 | `				 }` |
|        ! 0 | 1859 | `				 return rc;` |
|          - | 1860 | `			 }` |
|      76765 | 1861 | `			 pNode->pLeft = apNode[iLeft];` |
|      76765 | 1862 | `			 pNode->pRight = apNode[iRight];` |
|      76765 | 1863 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      38380 | 1864 | `		 }` |
|   37472843 | 1865 | `		 iLeft = iCur;` |
|   18736424 | 1866 | `	 }` |
|          - | 1867 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   14300119 | 1868 | `	  iLeft = 0;` |
|  106985883 | 1869 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|   92685771 | 1870 | `		  if( apNode[iCur] ){` |
|   37396079 | 1871 | `			  pNode = apNode[iCur];` |
|   37396079 | 1872 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1137669 | 1873 | `				  if( iLeft > 0 ){` |
|          - | 1874 | `					  /* Link the node to the tree */` |
|    1137667 | 1875 | `					  pNode->pLeft = apNode[iLeft];` |
|    1137667 | 1876 | `					  apNode[iLeft] = 0;` |
|    1137667 | 1877 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      53645 | 1878 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1879 | `							   /* Syntax error */` |
|        ! 0 | 1880 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1881 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 1882 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 1883 | `							  }` |
|        ! 0 | 1884 | `							  return rc;` |
|          - | 1885 | `						  }` |
|      26820 | 1886 | `					  }` |
|     568836 | 1887 | `				  }else{` |
|          - | 1888 | `					  /* Syntax error */` |
|          3 | 1889 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 1890 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 1891 | `						  rc = SXERR_SYNTAX;` |
|          1 | 1892 | `					  }` |
|          3 | 1893 | `					  return rc;` |
|          - | 1894 | `				  }` |
|     568831 | 1895 | `			  }` |
|          - | 1896 | `			  /* Save terminal position */` |
|   37396077 | 1897 | `			  iLeft = iCur;` |
|   18698036 | 1898 | `		  }` |
|   46342887 | 1899 | `	  }` |
|          - | 1900 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 1901 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 1902 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 1903 | `	  * yielding a right-leaning tree. */` |
|  106985881 | 1904 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|   92685769 | 1905 | `		 if( apNode[iCur] == 0 ){` |
|   56427473 | 1906 | `			 continue;` |
|          - | 1907 | `		 }` |
|   36258301 | 1908 | `		 pNode = apNode[iCur];` |
|   36258301 | 1909 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 1910 | `			 sxi32 iL, iR;` |
|          - | 1911 | `			 /* Find the right operand */` |
|        115 | 1912 | `			 iR = -1;` |
|          - | 1913 | `			 {` |
|          - | 1914 | `				 sxi32 j;` |
|        127 | 1915 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 1916 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 1917 | `				 }` |
|          - | 1918 | `			 }` |
|          - | 1919 | `			 /* Find the left operand */` |
|        115 | 1920 | `			 iL = -1;` |
|          - | 1921 | `			 {` |
|          - | 1922 | `				 sxi32 j;` |
|        183 | 1923 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 1924 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 1925 | `				 }` |
|          - | 1926 | `			 }` |
|        115 | 1927 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 1928 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1929 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1930 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1931 | `				 }` |
|        ! 0 | 1932 | `				 return rc;` |
|          - | 1933 | `			 }` |
|        115 | 1934 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 1935 | `			 pNode->pRight = apNode[iR];` |
|        115 | 1936 | `			 apNode[iL] = 0;` |
|        115 | 1937 | `			 apNode[iR] = 0;` |
|          - | 1938 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 1939 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 1940 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 1941 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 1942 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 1943 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 1944 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 1945 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 1946 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 1947 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 1948 | `			  * operands are respected. */` |
|        114 | 1949 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 1950 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 1951 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 1952 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 1953 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 1954 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 1955 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 1956 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 1957 | `				 while( pTail->pLeft` |
|         34 | 1958 | `					 && pTail->pLeft->pOp` |
|         23 | 1959 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 1960 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 1961 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 1962 | `					 pTail = pTail->pLeft;` |
|          1 | 1963 | `				 }` |
|          - | 1964 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 1965 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 1966 | `				 pTail->pLeft = pNode;` |
|         27 | 1967 | `				 apNode[iCur] = pHead;` |
|         13 | 1968 | `			 }` |
|         57 | 1969 | `		 }` |
|   18129153 | 1970 | `	 }` |
|          - | 1971 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  157301151 | 1972 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  143001049 | 1973 | `		 iLeft = -1;` |
| 1069858395 | 1974 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  926857361 | 1975 | `			 if( apNode[iCur] == 0 ){` |
|  628068999 | 1976 | `				 continue;` |
|          - | 1977 | `			 }` |
|  298788367 | 1978 | `			 pNode = apNode[iCur];` |
|  298788367 | 1979 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 1980 | `				 /* Get the right node */` |
|    5248741 | 1981 | `				 iRight = iCur + 1;` |
|    7902833 | 1982 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2654097 | 1983 | `					 iRight++;` |
|          5 | 1984 | `				 }` |
|    5248741 | 1985 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1986 | `					 /* Syntax error */` |
|         10 | 1987 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 1988 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 1989 | `						 rc = SXERR_SYNTAX;` |
|          4 | 1990 | `					 }` |
|         10 | 1991 | `					 return rc;` |
|          - | 1992 | `				 }` |
|    5248733 | 1993 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 1994 | `					 sxi32  iTmp;` |
|          - | 1995 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 1996 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 1997 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 1998 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 1999 | `					  * is swapped below. */` |
|         65 | 2000 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2001 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2002 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2003 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2004 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2005 | `						 }` |
|          3 | 2006 | `						 return rc;` |
|          - | 2007 | `					 }` |
|         63 | 2008 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|          - | 2009 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2010 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2011 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2012 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2013 | `						 }` |
|        ! 0 | 2014 | `						 return rc;` |
|          - | 2015 | `					 }` |
|         63 | 2016 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         45 | 2017 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2018 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2019 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2020 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2021 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2022 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2023 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2024 | `									 }` |
|        ! 0 | 2025 | `									 return rc;` |
|          - | 2026 | `							 }` |
|        ! 0 | 2027 | `						 }` |
|         21 | 2028 | `					 }` |
|          - | 2029 | `					 /* Swap operands */` |
|         63 | 2030 | `					 iTmp = iRight;` |
|         63 | 2031 | `					 iRight = iLeft;` |
|         63 | 2032 | `					 iLeft = iTmp;` |
|         30 | 2033 | `				 }` |
|          - | 2034 | `				 /* Link the node to the tree */` |
|    5248731 | 2035 | `				 pNode->pLeft = apNode[iLeft];` |
|    5248731 | 2036 | `				 pNode->pRight = apNode[iRight];` |
|    5248731 | 2037 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2624363 | 2038 | `			 }` |
|  298788357 | 2039 | `			 iLeft = iCur;` |
|  149394181 | 2040 | `		 }` |
|   71500522 | 2041 | `	 }` |
|          - | 2042 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2043 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2044 | `	  * we are dealing with a single operator.` |
|          - | 2045 | `	  */` |
|   14300107 | 2046 | `	  iLeft = -1;` |
|  103216167 | 2047 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   89374491 | 2048 | `		  if( apNode[iCur] == 0 ){` |
|   64985199 | 2049 | `			  continue;` |
|          - | 2050 | `		  }` |
|   24389297 | 2051 | `		  pNode = apNode[iCur];` |
|   24389297 | 2052 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     458431 | 2053 | `			  sxi32 iNest = 1;` |
|     458431 | 2054 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2055 | `				  /* Missing condition */` |
|          3 | 2056 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2057 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2058 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2059 | `				  }` |
|          3 | 2060 | `				  return rc;` |
|          - | 2061 | `			  }` |
|          - | 2062 | `			  /* Get the right node */` |
|     458429 | 2063 | `			  iRight = iCur + 1;` |
|    1888491 | 2064 | `			  while( iRight < nToken  ){` |
|    1888491 | 2065 | `				  if( apNode[iRight] ){` |
|     912961 | 2066 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2067 | `						  /* Increment nesting level */` |
|        ! 0 | 2068 | `						  ++iNest;` |
|     912961 | 2069 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2070 | `						  /* Decrement nesting level */` |
|     458429 | 2071 | `						  --iNest;` |
|     458429 | 2072 | `						  if( iNest <= 0 ){` |
|     458429 | 2073 | `							  break;` |
|          - | 2074 | `						  }` |
|        ! 0 | 2075 | `					  }` |
|     227266 | 2076 | `				  }` |
|    1430067 | 2077 | `				  iRight++;` |
|          5 | 2078 | `			  }` |
|     458429 | 2079 | `			  if( iRight > iCur + 1 ){` |
|          - | 2080 | `				  /* Recurse and process the then expression */` |
|     454537 | 2081 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     454537 | 2082 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2083 | `					  return rc;` |
|          - | 2084 | `				  }` |
|          - | 2085 | `				  /* Link the node to the tree */` |
|     454537 | 2086 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     227266 | 2087 | `			  }else{` |
|          - | 2088 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2089 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2090 | `			  }` |
|     458429 | 2091 | `			  apNode[iCur + 1] = 0;` |
|     458429 | 2092 | `			  if( iRight + 1 < nToken ){` |
|          - | 2093 | `				  /* Recurse and process the else expression */` |
|     458429 | 2094 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     458429 | 2095 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2096 | `					  return rc;` |
|          - | 2097 | `				  }` |
|          - | 2098 | `				  /* Link the node to the tree */` |
|     458429 | 2099 | `				  pNode->pRight = apNode[iRight + 1];` |
|     458429 | 2100 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     229217 | 2101 | `			  }else{` |
|        ! 0 | 2102 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2103 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2104 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2105 | `				 }` |
|        ! 0 | 2106 | `				 return rc;` |
|          - | 2107 | `			  }` |
|          - | 2108 | `			  /* Point to the condition */` |
|     458429 | 2109 | `			  pNode->pCond  = apNode[iLeft];` |
|     458429 | 2110 | `			  apNode[iLeft] = 0;` |
|     458429 | 2111 | `			  break;` |
|          - | 2112 | `		  }` |
|   23930871 | 2113 | `		  iLeft = iCur;` |
|   11965438 | 2114 | `	  }` |
|          - | 2115 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2116 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2117 | `	  * so there is no need for a precedence loop here.` |
|          - | 2118 | `	  */` |
|   14300105 | 2119 | `	 iRight = -1;` |
|  106985685 | 2120 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|   92685639 | 2121 | `		 if( apNode[iCur] == 0 ){` |
|   73570119 | 2122 | `			 continue;` |
|          - | 2123 | `		 }` |
|   19115525 | 2124 | `		 pNode = apNode[iCur];` |
|   19115525 | 2125 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2126 | `			 /* Get the left node */` |
|    4815347 | 2127 | `			 iLeft = iCur - 1;` |
|    6588143 | 2128 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1772801 | 2129 | `				 iLeft--;` |
|          5 | 2130 | `			 }` |
|    4815347 | 2131 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2132 | `				 /* Syntax error */` |
|         45 | 2133 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2134 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2135 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2136 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2137 | `				 }else{` |
|         41 | 2138 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2139 | `				 }` |
|         45 | 2140 | `				 if( rc != SXERR_ABORT ){` |
|         43 | 2141 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2142 | `				 }` |
|         45 | 2143 | `				 return rc;` |
|          - | 2144 | `			 }` |
|          - | 2145 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2146 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2147 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2148 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2149 | `			  * a write. */` |
|    4815305 | 2150 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2151 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2152 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2153 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2154 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2155 | `				 }` |
|         11 | 2156 | `				 return rc;` |
|          - | 2157 | `			 }` |
|          - | 2158 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2159 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2160 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2161 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2162 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    4815297 | 2163 | `			 pSuppress = 0;` |
|    4815292 | 2164 | `			 if( apNode[iLeft]->pOp` |
|    3073853 | 2165 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     666207 | 2166 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2167 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2168 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2169 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2170 | `			 }` |
|    4815297 | 2171 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2172 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2173 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2174 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2175 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2176 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2177 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        103 | 2178 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2179 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2180 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2181 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2182 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2183 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2184 | `					 }` |
|          8 | 2185 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2186 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2187 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2188 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2189 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2190 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2191 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2192 | `						 iRight = iCur;` |
|          9 | 2193 | `						 continue;` |
|          - | 2194 | `					 }` |
|        ! 0 | 2195 | `				 }` |
|        123 | 2196 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         88 | 2197 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2198 | `					 /* Left operand must be a modifiable l-value */` |
|          6 | 2199 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2200 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2201 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2202 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2203 | `					 }else{` |
|          4 | 2204 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          2 | 2205 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2206 | `					 }` |
|          6 | 2207 | `					 if( rc != SXERR_ABORT ){` |
|          6 | 2208 | `						 rc = SXERR_SYNTAX;` |
|          2 | 2209 | `					 }` |
|          6 | 2210 | `					 return rc;` |
|          - | 2211 | `				 }` |
|         43 | 2212 | `			 }` |
|          - | 2213 | `			 /* Link the node to the tree (Reverse) */` |
|    4815285 | 2214 | `			 pNode->pLeft = apNode[iRight];` |
|    4815285 | 2215 | `			 pNode->pRight = apNode[iLeft];` |
|    4815285 | 2216 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    4815285 | 2217 | `			 if( pSuppress ){` |
|          - | 2218 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2219 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2220 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2221 | `			 }` |
|    2407640 | 2222 | `		 }` |
|   19115463 | 2223 | `		 iRight = iCur;` |
|    9557734 | 2224 | `	 }` |
|          - | 2225 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   71500235 | 2226 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   57200189 | 2227 | `		 iLeft = -1;` |
|  427942453 | 2228 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  370742269 | 2229 | `			 if( apNode[iCur] == 0 ){` |
|  313541833 | 2230 | `				 continue;` |
|          - | 2231 | `			 }` |
|   57200441 | 2232 | `			 pNode = apNode[iCur];` |
|   57200441 | 2233 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2234 | `				 /* Get the right node */` |
|         51 | 2235 | `				 iRight = iCur + 1;` |
|         63 | 2236 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2237 | `					 iRight++;` |
|          1 | 2238 | `				 }` |
|         51 | 2239 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2240 | `					 /* Syntax error */` |
|        ! 0 | 2241 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2242 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2243 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2244 | `					 }` |
|        ! 0 | 2245 | `					 return rc;` |
|          - | 2246 | `				 }` |
|          - | 2247 | `				 /* Link the node to the tree */` |
|         51 | 2248 | `				 pNode->pLeft = apNode[iLeft];` |
|         51 | 2249 | `				 pNode->pRight = apNode[iRight];` |
|         51 | 2250 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         24 | 2251 | `			 }` |
|   57200441 | 2252 | `			 iLeft = iCur;` |
|   28600223 | 2253 | `		 }` |
|   28600097 | 2254 | `	 }` |
|          - | 2255 | `	 /* Point to the root of the expression tree */` |
|   92685543 | 2256 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   78385515 | 2257 | `		 if( apNode[iCur] ){` |
|   13694425 | 2258 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         23 | 2259 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|         23 | 2260 | `				  if( rc != SXERR_ABORT ){` |
|         23 | 2261 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2262 | `				  }` |
|         23 | 2263 | `				  return rc;` |
|          - | 2264 | `			 }` |
|   13694407 | 2265 | `			 apNode[0] = apNode[iCur];` |
|   13694407 | 2266 | `			 apNode[iCur] = 0;` |
|    6847201 | 2267 | `		 }` |
|   39192751 | 2268 | `	 }` |
|   14300033 | 2269 | `	 return SXRET_OK;` |
|   13031662 | 2270 | ` }` |
|          - | 2271 | ` /*` |
|          - | 2272 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2273 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2274 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2275 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2276 | `  */` |
|   15238318 | 2277 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2278 | `{` |
|          - | 2279 | `	ph7_expr_node **apNode;` |
|          - | 2280 | `	ph7_expr_node *pNode;` |
|          - | 2281 | `	sxi32 rc;` |
|          - | 2282 | `	/* Reset node container */` |
|   15238323 | 2283 | `	SySetReset(pExprNode);` |
|   15238323 | 2284 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2285 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2286 | `	{` |
|   15238323 | 2287 | `		int iLastWasTerm = 0;` |
|   15238323 | 2288 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|   95147611 | 2289 | `		while( pGen->pIn < pGen->pEnd ){` |
|   79909327 | 2290 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   79909327 | 2291 | `			if( rc != SXRET_OK ){` |
|         38 | 2292 | `				return rc;` |
|          - | 2293 | `			}` |
|          - | 2294 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   79909293 | 2295 | `			if( pNode->xCode ){` |
|          - | 2296 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   40710689 | 2297 | `				iLastWasTerm = 1;` |
|   59553951 | 2298 | `			}else if( pNode->pOp ){` |
|          - | 2299 | `				/* Operator node */` |
|   22613683 | 2300 | `				iLastWasTerm = 0;` |
|   11306844 | 2301 | `			}else{` |
|          - | 2302 | `				/* Delimiter: ')' and ']' end terms */` |
|   16584931 | 2303 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2304 | `			}` |
|          - | 2305 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2306 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2307 | `			 * node kind, so this single test covers all branches. */` |
|   79909293 | 2308 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2309 | `			/* Save the extracted node */` |
|   79909293 | 2310 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2311 | `		}` |
|          - | 2312 | `	}` |
|   15238289 | 2313 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2314 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2315 | `		*ppRoot = 0;` |
|        ! 0 | 2316 | `		return SXRET_OK;` |
|          - | 2317 | `	}` |
|   15238289 | 2318 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2319 | `	/* Make sure we are dealing with valid nodes */` |
|   15238289 | 2320 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   15238289 | 2321 | `	if( rc != SXRET_OK ){` |
|          - | 2322 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2323 | `		 * cleanup the mess left behind.` |
|          - | 2324 | `		 */` |
|         56 | 2325 | `		*ppRoot = 0;` |
|         56 | 2326 | `		return rc;` |
|          - | 2327 | `	}` |
|          - | 2328 | `	/* Build the tree */` |
|   15238237 | 2329 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   15238237 | 2330 | `	if( rc != SXRET_OK ){` |
|          - | 2331 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        103 | 2332 | `		*ppRoot = 0;` |
|        103 | 2333 | `		return rc;` |
|          - | 2334 | `	}` |
|          - | 2335 | `	/* Point to the root of the tree */` |
|   15238139 | 2336 | `	*ppRoot = apNode[0];` |
|   15238139 | 2337 | `	return SXRET_OK;` |
|    7619164 | 2338 | `}` |
|          - | 2339 |  |
