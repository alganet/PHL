# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1179/1358 lines (86.82%)

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
|   25734214 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   25734219 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  407282428 |  278 | `	for(;;){` |
|  814564861 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  814564861 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   77373001 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   38686503 |  285 | `		}else{` |
|  737191865 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  814564861 |  288 | `		if( rc == 0 ){` |
|   26056381 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   25634167 |  291 | `				return &aOpTable[n];` |
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
|  788830647 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   12867112 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    6990860 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    6990865 |  320 | `	SyToken *pCur = pIn;` |
|    6990865 |  321 | `	sxi32 iNest = 1;` |
|   73509524 |  322 | `	for(;;){` |
|  147019053 |  323 | `		if( pCur >= pEnd ){` |
|      15819 |  324 | `			break;` |
|          - |  325 | `		}` |
|  147003239 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    5706095 |  328 | `			iNest++;` |
|  144150194 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   12681141 |  331 | `			iNest--;` |
|   12681141 |  332 | `			if( iNest <= 0 ){` |
|    6975051 |  333 | `				break;` |
|          - |  334 | `			}` |
|    2853045 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  140028193 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    6990865 |  340 | `	*ppEnd = pCur;` |
|    6990865 |  341 | `}` |
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
|   15238438 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   15238443 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7685 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7685 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3840 |  383 | `	}` |
|   15238443 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|   95148183 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   79909781 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     216461 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   79693325 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    6621697 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     376042 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    6179717 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    6179717 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    6179717 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    6179717 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3089856 |  401 | `					}` |
|    3089856 |  402 | `			}` |
|    6621697 |  403 | `			iParen++;` |
|   76382479 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    6621697 |  405 | `			if( iParen <= 0 ){` |
|         15 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         15 |  407 | `				if( rc != SXERR_ABORT ){` |
|         15 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         15 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    6621685 |  412 | `			iParen--;` |
|   69760781 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    2875257 |  414 | `			iSquare++;` |
|   65012315 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    2875261 |  416 | `			if( iSquare <= 0 ){` |
|          8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          8 |  418 | `				if( rc != SXERR_ABORT ){` |
|          8 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          8 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    2875255 |  423 | `			iSquare--;` |
|   62137058 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|   60697515 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3849 |  437 | `			if( iBraces <= 0 ){` |
|         15 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         15 |  439 | `				if( rc != SXERR_ABORT ){` |
|         15 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         15 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3837 |  444 | `			iBraces--;` |
|   60693671 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     458723 |  446 | `			if( iQuesty > 0 ){` |
|     458433 |  447 | `				iQuesty--;` |
|     229509 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   60462394 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   19738541 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   19738541 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     458435 |  461 | `				iQuesty++;` |
|   19509326 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|    9869268 |  482 | `		}` |
|   39846647 |  483 | `	}` |
|   15238407 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         19 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         19 |  486 | `		if( rc != SXERR_ABORT ){` |
|         19 |  487 | `			rc = SXERR_SYNTAX;` |
|          8 |  488 | `		}` |
|         19 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   15238391 |  491 | `	return SXRET_OK;` |
|    7619224 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   11955956 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   11955961 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   11955961 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   11952067 |  502 | `		pIn++;` |
|    5976031 |  503 | `	}` |
|    5979963 |  504 | `	for(;;){` |
|   11959931 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       3975 |  506 | `			pIn++;` |
|       3975 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3973 |  508 | `				pIn++;` |
|       1984 |  509 | `			}` |
|       1990 |  510 | `		}else{` |
|    5977983 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   11955961 |  515 | `	*ppCur = pIn;` |
|   11955961 |  516 | `}` |
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
|   79914018 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  868 | `{` |
|          - |  869 | `	ph7_expr_node *pNode;` |
|          - |  870 | `	SyToken *pCur;` |
|          - |  871 | `	sxi32 rc;` |
|          - |  872 | `	/* Allocate a new node */` |
|   79914023 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   79914023 |  874 | `	if( pNode == 0 ){` |
|          - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  877 | `		 */` |
|        ! 0 |  878 | `		return SXERR_MEM;` |
|          - |  879 | `	}` |
|          - |  880 | `	/* Zero the structure */` |
|   79914023 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   79914023 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  883 | `	/* Point to the head of the token stream */` |
|   79914023 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  885 | `	/* Start collecting tokens */` |
|   79914023 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
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
|   79909855 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
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
|   79801626 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|          - |  942 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|          - |  943 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|          - |  944 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|          - |  945 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|          - |  946 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|          - |  947 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|          - |  948 | `		 * call, not the clone() intrinsic. */` |
|         19 |  949 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         19 |  950 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|         19 |  951 | `		pNode->xCode = PH7_CompileLiteral;` |
|   79693384 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   22613844 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   11335638 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|   79693368 |  980 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - |  981 | `		/* Point to the instance that describe this operator */` |
|   22613827 |  982 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - |  983 | `		/* Advance the stream cursor */` |
|   22613827 |  984 | `		pCur++;` |
|   68386446 |  985 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - |  986 | `		/* Isolate variable */` |
|   38669897 |  987 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   19334957 |  988 | `			pCur++; /* Variable variable */` |
|          5 |  989 | `		}` |
|   19334945 |  990 | `		if( pCur < pGen->pEnd ){` |
|   19334945 |  991 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - |  992 | `				/* Variable name */` |
|   19334917 |  993 | `				pCur++;` |
|    9667489 |  994 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|    9667468 | 1009 | `		}` |
|   19334941 | 1010 | `		pNode->xCode = PH7_CompileVariable;` |
|   47412063 | 1011 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|     960565 | 1012 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     960565 | 1013 | `		 if( bAfterMemberOp ){` |
|          - | 1014 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1015 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1016 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1017 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1018 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1019 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1020 | `			  * the word itself. */` |
|     126423 | 1021 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     126423 | 1022 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     126423 | 1023 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     897356 | 1024 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1025 | `			 /* List/Array node */` |
|     399593 | 1026 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1027 | `				 /* Assume a literal */` |
|        ! 0 | 1028 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1029 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1030 | `			 }else{` |
|     399593 | 1031 | `				 pCur += 2;` |
|          - | 1032 | `				 /* Collect array/list tokens */` |
|     399593 | 1033 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     399593 | 1034 | `				 if( pCur < pGen->pEnd ){` |
|     399591 | 1035 | `					 pCur++;` |
|     199798 | 1036 | `				 }else{` |
|          - | 1037 | `					 /* Syntax error */` |
|          4 | 1038 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          1 | 1039 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|          3 | 1040 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1041 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1042 | `					 }` |
|          3 | 1043 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1044 | `					 return rc;` |
|          - | 1045 | `				 }` |
|     399591 | 1046 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     399591 | 1047 | `				 if( pNode->xCode == PH7_CompileList ){` |
|         39 | 1048 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|         39 | 1049 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|          - | 1050 | `						 /* Syntax error */` |
|          3 | 1051 | `						 rc = PH7_GenSyntaxError(pGen,pNode->pStart,"\"=\"");` |
|          3 | 1052 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1053 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1054 | `						 }` |
|          3 | 1055 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1056 | `						 return rc;` |
|          - | 1057 | `					 }` |
|         16 | 1058 | `				 }` |
|          5 | 1059 | `			 }` |
|     634351 | 1060 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1061 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15687 | 1062 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15687 | 1063 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1064 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1065 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15687 | 1066 | `			 pNode->xCode = PH7_CompileYield;` |
|     426718 | 1067 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     418590 | 1068 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|         58 | 1069 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|         37 | 1070 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1071 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|        607 | 1072 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1073 | `				 /* Assume a literal */` |
|        ! 0 | 1074 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1075 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1076 | `			 }else{` |
|          - | 1077 | `				 /* Assemble annonymous functions body */` |
|        607 | 1078 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|        607 | 1079 | `				 if( rc != SXRET_OK ){` |
|         28 | 1080 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1081 | `					 return rc;` |
|          - | 1082 | `				 }` |
|        583 | 1083 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1084 | `			  }` |
|     418564 | 1085 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         41 | 1086 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         23 | 1087 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         12 | 1088 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|          9 | 1089 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1090 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1091 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1092 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1093 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         32 | 1094 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         32 | 1095 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1096 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1097 | `				 return rc;` |
|          - | 1098 | `			 }` |
|         32 | 1099 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     418260 | 1100 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     418011 | 1101 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|         48 | 1102 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|         27 | 1103 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1104 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        493 | 1105 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        493 | 1106 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1107 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1108 | `				 return rc;` |
|          - | 1109 | `			 }` |
|        493 | 1110 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     418003 | 1111 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1112 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         77 | 1113 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         77 | 1114 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1115 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1116 | `				 return rc;` |
|          - | 1117 | `			 }` |
|         77 | 1118 | `			 pNode->xCode = PH7_CompileMatch;` |
|     417723 | 1119 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1120 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1121 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1122 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1123 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1124 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1125 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1126 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1127 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     417669 | 1128 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1129 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         79 | 1130 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         79 | 1131 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         42 | 1132 | `		 }else{` |
|          - | 1133 | `			 /* Assume a literal */` |
|     417577 | 1134 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     417577 | 1135 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1136 | `		 }` |
|   37264301 | 1137 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1138 | `		 /* Constants,function name,namespace path,class name... */` |
|   11411953 | 1139 | `		 if( bAfterMemberOp ){` |
|          - | 1140 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1141 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1142 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1143 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    4592027 | 1144 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    2296011 | 1145 | `		 }` |
|   11411953 | 1146 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   11411953 | 1147 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    5705979 | 1148 | `	 }else{` |
|   25372087 | 1149 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1150 | `			 /* Point to the code generator routine */` |
|    8787027 | 1151 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    8787027 | 1152 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1153 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1154 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1155 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1156 | `				 }` |
|          3 | 1157 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1158 | `				 return rc;` |
|          - | 1159 | `			 }` |
|    4393510 | 1160 | `		 }` |
|          - | 1161 | `		/* Advance the stream cursor */` |
|   25372085 | 1162 | `		pCur++;` |
|          - | 1163 | `	 }` |
|          - | 1164 | `	/* Point to the end of the token stream */` |
|   79909821 | 1165 | `	pNode->pEnd = pCur;` |
|          - | 1166 | `	/* Save the node for later processing */` |
|   79909821 | 1167 | `	*ppNode = pNode;` |
|          - | 1168 | `	/* Synchronize cursors */` |
|   79909821 | 1169 | `	pGen->pIn = pCur;` |
|   79909821 | 1170 | `	return SXRET_OK;` |
|   39957014 | 1171 | `}` |
|          - | 1172 | `/*` |
|          - | 1173 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1174 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1175 | ` * level is zero.` |
|          - | 1176 | ` */` |
|    1788338 | 1177 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1178 | `{` |
|    1788343 | 1179 | `	SyToken *pCur = pStart;` |
|    1788343 | 1180 | `	sxi32 iNest = 0;` |
|    1788343 | 1181 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1182 | `		/* Last expression */` |
|     660105 | 1183 | `		return SXERR_EOF;` |
|          - | 1184 | `	}` |
|    4274191 | 1185 | `	while( pCur < pEnd ){` |
|    3999833 | 1186 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     853885 | 1187 | `			break;` |
|          - | 1188 | `		}` |
|    3145953 | 1189 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     214739 | 1190 | `			iNest++;` |
|    3038586 | 1191 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     214741 | 1192 | `			iNest--;` |
|     107368 | 1193 | `		}` |
|    3145953 | 1194 | `		pCur++;` |
|          5 | 1195 | `	}` |
|    1128243 | 1196 | `	*ppNext = pCur;` |
|    1128243 | 1197 | `	return SXRET_OK;` |
|     894174 | 1198 | `}` |
|          - | 1199 | `/*` |
|          - | 1200 | ` * Free an expression tree.` |
|          - | 1201 | ` */` |
|   68352462 | 1202 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1203 | `{` |
|   68352467 | 1204 | `	if( pNode->pLeft ){` |
|          - | 1205 | `		/* Release the left tree */` |
|   26745431 | 1206 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   13372713 | 1207 | `	}` |
|   68352467 | 1208 | `	if( pNode->pRight ){` |
|          - | 1209 | `		/* Release the right tree */` |
|   15551221 | 1210 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    7775608 | 1211 | `	}` |
|   68352467 | 1212 | `	if( pNode->pCond ){` |
|          - | 1213 | `		/* Release the conditional tree used by the ternary operator */` |
|     458431 | 1214 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     229213 | 1215 | `	}` |
|   68352467 | 1216 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1217 | `		ph7_expr_node **apArg;` |
|          - | 1218 | `		sxu32 n;` |
|          - | 1219 | `		/* Release node arguments */` |
|    7422399 | 1220 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   16888813 | 1221 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|    9466419 | 1222 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    4733212 | 1223 | `		}` |
|    7422399 | 1224 | `		SySetRelease(&pNode->aNodeArgs);` |
|    3711197 | 1225 | `	}` |
|          - | 1226 | `	/* Finally,release this node */` |
|   68352467 | 1227 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   68352467 | 1228 | `}` |
|          - | 1229 | `/*` |
|          - | 1230 | ` * Free an expression tree.` |
|          - | 1231 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1232 | ` */` |
|   15238468 | 1233 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1234 | `{` |
|          - | 1235 | `	ph7_expr_node **apNode;` |
|          - | 1236 | `	sxu32 n;` |
|   15238473 | 1237 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|   95148339 | 1238 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   79909871 | 1239 | `		if( apNode[n] ){` |
|   15238813 | 1240 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    7619404 | 1241 | `		}` |
|   39954938 | 1242 | `	}` |
|   15238473 | 1243 | `	return SXRET_OK;` |
|          5 | 1244 | `}` |
|          - | 1245 | `/*` |
|          - | 1246 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1247 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1248 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1249 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1250 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1251 | ` */` |
|   19453996 | 1252 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1253 | `{` |
|   19454001 | 1254 | `	if( pNode == 0 ){` |
|   12146217 | 1255 | `		return 0;` |
|          - | 1256 | `	}` |
|    7307789 | 1257 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1258 | `		return 1;` |
|          - | 1259 | `	}` |
|    7307777 | 1260 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1261 | `		return 1;` |
|          - | 1262 | `	}` |
|    7307773 | 1263 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1264 | `		return 1;` |
|          - | 1265 | `	}` |
|    7307773 | 1266 | `	return 0;` |
|    9727003 | 1267 | `}` |
|          - | 1268 | `/*` |
|          - | 1269 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1270 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1271 | ` */` |
|    4815428 | 1272 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1273 | `{` |
|          - | 1274 | `	sxi32 iExprOp;` |
|    4815433 | 1275 | `	if( pNode->pOp == 0 ){` |
|    3482985 | 1276 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1277 | `	}` |
|    1332453 | 1278 | `	iExprOp = pNode->pOp->iOp;` |
|    1332453 | 1279 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     819123 | 1280 | `			return TRUE;` |
|          - | 1281 | `	}` |
|     513335 | 1282 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     513321 | 1283 | `		if( pNode->pLeft->pOp ) {` |
|     118626 | 1284 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      49748 | 1285 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1286 | `				return FALSE;` |
|          5 | 1287 | `			}` |
|     454008 | 1288 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1289 | `			return FALSE;` |
|          - | 1290 | `		}` |
|     513321 | 1291 | `		return TRUE;` |
|          - | 1292 | `	}` |
|         16 | 1293 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1294 | `		return TRUE;` |
|          - | 1295 | `	}` |
|          - | 1296 | `	/* Not a modifiable l or r-value */` |
|          9 | 1297 | `	return FALSE;` |
|    2407719 | 1298 | `}` |
|          - | 1299 | `/* Forward declaration */` |
|          - | 1300 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|          - | 1301 | `/* Macro to check if the given node is a terminal.` |
|          - | 1302 | ` * A node is a term if it has no operator, or has already been linked into an` |
|          - | 1303 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|          - | 1304 | ` * linked ternary/elvis node). */` |
|          - | 1305 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|          - | 1306 | `/*` |
|          - | 1307 | ` * Buid an expression tree for each given function argument.` |
|          - | 1308 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1309 | ` */` |
|    4769338 | 1310 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1311 | `{` |
|          - | 1312 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1313 | `	sxi32 rc;` |
|          - | 1314 | `	/* Process function arguments from left to right */` |
|    4769343 | 1315 | `	iCur = 0;` |
|    5791336 | 1316 | `	for(;;){` |
|   11582677 | 1317 | `		if( iCur >= nToken ){` |
|          - | 1318 | `			/* No more arguments to process */` |
|    4769317 | 1319 | `			break;` |
|          - | 1320 | `		}` |
|    6813365 | 1321 | `		iNode = iCur;` |
|    6813365 | 1322 | `		iNest = 0;` |
|   21910849 | 1323 | `		while( iCur < nToken ){` |
|   17141535 | 1324 | `			if( apNode[iCur] ){` |
|   17095469 | 1325 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1022028 | 1326 | `					break;` |
|   15051418 | 1327 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    8107250 | 1328 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1160573 | 1329 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1330 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1331 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1332 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1333 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1334 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1335 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    1158059 | 1336 | `					iNest++;` |
|   14472396 | 1337 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    1158059 | 1338 | `					iNest--;` |
|     579027 | 1339 | `				}` |
|    7525709 | 1340 | `			}` |
|   15097489 | 1341 | `			iCur++;` |
|          5 | 1342 | `		}` |
|    6813365 | 1343 | `		if( iCur > iNode ){` |
|    6813359 | 1344 | `			SyString sArgName = {0, 0};` |
|          - | 1345 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1346 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1347 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    6813354 | 1348 | `			if( (iCur - iNode) >= 2` |
|    4594465 | 1349 | `				&& apNode[iNode]` |
|    2375562 | 1350 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1277119 | 1351 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     178421 | 1352 | `				&& apNode[iNode+1]` |
|     178157 | 1353 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1354 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        291 | 1355 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        291 | 1356 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        291 | 1357 | `				apNode[iNode] = 0;` |
|        291 | 1358 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        291 | 1359 | `				apNode[iNode+1] = 0;` |
|        291 | 1360 | `				iNode += 2;` |
|          - | 1361 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1362 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        291 | 1363 | `				if( iNode >= iCur ){` |
|          4 | 1364 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1365 | `						pOp->pStart->nLine,` |
|          - | 1366 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1367 | `						&sArgName);` |
|          3 | 1368 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1369 | `						rc = SXERR_SYNTAX;` |
|          1 | 1370 | `					}` |
|          3 | 1371 | `					return rc;` |
|          - | 1372 | `				}` |
|        142 | 1373 | `			}` |
|    6813352 | 1374 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1375 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1376 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1377 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1378 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1379 | `					apNode[iNode] = 0;` |
|        ! 0 | 1380 | `			}` |
|          - | 1381 | `			{` |
|          - | 1382 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1383 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1384 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1385 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1386 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1387 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1388 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1389 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1390 | `				 * NULL slots ahead of the flagged subtree. */` |
|    6813357 | 1391 | `				int bSpreadArg = 0;` |
|          - | 1392 | `				sxi32 iScan;` |
|    6813385 | 1393 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    6813385 | 1394 | `					if( apNode[iScan] ){` |
|    6813357 | 1395 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    6813357 | 1396 | `						break;` |
|          - | 1397 | `					}` |
|         15 | 1398 | `				}` |
|    6813357 | 1399 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    6813357 | 1400 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4023 | 1401 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2009 | 1402 | `				}` |
|          - | 1403 | `			}` |
|    6813357 | 1404 | `			if( apNode[iNode] ){` |
|    6813357 | 1405 | `				if( sArgName.nByte > 0 ){` |
|        289 | 1406 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        289 | 1407 | `					apNode[iNode]->sArgName = sArgName;` |
|        142 | 1408 | `				}` |
|          - | 1409 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    6813357 | 1410 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    3406681 | 1411 | `			}else{` |
|          - | 1412 | `				/* No expression before comma */` |
|        ! 0 | 1413 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1414 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1415 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1416 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1417 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1418 | `				}` |
|        ! 0 | 1419 | `				return rc;` |
|          - | 1420 | `			}` |
|    3406681 | 1421 | `		}else{` |
|          - | 1422 | `			/* Comma with no preceding argument */` |
|          8 | 1423 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1424 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1425 | `				rc = SXERR_SYNTAX;` |
|          3 | 1426 | `			}` |
|          8 | 1427 | `			return rc;` |
|          - | 1428 | `		}` |
|          - | 1429 | `		/* Jump trailing comma */` |
|    6813357 | 1430 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2044045 | 1431 | `			iCur++;` |
|    2044045 | 1432 | `			if( iCur >= nToken ){` |
|          - | 1433 | `				/* Trailing comma after last argument */` |
|         19 | 1434 | `				break;` |
|          - | 1435 | `			}` |
|    1022011 | 1436 | `		}` |
|          5 | 1437 | `	}` |
|    4769335 | 1438 | `	return SXRET_OK;` |
|    2384674 | 1439 | `}` |
|          - | 1440 | ` /*` |
|          - | 1441 | `  * Create an expression tree from an array of tokens.` |
|          - | 1442 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1443 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1444 | `  */` |
|   26228392 | 1445 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1446 | ` {` |
|          - | 1447 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1448 | `	 ph7_expr_node *pNode;` |
|          - | 1449 | `	 ph7_expr_node *pSuppress;` |
|          - | 1450 | `	 sxi32 iCur;` |
|          - | 1451 | `	 sxi32 rc;` |
|   26228397 | 1452 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1453 | `		 /* TICKET 1433-17: self evaluating node */` |
|   11763331 | 1454 | `		 return SXRET_OK;` |
|          - | 1455 | `	 }` |
|          - | 1456 | `	 /* Process expressions enclosed in parenthesis first */` |
|  103303927 | 1457 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1458 | `		 sxi32 iNest;` |
|          - | 1459 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1460 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1461 | `		  */` |
|   88838863 | 1462 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|   88396893 | 1463 | `			 continue;` |
|          - | 1464 | `		 }` |
|     441975 | 1465 | `		 iNest = 1;` |
|     441975 | 1466 | `		 iLeft = iCur;` |
|          - | 1467 | `		 /* Find the closing parenthesis */` |
|     441975 | 1468 | `		 iCur++;` |
|    3847621 | 1469 | `		 while( iCur < nToken ){` |
|    3847621 | 1470 | `			 if( apNode[iCur] ){` |
|    3847621 | 1471 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1472 | `					 /* Decrement nesting level */` |
|     642087 | 1473 | `					 iNest--;` |
|     642087 | 1474 | `					 if( iNest <= 0 ){` |
|     441975 | 1475 | `						 break;` |
|          5 | 1476 | `					 }` |
|    3305595 | 1477 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1478 | `					 /* Increment nesting level */` |
|     200117 | 1479 | `					 iNest++;` |
|     100056 | 1480 | `				 }` |
|    1702823 | 1481 | `			 }` |
|    3405651 | 1482 | `			 iCur++;` |
|          5 | 1483 | `		 }` |
|     441975 | 1484 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1485 | `			 sxi32 j;` |
|          - | 1486 | `			 /* Recurse and process this expression */` |
|     441975 | 1487 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     441975 | 1488 | `			 if( rc != SXRET_OK ){` |
|          3 | 1489 | `				 return rc;` |
|          - | 1490 | `			 }` |
|          - | 1491 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1492 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1493 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1494 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1495 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1496 | `			  * group's free below silently drops the unpacking. */` |
|     441973 | 1497 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     441973 | 1498 | `				 if( apNode[j] ){` |
|     441973 | 1499 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     441968 | 1500 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     441973 | 1501 | `					 break;` |
|          - | 1502 | `				 }` |
|        ! 0 | 1503 | `			 }` |
|     220984 | 1504 | `		 }` |
|          - | 1505 | `		 /* Free the left and right nodes */` |
|     441973 | 1506 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     441973 | 1507 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     441973 | 1508 | `		 apNode[iLeft] = 0;` |
|     441973 | 1509 | `		 apNode[iCur] = 0;` |
|     220989 | 1510 | `	 }` |
|          - | 1511 | `	  /* Process expressions enclosed in braces */` |
|  106814205 | 1512 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1513 | `		 sxi32 iNest;` |
|          - | 1514 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1515 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1516 | `		  */` |
|   92678805 | 1517 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|   92674973 | 1518 | `			 continue;` |
|          - | 1519 | `		 }` |
|       3837 | 1520 | `		 iNest = 1;` |
|       3837 | 1521 | `		 iLeft = iCur;` |
|          - | 1522 | `		 /* Find the closing parenthesis */` |
|       3837 | 1523 | `		 iCur++;` |
|       7667 | 1524 | `		 while( iCur < nToken ){` |
|       7667 | 1525 | `			 if( apNode[iCur] ){` |
|       7667 | 1526 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1527 | `					 /* Decrement nesting level */` |
|       3837 | 1528 | `					 iNest--;` |
|       3837 | 1529 | `					 if( iNest <= 0 ){` |
|       3837 | 1530 | `						 break;` |
|        ! 0 | 1531 | `					 }` |
|       3835 | 1532 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1533 | `					 /* Increment nesting level */` |
|        ! 0 | 1534 | `					 iNest++;` |
|        ! 0 | 1535 | `				 }` |
|       1915 | 1536 | `			 }` |
|       3835 | 1537 | `			 iCur++;` |
|          5 | 1538 | `		 }` |
|       3837 | 1539 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1540 | `			 /* Recurse and process this expression */` |
|       3835 | 1541 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3835 | 1542 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1543 | `				 return rc;` |
|          - | 1544 | `			 }` |
|       1915 | 1545 | `		 }` |
|          - | 1546 | `		 /* Free the left and right nodes */` |
|       3837 | 1547 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3837 | 1548 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3837 | 1549 | `		 apNode[iLeft] = 0;` |
|       3837 | 1550 | `		 apNode[iCur] = 0;` |
|       1921 | 1551 | `	 }` |
|          - | 1552 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   14135405 | 1553 | `	 iLeft = -1;` |
|  106821831 | 1554 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92686443 | 1555 | `		 if( apNode[iCur] == 0 ){` |
|   40124357 | 1556 | `			 continue;` |
|          - | 1557 | `		 }` |
|   52562091 | 1558 | `		 pNode = apNode[iCur];` |
|   52562091 | 1559 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   14006791 | 1560 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1561 | `				 /* Collect function arguments */` |
|    6179713 | 1562 | `				 sxi32 iPtr = 0;` |
|    6179713 | 1563 | `				 sxi32 nFuncTok = 0;` |
|   29500953 | 1564 | `				 while( nFuncTok + iCur < nToken ){` |
|   29500953 | 1565 | `					 if( apNode[nFuncTok+iCur] ){` |
|   29454887 | 1566 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    6545633 | 1567 | `							 iPtr++;` |
|   26182073 | 1568 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    6545633 | 1569 | `							 iPtr--;` |
|    6545633 | 1570 | `							 if( iPtr <= 0 ){` |
|    6179713 | 1571 | `								 break;` |
|          - | 1572 | `							 }` |
|     182960 | 1573 | `						 }` |
|   11637587 | 1574 | `					 }` |
|   23321245 | 1575 | `					 nFuncTok++;` |
|          5 | 1576 | `				 }` |
|    6179713 | 1577 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1578 | `					 /* Syntax error */` |
|        ! 0 | 1579 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1580 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1581 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1582 | `					 }` |
|        ! 0 | 1583 | `					 return rc;` |
|          - | 1584 | `				 }` |
|    6179713 | 1585 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1586 | `					 /* Syntax error */` |
|        ! 0 | 1587 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1588 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1589 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1590 | `					 }` |
|        ! 0 | 1591 | `					 return rc;` |
|          - | 1592 | `				 }` |
|    6179713 | 1593 | `				 if( nFuncTok > 1 ){` |
|          - | 1594 | `					 /* Process function arguments */` |
|    4769343 | 1595 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    4769343 | 1596 | `					 if( rc != SXRET_OK ){` |
|         11 | 1597 | `						 return rc;` |
|          - | 1598 | `					 }` |
|    2384665 | 1599 | `				 }` |
|          - | 1600 | `				 /* Link the node to the tree */` |
|    6179705 | 1601 | `				 pNode->pLeft = apNode[iLeft];` |
|    6179705 | 1602 | `				 apNode[iLeft] = 0;` |
|   29500921 | 1603 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   23321221 | 1604 | `					 apNode[iCur+iPtr] = 0;` |
|   11660613 | 1605 | `				 }` |
|          - | 1606 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1607 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1608 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1609 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1610 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1611 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1612 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1613 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1614 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1615 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1616 | `				 {` |
|    6179705 | 1617 | `					 sxi32 iNew = iLeft - 1;` |
|    8026045 | 1618 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    1846345 | 1619 | `						 iNew--;` |
|          5 | 1620 | `					 }` |
|    6179700 | 1621 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3681743 | 1622 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2210288 | 1623 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     752227 | 1624 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     752227 | 1625 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     752227 | 1626 | `						 apNode[iNew] = 0;` |
|     752227 | 1627 | `						 pNode = apNode[iCur];` |
|     376116 | 1628 | `					 }` |
|          - | 1629 | `				 }` |
|   10916933 | 1630 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1631 | `				 /* Subscripting */` |
|    2875255 | 1632 | `				 sxi32 iArrTok = iCur + 1;` |
|    2875255 | 1633 | `				 sxi32 iNest = 1;` |
|    2875250 | 1634 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         18 | 1635 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         14 | 1636 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         14 | 1637 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    2875250 | 1638 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1639 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1640 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     300455 | 1641 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1642 | `						 /* Syntax error */` |
|        ! 0 | 1643 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1644 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1645 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1646 | `						 }` |
|        ! 0 | 1647 | `						 return rc;` |
|          - | 1648 | `				 }` |
|          - | 1649 | `				 /* Collect index tokens */` |
|    6056199 | 1650 | `				 while( iArrTok < nToken ){` |
|    6056199 | 1651 | `					 if( apNode[iArrTok] ){` |
|    6056167 | 1652 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1653 | `							 /* Increment nesting level */` |
|      26773 | 1654 | `							 iNest++;` |
|    6042783 | 1655 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1656 | `							 /* Decrement nesting level */` |
|    2902023 | 1657 | `							 iNest--;` |
|    2902023 | 1658 | `							 if( iNest <= 0 ){` |
|    2875255 | 1659 | `								 break;` |
|          - | 1660 | `							 }` |
|      13384 | 1661 | `						 }` |
|    1590456 | 1662 | `					 }` |
|    3180949 | 1663 | `					 ++iArrTok;` |
|          5 | 1664 | `				 }` |
|    2875255 | 1665 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1666 | `					 /* Recurse and process this expression */` |
|    2653067 | 1667 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2653067 | 1668 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1669 | `						 return rc;` |
|          - | 1670 | `					 }` |
|          - | 1671 | `					 /* Link the node to it's index */` |
|    2653067 | 1672 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1326531 | 1673 | `				 }` |
|          - | 1674 | `				 /* Link the node to the tree */` |
|    2875255 | 1675 | `				 pNode->pLeft = apNode[iLeft];` |
|    2875255 | 1676 | `				 pNode->pRight = 0;` |
|    2875255 | 1677 | `				 apNode[iLeft] = 0;` |
|    8931449 | 1678 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6056199 | 1679 | `					 apNode[iNest] = 0;` |
|    3028102 | 1680 | `				 }` |
|    1437630 | 1681 | `			 }else{` |
|          - | 1682 | `				 /* Member access operators [i.e: '->','::'] */` |
|    4951833 | 1683 | `				  iRight = iCur + 1;` |
|    4955663 | 1684 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3835 | 1685 | `					 iRight++;` |
|          5 | 1686 | `				 }` |
|    4951833 | 1687 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1688 | `					 /* Syntax error */` |
|          5 | 1689 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1690 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1691 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1692 | `					 }` |
|          5 | 1693 | `					 return rc;` |
|          - | 1694 | `				 }` |
|          - | 1695 | `				 /* Link the node to the tree */` |
|    4951829 | 1696 | `				 pNode->pLeft = apNode[iLeft];` |
|    4951824 | 1697 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    4767783 | 1698 | `					 && pNode->pLeft->pOp == 0 &&` |
|    4506645 | 1699 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1700 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1701 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1702 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1703 | `					  * accepts. */` |
|          4 | 1704 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1705 | `						 /* Syntax error */` |
|        ! 0 | 1706 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1707 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1708 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1709 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1710 | `						 }` |
|        ! 0 | 1711 | `						 return rc;` |
|          - | 1712 | `				 }` |
|    4951829 | 1713 | `				 pNode->pRight = apNode[iRight];` |
|    4951829 | 1714 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1715 | `			 }` |
|    7003387 | 1716 | `		 }` |
|   52562079 | 1717 | `		 iLeft = iCur;` |
|   26281042 | 1718 | `	 }` |
|          - | 1719 | `	 /* Handle left associative (new, clone) operators */` |
|  106821799 | 1720 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92686411 | 1721 | `		 if( apNode[iCur] == 0 ){` |
|   54941095 | 1722 | `			 continue;` |
|          - | 1723 | `		 }` |
|   37745321 | 1724 | `		 pNode = apNode[iCur];` |
|   37745321 | 1725 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1726 | `			 SyToken *pToken;` |
|          - | 1727 | `			 /* Get the left node */` |
|      57747 | 1728 | `			 iLeft = iCur + 1;` |
|      57755 | 1729 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1730 | `				 iLeft++;` |
|          1 | 1731 | `			 }` |
|      57747 | 1732 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1733 | `				  /* Syntax error */` |
|        ! 0 | 1734 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1735 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1736 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1737 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1738 | `				 }` |
|        ! 0 | 1739 | `				 return rc;` |
|          - | 1740 | `			 }` |
|          - | 1741 | `			 /* Make sure the operand are of a valid type */` |
|      57747 | 1742 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1743 | `				 /* Clone:` |
|          - | 1744 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1745 | `				  *  ++ function call (including annonymous)` |
|          - | 1746 | `				  *  ++ array member` |
|          - | 1747 | `				  *  ++ 'new' operator` |
|          - | 1748 | `				  * Example:` |
|          - | 1749 | `				  *   clone $pObj;` |
|          - | 1750 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1751 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1752 | `				  */` |
|      57405 | 1753 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      57399 | 1754 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1755 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1756 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1757 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1758 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1759 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1760 | `						 }` |
|        ! 0 | 1761 | `						 return rc;` |
|          - | 1762 | `					 }` |
|      28697 | 1763 | `				 }` |
|      28705 | 1764 | `			 }else{` |
|          - | 1765 | `				 /* New */` |
|        342 | 1766 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1767 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1768 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1769 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1770 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1771 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1772 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1773 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1774 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1775 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1776 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1777 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1778 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1779 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1780 | `					 }` |
|        ! 0 | 1781 | `					 return rc;` |
|          - | 1782 | `				 }` |
|        347 | 1783 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|        347 | 1784 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|        342 | 1785 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         33 | 1786 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1787 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1788 | `						 /* Syntax error */` |
|        ! 0 | 1789 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1790 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1791 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1792 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1793 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1794 | `						 }` |
|        ! 0 | 1795 | `						 return rc;` |
|          - | 1796 | `					 }` |
|        171 | 1797 | `				 }` |
|          - | 1798 | `			 }` |
|          - | 1799 | `			  /* Link the node to the tree */` |
|      57747 | 1800 | `			 pNode->pLeft = apNode[iLeft];` |
|      57747 | 1801 | `			 apNode[iLeft] = 0;` |
|      57747 | 1802 | `			 pNode->pRight = 0; /* Paranoid */` |
|      28871 | 1803 | `		 }` |
|   18872663 | 1804 | `	 }` |
|          - | 1805 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   14135393 | 1806 | `	 iLeft = -1;` |
|  106986631 | 1807 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92686411 | 1808 | `		 if( apNode[iCur] == 0 ){` |
|   54941095 | 1809 | `			 continue;` |
|          - | 1810 | `		 }` |
|   37745321 | 1811 | `		 pNode = apNode[iCur];` |
|   37745321 | 1812 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     195533 | 1813 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     172531 | 1814 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1815 | `					 /* Link the node to the tree */` |
|     180205 | 1816 | `					 pNode->pLeft = apNode[iLeft];` |
|     180205 | 1817 | `					 apNode[iLeft] = 0;` |
|      90100 | 1818 | `			 }` |
|     262596 | 1819 | `		  }` |
|   37910153 | 1820 | `		 iLeft = iCur;` |
|   19037495 | 1821 | `	  }` |
|   14300225 | 1822 | `	 iLeft = -1;` |
|  106986631 | 1823 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|   92686411 | 1824 | `		 if( apNode[iCur] == 0 ){` |
|   55121295 | 1825 | `			 continue;` |
|          - | 1826 | `		 }` |
|   37565121 | 1827 | `		 pNode = apNode[iCur];` |
|   37565121 | 1828 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15328 | 1829 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15333 | 1830 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1831 | `					 /* Syntax error */` |
|        ! 0 | 1832 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1833 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1834 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1835 | `					 }` |
|        ! 0 | 1836 | `					 return rc;` |
|          - | 1837 | `			 }` |
|          - | 1838 | `			 /* Link the node to the tree */` |
|      15333 | 1839 | `			 pNode->pLeft = apNode[iLeft];` |
|      15333 | 1840 | `			 apNode[iLeft] = 0;` |
|          - | 1841 | `			 /* Mark as pre-increment/decrement node */` |
|      15333 | 1842 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7664 | 1843 | `		  }` |
|   37565121 | 1844 | `		 iLeft = iCur;` |
|   18782563 | 1845 | `	 }` |
|          - | 1846 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1847 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1848 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1849 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1850 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1851 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1852 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1853 | `	  * pass below skips it (pLeft != 0). */` |
|   14300225 | 1854 | `	 iLeft = -1;` |
|  106986631 | 1855 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   92686411 | 1856 | `		 if( apNode[iCur] == 0 ){` |
|   55213387 | 1857 | `			 continue;` |
|          - | 1858 | `		 }` |
|   37473029 | 1859 | `		 pNode = apNode[iCur];` |
|   37473029 | 1860 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      76769 | 1861 | `			 iRight = iCur + 1;` |
|      76769 | 1862 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1863 | `				 iRight++;` |
|        ! 0 | 1864 | `			 }` |
|      76769 | 1865 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1866 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1867 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1868 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1869 | `				 }` |
|        ! 0 | 1870 | `				 return rc;` |
|          - | 1871 | `			 }` |
|      76769 | 1872 | `			 pNode->pLeft = apNode[iLeft];` |
|      76769 | 1873 | `			 pNode->pRight = apNode[iRight];` |
|      76769 | 1874 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      38382 | 1875 | `		 }` |
|   37473029 | 1876 | `		 iLeft = iCur;` |
|   18736517 | 1877 | `	 }` |
|          - | 1878 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   14300225 | 1879 | `	  iLeft = 0;` |
|  106986625 | 1880 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|   92686407 | 1881 | `		  if( apNode[iCur] ){` |
|   37396261 | 1882 | `			  pNode = apNode[iCur];` |
|   37396261 | 1883 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1137669 | 1884 | `				  if( iLeft > 0 ){` |
|          - | 1885 | `					  /* Link the node to the tree */` |
|    1137667 | 1886 | `					  pNode->pLeft = apNode[iLeft];` |
|    1137667 | 1887 | `					  apNode[iLeft] = 0;` |
|    1137667 | 1888 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      53645 | 1889 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1890 | `							   /* Syntax error */` |
|        ! 0 | 1891 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1892 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 1893 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 1894 | `							  }` |
|        ! 0 | 1895 | `							  return rc;` |
|          - | 1896 | `						  }` |
|      26820 | 1897 | `					  }` |
|     568836 | 1898 | `				  }else{` |
|          - | 1899 | `					  /* Syntax error */` |
|          3 | 1900 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 1901 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 1902 | `						  rc = SXERR_SYNTAX;` |
|          1 | 1903 | `					  }` |
|          3 | 1904 | `					  return rc;` |
|          - | 1905 | `				  }` |
|     568831 | 1906 | `			  }` |
|          - | 1907 | `			  /* Save terminal position */` |
|   37396259 | 1908 | `			  iLeft = iCur;` |
|   18698127 | 1909 | `		  }` |
|   46343205 | 1910 | `	  }` |
|          - | 1911 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 1912 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 1913 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 1914 | `	  * yielding a right-leaning tree. */` |
|  106986623 | 1915 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|   92686405 | 1916 | `		 if( apNode[iCur] == 0 ){` |
|   56427927 | 1917 | `			 continue;` |
|          - | 1918 | `		 }` |
|   36258483 | 1919 | `		 pNode = apNode[iCur];` |
|   36258483 | 1920 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 1921 | `			 sxi32 iL, iR;` |
|          - | 1922 | `			 /* Find the right operand */` |
|        115 | 1923 | `			 iR = -1;` |
|          - | 1924 | `			 {` |
|          - | 1925 | `				 sxi32 j;` |
|        127 | 1926 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 1927 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 1928 | `				 }` |
|          - | 1929 | `			 }` |
|          - | 1930 | `			 /* Find the left operand */` |
|        115 | 1931 | `			 iL = -1;` |
|          - | 1932 | `			 {` |
|          - | 1933 | `				 sxi32 j;` |
|        183 | 1934 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 1935 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 1936 | `				 }` |
|          - | 1937 | `			 }` |
|        115 | 1938 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 1939 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1940 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1941 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1942 | `				 }` |
|        ! 0 | 1943 | `				 return rc;` |
|          - | 1944 | `			 }` |
|        115 | 1945 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 1946 | `			 pNode->pRight = apNode[iR];` |
|        115 | 1947 | `			 apNode[iL] = 0;` |
|        115 | 1948 | `			 apNode[iR] = 0;` |
|          - | 1949 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 1950 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 1951 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 1952 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 1953 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 1954 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 1955 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 1956 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 1957 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 1958 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 1959 | `			  * operands are respected. */` |
|        114 | 1960 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 1961 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 1962 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 1963 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 1964 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 1965 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 1966 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 1967 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 1968 | `				 while( pTail->pLeft` |
|         34 | 1969 | `					 && pTail->pLeft->pOp` |
|         23 | 1970 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 1971 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 1972 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 1973 | `					 pTail = pTail->pLeft;` |
|          1 | 1974 | `				 }` |
|          - | 1975 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 1976 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 1977 | `				 pTail->pLeft = pNode;` |
|         27 | 1978 | `				 apNode[iCur] = pHead;` |
|         13 | 1979 | `			 }` |
|         57 | 1980 | `		 }` |
|   18129244 | 1981 | `	 }` |
|          - | 1982 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  157302317 | 1983 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  143002109 | 1984 | `		 iLeft = -1;` |
| 1069865815 | 1985 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  926863721 | 1986 | `			 if( apNode[iCur] == 0 ){` |
|  628073673 | 1987 | `				 continue;` |
|          - | 1988 | `			 }` |
|  298790053 | 1989 | `			 pNode = apNode[iCur];` |
|  298790053 | 1990 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 1991 | `				 /* Get the right node */` |
|    5248751 | 1992 | `				 iRight = iCur + 1;` |
|    7902847 | 1993 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2654101 | 1994 | `					 iRight++;` |
|          5 | 1995 | `				 }` |
|    5248751 | 1996 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1997 | `					 /* Syntax error */` |
|         10 | 1998 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 1999 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2000 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2001 | `					 }` |
|         10 | 2002 | `					 return rc;` |
|          - | 2003 | `				 }` |
|    5248743 | 2004 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2005 | `					 sxi32  iTmp;` |
|          - | 2006 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2007 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2008 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2009 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2010 | `					  * is swapped below. */` |
|         65 | 2011 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2012 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2013 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2014 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2015 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2016 | `						 }` |
|          3 | 2017 | `						 return rc;` |
|          - | 2018 | `					 }` |
|         63 | 2019 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|          - | 2020 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2021 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2022 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2023 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2024 | `						 }` |
|        ! 0 | 2025 | `						 return rc;` |
|          - | 2026 | `					 }` |
|         63 | 2027 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         45 | 2028 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2029 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2030 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2031 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2032 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2033 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2034 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2035 | `									 }` |
|        ! 0 | 2036 | `									 return rc;` |
|          - | 2037 | `							 }` |
|        ! 0 | 2038 | `						 }` |
|         21 | 2039 | `					 }` |
|          - | 2040 | `					 /* Swap operands */` |
|         63 | 2041 | `					 iTmp = iRight;` |
|         63 | 2042 | `					 iRight = iLeft;` |
|         63 | 2043 | `					 iLeft = iTmp;` |
|         30 | 2044 | `				 }` |
|          - | 2045 | `				 /* Link the node to the tree */` |
|    5248741 | 2046 | `				 pNode->pLeft = apNode[iLeft];` |
|    5248741 | 2047 | `				 pNode->pRight = apNode[iRight];` |
|    5248741 | 2048 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2624368 | 2049 | `			 }` |
|  298790043 | 2050 | `			 iLeft = iCur;` |
|  149395024 | 2051 | `		 }` |
|   71501052 | 2052 | `	 }` |
|          - | 2053 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2054 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2055 | `	  * we are dealing with a single operator.` |
|          - | 2056 | `	  */` |
|   14300213 | 2057 | `	  iLeft = -1;` |
|  103216893 | 2058 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   89375113 | 2059 | `		  if( apNode[iCur] == 0 ){` |
|   64985665 | 2060 | `			  continue;` |
|          - | 2061 | `		  }` |
|   24389453 | 2062 | `		  pNode = apNode[iCur];` |
|   24389453 | 2063 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     458433 | 2064 | `			  sxi32 iNest = 1;` |
|     458433 | 2065 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2066 | `				  /* Missing condition */` |
|          3 | 2067 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2068 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2069 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2070 | `				  }` |
|          3 | 2071 | `				  return rc;` |
|          - | 2072 | `			  }` |
|          - | 2073 | `			  /* Get the right node */` |
|     458431 | 2074 | `			  iRight = iCur + 1;` |
|    1888503 | 2075 | `			  while( iRight < nToken  ){` |
|    1888503 | 2076 | `				  if( apNode[iRight] ){` |
|     912965 | 2077 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2078 | `						  /* Increment nesting level */` |
|        ! 0 | 2079 | `						  ++iNest;` |
|     912965 | 2080 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2081 | `						  /* Decrement nesting level */` |
|     458431 | 2082 | `						  --iNest;` |
|     458431 | 2083 | `						  if( iNest <= 0 ){` |
|     458431 | 2084 | `							  break;` |
|          - | 2085 | `						  }` |
|        ! 0 | 2086 | `					  }` |
|     227267 | 2087 | `				  }` |
|    1430077 | 2088 | `				  iRight++;` |
|          5 | 2089 | `			  }` |
|     458431 | 2090 | `			  if( iRight > iCur + 1 ){` |
|          - | 2091 | `				  /* Recurse and process the then expression */` |
|     454539 | 2092 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     454539 | 2093 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2094 | `					  return rc;` |
|          - | 2095 | `				  }` |
|          - | 2096 | `				  /* Link the node to the tree */` |
|     454539 | 2097 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     227267 | 2098 | `			  }else{` |
|          - | 2099 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2100 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2101 | `			  }` |
|     458431 | 2102 | `			  apNode[iCur + 1] = 0;` |
|     458431 | 2103 | `			  if( iRight + 1 < nToken ){` |
|          - | 2104 | `				  /* Recurse and process the else expression */` |
|     458431 | 2105 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     458431 | 2106 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2107 | `					  return rc;` |
|          - | 2108 | `				  }` |
|          - | 2109 | `				  /* Link the node to the tree */` |
|     458431 | 2110 | `				  pNode->pRight = apNode[iRight + 1];` |
|     458431 | 2111 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     229218 | 2112 | `			  }else{` |
|        ! 0 | 2113 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2114 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2115 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2116 | `				 }` |
|        ! 0 | 2117 | `				 return rc;` |
|          - | 2118 | `			  }` |
|          - | 2119 | `			  /* Point to the condition */` |
|     458431 | 2120 | `			  pNode->pCond  = apNode[iLeft];` |
|     458431 | 2121 | `			  apNode[iLeft] = 0;` |
|     458431 | 2122 | `			  break;` |
|          - | 2123 | `		  }` |
|   23931025 | 2124 | `		  iLeft = iCur;` |
|   11965515 | 2125 | `	  }` |
|          - | 2126 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2127 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2128 | `	  * so there is no need for a precedence loop here.` |
|          - | 2129 | `	  */` |
|   14300211 | 2130 | `	 iRight = -1;` |
|  106986427 | 2131 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|   92686275 | 2132 | `		 if( apNode[iCur] == 0 ){` |
|   73570625 | 2133 | `			 continue;` |
|          - | 2134 | `		 }` |
|   19115655 | 2135 | `		 pNode = apNode[iCur];` |
|   19115655 | 2136 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2137 | `			 /* Get the left node */` |
|    4815371 | 2138 | `			 iLeft = iCur - 1;` |
|    6588169 | 2139 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1772803 | 2140 | `				 iLeft--;` |
|          5 | 2141 | `			 }` |
|    4815371 | 2142 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2143 | `				 /* Syntax error */` |
|         45 | 2144 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2145 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2146 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2147 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2148 | `				 }else{` |
|         41 | 2149 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2150 | `				 }` |
|         45 | 2151 | `				 if( rc != SXERR_ABORT ){` |
|         43 | 2152 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2153 | `				 }` |
|         45 | 2154 | `				 return rc;` |
|          - | 2155 | `			 }` |
|          - | 2156 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2157 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2158 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2159 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2160 | `			  * a write. */` |
|    4815329 | 2161 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2162 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2163 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2164 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2165 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2166 | `				 }` |
|         11 | 2167 | `				 return rc;` |
|          - | 2168 | `			 }` |
|          - | 2169 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2170 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2171 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2172 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2173 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    4815321 | 2174 | `			 pSuppress = 0;` |
|    4815316 | 2175 | `			 if( apNode[iLeft]->pOp` |
|    3073866 | 2176 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     666208 | 2177 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2178 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2179 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2180 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2181 | `			 }` |
|    4815321 | 2182 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2183 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2184 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2185 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2186 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2187 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2188 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        103 | 2189 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2190 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2191 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2192 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2193 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2194 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2195 | `					 }` |
|          8 | 2196 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2197 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2198 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2199 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2200 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2201 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2202 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2203 | `						 iRight = iCur;` |
|          9 | 2204 | `						 continue;` |
|          - | 2205 | `					 }` |
|        ! 0 | 2206 | `				 }` |
|        123 | 2207 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         88 | 2208 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2209 | `					 /* Left operand must be a modifiable l-value */` |
|          6 | 2210 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2211 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2212 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2213 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2214 | `					 }else{` |
|          4 | 2215 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          2 | 2216 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2217 | `					 }` |
|          6 | 2218 | `					 if( rc != SXERR_ABORT ){` |
|          6 | 2219 | `						 rc = SXERR_SYNTAX;` |
|          2 | 2220 | `					 }` |
|          6 | 2221 | `					 return rc;` |
|          - | 2222 | `				 }` |
|         43 | 2223 | `			 }` |
|          - | 2224 | `			 /* Link the node to the tree (Reverse) */` |
|    4815309 | 2225 | `			 pNode->pLeft = apNode[iRight];` |
|    4815309 | 2226 | `			 pNode->pRight = apNode[iLeft];` |
|    4815309 | 2227 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    4815309 | 2228 | `			 if( pSuppress ){` |
|          - | 2229 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2230 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2231 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2232 | `			 }` |
|    2407652 | 2233 | `		 }` |
|   19115593 | 2234 | `		 iRight = iCur;` |
|    9557799 | 2235 | `	 }` |
|          - | 2236 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   71500765 | 2237 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   57200613 | 2238 | `		 iLeft = -1;` |
|  427945421 | 2239 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  370744813 | 2240 | `			 if( apNode[iCur] == 0 ){` |
|  313543953 | 2241 | `				 continue;` |
|          - | 2242 | `			 }` |
|   57200865 | 2243 | `			 pNode = apNode[iCur];` |
|   57200865 | 2244 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2245 | `				 /* Get the right node */` |
|         51 | 2246 | `				 iRight = iCur + 1;` |
|         63 | 2247 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2248 | `					 iRight++;` |
|          1 | 2249 | `				 }` |
|         51 | 2250 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2251 | `					 /* Syntax error */` |
|        ! 0 | 2252 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2253 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2254 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2255 | `					 }` |
|        ! 0 | 2256 | `					 return rc;` |
|          - | 2257 | `				 }` |
|          - | 2258 | `				 /* Link the node to the tree */` |
|         51 | 2259 | `				 pNode->pLeft = apNode[iLeft];` |
|         51 | 2260 | `				 pNode->pRight = apNode[iRight];` |
|         51 | 2261 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         24 | 2262 | `			 }` |
|   57200865 | 2263 | `			 iLeft = iCur;` |
|   28600435 | 2264 | `		 }` |
|   28600309 | 2265 | `	 }` |
|          - | 2266 | `	 /* Point to the root of the expression tree */` |
|   92686179 | 2267 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   78386045 | 2268 | `		 if( apNode[iCur] ){` |
|   13694531 | 2269 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         23 | 2270 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|         23 | 2271 | `				  if( rc != SXERR_ABORT ){` |
|         23 | 2272 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2273 | `				  }` |
|         23 | 2274 | `				  return rc;` |
|          - | 2275 | `			 }` |
|   13694513 | 2276 | `			 apNode[0] = apNode[iCur];` |
|   13694513 | 2277 | `			 apNode[iCur] = 0;` |
|    6847254 | 2278 | `		 }` |
|   39193016 | 2279 | `	 }` |
|   14300139 | 2280 | `	 return SXRET_OK;` |
|   13031785 | 2281 | ` }` |
|          - | 2282 | ` /*` |
|          - | 2283 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2284 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2285 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2286 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2287 | `  */` |
|   15238472 | 2288 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2289 | `{` |
|          - | 2290 | `	ph7_expr_node **apNode;` |
|          - | 2291 | `	ph7_expr_node *pNode;` |
|          - | 2292 | `	sxi32 rc;` |
|          - | 2293 | `	/* Reset node container */` |
|   15238477 | 2294 | `	SySetReset(pExprNode);` |
|   15238477 | 2295 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2296 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2297 | `	{` |
|   15238477 | 2298 | `		int iLastWasTerm = 0;` |
|   15238477 | 2299 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|   95148373 | 2300 | `		while( pGen->pIn < pGen->pEnd ){` |
|   79909935 | 2301 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   79909935 | 2302 | `			if( rc != SXRET_OK ){` |
|         38 | 2303 | `				return rc;` |
|          - | 2304 | `			}` |
|          - | 2305 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   79909901 | 2306 | `			if( pNode->xCode ){` |
|          - | 2307 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   40711019 | 2308 | `				iLastWasTerm = 1;` |
|   59554394 | 2309 | `			}else if( pNode->pOp ){` |
|          - | 2310 | `				/* Operator node */` |
|   22613827 | 2311 | `				iLastWasTerm = 0;` |
|   11306916 | 2312 | `			}else{` |
|          - | 2313 | `				/* Delimiter: ')' and ']' end terms */` |
|   16585065 | 2314 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2315 | `			}` |
|          - | 2316 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2317 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2318 | `			 * node kind, so this single test covers all branches. */` |
|   79909901 | 2319 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2320 | `			/* Save the extracted node */` |
|   79909901 | 2321 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2322 | `		}` |
|          - | 2323 | `	}` |
|   15238443 | 2324 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2325 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2326 | `		*ppRoot = 0;` |
|        ! 0 | 2327 | `		return SXRET_OK;` |
|          - | 2328 | `	}` |
|   15238443 | 2329 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2330 | `	/* Make sure we are dealing with valid nodes */` |
|   15238443 | 2331 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   15238443 | 2332 | `	if( rc != SXRET_OK ){` |
|          - | 2333 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2334 | `		 * cleanup the mess left behind.` |
|          - | 2335 | `		 */` |
|         56 | 2336 | `		*ppRoot = 0;` |
|         56 | 2337 | `		return rc;` |
|          - | 2338 | `	}` |
|          - | 2339 | `	/* Build the tree */` |
|   15238391 | 2340 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   15238391 | 2341 | `	if( rc != SXRET_OK ){` |
|          - | 2342 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        103 | 2343 | `		*ppRoot = 0;` |
|        103 | 2344 | `		return rc;` |
|          - | 2345 | `	}` |
|          - | 2346 | `	/* Point to the root of the tree */` |
|   15238293 | 2347 | `	*ppRoot = apNode[0];` |
|   15238293 | 2348 | `	return SXRET_OK;` |
|    7619241 | 2349 | `}` |
|          - | 2350 |  |
