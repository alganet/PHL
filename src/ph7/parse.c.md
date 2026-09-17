# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1208/1392 lines (86.78%)

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
|   29172506 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   29172511 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  460292108 |  278 | `	for(;;){` |
|  920584221 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  920584221 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   87985337 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   43992671 |  285 | `		}else{` |
|  832598889 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  920584221 |  288 | `		if( rc == 0 ){` |
|   29507321 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   29051535 |  291 | `				return &aOpTable[n];` |
|          - |  292 | `			}` |
|          - |  293 | `			/* Handle ambiguity */` |
|     455791 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|          - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      43209 |  296 | `				return &aOpTable[n];` |
|          - |  297 | `			}` |
|     412587 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      77785 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|          - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      77785 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|          - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      77777 |  303 | `					return &aOpTable[n];` |
|          - |  304 | `				}` |
|          - |  305 |  |
|          4 |  306 | `			}` |
|     167405 |  307 | `		}` |
|  891411715 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   14586258 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    7931416 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    7931421 |  320 | `	SyToken *pCur = pIn;` |
|    7931421 |  321 | `	sxi32 iNest = 1;` |
|   85063091 |  322 | `	for(;;){` |
|  170126187 |  323 | `		if( pCur >= pEnd ){` |
|      16037 |  324 | `			break;` |
|          - |  325 | `		}` |
|  170110155 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    6557211 |  328 | `			iNest++;` |
|  166831552 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   14472595 |  331 | `			iNest--;` |
|   14472595 |  332 | `			if( iNest <= 0 ){` |
|    7915389 |  333 | `				break;` |
|          - |  334 | `			}` |
|    3278603 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  162194771 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    7931421 |  340 | `	*ppEnd = pCur;` |
|    7931421 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     541020 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     541020 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     540983 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        131 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     540899 |  358 | `	if( bCheckFunc ){` |
|      58670 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      58658 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      58634 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|         61 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      29307 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     540843 |  366 | `	return FALSE;` |
|     270515 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   17112018 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   17112023 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7803 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7803 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3899 |  383 | `	}` |
|   17112023 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  107263631 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   90151649 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     243317 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   89908337 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    7553153 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     408902 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    7061857 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    7061857 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    7061857 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    7061857 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3530926 |  401 | `					}` |
|    3530926 |  402 | `			}` |
|    7553153 |  403 | `			iParen++;` |
|   86131763 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    7553157 |  405 | `			if( iParen <= 0 ){` |
|         16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         16 |  407 | `				if( rc != SXERR_ABORT ){` |
|         16 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         16 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    7553145 |  412 | `			iParen--;` |
|   78578607 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    3027645 |  414 | `			iSquare++;` |
|   73288217 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    3027649 |  416 | `			if( iSquare <= 0 ){` |
|          9 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          9 |  418 | `				if( rc != SXERR_ABORT ){` |
|          9 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          9 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    3027643 |  423 | `			iSquare--;` |
|   70260572 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       3897 |  425 | `			iBraces++;` |
|       3897 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|          - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|          - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|          - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|          3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|          3 |  431 | `				if( rc != SXERR_ABORT ){` |
|          3 |  432 | `					rc = SXERR_SYNTAX;` |
|          1 |  433 | `				}` |
|          3 |  434 | `				return rc;` |
|          5 |  435 | `			}` |
|   68744806 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3907 |  437 | `			if( iBraces <= 0 ){` |
|         16 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         16 |  439 | `				if( rc != SXERR_ABORT ){` |
|         16 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         16 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3895 |  444 | `			iBraces--;` |
|   68740904 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     504361 |  446 | `			if( iQuesty > 0 ){` |
|     504059 |  447 | `				iQuesty--;` |
|     252334 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   68486779 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   22537595 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   22537595 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     504061 |  461 | `				iQuesty++;` |
|   22285567 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      93727 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      46861 |  481 | `			}` |
|   11268795 |  482 | `		}` |
|   44954153 |  483 | `	}` |
|   17111987 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         15 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         15 |  486 | `		if( rc != SXERR_ABORT ){` |
|         15 |  487 | `			rc = SXERR_SYNTAX;` |
|          6 |  488 | `		}` |
|         15 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   17111975 |  491 | `	return SXRET_OK;` |
|    8556014 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   14110212 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   14110217 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   14110217 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   14106265 |  502 | `		pIn++;` |
|    7053130 |  503 | `	}` |
|    7057123 |  504 | `	for(;;){` |
|   14114251 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       4039 |  506 | `			pIn++;` |
|       4039 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       4037 |  508 | `				pIn++;` |
|       2016 |  509 | `			}` |
|       2022 |  510 | `		}else{` |
|    7055111 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   14110217 |  515 | `	*ppCur = pIn;` |
|   14110217 |  516 | `}` |
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
|       1298 |  561 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|          5 |  562 | `{` |
|       1303 |  563 | `	SyToken *pIn = *ppIn;` |
|       1303 |  564 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       1303 |  602 | `	*ppIn = pIn;` |
|       1303 |  603 | `}` |
|        642 |  604 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  605 | `{` |
|        647 |  606 | `	SyToken *pIn = *ppCur;` |
|          - |  607 | `	sxi32 rc;` |
|          - |  608 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|          - |  609 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|          - |  610 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|          - |  611 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|          - |  612 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|        647 |  613 | `	pIn++;` |
|        642 |  614 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|        339 |  615 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|         26 |  616 | `		pIn++;` |
|         12 |  617 | `	}` |
|        647 |  618 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  619 | `		/* Syntax error */` |
|          6 |  620 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  621 | `		if( rc != SXERR_ABORT ){` |
|          6 |  622 | `			rc = SXERR_SYNTAX;` |
|          2 |  623 | `		}` |
|          6 |  624 | `		goto Synchronize;` |
|          - |  625 | `	}` |
|        643 |  626 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|        643 |  627 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        643 |  628 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|          - |  629 | `		/* Two different failures used to share this arm and both claimed the body was` |
|          - |  630 | `		 * missing. They are distinguishable: the delimiter search leaves pIn ON the` |
|          - |  631 | `		 * ')' when it found one, and AT pEnd when it did not.` |
|          - |  632 | ``		 *   pIn >= pEnd      the parameter list never closed (`function($x {`)`` |
|          - |  633 | `		 *                    -> php expects ')'` |
|          - |  634 | `		 *   &pIn[1] >= pEnd  ')' closed it but nothing follows -> php expects '{'` |
|          - |  635 | `		 * php names the token that actually comes next, which lives just past the` |
|          - |  636 | `		 * expression slice, still in the raw stream. */` |
|          6 |  637 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|          6 |  638 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,pIn >= pEnd ? "\")\"" : "\"{\"");` |
|          6 |  639 | `		if( rc != SXERR_ABORT ){` |
|          6 |  640 | `			rc = SXERR_SYNTAX;` |
|          2 |  641 | `		}` |
|          6 |  642 | `		goto Synchronize;` |
|          - |  643 | `	}` |
|        639 |  644 | `	pIn++; /* Jump the trailing parenthesis */` |
|          - |  645 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|        639 |  646 | `	ExprSkipReturnType(&pIn,pEnd);` |
|        639 |  647 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|        139 |  648 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|          - |  649 | `		/* Check if we are dealing with a closure */` |
|        139 |  650 | `		if( nKey == PH7_TKWRD_USE ){` |
|        131 |  651 | `			pIn++; /* Jump the 'use' keyword */` |
|        131 |  652 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  653 | `				/* Syntax error */` |
|          6 |  654 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  655 | `				if( rc != SXERR_ABORT ){` |
|          6 |  656 | `					rc = SXERR_SYNTAX;` |
|          2 |  657 | `				}` |
|          6 |  658 | `				goto Synchronize;` |
|          - |  659 | `			}` |
|        127 |  660 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|          - |  661 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|          - |  662 | `			 * has no nested structure to balance here, so the first token that is not` |
|          - |  663 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|          - |  664 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|          - |  665 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|          - |  666 | `			 * the first foreign token. */` |
|          - |  667 | `			{` |
|        127 |  668 | `				SyToken *pUse = pIn;` |
|        127 |  669 | `				int bClosed = 0;` |
|        533 |  670 | `				while( pUse < pEnd ){` |
|        533 |  671 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|        413 |  672 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        411 |  673 | `						pUse++;` |
|        411 |  674 | `						continue;` |
|          - |  675 | `					}` |
|          3 |  676 | `					break; /* foreign token: php names this one */` |
|        ! 0 |  677 | `				}` |
|        127 |  678 | `				if( !bClosed ){` |
|          - |  679 | `					/* php names the offending token and expects ')'; if the list simply` |
|          - |  680 | `					 * ran off the end of the slice, that token sits just past it. */` |
|          3 |  681 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|          3 |  682 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|          3 |  683 | `					if( rc != SXERR_ABORT ){` |
|          3 |  684 | `						rc = SXERR_SYNTAX;` |
|          1 |  685 | `					}` |
|          3 |  686 | `					goto Synchronize;` |
|          - |  687 | `				}` |
|        125 |  688 | `				pIn = pUse; /* on the ')' */` |
|          - |  689 | `			}` |
|        125 |  690 | `			if( &pIn[1] >= pEnd ){` |
|          - |  691 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|          3 |  692 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|          3 |  693 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|          3 |  694 | `				if( rc != SXERR_ABORT ){` |
|          3 |  695 | `					rc = SXERR_SYNTAX;` |
|          1 |  696 | `				}` |
|          3 |  697 | `				goto Synchronize;` |
|          - |  698 | `			}` |
|        123 |  699 | `			pIn++;` |
|          - |  700 | `			/* php 7.1+: the return type may also follow the use clause —` |
|          - |  701 | ``			 * `function (...) use (...) : int {` */`` |
|        123 |  702 | `			ExprSkipReturnType(&pIn,pEnd);` |
|         64 |  703 | `		}else{` |
|          - |  704 | `			/* Syntax error */` |
|         12 |  705 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|         12 |  706 | `			if( rc != SXERR_ABORT ){` |
|         12 |  707 | `				rc = SXERR_SYNTAX;` |
|          4 |  708 | `			}` |
|         12 |  709 | `			goto Synchronize;` |
|          - |  710 | `		}` |
|         59 |  711 | `	}` |
|          - |  712 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|          - |  713 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|          - |  714 | `	 * the type), and pEnd is one past the last token. */` |
|        623 |  715 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|        623 |  716 | `		pIn++; /* Jump the leading curly '{' */` |
|        623 |  717 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        623 |  718 | `		if( pIn < pEnd ){` |
|        623 |  719 | `			pIn++;` |
|        309 |  720 | `		}` |
|        314 |  721 | `	}else{` |
|          - |  722 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|          - |  723 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|          - |  724 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|          - |  725 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|        ! 0 |  726 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|        ! 0 |  727 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|        ! 0 |  728 | `		if( rc == SXERR_ABORT ){` |
|        ! 0 |  729 | `			return SXERR_ABORT;` |
|          - |  730 | `		}` |
|          - |  731 | `	}` |
|        623 |  732 | `	rc = SXRET_OK;` |
|        321 |  733 | `Synchronize:` |
|          - |  734 | `	/* Synchronize pointers */` |
|        647 |  735 | `	*ppCur = pIn;` |
|        647 |  736 | `	return rc;` |
|        326 |  737 | `}` |
|          - |  738 | `/*` |
|          - |  739 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|          - |  740 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|          - |  741 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|          - |  742 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|          - |  743 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|          - |  744 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|          - |  745 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|          - |  746 | ` */` |
|         30 |  747 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          4 |  748 | `{` |
|         34 |  749 | `	SyToken *pIn = *ppCur;` |
|         34 |  750 | `	sxu32 nLine = pIn->nLine;` |
|          - |  751 | `	sxi32 rc;` |
|         34 |  752 | `	pIn++; /* Jump the 'class' keyword */` |
|          - |  753 | `	/* Optional constructor argument list */` |
|         34 |  754 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|          7 |  755 | `		pIn++; /* Jump '(' */` |
|          7 |  756 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|          7 |  757 | `		if( pIn < pEnd ){` |
|          7 |  758 | `			pIn++; /* Jump ')' */` |
|          3 |  759 | `		}` |
|          3 |  760 | `	}` |
|          - |  761 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|          - |  762 | `	 * (no braces appear between ')' and the class body). */` |
|         62 |  763 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|         32 |  764 | `		pIn++;` |
|          4 |  765 | `	}` |
|         34 |  766 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|          - |  767 | `		/* Syntax error: missing class body */` |
|        ! 0 |  768 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|          - |  769 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|        ! 0 |  770 | `		if( rc != SXERR_ABORT ){` |
|        ! 0 |  771 | `			rc = SXERR_SYNTAX;` |
|        ! 0 |  772 | `		}` |
|        ! 0 |  773 | `		*ppCur = pIn;` |
|        ! 0 |  774 | `		return rc;` |
|          - |  775 | `	}` |
|         34 |  776 | `	pIn++; /* Jump the leading '{' */` |
|         34 |  777 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|         34 |  778 | `	if( pIn < pEnd ){` |
|         34 |  779 | `		pIn++; /* Jump the trailing '}' */` |
|         15 |  780 | `	}` |
|         34 |  781 | `	*ppCur = pIn;` |
|         34 |  782 | `	return SXRET_OK;` |
|         19 |  783 | `}` |
|          - |  784 | `/*` |
|          - |  785 | ` * Assemble a PHP 7.4 arrow function token range:` |
|          - |  786 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|          - |  787 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|          - |  788 | ` * past the body expression — the body ends at the first top-level comma,` |
|          - |  789 | ` * semicolon, or unbalanced closing delimiter.` |
|          - |  790 | ` */` |
|        546 |  791 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  792 | `{` |
|        551 |  793 | `	SyToken *pIn = *ppCur;` |
|          - |  794 | `	sxu32 nLine;` |
|          - |  795 | `	sxi32 rc;` |
|          - |  796 | `	int iNest;` |
|        551 |  797 | `	nLine = pIn->nLine;` |
|          - |  798 | `	/* Optional 'static' prefix */` |
|        546 |  799 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|        551 |  800 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|          9 |  801 | `		pIn++;` |
|          4 |  802 | `	}` |
|          - |  803 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|        546 |  804 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        551 |  805 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|        ! 0 |  806 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  807 | `		goto Synchronize;` |
|          - |  808 | `	}` |
|        551 |  809 | `	pIn++; /* Jump 'fn' */` |
|        273 |  810 | `	SXUNUSED(nLine);` |
|        273 |  811 | `	SXUNUSED(pGen);` |
|          - |  812 | `	/* Optional '&' for return-by-reference */` |
|        551 |  813 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        ! 0 |  814 | `		pIn++;` |
|        ! 0 |  815 | `	}` |
|          - |  816 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|          - |  817 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|          - |  818 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|          - |  819 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|        551 |  820 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        549 |  821 | `		pIn++; /* '(' */` |
|        549 |  822 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        549 |  823 | `		if( pIn < pEnd ){` |
|        547 |  824 | `			pIn++; /* ')' */` |
|        271 |  825 | `		}` |
|        272 |  826 | `	}` |
|          - |  827 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|        551 |  828 | `	ExprSkipReturnType(&pIn,pEnd);` |
|          - |  829 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|        551 |  830 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        545 |  831 | `		pIn++;` |
|        270 |  832 | `	}` |
|          - |  833 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|        551 |  834 | `	iNest = 0;` |
|       4229 |  835 | `	while( pIn < pEnd ){` |
|       4093 |  836 | `		if( iNest == 0 && (pIn->nType &` |
|          - |  837 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        414 |  838 | `			break;` |
|          - |  839 | `		}` |
|       3683 |  840 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        546 |  841 | `			iNest++;` |
|       3412 |  842 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        546 |  843 | `			iNest--;` |
|        271 |  844 | `		}` |
|       3683 |  845 | `		pIn++;` |
|          5 |  846 | `	}` |
|        551 |  847 | `	rc = SXRET_OK;` |
|        273 |  848 | `Synchronize:` |
|        551 |  849 | `	*ppCur = pIn;` |
|        551 |  850 | `	return rc;` |
|          5 |  851 | `}` |
|          - |  852 | `/*` |
|          - |  853 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|          - |  854 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|          - |  855 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|          - |  856 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|          - |  857 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|          - |  858 | ` */` |
|         74 |  859 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          4 |  860 | `{` |
|         78 |  861 | `	SyToken *pIn = *ppCur;` |
|          - |  862 | `	sxi32 rc;` |
|         37 |  863 | `	SXUNUSED(pGen);` |
|          - |  864 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|         74 |  865 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|         78 |  866 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|        ! 0 |  867 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  868 | `		goto Synchronize;` |
|          - |  869 | `	}` |
|         78 |  870 | `	pIn++; /* Jump 'match' */` |
|          - |  871 | `	/* Optional '(' subject ')' */` |
|         78 |  872 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         78 |  873 | `		pIn++;` |
|         78 |  874 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|         78 |  875 | `		if( pIn < pEnd ){` |
|         78 |  876 | `			pIn++; /* ')' */` |
|         37 |  877 | `		}` |
|         37 |  878 | `	}` |
|          - |  879 | `	/* Optional '{' arms '}' */` |
|         78 |  880 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|         78 |  881 | `		pIn++;` |
|         78 |  882 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|         78 |  883 | `		if( pIn < pEnd ){` |
|         78 |  884 | `			pIn++; /* '}' */` |
|         37 |  885 | `		}` |
|         37 |  886 | `	}` |
|         78 |  887 | `	rc = SXRET_OK;` |
|         37 |  888 | `Synchronize:` |
|         78 |  889 | `	*ppCur = pIn;` |
|         78 |  890 | `	return rc;` |
|          4 |  891 | `}` |
|          - |  892 | `/*` |
|          - |  893 | ` * Extract a single expression node from the input.` |
|          - |  894 | ` * On success store the freshly extractd node in ppNode.` |
|          - |  895 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  896 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|          - |  897 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|          - |  898 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|          - |  899 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|          - |  900 | ` */` |
|   90155962 |  901 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  902 | `{` |
|          - |  903 | `	ph7_expr_node *pNode;` |
|          - |  904 | `	SyToken *pCur;` |
|          - |  905 | `	sxi32 rc;` |
|          - |  906 | `	/* Allocate a new node */` |
|   90155967 |  907 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   90155967 |  908 | `	if( pNode == 0 ){` |
|          - |  909 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  910 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  911 | `		 */` |
|        ! 0 |  912 | `		return SXERR_MEM;` |
|          - |  913 | `	}` |
|          - |  914 | `	/* Zero the structure */` |
|   90155967 |  915 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   90155967 |  916 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  917 | `	/* Point to the head of the token stream */` |
|   90155967 |  918 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  919 | `	/* Start collecting tokens */` |
|   90155967 |  920 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4237 |  921 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|          - |  922 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|          - |  923 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|          - |  924 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|          - |  925 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|         84 |  926 | `			pNode->pEnd = pCur;` |
|         84 |  927 | `			pCur++;` |
|         84 |  928 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|         84 |  929 | `			pNode->xCode = PH7_CompileFccMarker;` |
|         84 |  930 | `			pGen->pIn = pCur;` |
|         84 |  931 | `			*ppNode = pNode;` |
|         84 |  932 | `			return SXRET_OK;` |
|          - |  933 | `		}` |
|          - |  934 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|          - |  935 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       4155 |  936 | `		pCur++;` |
|       4155 |  937 | `		pGen->pIn = pCur;` |
|       4155 |  938 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4155 |  939 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4155 |  940 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4155 |  941 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2075 |  942 | `		}` |
|       4155 |  943 | `		return rc;` |
|          - |  944 | `	}` |
|   90151735 |  945 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - |  946 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - |  947 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - |  948 | `		 */` |
|     243319 |  949 | `		pCur++; /* Skip the opening '[' */` |
|     243319 |  950 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     243319 |  951 | `		if( pCur < pGen->pEnd ){` |
|     243319 |  952 | `			pCur++; /* Skip past the closing ']' */` |
|     121662 |  953 | `		}else{` |
|        ! 0 |  954 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - |  955 | `				"Short array: Missing closing bracket ']'");` |
|        ! 0 |  956 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 |  957 | `				rc = SXERR_SYNTAX;` |
|        ! 0 |  958 | `			}` |
|        ! 0 |  959 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 |  960 | `			return rc;` |
|          - |  961 | `		}` |
|          - |  962 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|          - |  963 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|          - |  964 | `		 */` |
|     243530 |  965 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        427 |  966 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        427 |  967 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|         67 |  968 | `				pNode->xCode = PH7_CompileShortList;` |
|         36 |  969 | `			}else{` |
|        365 |  970 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - |  971 | `			}` |
|        216 |  972 | `		}else{` |
|     242897 |  973 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 |  974 | `		}` |
|   90030078 |  975 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|          - |  976 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|          - |  977 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|          - |  978 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|          - |  979 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|          - |  980 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|          - |  981 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|          - |  982 | `		 * call, not the clone() intrinsic. */` |
|         19 |  983 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         19 |  984 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|         19 |  985 | `		pNode->xCode = PH7_CompileLiteral;` |
|   89908408 |  986 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   25565290 |  987 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   12811799 |  988 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|          - |  989 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|          - |  990 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|          - |  991 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|          - |  992 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|          - |  993 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|          - |  994 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|          - |  995 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|          - |  996 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|          - |  997 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|          - |  998 | `		 * operator (which would dereference the NULL pOp). */` |
|         24 |  999 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         24 | 1000 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|         24 | 1001 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         24 | 1002 | `		if( pCur < pGen->pEnd ){` |
|         24 | 1003 | `			pCur++; /* skip the closing ')' */` |
|         13 | 1004 | `		}else{` |
|        ! 0 | 1005 | `			rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|        ! 0 | 1006 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 | 1007 | `				rc = SXERR_SYNTAX;` |
|        ! 0 | 1008 | `			}` |
|        ! 0 | 1009 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1010 | `			return rc;` |
|          - | 1011 | `		}` |
|         24 | 1012 | `		pNode->xCode = PH7_CompileCloneCall;` |
|   89908392 | 1013 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - | 1014 | `		/* Point to the instance that describe this operator */` |
|   25565273 | 1015 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - | 1016 | `		/* Advance the stream cursor */` |
|   25565273 | 1017 | `		pCur++;` |
|   77125747 | 1018 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - | 1019 | `		/* Isolate variable */` |
|   43246373 | 1020 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   21623195 | 1021 | `			pCur++; /* Variable variable */` |
|          5 | 1022 | `		}` |
|   21623183 | 1023 | `		if( pCur < pGen->pEnd ){` |
|   21623183 | 1024 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - | 1025 | `				/* Variable name */` |
|   21623157 | 1026 | `				pCur++;` |
|   10811607 | 1027 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|         22 | 1028 | `				pCur++;` |
|          - | 1029 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|         22 | 1030 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|         22 | 1031 | `				if( pCur < pGen->pEnd ){` |
|         19 | 1032 | `					pCur++;` |
|         11 | 1033 | `				}else{` |
|          - | 1034 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|          - | 1035 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|          - | 1036 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|          - | 1037 | `					 * delimiter search stops at the slice end, so the token php names` |
|          - | 1038 | `					 * usually sits just past it, still inside the chunk stream. */` |
|          - | 1039 | `					{` |
|          3 | 1040 | `						SyToken *pBad = 0;` |
|          3 | 1041 | `						if( pGen->pTokenSet ){` |
|          3 | 1042 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|          3 | 1043 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|          3 | 1044 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|        ! 0 | 1045 | `								pBad = pCur;` |
|        ! 0 | 1046 | `							}` |
|          1 | 1047 | `						}` |
|          3 | 1048 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|          - | 1049 | `					}` |
|          3 | 1050 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1051 | `						rc = SXERR_SYNTAX;` |
|          1 | 1052 | `					}` |
|          3 | 1053 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1054 | `					return rc;` |
|          - | 1055 | `				}` |
|         11 | 1056 | `			}else{` |
|          - | 1057 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|          - | 1058 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|          - | 1059 | `				 * covered only the '$' and the offending token drifted into a later` |
|          - | 1060 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|          - | 1061 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|         10 | 1062 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|         10 | 1063 | `				if( rc != SXERR_ABORT ){` |
|         10 | 1064 | `					rc = SXERR_SYNTAX;` |
|          4 | 1065 | `				}` |
|         10 | 1066 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         10 | 1067 | `				return rc;` |
|          - | 1068 | `			}` |
|   10811584 | 1069 | `		}` |
|   21623173 | 1070 | `		pNode->xCode = PH7_CompileVariable;` |
|   53531519 | 1071 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1056795 | 1072 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    1056795 | 1073 | `		 if( bAfterMemberOp ){` |
|          - | 1074 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1075 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1076 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1077 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1078 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1079 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1080 | `			  * the word itself. */` |
|     128361 | 1081 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     128361 | 1082 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     128361 | 1083 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     992617 | 1084 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1085 | `			 /* List/Array node */` |
|     428899 | 1086 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1087 | `				 /* Assume a literal */` |
|        ! 0 | 1088 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1089 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1090 | `			 }else{` |
|     428899 | 1091 | `				 pCur += 2;` |
|          - | 1092 | `				 /* Collect array/list tokens */` |
|     428899 | 1093 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     428899 | 1094 | `				 if( pCur < pGen->pEnd ){` |
|     428897 | 1095 | `					 pCur++;` |
|     214451 | 1096 | `				 }else{` |
|          - | 1097 | `					 /* Syntax error */` |
|          - | 1098 | `					 /* php names the token it stopped on and says it expected ")". */` |
|          3 | 1099 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|          3 | 1100 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1101 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1102 | `					 }` |
|          3 | 1103 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1104 | `					 return rc;` |
|          - | 1105 | `				 }` |
|     428897 | 1106 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     428897 | 1107 | `				 if( pNode->xCode == PH7_CompileList ){` |
|         39 | 1108 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|         39 | 1109 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|          - | 1110 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|          - | 1111 | ``						  * not the `list` the construct started at. */`` |
|          3 | 1112 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|          3 | 1113 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1114 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1115 | `						 }` |
|          3 | 1116 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1117 | `						 return rc;` |
|          - | 1118 | `					 }` |
|         16 | 1119 | `				 }` |
|          5 | 1120 | `			 }` |
|     713990 | 1121 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1122 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15921 | 1123 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15921 | 1124 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1125 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1126 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15921 | 1127 | `			 pNode->xCode = PH7_CompileYield;` |
|     491587 | 1128 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     483336 | 1129 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7856 | 1130 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3943 | 1131 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1132 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|        647 | 1133 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1134 | `				 /* Assume a literal */` |
|        ! 0 | 1135 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1136 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1137 | `			 }else{` |
|          - | 1138 | `				 /* Assemble annonymous functions body */` |
|        647 | 1139 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|        647 | 1140 | `				 if( rc != SXRET_OK ){` |
|         28 | 1141 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1142 | `					 return rc;` |
|          - | 1143 | `				 }` |
|        623 | 1144 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1145 | `			  }` |
|     483296 | 1146 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         43 | 1147 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         24 | 1148 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         12 | 1149 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|          9 | 1150 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1151 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1152 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1153 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1154 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         34 | 1155 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         34 | 1156 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1157 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1158 | `				 return rc;` |
|          - | 1159 | `			 }` |
|         34 | 1160 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     482971 | 1161 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     482692 | 1162 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7832 | 1163 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3919 | 1164 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1165 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        551 | 1166 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        551 | 1167 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1168 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1169 | `				 return rc;` |
|          - | 1170 | `			 }` |
|        551 | 1171 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     482684 | 1172 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1173 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         78 | 1174 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         78 | 1175 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1176 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1177 | `				 return rc;` |
|          - | 1178 | `			 }` |
|         78 | 1179 | `			 pNode->xCode = PH7_CompileMatch;` |
|     482374 | 1180 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1181 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1182 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1183 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1184 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1185 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1186 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1187 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1188 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     482319 | 1189 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1190 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         73 | 1191 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         73 | 1192 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         39 | 1193 | `		 }else{` |
|          - | 1194 | `			 /* Assume a literal */` |
|     482233 | 1195 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     482233 | 1196 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1197 | `		 }` |
|   42191526 | 1198 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1199 | `		 /* Constants,function name,namespace path,class name... */` |
|   13499615 | 1200 | `		 if( bAfterMemberOp ){` |
|          - | 1201 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1202 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1203 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1204 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    5625225 | 1205 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    2812610 | 1206 | `		 }` |
|   13499615 | 1207 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   13499615 | 1208 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    6749810 | 1209 | `	 }else{` |
|   28163535 | 1210 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1211 | `			 /* Point to the code generator routine */` |
|    9517417 | 1212 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    9517417 | 1213 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1214 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1215 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1216 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1217 | `				 }` |
|          3 | 1218 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1219 | `				 return rc;` |
|          - | 1220 | `			 }` |
|    4758705 | 1221 | `		 }` |
|          - | 1222 | `		/* Advance the stream cursor */` |
|   28163533 | 1223 | `		pCur++;` |
|          - | 1224 | `	 }` |
|          - | 1225 | `	/* Point to the end of the token stream */` |
|   90151695 | 1226 | `	pNode->pEnd = pCur;` |
|          - | 1227 | `	/* Save the node for later processing */` |
|   90151695 | 1228 | `	*ppNode = pNode;` |
|          - | 1229 | `	/* Synchronize cursors */` |
|   90151695 | 1230 | `	pGen->pIn = pCur;` |
|   90151695 | 1231 | `	return SXRET_OK;` |
|   45077986 | 1232 | `}` |
|          - | 1233 | `/*` |
|          - | 1234 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1235 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1236 | ` * level is zero.` |
|          - | 1237 | ` */` |
|    1939036 | 1238 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1239 | `{` |
|    1939041 | 1240 | `	SyToken *pCur = pStart;` |
|    1939041 | 1241 | `	sxi32 iNest = 0;` |
|    1939041 | 1242 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1243 | `		/* Last expression */` |
|     716063 | 1244 | `		return SXERR_EOF;` |
|          - | 1245 | `	}` |
|    4641261 | 1246 | `	while( pCur < pEnd ){` |
|    4339443 | 1247 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     921165 | 1248 | `			break;` |
|          - | 1249 | `		}` |
|    3418283 | 1250 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     241613 | 1251 | `			iNest++;` |
|    3297479 | 1252 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     241617 | 1253 | `			iNest--;` |
|     120806 | 1254 | `		}` |
|    3418283 | 1255 | `		pCur++;` |
|          5 | 1256 | `	}` |
|    1222983 | 1257 | `	*ppNext = pCur;` |
|    1222983 | 1258 | `	return SXRET_OK;` |
|     969523 | 1259 | `}` |
|          - | 1260 | `/*` |
|          - | 1261 | ` * Free an expression tree.` |
|          - | 1262 | ` */` |
|   77052326 | 1263 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1264 | `{` |
|   77052331 | 1265 | `	if( pNode->pLeft ){` |
|          - | 1266 | `		/* Release the left tree */` |
|   30117105 | 1267 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   15058550 | 1268 | `	}` |
|   77052331 | 1269 | `	if( pNode->pRight ){` |
|          - | 1270 | `		/* Release the right tree */` |
|   17518207 | 1271 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    8759101 | 1272 | `	}` |
|   77052331 | 1273 | `	if( pNode->pCond ){` |
|          - | 1274 | `		/* Release the conditional tree used by the ternary operator */` |
|     504057 | 1275 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     252026 | 1276 | `	}` |
|   77052331 | 1277 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1278 | `		ph7_expr_node **apArg;` |
|          - | 1279 | `		sxu32 n;` |
|          - | 1280 | `		/* Release node arguments */` |
|    8303777 | 1281 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   19113423 | 1282 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   10809651 | 1283 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    5404828 | 1284 | `		}` |
|    8303777 | 1285 | `		SySetRelease(&pNode->aNodeArgs);` |
|    4151886 | 1286 | `	}` |
|          - | 1287 | `	/* Finally,release this node */` |
|   77052331 | 1288 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   77052331 | 1289 | `}` |
|          - | 1290 | `/*` |
|          - | 1291 | ` * Free an expression tree.` |
|          - | 1292 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1293 | ` */` |
|   17112054 | 1294 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1295 | `{` |
|          - | 1296 | `	ph7_expr_node **apNode;` |
|          - | 1297 | `	sxu32 n;` |
|   17112059 | 1298 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  107263801 | 1299 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   90151747 | 1300 | `		if( apNode[n] ){` |
|   17112383 | 1301 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    8556189 | 1302 | `		}` |
|   45075876 | 1303 | `	}` |
|   17112059 | 1304 | `	return SXRET_OK;` |
|          5 | 1305 | `}` |
|          - | 1306 | `/*` |
|          - | 1307 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1308 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1309 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1310 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1311 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1312 | ` */` |
|   21312948 | 1313 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1314 | `{` |
|   21312953 | 1315 | `	if( pNode == 0 ){` |
|   13277261 | 1316 | `		return 0;` |
|          - | 1317 | `	}` |
|    8035697 | 1318 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1319 | `		return 1;` |
|          - | 1320 | `	}` |
|    8035685 | 1321 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1322 | `		return 1;` |
|          - | 1323 | `	}` |
|    8035681 | 1324 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1325 | `		return 1;` |
|          - | 1326 | `	}` |
|    8035681 | 1327 | `	return 0;` |
|   10656479 | 1328 | `}` |
|          - | 1329 | `/*` |
|          - | 1330 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1331 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1332 | ` */` |
|    5218224 | 1333 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1334 | `{` |
|          - | 1335 | `	sxi32 iExprOp;` |
|    5218229 | 1336 | `	if( pNode->pOp == 0 ){` |
|    3721913 | 1337 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1338 | `	}` |
|    1496321 | 1339 | `	iExprOp = pNode->pOp->iOp;` |
|    1496321 | 1340 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     959701 | 1341 | `			return TRUE;` |
|          - | 1342 | `	}` |
|     536625 | 1343 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     536611 | 1344 | `		if( pNode->pLeft->pOp ) {` |
|     128190 | 1345 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      54384 | 1346 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1347 | `				return FALSE;` |
|          5 | 1348 | `			}` |
|     472516 | 1349 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1350 | `			return FALSE;` |
|          - | 1351 | `		}` |
|     536611 | 1352 | `		return TRUE;` |
|          - | 1353 | `	}` |
|         16 | 1354 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1355 | `		return TRUE;` |
|          - | 1356 | `	}` |
|          - | 1357 | `	/* Not a modifiable l or r-value */` |
|          9 | 1358 | `	return FALSE;` |
|    2609117 | 1359 | `}` |
|          - | 1360 | `/* Forward declaration */` |
|          - | 1361 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|          - | 1362 | `/* Macro to check if the given node is a terminal.` |
|          - | 1363 | ` * A node is a term if it has no operator, or has already been linked into an` |
|          - | 1364 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|          - | 1365 | ` * linked ternary/elvis node). */` |
|          - | 1366 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|          - | 1367 | `/*` |
|          - | 1368 | ` * Buid an expression tree for each given function argument.` |
|          - | 1369 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1370 | ` */` |
|    5513388 | 1371 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1372 | `{` |
|          - | 1373 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1374 | `	sxi32 rc;` |
|          - | 1375 | `	/* Process function arguments from left to right */` |
|    5513393 | 1376 | `	iCur = 0;` |
|    6766312 | 1377 | `	for(;;){` |
|   13532629 | 1378 | `		if( iCur >= nToken ){` |
|          - | 1379 | `			/* No more arguments to process */` |
|    5513365 | 1380 | `			break;` |
|          - | 1381 | `		}` |
|    8019269 | 1382 | `		iNode = iCur;` |
|    8019269 | 1383 | `		iNest = 0;` |
|   26271801 | 1384 | `		while( iCur < nToken ){` |
|   20758439 | 1385 | `			if( apNode[iCur] ){` |
|   20711549 | 1386 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1252956 | 1387 | `					break;` |
|   18205642 | 1388 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    9747800 | 1389 | `					&& apNode[iCur]->pLeft == 0` |
|    1289951 | 1390 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1287368 | 1391 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1392 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1393 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1394 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1395 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1396 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1397 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|          - | 1398 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|          - | 1399 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|          - | 1400 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|          - | 1401 | `					 * and swallow the following argument separator. */` |
|    1284787 | 1402 | `					iNest++;` |
|   17563256 | 1403 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    9102826 | 1404 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    1284787 | 1405 | `					iNest--;` |
|     642391 | 1406 | `				}` |
|    9102821 | 1407 | `			}` |
|   18252537 | 1408 | `			iCur++;` |
|          5 | 1409 | `		}` |
|    8019269 | 1410 | `		if( iCur > iNode ){` |
|    8019263 | 1411 | `			SyString sArgName = {0, 0};` |
|          - | 1412 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1413 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1414 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    8019258 | 1415 | `			if( (iCur - iNode) >= 2` |
|    5578716 | 1416 | `				&& apNode[iNode]` |
|    3138151 | 1417 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1692888 | 1418 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     247373 | 1419 | `				&& apNode[iNode+1]` |
|     247103 | 1420 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1421 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        303 | 1422 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        303 | 1423 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        303 | 1424 | `				apNode[iNode] = 0;` |
|        303 | 1425 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        303 | 1426 | `				apNode[iNode+1] = 0;` |
|        303 | 1427 | `				iNode += 2;` |
|          - | 1428 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1429 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        303 | 1430 | `				if( iNode >= iCur ){` |
|          4 | 1431 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1432 | `						pOp->pStart->nLine,` |
|          - | 1433 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1434 | `						&sArgName);` |
|          3 | 1435 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1436 | `						rc = SXERR_SYNTAX;` |
|          1 | 1437 | `					}` |
|          3 | 1438 | `					return rc;` |
|          - | 1439 | `				}` |
|        148 | 1440 | `			}` |
|    8019256 | 1441 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1442 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1443 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1444 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1445 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1446 | `					apNode[iNode] = 0;` |
|        ! 0 | 1447 | `			}` |
|          - | 1448 | `			{` |
|          - | 1449 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1450 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1451 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1452 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1453 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1454 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1455 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1456 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1457 | `				 * NULL slots ahead of the flagged subtree. */` |
|    8019261 | 1458 | `				int bSpreadArg = 0;` |
|          - | 1459 | `				sxi32 iScan;` |
|    8019307 | 1460 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    8019307 | 1461 | `					if( apNode[iScan] ){` |
|    8019261 | 1462 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    8019261 | 1463 | `						break;` |
|          - | 1464 | `					}` |
|         26 | 1465 | `				}` |
|    8019261 | 1466 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    8019261 | 1467 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4085 | 1468 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2040 | 1469 | `				}` |
|          - | 1470 | `			}` |
|    8019261 | 1471 | `			if( apNode[iNode] ){` |
|    8019261 | 1472 | `				if( sArgName.nByte > 0 ){` |
|        300 | 1473 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        300 | 1474 | `					apNode[iNode]->sArgName = sArgName;` |
|        148 | 1475 | `				}` |
|          - | 1476 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    8019261 | 1477 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    4009633 | 1478 | `			}else{` |
|          - | 1479 | `				/* No expression before comma */` |
|        ! 0 | 1480 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1481 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1482 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1483 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1484 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1485 | `				}` |
|        ! 0 | 1486 | `				return rc;` |
|          - | 1487 | `			}` |
|    4009633 | 1488 | `		}else{` |
|          - | 1489 | `			/* Comma with no preceding argument */` |
|          8 | 1490 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1491 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1492 | `				rc = SXERR_SYNTAX;` |
|          3 | 1493 | `			}` |
|          8 | 1494 | `			return rc;` |
|          - | 1495 | `		}` |
|          - | 1496 | `		/* Jump trailing comma */` |
|    8019261 | 1497 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2505901 | 1498 | `			iCur++;` |
|    2505901 | 1499 | `			if( iCur >= nToken ){` |
|          - | 1500 | `				/* Trailing comma after last argument */` |
|         21 | 1501 | `				break;` |
|          - | 1502 | `			}` |
|    1252938 | 1503 | `		}` |
|          5 | 1504 | `	}` |
|    5513385 | 1505 | `	return SXRET_OK;` |
|    2756699 | 1506 | `}` |
|          - | 1507 | ` /*` |
|          - | 1508 | `  * Create an expression tree from an array of tokens.` |
|          - | 1509 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1510 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1511 | `  */` |
|   29576556 | 1512 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1513 | ` {` |
|          - | 1514 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1515 | `	 ph7_expr_node *pNode;` |
|          - | 1516 | `	 ph7_expr_node *pSuppress;` |
|          - | 1517 | `	 sxi32 iCur;` |
|          - | 1518 | `	 sxi32 rc;` |
|   29576561 | 1519 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1520 | `		 /* TICKET 1433-17: self evaluating node */` |
|   12997365 | 1521 | `		 return SXRET_OK;` |
|          - | 1522 | `	 }` |
|          - | 1523 | `	 /* Process expressions enclosed in parenthesis first */` |
|  117984001 | 1524 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1525 | `		 sxi32 iNest;` |
|          - | 1526 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1527 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1528 | `		  */` |
|  101404807 | 1529 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  100913519 | 1530 | `			 continue;` |
|          - | 1531 | `		 }` |
|     491293 | 1532 | `		 iNest = 1;` |
|     491293 | 1533 | `		 iLeft = iCur;` |
|          - | 1534 | `		 /* Find the closing parenthesis */` |
|     491293 | 1535 | `		 iCur++;` |
|    4379009 | 1536 | `		 while( iCur < nToken ){` |
|    4379009 | 1537 | `			 if( apNode[iCur] ){` |
|    4379009 | 1538 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1539 | `					 /* Decrement nesting level */` |
|     717723 | 1540 | `					 iNest--;` |
|     717723 | 1541 | `					 if( iNest <= 0 ){` |
|     491293 | 1542 | `						 break;` |
|          5 | 1543 | `					 }` |
|    3774506 | 1544 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1545 | `					 /* Increment nesting level */` |
|     226435 | 1546 | `					 iNest++;` |
|     113215 | 1547 | `				 }` |
|    1943858 | 1548 | `			 }` |
|    3887721 | 1549 | `			 iCur++;` |
|          5 | 1550 | `		 }` |
|     491293 | 1551 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1552 | `			 sxi32 j;` |
|          - | 1553 | `			 /* Recurse and process this expression */` |
|     491293 | 1554 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     491293 | 1555 | `			 if( rc != SXRET_OK ){` |
|          3 | 1556 | `				 return rc;` |
|          - | 1557 | `			 }` |
|          - | 1558 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1559 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1560 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1561 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1562 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1563 | `			  * group's free below silently drops the unpacking. */` |
|     491291 | 1564 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     491291 | 1565 | `				 if( apNode[j] ){` |
|     491291 | 1566 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     491286 | 1567 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     491291 | 1568 | `					 break;` |
|          - | 1569 | `				 }` |
|        ! 0 | 1570 | `			 }` |
|     245643 | 1571 | `		 }` |
|          - | 1572 | `		 /* Free the left and right nodes */` |
|     491291 | 1573 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     491291 | 1574 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     491291 | 1575 | `		 apNode[iLeft] = 0;` |
|     491291 | 1576 | `		 apNode[iCur] = 0;` |
|     245648 | 1577 | `	 }` |
|          - | 1578 | `	  /* Process expressions enclosed in braces */` |
|  122043991 | 1579 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1580 | `		 sxi32 iNest;` |
|          - | 1581 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1582 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1583 | `		  */` |
|  105776021 | 1584 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  105772131 | 1585 | `			 continue;` |
|          - | 1586 | `		 }` |
|       3895 | 1587 | `		 iNest = 1;` |
|       3895 | 1588 | `		 iLeft = iCur;` |
|          - | 1589 | `		 /* Find the closing parenthesis */` |
|       3895 | 1590 | `		 iCur++;` |
|       7783 | 1591 | `		 while( iCur < nToken ){` |
|       7783 | 1592 | `			 if( apNode[iCur] ){` |
|       7783 | 1593 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1594 | `					 /* Decrement nesting level */` |
|       3895 | 1595 | `					 iNest--;` |
|       3895 | 1596 | `					 if( iNest <= 0 ){` |
|       3895 | 1597 | `						 break;` |
|        ! 0 | 1598 | `					 }` |
|       3893 | 1599 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1600 | `					 /* Increment nesting level */` |
|        ! 0 | 1601 | `					 iNest++;` |
|        ! 0 | 1602 | `				 }` |
|       1944 | 1603 | `			 }` |
|       3893 | 1604 | `			 iCur++;` |
|          5 | 1605 | `		 }` |
|       3895 | 1606 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1607 | `			 /* Recurse and process this expression */` |
|       3893 | 1608 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3893 | 1609 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1610 | `				 return rc;` |
|          - | 1611 | `			 }` |
|       1944 | 1612 | `		 }` |
|          - | 1613 | `		 /* Free the left and right nodes */` |
|       3895 | 1614 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3895 | 1615 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3895 | 1616 | `		 apNode[iLeft] = 0;` |
|       3895 | 1617 | `		 apNode[iCur] = 0;` |
|       1950 | 1618 | `	 }` |
|          - | 1619 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   16267975 | 1620 | `	 iLeft = -1;` |
|  122051733 | 1621 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105783775 | 1622 | `		 if( apNode[iCur] == 0 ){` |
|   46710995 | 1623 | `			 continue;` |
|          - | 1624 | `		 }` |
|   59072785 | 1625 | `		 pNode = apNode[iCur];` |
|   59072785 | 1626 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   16083883 | 1627 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1628 | `				 /* Collect function arguments */` |
|    7061855 | 1629 | `				 sxi32 iPtr = 0;` |
|    7061855 | 1630 | `				 sxi32 nFuncTok = 0;` |
|   34882141 | 1631 | `				 while( nFuncTok + iCur < nToken ){` |
|   34882141 | 1632 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|          - | 1633 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|          - | 1634 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|          - | 1635 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|          - | 1636 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|          - | 1637 | `					  * nulled, so counting it here would over-count and never find` |
|          - | 1638 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   34882141 | 1639 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   34823555 | 1640 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    7523019 | 1641 | `							 iPtr++;` |
|   31062048 | 1642 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    7523019 | 1643 | `							 iPtr--;` |
|    7523019 | 1644 | `							 if( iPtr <= 0 ){` |
|    7061855 | 1645 | `								 break;` |
|          - | 1646 | `							 }` |
|     230582 | 1647 | `						 }` |
|   13880850 | 1648 | `					 }` |
|   27820291 | 1649 | `					 nFuncTok++;` |
|          5 | 1650 | `				 }` |
|    7061855 | 1651 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1652 | `					 /* Syntax error */` |
|        ! 0 | 1653 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1654 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1655 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1656 | `					 }` |
|        ! 0 | 1657 | `					 return rc;` |
|          - | 1658 | `				 }` |
|    7061855 | 1659 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1660 | `					 /* Syntax error */` |
|        ! 0 | 1661 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1662 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1663 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1664 | `					 }` |
|        ! 0 | 1665 | `					 return rc;` |
|          - | 1666 | `				 }` |
|    7061855 | 1667 | `				 if( nFuncTok > 1 ){` |
|          - | 1668 | `					 /* Process function arguments */` |
|    5513393 | 1669 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    5513393 | 1670 | `					 if( rc != SXRET_OK ){` |
|         11 | 1671 | `						 return rc;` |
|          - | 1672 | `					 }` |
|    2756690 | 1673 | `				 }` |
|          - | 1674 | `				 /* Link the node to the tree */` |
|    7061847 | 1675 | `				 pNode->pLeft = apNode[iLeft];` |
|    7061847 | 1676 | `				 apNode[iLeft] = 0;` |
|   34882109 | 1677 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   27820267 | 1678 | `					 apNode[iCur+iPtr] = 0;` |
|   13910136 | 1679 | `				 }` |
|          - | 1680 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1681 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1682 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1683 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1684 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1685 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1686 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1687 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1688 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1689 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1690 | `				 {` |
|    7061847 | 1691 | `					 sxi32 iNew = iLeft - 1;` |
|    9127271 | 1692 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    2065429 | 1693 | `						 iNew--;` |
|          5 | 1694 | `					 }` |
|    7061842 | 1695 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3981798 | 1696 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2412523 | 1697 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     856847 | 1698 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     856847 | 1699 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     856847 | 1700 | `						 apNode[iNew] = 0;` |
|     856847 | 1701 | `						 pNode = apNode[iCur];` |
|     428426 | 1702 | `					 }` |
|          - | 1703 | `				 }` |
|   12552954 | 1704 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1705 | `				 /* Subscripting */` |
|    3027643 | 1706 | `				 sxi32 iArrTok = iCur + 1;` |
|    3027643 | 1707 | `				 sxi32 iNest = 1;` |
|    3027638 | 1708 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         34 | 1709 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         28 | 1710 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|          - | 1711 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|          - | 1712 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|          - | 1713 | `					  * subscripting a global constant raised "Invalid array name" while` |
|          - | 1714 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|          - | 1715 | `					  * worked. */` |
|         22 | 1716 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|         16 | 1717 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    3027638 | 1718 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1719 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1720 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     322490 | 1721 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1722 | `						 /* Syntax error */` |
|        ! 0 | 1723 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1724 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1725 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1726 | `						 }` |
|        ! 0 | 1727 | `						 return rc;` |
|          - | 1728 | `				 }` |
|          - | 1729 | `				 /* Collect index tokens */` |
|    6361663 | 1730 | `				 while( iArrTok < nToken ){` |
|    6361663 | 1731 | `					 if( apNode[iArrTok] ){` |
|    6361631 | 1732 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1733 | `							 /* Increment nesting level */` |
|      27179 | 1734 | `							 iNest++;` |
|    6348044 | 1735 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1736 | `							 /* Decrement nesting level */` |
|    3054817 | 1737 | `							 iNest--;` |
|    3054817 | 1738 | `							 if( iNest <= 0 ){` |
|    3027643 | 1739 | `								 break;` |
|          - | 1740 | `							 }` |
|      13587 | 1741 | `						 }` |
|    1666994 | 1742 | `					 }` |
|    3334025 | 1743 | `					 ++iArrTok;` |
|          5 | 1744 | `				 }` |
|    3027643 | 1745 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1746 | `					 /* Recurse and process this expression */` |
|    2790395 | 1747 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2790395 | 1748 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1749 | `						 return rc;` |
|          - | 1750 | `					 }` |
|          - | 1751 | `					 /* Link the node to it's index */` |
|    2790395 | 1752 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1395195 | 1753 | `				 }` |
|          - | 1754 | `				 /* Link the node to the tree */` |
|    3027643 | 1755 | `				 pNode->pLeft = apNode[iLeft];` |
|    3027643 | 1756 | `				 pNode->pRight = 0;` |
|    3027643 | 1757 | `				 apNode[iLeft] = 0;` |
|    9389301 | 1758 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6361663 | 1759 | `					 apNode[iNest] = 0;` |
|    3180834 | 1760 | `				 }` |
|    1513824 | 1761 | `			 }else{` |
|          - | 1762 | `				 /* Member access operators [i.e: '->','::'] */` |
|    5994395 | 1763 | `				  iRight = iCur + 1;` |
|    5998283 | 1764 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3893 | 1765 | `					 iRight++;` |
|          5 | 1766 | `				 }` |
|    5994395 | 1767 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1768 | `					 /* Syntax error */` |
|          5 | 1769 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1770 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1771 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1772 | `					 }` |
|          5 | 1773 | `					 return rc;` |
|          - | 1774 | `				 }` |
|          - | 1775 | `				 /* Link the node to the tree */` |
|    5994391 | 1776 | `				 pNode->pLeft = apNode[iLeft];` |
|    5994386 | 1777 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    5784221 | 1778 | `					 && pNode->pLeft->pOp == 0 &&` |
|    5483995 | 1779 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1780 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1781 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1782 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1783 | `					  * accepts. */` |
|          4 | 1784 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1785 | `						 /* Syntax error */` |
|        ! 0 | 1786 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1787 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1788 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1789 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1790 | `						 }` |
|        ! 0 | 1791 | `						 return rc;` |
|          - | 1792 | `				 }` |
|    5994391 | 1793 | `				 pNode->pRight = apNode[iRight];` |
|    5994391 | 1794 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1795 | `			 }` |
|    8041933 | 1796 | `		 }` |
|   59072773 | 1797 | `		 iLeft = iCur;` |
|   29536389 | 1798 | `	 }` |
|          - | 1799 | `	 /* Handle left associative (new, clone) operators */` |
|  122051701 | 1800 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105783743 | 1801 | `		 if( apNode[iCur] == 0 ){` |
|   63714285 | 1802 | `			 continue;` |
|          - | 1803 | `		 }` |
|   42069463 | 1804 | `		 pNode = apNode[iCur];` |
|   42069463 | 1805 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1806 | `			 SyToken *pToken;` |
|          - | 1807 | `			 /* Get the left node */` |
|      62587 | 1808 | `			 iLeft = iCur + 1;` |
|      62595 | 1809 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1810 | `				 iLeft++;` |
|          1 | 1811 | `			 }` |
|      62587 | 1812 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1813 | `				  /* Syntax error */` |
|        ! 0 | 1814 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1815 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1816 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1817 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1818 | `				 }` |
|        ! 0 | 1819 | `				 return rc;` |
|          - | 1820 | `			 }` |
|          - | 1821 | `			 /* Make sure the operand are of a valid type */` |
|      62587 | 1822 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1823 | `				 /* Clone:` |
|          - | 1824 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1825 | `				  *  ++ function call (including annonymous)` |
|          - | 1826 | `				  *  ++ array member` |
|          - | 1827 | `				  *  ++ 'new' operator` |
|          - | 1828 | `				  * Example:` |
|          - | 1829 | `				  *   clone $pObj;` |
|          - | 1830 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1831 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1832 | `				  */` |
|      58281 | 1833 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      58275 | 1834 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1835 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1836 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1837 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1838 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1839 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1840 | `						 }` |
|        ! 0 | 1841 | `						 return rc;` |
|          - | 1842 | `					 }` |
|      29135 | 1843 | `				 }` |
|      29143 | 1844 | `			 }else{` |
|          - | 1845 | `				 /* New */` |
|       4306 | 1846 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1847 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1848 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1849 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1850 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1851 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1852 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1853 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1854 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1855 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1856 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1857 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1858 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1859 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1860 | `					 }` |
|        ! 0 | 1861 | `					 return rc;` |
|          - | 1862 | `				 }` |
|       4311 | 1863 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       4311 | 1864 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       4306 | 1865 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         35 | 1866 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1867 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1868 | `						 /* Syntax error */` |
|        ! 0 | 1869 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1870 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1871 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1872 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1873 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1874 | `						 }` |
|        ! 0 | 1875 | `						 return rc;` |
|          - | 1876 | `					 }` |
|       2153 | 1877 | `				 }` |
|          - | 1878 | `			 }` |
|          - | 1879 | `			  /* Link the node to the tree */` |
|      62587 | 1880 | `			 pNode->pLeft = apNode[iLeft];` |
|      62587 | 1881 | `			 apNode[iLeft] = 0;` |
|      62587 | 1882 | `			 pNode->pRight = 0; /* Paranoid */` |
|      31291 | 1883 | `		 }` |
|   21034734 | 1884 | `	 }` |
|          - | 1885 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   16267963 | 1886 | `	 iLeft = -1;` |
|  122051701 | 1887 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105628131 | 1888 | `		 if( apNode[iCur] == 0 ){` |
|   63714285 | 1889 | `			 continue;` |
|          - | 1890 | `		 }` |
|   41913851 | 1891 | `		 pNode = apNode[iCur];` |
|   41913851 | 1892 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     373459 | 1893 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     186733 | 1894 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1895 | `					 /* Link the node to the tree */` |
|     202291 | 1896 | `					 pNode->pLeft = apNode[iLeft];` |
|     202291 | 1897 | `					 apNode[iLeft] = 0;` |
|     101143 | 1898 | `			 }` |
|     435695 | 1899 | `		  }` |
|   42069463 | 1900 | `		 iLeft = iCur;` |
|   21034734 | 1901 | `	  }` |
|   16423575 | 1902 | `	 iLeft = -1;` |
|  122207313 | 1903 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105783743 | 1904 | `		 if( apNode[iCur] == 0 ){` |
|   63916571 | 1905 | `			 continue;` |
|          - | 1906 | `		 }` |
|   41867177 | 1907 | `		 pNode = apNode[iCur];` |
|   41867177 | 1908 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15556 | 1909 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15561 | 1910 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1911 | `					 /* Syntax error */` |
|        ! 0 | 1912 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1913 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1914 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1915 | `					 }` |
|        ! 0 | 1916 | `					 return rc;` |
|          - | 1917 | `			 }` |
|          - | 1918 | `			 /* Link the node to the tree */` |
|      15561 | 1919 | `			 pNode->pLeft = apNode[iLeft];` |
|      15561 | 1920 | `			 apNode[iLeft] = 0;` |
|          - | 1921 | `			 /* Mark as pre-increment/decrement node */` |
|      15561 | 1922 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7778 | 1923 | `		  }` |
|   41867177 | 1924 | `		 iLeft = iCur;` |
|   20933591 | 1925 | `	 }` |
|          - | 1926 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1927 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1928 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1929 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1930 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1931 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1932 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1933 | `	  * pass below skips it (pLeft != 0). */` |
|   16423575 | 1934 | `	 iLeft = -1;` |
|  122207313 | 1935 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105783743 | 1936 | `		 if( apNode[iCur] == 0 ){` |
|   64013957 | 1937 | `			 continue;` |
|          - | 1938 | `		 }` |
|   41769791 | 1939 | `		 pNode = apNode[iCur];` |
|   41769791 | 1940 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      81835 | 1941 | `			 iRight = iCur + 1;` |
|      81835 | 1942 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1943 | `				 iRight++;` |
|        ! 0 | 1944 | `			 }` |
|      81835 | 1945 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1946 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1947 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1948 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1949 | `				 }` |
|        ! 0 | 1950 | `				 return rc;` |
|          - | 1951 | `			 }` |
|      81835 | 1952 | `			 pNode->pLeft = apNode[iLeft];` |
|      81835 | 1953 | `			 pNode->pRight = apNode[iRight];` |
|      81835 | 1954 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      40915 | 1955 | `		 }` |
|   41769791 | 1956 | `		 iLeft = iCur;` |
|   20884898 | 1957 | `	 }` |
|          - | 1958 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   16423575 | 1959 | `	  iLeft = 0;` |
|  122207307 | 1960 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105783739 | 1961 | `		  if( apNode[iCur] ){` |
|   41687957 | 1962 | `			  pNode = apNode[iCur];` |
|   41687957 | 1963 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1376113 | 1964 | `				  if( iLeft > 0 ){` |
|          - | 1965 | `					  /* Link the node to the tree */` |
|    1376111 | 1966 | `					  pNode->pLeft = apNode[iLeft];` |
|    1376111 | 1967 | `					  apNode[iLeft] = 0;` |
|    1376111 | 1968 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      54463 | 1969 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1970 | `							   /* Syntax error */` |
|        ! 0 | 1971 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1972 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 1973 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 1974 | `							  }` |
|        ! 0 | 1975 | `							  return rc;` |
|          - | 1976 | `						  }` |
|      27229 | 1977 | `					  }` |
|     688058 | 1978 | `				  }else{` |
|          - | 1979 | `					  /* Syntax error */` |
|          3 | 1980 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 1981 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 1982 | `						  rc = SXERR_SYNTAX;` |
|          1 | 1983 | `					  }` |
|          3 | 1984 | `					  return rc;` |
|          - | 1985 | `				  }` |
|     688053 | 1986 | `			  }` |
|          - | 1987 | `			  /* Save terminal position */` |
|   41687955 | 1988 | `			  iLeft = iCur;` |
|   20843975 | 1989 | `		  }` |
|   52891871 | 1990 | `	  }` |
|          - | 1991 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 1992 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 1993 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 1994 | `	  * yielding a right-leaning tree. */` |
|  122207305 | 1995 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  105783737 | 1996 | `		 if( apNode[iCur] == 0 ){` |
|   65472007 | 1997 | `			 continue;` |
|          - | 1998 | `		 }` |
|   40311735 | 1999 | `		 pNode = apNode[iCur];` |
|   40311735 | 2000 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 2001 | `			 sxi32 iL, iR;` |
|          - | 2002 | `			 /* Find the right operand */` |
|        115 | 2003 | `			 iR = -1;` |
|          - | 2004 | `			 {` |
|          - | 2005 | `				 sxi32 j;` |
|        127 | 2006 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 2007 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 2008 | `				 }` |
|          - | 2009 | `			 }` |
|          - | 2010 | `			 /* Find the left operand */` |
|        115 | 2011 | `			 iL = -1;` |
|          - | 2012 | `			 {` |
|          - | 2013 | `				 sxi32 j;` |
|        183 | 2014 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 2015 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 2016 | `				 }` |
|          - | 2017 | `			 }` |
|        115 | 2018 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 2019 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2020 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2021 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2022 | `				 }` |
|        ! 0 | 2023 | `				 return rc;` |
|          - | 2024 | `			 }` |
|        115 | 2025 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 2026 | `			 pNode->pRight = apNode[iR];` |
|        115 | 2027 | `			 apNode[iL] = 0;` |
|        115 | 2028 | `			 apNode[iR] = 0;` |
|          - | 2029 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 2030 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 2031 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 2032 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 2033 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 2034 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 2035 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 2036 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 2037 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 2038 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 2039 | `			  * operands are respected. */` |
|        114 | 2040 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 2041 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 2042 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 2043 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 2044 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 2045 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 2046 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 2047 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 2048 | `				 while( pTail->pLeft` |
|         34 | 2049 | `					 && pTail->pLeft->pOp` |
|         23 | 2050 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 2051 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 2052 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 2053 | `					 pTail = pTail->pLeft;` |
|          1 | 2054 | `				 }` |
|          - | 2055 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 2056 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 2057 | `				 pTail->pLeft = pNode;` |
|         27 | 2058 | `				 apNode[iCur] = pHead;` |
|         13 | 2059 | `			 }` |
|         57 | 2060 | `		 }` |
|   20155870 | 2061 | `	 }` |
|          - | 2062 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  180659167 | 2063 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  164235609 | 2064 | `		 iLeft = -1;` |
| 1222072635 | 2065 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 1057837041 | 2066 | `			 if( apNode[iCur] == 0 ){` |
|  723519279 | 2067 | `				 continue;` |
|          - | 2068 | `			 }` |
|  334317767 | 2069 | `			 pNode = apNode[iCur];` |
|  334317767 | 2070 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2071 | `				 /* Get the right node */` |
|    5719709 | 2072 | `				 iRight = iCur + 1;` |
|    8666181 | 2073 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2946477 | 2074 | `					 iRight++;` |
|          5 | 2075 | `				 }` |
|    5719709 | 2076 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2077 | `					 /* Syntax error */` |
|         10 | 2078 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 2079 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2080 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2081 | `					 }` |
|         10 | 2082 | `					 return rc;` |
|          - | 2083 | `				 }` |
|    5719701 | 2084 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2085 | `					 sxi32  iTmp;` |
|          - | 2086 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2087 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2088 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2089 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2090 | `					  * is swapped below. */` |
|         75 | 2091 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2092 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2093 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2094 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2095 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2096 | `						 }` |
|          3 | 2097 | `						 return rc;` |
|          - | 2098 | `					 }` |
|          - | 2099 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|          - | 2100 | `					  * reference target — ExprIsModifiableValue already accepts` |
|          - | 2101 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|          - | 2102 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|          - | 2103 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|          - | 2104 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|         73 | 2105 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2106 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2107 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2108 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2109 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2110 | `						 }` |
|        ! 0 | 2111 | `						 return rc;` |
|          - | 2112 | `					 }` |
|         73 | 2113 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         55 | 2114 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2115 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2116 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2117 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2118 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2119 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2120 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2121 | `									 }` |
|        ! 0 | 2122 | `									 return rc;` |
|          - | 2123 | `							 }` |
|        ! 0 | 2124 | `						 }` |
|         26 | 2125 | `					 }` |
|          - | 2126 | `					 /* Swap operands */` |
|         73 | 2127 | `					 iTmp = iRight;` |
|         73 | 2128 | `					 iRight = iLeft;` |
|         73 | 2129 | `					 iLeft = iTmp;` |
|         35 | 2130 | `				 }` |
|          - | 2131 | `				 /* Link the node to the tree */` |
|    5719699 | 2132 | `				 pNode->pLeft = apNode[iLeft];` |
|    5719699 | 2133 | `				 pNode->pRight = apNode[iRight];` |
|    5719699 | 2134 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2859847 | 2135 | `			 }` |
|  334317757 | 2136 | `			 iLeft = iCur;` |
|  167158881 | 2137 | `		 }` |
|   82117802 | 2138 | `	 }` |
|          - | 2139 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2140 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2141 | `	  * we are dealing with a single operator.` |
|          - | 2142 | `	  */` |
|   16423563 | 2143 | `	  iLeft = -1;` |
|  118047069 | 2144 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  102127565 | 2145 | `		  if( apNode[iCur] == 0 ){` |
|   74763599 | 2146 | `			  continue;` |
|          - | 2147 | `		  }` |
|   27363971 | 2148 | `		  pNode = apNode[iCur];` |
|   27363971 | 2149 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     504059 | 2150 | `			  sxi32 iNest = 1;` |
|     504059 | 2151 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2152 | `				  /* Missing condition */` |
|          3 | 2153 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2154 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2155 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2156 | `				  }` |
|          3 | 2157 | `				  return rc;` |
|          - | 2158 | `			  }` |
|          - | 2159 | `			  /* Get the right node */` |
|     504057 | 2160 | `			  iRight = iCur + 1;` |
|    2142069 | 2161 | `			  while( iRight < nToken  ){` |
|    2142069 | 2162 | `				  if( apNode[iRight] ){` |
|    1004157 | 2163 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2164 | `						  /* Increment nesting level */` |
|        ! 0 | 2165 | `						  ++iNest;` |
|    1004157 | 2166 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2167 | `						  /* Decrement nesting level */` |
|     504057 | 2168 | `						  --iNest;` |
|     504057 | 2169 | `						  if( iNest <= 0 ){` |
|     504057 | 2170 | `							  break;` |
|          - | 2171 | `						  }` |
|        ! 0 | 2172 | `					  }` |
|     250050 | 2173 | `				  }` |
|    1638017 | 2174 | `				  iRight++;` |
|          5 | 2175 | `			  }` |
|     504057 | 2176 | `			  if( iRight > iCur + 1 ){` |
|          - | 2177 | `				  /* Recurse and process the then expression */` |
|     500105 | 2178 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     500105 | 2179 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2180 | `					  return rc;` |
|          - | 2181 | `				  }` |
|          - | 2182 | `				  /* Link the node to the tree */` |
|     500105 | 2183 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     250050 | 2184 | `			  }else{` |
|          - | 2185 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2186 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2187 | `			  }` |
|     504057 | 2188 | `			  apNode[iCur + 1] = 0;` |
|     504057 | 2189 | `			  if( iRight + 1 < nToken ){` |
|          - | 2190 | `				  /* Recurse and process the else expression */` |
|     504057 | 2191 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     504057 | 2192 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2193 | `					  return rc;` |
|          - | 2194 | `				  }` |
|          - | 2195 | `				  /* Link the node to the tree */` |
|     504057 | 2196 | `				  pNode->pRight = apNode[iRight + 1];` |
|     504057 | 2197 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     252031 | 2198 | `			  }else{` |
|        ! 0 | 2199 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2200 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2201 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2202 | `				 }` |
|        ! 0 | 2203 | `				 return rc;` |
|          - | 2204 | `			  }` |
|          - | 2205 | `			  /* Point to the condition */` |
|     504057 | 2206 | `			  pNode->pCond  = apNode[iLeft];` |
|     504057 | 2207 | `			  apNode[iLeft] = 0;` |
|     504057 | 2208 | `			  break;` |
|          - | 2209 | `		  }` |
|   26859917 | 2210 | `		  iLeft = iCur;` |
|   13429961 | 2211 | `	  }` |
|          - | 2212 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2213 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2214 | `	  * so there is no need for a precedence loop here.` |
|          - | 2215 | `	  */` |
|   16423561 | 2216 | `	 iRight = -1;` |
|  122207115 | 2217 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  105783611 | 2218 | `		 if( apNode[iCur] == 0 ){` |
|   84141843 | 2219 | `			 continue;` |
|          - | 2220 | `		 }` |
|   21641773 | 2221 | `		 pNode = apNode[iCur];` |
|   21641773 | 2222 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2223 | `			 /* Get the left node */` |
|    5218147 | 2224 | `			 iLeft = iCur - 1;` |
|    7165263 | 2225 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1947121 | 2226 | `				 iLeft--;` |
|          5 | 2227 | `			 }` |
|    5218147 | 2228 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2229 | `				 /* Syntax error */` |
|         46 | 2230 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2231 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2232 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2233 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2234 | `				 }else{` |
|         41 | 2235 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2236 | `				 }` |
|         46 | 2237 | `				 if( rc != SXERR_ABORT ){` |
|         44 | 2238 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2239 | `				 }` |
|         46 | 2240 | `				 return rc;` |
|          - | 2241 | `			 }` |
|          - | 2242 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2243 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2244 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2245 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2246 | `			  * a write. */` |
|    5218105 | 2247 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2248 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2249 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2250 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2251 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2252 | `				 }` |
|         11 | 2253 | `				 return rc;` |
|          - | 2254 | `			 }` |
|          - | 2255 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2256 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2257 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2258 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2259 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    5218097 | 2260 | `			 pSuppress = 0;` |
|    5218092 | 2261 | `			 if( apNode[iLeft]->pOp` |
|    3357183 | 2262 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     748137 | 2263 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2264 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2265 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2266 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2267 | `			 }` |
|    5218097 | 2268 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2269 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2270 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2271 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2272 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2273 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2274 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        109 | 2275 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2276 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2277 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2278 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2279 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2280 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2281 | `					 }` |
|          8 | 2282 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2283 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2284 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2285 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2286 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2287 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2288 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2289 | `						 iRight = iCur;` |
|          9 | 2290 | `						 continue;` |
|          - | 2291 | `					 }` |
|        ! 0 | 2292 | `				 }` |
|        132 | 2293 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         94 | 2294 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2295 | `					 /* Left operand must be a modifiable l-value */` |
|          3 | 2296 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2297 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2298 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2299 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2300 | `					 }else{` |
|        ! 0 | 2301 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 2302 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2303 | `					 }` |
|          3 | 2304 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 2305 | `						 rc = SXERR_SYNTAX;` |
|          1 | 2306 | `					 }` |
|          3 | 2307 | `					 return rc;` |
|          - | 2308 | `				 }` |
|         47 | 2309 | `			 }` |
|          - | 2310 | `			 /* Link the node to the tree (Reverse) */` |
|    5218087 | 2311 | `			 pNode->pLeft = apNode[iRight];` |
|    5218087 | 2312 | `			 pNode->pRight = apNode[iLeft];` |
|    5218087 | 2313 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    5218087 | 2314 | `			 if( pSuppress ){` |
|          - | 2315 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2316 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2317 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2318 | `			 }` |
|    2609041 | 2319 | `		 }` |
|   21641713 | 2320 | `		 iRight = iCur;` |
|   10820859 | 2321 | `	 }` |
|          - | 2322 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   82117525 | 2323 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   65694021 | 2324 | `		 iLeft = -1;` |
|  488828189 | 2325 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  423134173 | 2326 | `			 if( apNode[iCur] == 0 ){` |
|  357439917 | 2327 | `				 continue;` |
|          - | 2328 | `			 }` |
|   65694261 | 2329 | `			 pNode = apNode[iCur];` |
|   65694261 | 2330 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2331 | `				 /* Get the right node */` |
|         48 | 2332 | `				 iRight = iCur + 1;` |
|         60 | 2333 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2334 | `					 iRight++;` |
|          1 | 2335 | `				 }` |
|         48 | 2336 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2337 | `					 /* Syntax error */` |
|        ! 0 | 2338 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2339 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2340 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2341 | `					 }` |
|        ! 0 | 2342 | `					 return rc;` |
|          - | 2343 | `				 }` |
|          - | 2344 | `				 /* Link the node to the tree */` |
|         48 | 2345 | `				 pNode->pLeft = apNode[iLeft];` |
|         48 | 2346 | `				 pNode->pRight = apNode[iRight];` |
|         48 | 2347 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         22 | 2348 | `			 }` |
|   65694261 | 2349 | `			 iLeft = iCur;` |
|   32847133 | 2350 | `		 }` |
|   32847013 | 2351 | `	 }` |
|          - | 2352 | `	 /* Point to the root of the expression tree */` |
|  105783519 | 2353 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   89360033 | 2354 | `		 if( apNode[iCur] ){` |
|   15622355 | 2355 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         22 | 2356 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|         22 | 2357 | `				  if( rc != SXERR_ABORT ){` |
|         22 | 2358 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2359 | `				  }` |
|         22 | 2360 | `				  return rc;` |
|          - | 2361 | `			 }` |
|   15622337 | 2362 | `			 apNode[0] = apNode[iCur];` |
|   15622337 | 2363 | `			 apNode[iCur] = 0;` |
|    7811166 | 2364 | `		 }` |
|   44680010 | 2365 | `	 }` |
|   16423491 | 2366 | `	 return SXRET_OK;` |
|   14710477 | 2367 | ` }` |
|          - | 2368 | ` /*` |
|          - | 2369 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2370 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2371 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2372 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2373 | `  */` |
|   17112058 | 2374 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2375 | `{` |
|          - | 2376 | `	ph7_expr_node **apNode;` |
|          - | 2377 | `	ph7_expr_node *pNode;` |
|          - | 2378 | `	sxi32 rc;` |
|          - | 2379 | `	/* Reset node container */` |
|   17112063 | 2380 | `	SySetReset(pExprNode);` |
|   17112063 | 2381 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2382 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2383 | `	{` |
|   17112063 | 2384 | `		int iLastWasTerm = 0;` |
|   17112063 | 2385 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  107263835 | 2386 | `		while( pGen->pIn < pGen->pEnd ){` |
|   90151817 | 2387 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   90151817 | 2388 | `			if( rc != SXRET_OK ){` |
|         44 | 2389 | `				return rc;` |
|          - | 2390 | `			}` |
|          - | 2391 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   90151777 | 2392 | `			if( pNode->xCode ){` |
|          - | 2393 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   45940391 | 2394 | `				iLastWasTerm = 1;` |
|   67181584 | 2395 | `			}else if( pNode->pOp ){` |
|          - | 2396 | `				/* Operator node */` |
|   25565273 | 2397 | `				iLastWasTerm = 0;` |
|   12782639 | 2398 | `			}else{` |
|          - | 2399 | `				/* Delimiter: ')' and ']' end terms */` |
|   18646123 | 2400 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2401 | `			}` |
|          - | 2402 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2403 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2404 | `			 * node kind, so this single test covers all branches. */` |
|   90151777 | 2405 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2406 | `			/* Save the extracted node */` |
|   90151777 | 2407 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2408 | `		}` |
|          - | 2409 | `	}` |
|   17112023 | 2410 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2411 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2412 | `		*ppRoot = 0;` |
|        ! 0 | 2413 | `		return SXRET_OK;` |
|          - | 2414 | `	}` |
|   17112023 | 2415 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2416 | `	/* Make sure we are dealing with valid nodes */` |
|   17112023 | 2417 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17112023 | 2418 | `	if( rc != SXRET_OK ){` |
|          - | 2419 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2420 | `		 * cleanup the mess left behind.` |
|          - | 2421 | `		 */` |
|         52 | 2422 | `		*ppRoot = 0;` |
|         52 | 2423 | `		return rc;` |
|          - | 2424 | `	}` |
|          - | 2425 | `	/* Build the tree */` |
|   17111975 | 2426 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17111975 | 2427 | `	if( rc != SXRET_OK ){` |
|          - | 2428 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        101 | 2429 | `		*ppRoot = 0;` |
|        101 | 2430 | `		return rc;` |
|          - | 2431 | `	}` |
|          - | 2432 | `	/* Point to the root of the tree */` |
|   17111879 | 2433 | `	*ppRoot = apNode[0];` |
|   17111879 | 2434 | `	return SXRET_OK;` |
|    8556034 | 2435 | `}` |
|          - | 2436 |  |
