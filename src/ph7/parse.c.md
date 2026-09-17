# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1183/1362 lines (86.86%)

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
|   29202186 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   29202191 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  460757113 |  278 | `	for(;;){` |
|  921514231 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  921514231 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   88074335 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   44037170 |  285 | `		}else{` |
|  833439901 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  921514231 |  288 | `		if( rc == 0 ){` |
|   29537345 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   29081091 |  291 | `				return &aOpTable[n];` |
|          - |  292 | `			}` |
|          - |  293 | `			/* Handle ambiguity */` |
|     456259 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|          - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      43253 |  296 | `				return &aOpTable[n];` |
|          - |  297 | `			}` |
|     413011 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      77865 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|          - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      77865 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|          - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      77857 |  303 | `					return &aOpTable[n];` |
|          - |  304 | `				}` |
|          - |  305 |  |
|          4 |  306 | `			}` |
|     167577 |  307 | `		}` |
|  892312045 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   14601098 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    7939670 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    7939675 |  320 | `	SyToken *pCur = pIn;` |
|    7939675 |  321 | `	sxi32 iNest = 1;` |
|   85150766 |  322 | `	for(;;){` |
|  170301537 |  323 | `		if( pCur >= pEnd ){` |
|      16059 |  324 | `			break;` |
|          - |  325 | `		}` |
|  170285483 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    6563977 |  328 | `			iNest++;` |
|  167003497 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   14487593 |  331 | `			iNest--;` |
|   14487593 |  332 | `			if( iNest <= 0 ){` |
|    7923621 |  333 | `				break;` |
|          - |  334 | `			}` |
|    3281986 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  162361867 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    7939675 |  340 | `	*ppEnd = pCur;` |
|    7939675 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     541568 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     541568 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     541531 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        131 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     541447 |  358 | `	if( bCheckFunc ){` |
|      58730 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      58718 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      58694 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|         61 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      29337 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     541391 |  366 | `	return FALSE;` |
|     270789 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   17129402 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   17129407 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7811 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7811 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3903 |  383 | `	}` |
|   17129407 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  107372931 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   90243565 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     243555 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   90000015 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    7560855 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     409314 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    7069063 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    7069063 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    7069063 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    7069063 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3534529 |  401 | `					}` |
|    3534529 |  402 | `			}` |
|    7560855 |  403 | `			iParen++;` |
|   86219590 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    7560857 |  405 | `			if( iParen <= 0 ){` |
|         15 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         15 |  407 | `				if( rc != SXERR_ABORT ){` |
|         15 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         15 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    7560845 |  412 | `			iParen--;` |
|   78658733 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    3030757 |  414 | `			iSquare++;` |
|   73362937 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    3030761 |  416 | `			if( iSquare <= 0 ){` |
|          8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          8 |  418 | `				if( rc != SXERR_ABORT ){` |
|          8 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          8 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    3030755 |  423 | `			iSquare--;` |
|   70332180 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       3901 |  425 | `			iBraces++;` |
|       3901 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|          - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|          - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|          - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|          3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|          3 |  431 | `				if( rc != SXERR_ABORT ){` |
|          3 |  432 | `					rc = SXERR_SYNTAX;` |
|          1 |  433 | `				}` |
|          3 |  434 | `				return rc;` |
|          5 |  435 | `			}` |
|   68814856 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3911 |  437 | `			if( iBraces <= 0 ){` |
|         16 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         16 |  439 | `				if( rc != SXERR_ABORT ){` |
|         16 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         16 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3899 |  444 | `			iBraces--;` |
|   68810950 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     504861 |  446 | `			if( iQuesty > 0 ){` |
|     504559 |  447 | `				iQuesty--;` |
|     252584 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   68556573 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   22560587 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   22560587 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     504561 |  461 | `				iQuesty++;` |
|   22308309 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      93823 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      46909 |  481 | `			}` |
|   11280291 |  482 | `		}` |
|   44999992 |  483 | `	}` |
|   17129371 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         17 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         17 |  486 | `		if( rc != SXERR_ABORT ){` |
|         17 |  487 | `			rc = SXERR_SYNTAX;` |
|          7 |  488 | `		}` |
|         17 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   17129357 |  491 | `	return SXRET_OK;` |
|    8564706 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   14124630 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   14124635 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   14124635 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   14120679 |  502 | `		pIn++;` |
|    7060337 |  503 | `	}` |
|    7064334 |  504 | `	for(;;){` |
|   14128673 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       4043 |  506 | `			pIn++;` |
|       4043 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       4041 |  508 | `				pIn++;` |
|       2018 |  509 | `			}` |
|       2024 |  510 | `		}else{` |
|    7062320 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   14124635 |  515 | `	*ppCur = pIn;` |
|   14124635 |  516 | `}` |
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
|        639 |  639 | `	pIn++; /* Jump the trailing parenthesis */` |
|          - |  640 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|        639 |  641 | `	ExprSkipReturnType(&pIn,pEnd);` |
|        639 |  642 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|        139 |  643 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|          - |  644 | `		/* Check if we are dealing with a closure */` |
|        139 |  645 | `		if( nKey == PH7_TKWRD_USE ){` |
|        131 |  646 | `			pIn++; /* Jump the 'use' keyword */` |
|        131 |  647 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  648 | `				/* Syntax error */` |
|          6 |  649 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  650 | `				if( rc != SXERR_ABORT ){` |
|          6 |  651 | `					rc = SXERR_SYNTAX;` |
|          2 |  652 | `				}` |
|          6 |  653 | `				goto Synchronize;` |
|          - |  654 | `			}` |
|        127 |  655 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|        127 |  656 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        127 |  657 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|          - |  658 | `				/* Syntax error */` |
|          6 |  659 | `				rc = PH7_GenSyntaxError(&(*pGen),0 /* ran off the end */,0);` |
|          6 |  660 | `				if( rc != SXERR_ABORT ){` |
|          6 |  661 | `					rc = SXERR_SYNTAX;` |
|          2 |  662 | `				}` |
|          6 |  663 | `				goto Synchronize;` |
|          - |  664 | `			}` |
|        123 |  665 | `			pIn++;` |
|          - |  666 | `			/* php 7.1+: the return type may also follow the use clause —` |
|          - |  667 | ``			 * `function (...) use (...) : int {` */`` |
|        123 |  668 | `			ExprSkipReturnType(&pIn,pEnd);` |
|         64 |  669 | `		}else{` |
|          - |  670 | `			/* Syntax error */` |
|         11 |  671 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|         11 |  672 | `			if( rc != SXERR_ABORT ){` |
|         11 |  673 | `				rc = SXERR_SYNTAX;` |
|          4 |  674 | `			}` |
|         11 |  675 | `			goto Synchronize;` |
|          - |  676 | `		}` |
|         59 |  677 | `	}` |
|          - |  678 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|          - |  679 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|          - |  680 | `	 * the type), and pEnd is one past the last token. */` |
|        623 |  681 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|        623 |  682 | `		pIn++; /* Jump the leading curly '{' */` |
|        623 |  683 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        623 |  684 | `		if( pIn < pEnd ){` |
|        623 |  685 | `			pIn++;` |
|        309 |  686 | `		}` |
|        314 |  687 | `	}else{` |
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
|        623 |  698 | `	rc = SXRET_OK;` |
|        321 |  699 | `Synchronize:` |
|          - |  700 | `	/* Synchronize pointers */` |
|        647 |  701 | `	*ppCur = pIn;` |
|        647 |  702 | `	return rc;` |
|        326 |  703 | `}` |
|          - |  704 | `/*` |
|          - |  705 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|          - |  706 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|          - |  707 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|          - |  708 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|          - |  709 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|          - |  710 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|          - |  711 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|          - |  712 | ` */` |
|         30 |  713 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          4 |  714 | `{` |
|         34 |  715 | `	SyToken *pIn = *ppCur;` |
|         34 |  716 | `	sxu32 nLine = pIn->nLine;` |
|          - |  717 | `	sxi32 rc;` |
|         34 |  718 | `	pIn++; /* Jump the 'class' keyword */` |
|          - |  719 | `	/* Optional constructor argument list */` |
|         34 |  720 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|          7 |  721 | `		pIn++; /* Jump '(' */` |
|          7 |  722 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|          7 |  723 | `		if( pIn < pEnd ){` |
|          7 |  724 | `			pIn++; /* Jump ')' */` |
|          3 |  725 | `		}` |
|          3 |  726 | `	}` |
|          - |  727 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|          - |  728 | `	 * (no braces appear between ')' and the class body). */` |
|         62 |  729 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|         32 |  730 | `		pIn++;` |
|          4 |  731 | `	}` |
|         34 |  732 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|          - |  733 | `		/* Syntax error: missing class body */` |
|        ! 0 |  734 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|          - |  735 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|        ! 0 |  736 | `		if( rc != SXERR_ABORT ){` |
|        ! 0 |  737 | `			rc = SXERR_SYNTAX;` |
|        ! 0 |  738 | `		}` |
|        ! 0 |  739 | `		*ppCur = pIn;` |
|        ! 0 |  740 | `		return rc;` |
|          - |  741 | `	}` |
|         34 |  742 | `	pIn++; /* Jump the leading '{' */` |
|         34 |  743 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|         34 |  744 | `	if( pIn < pEnd ){` |
|         34 |  745 | `		pIn++; /* Jump the trailing '}' */` |
|         15 |  746 | `	}` |
|         34 |  747 | `	*ppCur = pIn;` |
|         34 |  748 | `	return SXRET_OK;` |
|         19 |  749 | `}` |
|          - |  750 | `/*` |
|          - |  751 | ` * Assemble a PHP 7.4 arrow function token range:` |
|          - |  752 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|          - |  753 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|          - |  754 | ` * past the body expression — the body ends at the first top-level comma,` |
|          - |  755 | ` * semicolon, or unbalanced closing delimiter.` |
|          - |  756 | ` */` |
|        546 |  757 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  758 | `{` |
|        551 |  759 | `	SyToken *pIn = *ppCur;` |
|          - |  760 | `	sxu32 nLine;` |
|          - |  761 | `	sxi32 rc;` |
|          - |  762 | `	int iNest;` |
|        551 |  763 | `	nLine = pIn->nLine;` |
|          - |  764 | `	/* Optional 'static' prefix */` |
|        546 |  765 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|        551 |  766 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|          9 |  767 | `		pIn++;` |
|          4 |  768 | `	}` |
|          - |  769 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|        546 |  770 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        551 |  771 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|        ! 0 |  772 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  773 | `		goto Synchronize;` |
|          - |  774 | `	}` |
|        551 |  775 | `	pIn++; /* Jump 'fn' */` |
|        273 |  776 | `	SXUNUSED(nLine);` |
|        273 |  777 | `	SXUNUSED(pGen);` |
|          - |  778 | `	/* Optional '&' for return-by-reference */` |
|        551 |  779 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        ! 0 |  780 | `		pIn++;` |
|        ! 0 |  781 | `	}` |
|          - |  782 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|          - |  783 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|          - |  784 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|          - |  785 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|        551 |  786 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        549 |  787 | `		pIn++; /* '(' */` |
|        549 |  788 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        549 |  789 | `		if( pIn < pEnd ){` |
|        547 |  790 | `			pIn++; /* ')' */` |
|        271 |  791 | `		}` |
|        272 |  792 | `	}` |
|          - |  793 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|        551 |  794 | `	ExprSkipReturnType(&pIn,pEnd);` |
|          - |  795 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|        551 |  796 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        545 |  797 | `		pIn++;` |
|        270 |  798 | `	}` |
|          - |  799 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|        551 |  800 | `	iNest = 0;` |
|       4229 |  801 | `	while( pIn < pEnd ){` |
|       4093 |  802 | `		if( iNest == 0 && (pIn->nType &` |
|          - |  803 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        414 |  804 | `			break;` |
|          - |  805 | `		}` |
|       3683 |  806 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        547 |  807 | `			iNest++;` |
|       3412 |  808 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        547 |  809 | `			iNest--;` |
|        271 |  810 | `		}` |
|       3683 |  811 | `		pIn++;` |
|          5 |  812 | `	}` |
|        551 |  813 | `	rc = SXRET_OK;` |
|        273 |  814 | `Synchronize:` |
|        551 |  815 | `	*ppCur = pIn;` |
|        551 |  816 | `	return rc;` |
|          5 |  817 | `}` |
|          - |  818 | `/*` |
|          - |  819 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|          - |  820 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|          - |  821 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|          - |  822 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|          - |  823 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|          - |  824 | ` */` |
|         74 |  825 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          4 |  826 | `{` |
|         78 |  827 | `	SyToken *pIn = *ppCur;` |
|          - |  828 | `	sxi32 rc;` |
|         37 |  829 | `	SXUNUSED(pGen);` |
|          - |  830 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|         74 |  831 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|         78 |  832 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|        ! 0 |  833 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  834 | `		goto Synchronize;` |
|          - |  835 | `	}` |
|         78 |  836 | `	pIn++; /* Jump 'match' */` |
|          - |  837 | `	/* Optional '(' subject ')' */` |
|         78 |  838 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         78 |  839 | `		pIn++;` |
|         78 |  840 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|         78 |  841 | `		if( pIn < pEnd ){` |
|         78 |  842 | `			pIn++; /* ')' */` |
|         37 |  843 | `		}` |
|         37 |  844 | `	}` |
|          - |  845 | `	/* Optional '{' arms '}' */` |
|         78 |  846 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|         78 |  847 | `		pIn++;` |
|         78 |  848 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|         78 |  849 | `		if( pIn < pEnd ){` |
|         78 |  850 | `			pIn++; /* '}' */` |
|         37 |  851 | `		}` |
|         37 |  852 | `	}` |
|         78 |  853 | `	rc = SXRET_OK;` |
|         37 |  854 | `Synchronize:` |
|         78 |  855 | `	*ppCur = pIn;` |
|         78 |  856 | `	return rc;` |
|          4 |  857 | `}` |
|          - |  858 | `/*` |
|          - |  859 | ` * Extract a single expression node from the input.` |
|          - |  860 | ` * On success store the freshly extractd node in ppNode.` |
|          - |  861 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  862 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|          - |  863 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|          - |  864 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|          - |  865 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|          - |  866 | ` */` |
|   90247866 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  868 | `{` |
|          - |  869 | `	ph7_expr_node *pNode;` |
|          - |  870 | `	SyToken *pCur;` |
|          - |  871 | `	sxi32 rc;` |
|          - |  872 | `	/* Allocate a new node */` |
|   90247871 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   90247871 |  874 | `	if( pNode == 0 ){` |
|          - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  877 | `		 */` |
|        ! 0 |  878 | `		return SXERR_MEM;` |
|          - |  879 | `	}` |
|          - |  880 | `	/* Zero the structure */` |
|   90247871 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   90247871 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  883 | `	/* Point to the head of the token stream */` |
|   90247871 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  885 | `	/* Start collecting tokens */` |
|   90247871 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4241 |  887 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|          - |  888 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|          - |  889 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|          - |  890 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|          - |  891 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|         84 |  892 | `			pNode->pEnd = pCur;` |
|         84 |  893 | `			pCur++;` |
|         84 |  894 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|         84 |  895 | `			pNode->xCode = PH7_CompileFccMarker;` |
|         84 |  896 | `			pGen->pIn = pCur;` |
|         84 |  897 | `			*ppNode = pNode;` |
|         84 |  898 | `			return SXRET_OK;` |
|          - |  899 | `		}` |
|          - |  900 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|          - |  901 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       4159 |  902 | `		pCur++;` |
|       4159 |  903 | `		pGen->pIn = pCur;` |
|       4159 |  904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4159 |  905 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4159 |  906 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4159 |  907 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2077 |  908 | `		}` |
|       4159 |  909 | `		return rc;` |
|          - |  910 | `	}` |
|   90243635 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - |  912 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - |  913 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - |  914 | `		 */` |
|     243557 |  915 | `		pCur++; /* Skip the opening '[' */` |
|     243557 |  916 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     243557 |  917 | `		if( pCur < pGen->pEnd ){` |
|     243557 |  918 | `			pCur++; /* Skip past the closing ']' */` |
|     121781 |  919 | `		}else{` |
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
|     243764 |  931 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        419 |  932 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        419 |  933 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|         57 |  934 | `				pNode->xCode = PH7_CompileShortList;` |
|         30 |  935 | `			}else{` |
|        365 |  936 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - |  937 | `			}` |
|        212 |  938 | `		}else{` |
|     243143 |  939 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 |  940 | `		}` |
|   90121859 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|   90000070 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   25591390 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   12824879 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|   90000054 |  980 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - |  981 | `		/* Point to the instance that describe this operator */` |
|   25591373 |  982 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - |  983 | `		/* Advance the stream cursor */` |
|   25591373 |  984 | `		pCur++;` |
|   77204359 |  985 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - |  986 | `		/* Isolate variable */` |
|   43290673 |  987 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   21645345 |  988 | `			pCur++; /* Variable variable */` |
|          5 |  989 | `		}` |
|   21645333 |  990 | `		if( pCur < pGen->pEnd ){` |
|   21645333 |  991 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - |  992 | `				/* Variable name */` |
|   21645307 |  993 | `				pCur++;` |
|   10822682 |  994 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|         22 |  995 | `				pCur++;` |
|          - |  996 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|         22 |  997 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|         22 |  998 | `				if( pCur < pGen->pEnd ){` |
|         19 |  999 | `					pCur++;` |
|         11 | 1000 | `				}else{` |
|          3 | 1001 | `					rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1002 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1003 | `						rc = SXERR_SYNTAX;` |
|          1 | 1004 | `					}` |
|          3 | 1005 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1006 | `					return rc;` |
|          - | 1007 | `				}` |
|          8 | 1008 | `			}` |
|   10822663 | 1009 | `		}` |
|   21645331 | 1010 | `		pNode->xCode = PH7_CompileVariable;` |
|   53586010 | 1011 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1057851 | 1012 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    1057851 | 1013 | `		 if( bAfterMemberOp ){` |
|          - | 1014 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1015 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1016 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1017 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1018 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1019 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1020 | `			  * the word itself. */` |
|     128493 | 1021 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     128493 | 1022 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     128493 | 1023 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     993607 | 1024 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1025 | `			 /* List/Array node */` |
|     429319 | 1026 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1027 | `				 /* Assume a literal */` |
|        ! 0 | 1028 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1029 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1030 | `			 }else{` |
|     429319 | 1031 | `				 pCur += 2;` |
|          - | 1032 | `				 /* Collect array/list tokens */` |
|     429319 | 1033 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     429319 | 1034 | `				 if( pCur < pGen->pEnd ){` |
|     429317 | 1035 | `					 pCur++;` |
|     214661 | 1036 | `				 }else{` |
|          - | 1037 | `					 /* Syntax error */` |
|          4 | 1038 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          1 | 1039 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|          3 | 1040 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1041 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1042 | `					 }` |
|          3 | 1043 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1044 | `					 return rc;` |
|          - | 1045 | `				 }` |
|     429317 | 1046 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     429317 | 1047 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|     714704 | 1060 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1061 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15937 | 1062 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15937 | 1063 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1064 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1065 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15937 | 1066 | `			 pNode->xCode = PH7_CompileYield;` |
|     492083 | 1067 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     483824 | 1068 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7864 | 1069 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3947 | 1070 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1071 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|        647 | 1072 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1073 | `				 /* Assume a literal */` |
|        ! 0 | 1074 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1075 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1076 | `			 }else{` |
|          - | 1077 | `				 /* Assemble annonymous functions body */` |
|        647 | 1078 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|        647 | 1079 | `				 if( rc != SXRET_OK ){` |
|         28 | 1080 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1081 | `					 return rc;` |
|          - | 1082 | `				 }` |
|        623 | 1083 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1084 | `			  }` |
|     483784 | 1085 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         43 | 1086 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         24 | 1087 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         12 | 1088 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|          9 | 1089 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1090 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1091 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1092 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1093 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         34 | 1094 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         34 | 1095 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1096 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1097 | `				 return rc;` |
|          - | 1098 | `			 }` |
|         34 | 1099 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     483459 | 1100 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     483180 | 1101 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7840 | 1102 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3923 | 1103 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1104 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        551 | 1105 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        551 | 1106 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1107 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1108 | `				 return rc;` |
|          - | 1109 | `			 }` |
|        551 | 1110 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     483172 | 1111 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1112 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         78 | 1113 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         78 | 1114 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1115 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1116 | `				 return rc;` |
|          - | 1117 | `			 }` |
|         78 | 1118 | `			 pNode->xCode = PH7_CompileMatch;` |
|     482862 | 1119 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1120 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1121 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1122 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1123 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1124 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1125 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1126 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1127 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     482807 | 1128 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1129 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         73 | 1130 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         73 | 1131 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         39 | 1132 | `		 }else{` |
|          - | 1133 | `			 /* Assume a literal */` |
|     482721 | 1134 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     482721 | 1135 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1136 | `		 }` |
|   42234410 | 1137 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1138 | `		 /* Constants,function name,namespace path,class name... */` |
|   13513413 | 1139 | `		 if( bAfterMemberOp ){` |
|          - | 1140 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1141 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1142 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1143 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    5631007 | 1144 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    2815501 | 1145 | `		 }` |
|   13513413 | 1146 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   13513413 | 1147 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    6756709 | 1148 | `	 }else{` |
|   28192093 | 1149 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1150 | `			 /* Point to the code generator routine */` |
|    9526953 | 1151 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    9526953 | 1152 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1153 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1154 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1155 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1156 | `				 }` |
|          3 | 1157 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1158 | `				 return rc;` |
|          - | 1159 | `			 }` |
|    4763473 | 1160 | `		 }` |
|          - | 1161 | `		/* Advance the stream cursor */` |
|   28192091 | 1162 | `		pCur++;` |
|          - | 1163 | `	 }` |
|          - | 1164 | `	/* Point to the end of the token stream */` |
|   90243603 | 1165 | `	pNode->pEnd = pCur;` |
|          - | 1166 | `	/* Save the node for later processing */` |
|   90243603 | 1167 | `	*ppNode = pNode;` |
|          - | 1168 | `	/* Synchronize cursors */` |
|   90243603 | 1169 | `	pGen->pIn = pCur;` |
|   90243603 | 1170 | `	return SXRET_OK;` |
|   45123938 | 1171 | `}` |
|          - | 1172 | `/*` |
|          - | 1173 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1174 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1175 | ` * level is zero.` |
|          - | 1176 | ` */` |
|    1940764 | 1177 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1178 | `{` |
|    1940769 | 1179 | `	SyToken *pCur = pStart;` |
|    1940769 | 1180 | `	sxi32 iNest = 0;` |
|    1940769 | 1181 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1182 | `		/* Last expression */` |
|     716723 | 1183 | `		return SXERR_EOF;` |
|          - | 1184 | `	}` |
|    4644899 | 1185 | `	while( pCur < pEnd ){` |
|    4342819 | 1186 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     921971 | 1187 | `			break;` |
|          - | 1188 | `		}` |
|    3420853 | 1189 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     241785 | 1190 | `			iNest++;` |
|    3299963 | 1191 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     241789 | 1192 | `			iNest--;` |
|     120892 | 1193 | `		}` |
|    3420853 | 1194 | `		pCur++;` |
|          5 | 1195 | `	}` |
|    1224051 | 1196 | `	*ppNext = pCur;` |
|    1224051 | 1197 | `	return SXRET_OK;` |
|     970387 | 1198 | `}` |
|          - | 1199 | `/*` |
|          - | 1200 | ` * Free an expression tree.` |
|          - | 1201 | ` */` |
|   77130908 | 1202 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1203 | `{` |
|   77130913 | 1204 | `	if( pNode->pLeft ){` |
|          - | 1205 | `		/* Release the left tree */` |
|   30147895 | 1206 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   15073945 | 1207 | `	}` |
|   77130913 | 1208 | `	if( pNode->pRight ){` |
|          - | 1209 | `		/* Release the right tree */` |
|   17536107 | 1210 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    8768051 | 1211 | `	}` |
|   77130913 | 1212 | `	if( pNode->pCond ){` |
|          - | 1213 | `		/* Release the conditional tree used by the ternary operator */` |
|     504557 | 1214 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     252276 | 1215 | `	}` |
|   77130913 | 1216 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1217 | `		ph7_expr_node **apArg;` |
|          - | 1218 | `		sxu32 n;` |
|          - | 1219 | `		/* Release node arguments */` |
|    8312271 | 1220 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   19132921 | 1221 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   10820655 | 1222 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    5410330 | 1223 | `		}` |
|    8312271 | 1224 | `		SySetRelease(&pNode->aNodeArgs);` |
|    4156133 | 1225 | `	}` |
|          - | 1226 | `	/* Finally,release this node */` |
|   77130913 | 1227 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   77130913 | 1228 | `}` |
|          - | 1229 | `/*` |
|          - | 1230 | ` * Free an expression tree.` |
|          - | 1231 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1232 | ` */` |
|   17129430 | 1233 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1234 | `{` |
|          - | 1235 | `	ph7_expr_node **apNode;` |
|          - | 1236 | `	sxu32 n;` |
|   17129435 | 1237 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  107373085 | 1238 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   90243655 | 1239 | `		if( apNode[n] ){` |
|   17129771 | 1240 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    8564883 | 1241 | `		}` |
|   45121830 | 1242 | `	}` |
|   17129435 | 1243 | `	return SXRET_OK;` |
|          5 | 1244 | `}` |
|          - | 1245 | `/*` |
|          - | 1246 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1247 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1248 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1249 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1250 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1251 | ` */` |
|   21334782 | 1252 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1253 | `{` |
|   21334787 | 1254 | `	if( pNode == 0 ){` |
|   13290859 | 1255 | `		return 0;` |
|          - | 1256 | `	}` |
|    8043933 | 1257 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1258 | `		return 1;` |
|          - | 1259 | `	}` |
|    8043921 | 1260 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1261 | `		return 1;` |
|          - | 1262 | `	}` |
|    8043917 | 1263 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1264 | `		return 1;` |
|          - | 1265 | `	}` |
|    8043917 | 1266 | `	return 0;` |
|   10667396 | 1267 | `}` |
|          - | 1268 | `/*` |
|          - | 1269 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1270 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1271 | ` */` |
|    5223562 | 1272 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1273 | `{` |
|          - | 1274 | `	sxi32 iExprOp;` |
|    5223567 | 1275 | `	if( pNode->pOp == 0 ){` |
|    3725713 | 1276 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1277 | `	}` |
|    1497859 | 1278 | `	iExprOp = pNode->pOp->iOp;` |
|    1497859 | 1279 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     960689 | 1280 | `			return TRUE;` |
|          - | 1281 | `	}` |
|     537175 | 1282 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     537161 | 1283 | `		if( pNode->pLeft->pOp ) {` |
|     128322 | 1284 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      54440 | 1285 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1286 | `				return FALSE;` |
|          5 | 1287 | `			}` |
|     473000 | 1288 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1289 | `			return FALSE;` |
|          - | 1290 | `		}` |
|     537161 | 1291 | `		return TRUE;` |
|          - | 1292 | `	}` |
|         16 | 1293 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1294 | `		return TRUE;` |
|          - | 1295 | `	}` |
|          - | 1296 | `	/* Not a modifiable l or r-value */` |
|          9 | 1297 | `	return FALSE;` |
|    2611786 | 1298 | `}` |
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
|    5519014 | 1310 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1311 | `{` |
|          - | 1312 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1313 | `	sxi32 rc;` |
|          - | 1314 | `	/* Process function arguments from left to right */` |
|    5519019 | 1315 | `	iCur = 0;` |
|    6773193 | 1316 | `	for(;;){` |
|   13546391 | 1317 | `		if( iCur >= nToken ){` |
|          - | 1318 | `			/* No more arguments to process */` |
|    5518991 | 1319 | `			break;` |
|          - | 1320 | `		}` |
|    8027405 | 1321 | `		iNode = iCur;` |
|    8027405 | 1322 | `		iNest = 0;` |
|   26298569 | 1323 | `		while( iCur < nToken ){` |
|   20779581 | 1324 | `			if( apNode[iCur] ){` |
|   20732643 | 1325 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1254211 | 1326 | `					break;` |
|   18224226 | 1327 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    9757749 | 1328 | `					&& apNode[iCur]->pLeft == 0` |
|    1291265 | 1329 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1288682 | 1330 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1331 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1332 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1333 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1334 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1335 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1336 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|          - | 1337 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|          - | 1338 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|          - | 1339 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|          - | 1340 | `					 * and swallow the following argument separator. */` |
|    1286101 | 1341 | `					iNest++;` |
|   17581183 | 1342 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    9112118 | 1343 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    1286101 | 1344 | `					iNest--;` |
|     643048 | 1345 | `				}` |
|    9112113 | 1346 | `			}` |
|   18271169 | 1347 | `			iCur++;` |
|          5 | 1348 | `		}` |
|    8027405 | 1349 | `		if( iCur > iNode ){` |
|    8027399 | 1350 | `			SyString sArgName = {0, 0};` |
|          - | 1351 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1352 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1353 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    8027394 | 1354 | `			if( (iCur - iNode) >= 2` |
|    5584394 | 1355 | `				&& apNode[iNode]` |
|    3141371 | 1356 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1694621 | 1357 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     247619 | 1358 | `				&& apNode[iNode+1]` |
|     247349 | 1359 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1360 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        303 | 1361 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        303 | 1362 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        303 | 1363 | `				apNode[iNode] = 0;` |
|        303 | 1364 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        303 | 1365 | `				apNode[iNode+1] = 0;` |
|        303 | 1366 | `				iNode += 2;` |
|          - | 1367 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1368 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        303 | 1369 | `				if( iNode >= iCur ){` |
|          4 | 1370 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1371 | `						pOp->pStart->nLine,` |
|          - | 1372 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1373 | `						&sArgName);` |
|          3 | 1374 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1375 | `						rc = SXERR_SYNTAX;` |
|          1 | 1376 | `					}` |
|          3 | 1377 | `					return rc;` |
|          - | 1378 | `				}` |
|        148 | 1379 | `			}` |
|    8027392 | 1380 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1381 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1382 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1383 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1384 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1385 | `					apNode[iNode] = 0;` |
|        ! 0 | 1386 | `			}` |
|          - | 1387 | `			{` |
|          - | 1388 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1389 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1390 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1391 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1392 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1393 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1394 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1395 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1396 | `				 * NULL slots ahead of the flagged subtree. */` |
|    8027397 | 1397 | `				int bSpreadArg = 0;` |
|          - | 1398 | `				sxi32 iScan;` |
|    8027443 | 1399 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    8027443 | 1400 | `					if( apNode[iScan] ){` |
|    8027397 | 1401 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    8027397 | 1402 | `						break;` |
|          - | 1403 | `					}` |
|         26 | 1404 | `				}` |
|    8027397 | 1405 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    8027397 | 1406 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4089 | 1407 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2042 | 1408 | `				}` |
|          - | 1409 | `			}` |
|    8027397 | 1410 | `			if( apNode[iNode] ){` |
|    8027397 | 1411 | `				if( sArgName.nByte > 0 ){` |
|        300 | 1412 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        300 | 1413 | `					apNode[iNode]->sArgName = sArgName;` |
|        148 | 1414 | `				}` |
|          - | 1415 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    8027397 | 1416 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    4013701 | 1417 | `			}else{` |
|          - | 1418 | `				/* No expression before comma */` |
|        ! 0 | 1419 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1420 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1421 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1422 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1423 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1424 | `				}` |
|        ! 0 | 1425 | `				return rc;` |
|          - | 1426 | `			}` |
|    4013701 | 1427 | `		}else{` |
|          - | 1428 | `			/* Comma with no preceding argument */` |
|          8 | 1429 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1430 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1431 | `				rc = SXERR_SYNTAX;` |
|          3 | 1432 | `			}` |
|          8 | 1433 | `			return rc;` |
|          - | 1434 | `		}` |
|          - | 1435 | `		/* Jump trailing comma */` |
|    8027397 | 1436 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2508411 | 1437 | `			iCur++;` |
|    2508411 | 1438 | `			if( iCur >= nToken ){` |
|          - | 1439 | `				/* Trailing comma after last argument */` |
|         21 | 1440 | `				break;` |
|          - | 1441 | `			}` |
|    1254193 | 1442 | `		}` |
|          5 | 1443 | `	}` |
|    5519011 | 1444 | `	return SXRET_OK;` |
|    2759512 | 1445 | `}` |
|          - | 1446 | ` /*` |
|          - | 1447 | `  * Create an expression tree from an array of tokens.` |
|          - | 1448 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1449 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1450 | `  */` |
|   29606596 | 1451 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1452 | ` {` |
|          - | 1453 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1454 | `	 ph7_expr_node *pNode;` |
|          - | 1455 | `	 ph7_expr_node *pSuppress;` |
|          - | 1456 | `	 sxi32 iCur;` |
|          - | 1457 | `	 sxi32 rc;` |
|   29606601 | 1458 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1459 | `		 /* TICKET 1433-17: self evaluating node */` |
|   13010449 | 1460 | `		 return SXRET_OK;` |
|          - | 1461 | `	 }` |
|          - | 1462 | `	 /* Process expressions enclosed in parenthesis first */` |
|  118104553 | 1463 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1464 | `		 sxi32 iNest;` |
|          - | 1465 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1466 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1467 | `		  */` |
|  101508403 | 1468 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  101016619 | 1469 | `			 continue;` |
|          - | 1470 | `		 }` |
|     491789 | 1471 | `		 iNest = 1;` |
|     491789 | 1472 | `		 iLeft = iCur;` |
|          - | 1473 | `		 /* Find the closing parenthesis */` |
|     491789 | 1474 | `		 iCur++;` |
|    4383401 | 1475 | `		 while( iCur < nToken ){` |
|    4383401 | 1476 | `			 if( apNode[iCur] ){` |
|    4383401 | 1477 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1478 | `					 /* Decrement nesting level */` |
|     718443 | 1479 | `					 iNest--;` |
|     718443 | 1480 | `					 if( iNest <= 0 ){` |
|     491789 | 1481 | `						 break;` |
|          5 | 1482 | `					 }` |
|    3778290 | 1483 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1484 | `					 /* Increment nesting level */` |
|     226659 | 1485 | `					 iNest++;` |
|     113327 | 1486 | `				 }` |
|    1945806 | 1487 | `			 }` |
|    3891617 | 1488 | `			 iCur++;` |
|          5 | 1489 | `		 }` |
|     491789 | 1490 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1491 | `			 sxi32 j;` |
|          - | 1492 | `			 /* Recurse and process this expression */` |
|     491789 | 1493 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     491789 | 1494 | `			 if( rc != SXRET_OK ){` |
|          3 | 1495 | `				 return rc;` |
|          - | 1496 | `			 }` |
|          - | 1497 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1498 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1499 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1500 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1501 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1502 | `			  * group's free below silently drops the unpacking. */` |
|     491787 | 1503 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     491787 | 1504 | `				 if( apNode[j] ){` |
|     491787 | 1505 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     491782 | 1506 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     491787 | 1507 | `					 break;` |
|          - | 1508 | `				 }` |
|        ! 0 | 1509 | `			 }` |
|     245891 | 1510 | `		 }` |
|          - | 1511 | `		 /* Free the left and right nodes */` |
|     491787 | 1512 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     491787 | 1513 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     491787 | 1514 | `		 apNode[iLeft] = 0;` |
|     491787 | 1515 | `		 apNode[iCur] = 0;` |
|     245896 | 1516 | `	 }` |
|          - | 1517 | `	  /* Process expressions enclosed in braces */` |
|  122168611 | 1518 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1519 | `		 sxi32 iNest;` |
|          - | 1520 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1521 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1522 | `		  */` |
|  105884001 | 1523 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  105880107 | 1524 | `			 continue;` |
|          - | 1525 | `		 }` |
|       3899 | 1526 | `		 iNest = 1;` |
|       3899 | 1527 | `		 iLeft = iCur;` |
|          - | 1528 | `		 /* Find the closing parenthesis */` |
|       3899 | 1529 | `		 iCur++;` |
|       7791 | 1530 | `		 while( iCur < nToken ){` |
|       7791 | 1531 | `			 if( apNode[iCur] ){` |
|       7791 | 1532 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1533 | `					 /* Decrement nesting level */` |
|       3899 | 1534 | `					 iNest--;` |
|       3899 | 1535 | `					 if( iNest <= 0 ){` |
|       3899 | 1536 | `						 break;` |
|        ! 0 | 1537 | `					 }` |
|       3897 | 1538 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1539 | `					 /* Increment nesting level */` |
|        ! 0 | 1540 | `					 iNest++;` |
|        ! 0 | 1541 | `				 }` |
|       1946 | 1542 | `			 }` |
|       3897 | 1543 | `			 iCur++;` |
|          5 | 1544 | `		 }` |
|       3899 | 1545 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1546 | `			 /* Recurse and process this expression */` |
|       3897 | 1547 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3897 | 1548 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1549 | `				 return rc;` |
|          - | 1550 | `			 }` |
|       1946 | 1551 | `		 }` |
|          - | 1552 | `		 /* Free the left and right nodes */` |
|       3899 | 1553 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3899 | 1554 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3899 | 1555 | `		 apNode[iLeft] = 0;` |
|       3899 | 1556 | `		 apNode[iCur] = 0;` |
|       1952 | 1557 | `	 }` |
|          - | 1558 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   16284615 | 1559 | `	 iLeft = -1;` |
|  122176361 | 1560 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105891763 | 1561 | `		 if( apNode[iCur] == 0 ){` |
|   46758651 | 1562 | `			 continue;` |
|          - | 1563 | `		 }` |
|   59133117 | 1564 | `		 pNode = apNode[iCur];` |
|   59133117 | 1565 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   16100361 | 1566 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1567 | `				 /* Collect function arguments */` |
|    7069059 | 1568 | `				 sxi32 iPtr = 0;` |
|    7069059 | 1569 | `				 sxi32 nFuncTok = 0;` |
|   34917691 | 1570 | `				 while( nFuncTok + iCur < nToken ){` |
|   34917691 | 1571 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|          - | 1572 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|          - | 1573 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|          - | 1574 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|          - | 1575 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|          - | 1576 | `					  * nulled, so counting it here would over-count and never find` |
|          - | 1577 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   34917691 | 1578 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   34859045 | 1579 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    7530689 | 1580 | `							 iPtr++;` |
|   31093703 | 1581 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    7530689 | 1582 | `							 iPtr--;` |
|    7530689 | 1583 | `							 if( iPtr <= 0 ){` |
|    7069059 | 1584 | `								 break;` |
|          - | 1585 | `							 }` |
|     230815 | 1586 | `						 }` |
|   13894993 | 1587 | `					 }` |
|   27848637 | 1588 | `					 nFuncTok++;` |
|          5 | 1589 | `				 }` |
|    7069059 | 1590 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1591 | `					 /* Syntax error */` |
|        ! 0 | 1592 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1593 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1594 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1595 | `					 }` |
|        ! 0 | 1596 | `					 return rc;` |
|          - | 1597 | `				 }` |
|    7069059 | 1598 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1599 | `					 /* Syntax error */` |
|        ! 0 | 1600 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1601 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1602 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1603 | `					 }` |
|        ! 0 | 1604 | `					 return rc;` |
|          - | 1605 | `				 }` |
|    7069059 | 1606 | `				 if( nFuncTok > 1 ){` |
|          - | 1607 | `					 /* Process function arguments */` |
|    5519019 | 1608 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    5519019 | 1609 | `					 if( rc != SXRET_OK ){` |
|         11 | 1610 | `						 return rc;` |
|          - | 1611 | `					 }` |
|    2759503 | 1612 | `				 }` |
|          - | 1613 | `				 /* Link the node to the tree */` |
|    7069051 | 1614 | `				 pNode->pLeft = apNode[iLeft];` |
|    7069051 | 1615 | `				 apNode[iLeft] = 0;` |
|   34917659 | 1616 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   27848613 | 1617 | `					 apNode[iCur+iPtr] = 0;` |
|   13924309 | 1618 | `				 }` |
|          - | 1619 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1620 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1621 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1622 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1623 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1624 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1625 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1626 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1627 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1628 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1629 | `				 {` |
|    7069051 | 1630 | `					 sxi32 iNew = iLeft - 1;` |
|    9136589 | 1631 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    2067543 | 1632 | `						 iNew--;` |
|          5 | 1633 | `					 }` |
|    7069046 | 1634 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3985870 | 1635 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2414990 | 1636 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     857723 | 1637 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     857723 | 1638 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     857723 | 1639 | `						 apNode[iNew] = 0;` |
|     857723 | 1640 | `						 pNode = apNode[iCur];` |
|     428864 | 1641 | `					 }` |
|          - | 1642 | `				 }` |
|   12565830 | 1643 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1644 | `				 /* Subscripting */` |
|    3030755 | 1645 | `				 sxi32 iArrTok = iCur + 1;` |
|    3030755 | 1646 | `				 sxi32 iNest = 1;` |
|    3030750 | 1647 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         34 | 1648 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         28 | 1649 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|          - | 1650 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|          - | 1651 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|          - | 1652 | `					  * subscripting a global constant raised "Invalid array name" while` |
|          - | 1653 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|          - | 1654 | `					  * worked. */` |
|         22 | 1655 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|         16 | 1656 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    3030750 | 1657 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1658 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1659 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     322822 | 1660 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1661 | `						 /* Syntax error */` |
|        ! 0 | 1662 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1663 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1664 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1665 | `						 }` |
|        ! 0 | 1666 | `						 return rc;` |
|          - | 1667 | `				 }` |
|          - | 1668 | `				 /* Collect index tokens */` |
|    6368199 | 1669 | `				 while( iArrTok < nToken ){` |
|    6368199 | 1670 | `					 if( apNode[iArrTok] ){` |
|    6368167 | 1671 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1672 | `							 /* Increment nesting level */` |
|      27207 | 1673 | `							 iNest++;` |
|    6354566 | 1674 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1675 | `							 /* Decrement nesting level */` |
|    3057957 | 1676 | `							 iNest--;` |
|    3057957 | 1677 | `							 if( iNest <= 0 ){` |
|    3030755 | 1678 | `								 break;` |
|          - | 1679 | `							 }` |
|      13601 | 1680 | `						 }` |
|    1668706 | 1681 | `					 }` |
|    3337449 | 1682 | `					 ++iArrTok;` |
|          5 | 1683 | `				 }` |
|    3030755 | 1684 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1685 | `					 /* Recurse and process this expression */` |
|    2793263 | 1686 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2793263 | 1687 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1688 | `						 return rc;` |
|          - | 1689 | `					 }` |
|          - | 1690 | `					 /* Link the node to it's index */` |
|    2793263 | 1691 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1396629 | 1692 | `				 }` |
|          - | 1693 | `				 /* Link the node to the tree */` |
|    3030755 | 1694 | `				 pNode->pLeft = apNode[iLeft];` |
|    3030755 | 1695 | `				 pNode->pRight = 0;` |
|    3030755 | 1696 | `				 apNode[iLeft] = 0;` |
|    9398949 | 1697 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6368199 | 1698 | `					 apNode[iNest] = 0;` |
|    3184102 | 1699 | `				 }` |
|    1515380 | 1700 | `			 }else{` |
|          - | 1701 | `				 /* Member access operators [i.e: '->','::'] */` |
|    6000557 | 1702 | `				  iRight = iCur + 1;` |
|    6004449 | 1703 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3897 | 1704 | `					 iRight++;` |
|          5 | 1705 | `				 }` |
|    6000557 | 1706 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1707 | `					 /* Syntax error */` |
|          5 | 1708 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1709 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1710 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1711 | `					 }` |
|          5 | 1712 | `					 return rc;` |
|          - | 1713 | `				 }` |
|          - | 1714 | `				 /* Link the node to the tree */` |
|    6000553 | 1715 | `				 pNode->pLeft = apNode[iLeft];` |
|    6000548 | 1716 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    5790168 | 1717 | `					 && pNode->pLeft->pOp == 0 &&` |
|    5489635 | 1718 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1719 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1720 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1721 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1722 | `					  * accepts. */` |
|          4 | 1723 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1724 | `						 /* Syntax error */` |
|        ! 0 | 1725 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1726 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1727 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1728 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1729 | `						 }` |
|        ! 0 | 1730 | `						 return rc;` |
|          - | 1731 | `				 }` |
|    6000553 | 1732 | `				 pNode->pRight = apNode[iRight];` |
|    6000553 | 1733 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1734 | `			 }` |
|    8050172 | 1735 | `		 }` |
|   59133105 | 1736 | `		 iLeft = iCur;` |
|   29566555 | 1737 | `	 }` |
|          - | 1738 | `	 /* Handle left associative (new, clone) operators */` |
|  122176329 | 1739 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105891731 | 1740 | `		 if( apNode[iCur] == 0 ){` |
|   63779361 | 1741 | `			 continue;` |
|          - | 1742 | `		 }` |
|   42112375 | 1743 | `		 pNode = apNode[iCur];` |
|   42112375 | 1744 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1745 | `			 SyToken *pToken;` |
|          - | 1746 | `			 /* Get the left node */` |
|      62653 | 1747 | `			 iLeft = iCur + 1;` |
|      62661 | 1748 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1749 | `				 iLeft++;` |
|          1 | 1750 | `			 }` |
|      62653 | 1751 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1752 | `				  /* Syntax error */` |
|        ! 0 | 1753 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1754 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1755 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1756 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1757 | `				 }` |
|        ! 0 | 1758 | `				 return rc;` |
|          - | 1759 | `			 }` |
|          - | 1760 | `			 /* Make sure the operand are of a valid type */` |
|      62653 | 1761 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1762 | `				 /* Clone:` |
|          - | 1763 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1764 | `				  *  ++ function call (including annonymous)` |
|          - | 1765 | `				  *  ++ array member` |
|          - | 1766 | `				  *  ++ 'new' operator` |
|          - | 1767 | `				  * Example:` |
|          - | 1768 | `				  *   clone $pObj;` |
|          - | 1769 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1770 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1771 | `				  */` |
|      58341 | 1772 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      58335 | 1773 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1774 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1775 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1776 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1777 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1778 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1779 | `						 }` |
|        ! 0 | 1780 | `						 return rc;` |
|          - | 1781 | `					 }` |
|      29165 | 1782 | `				 }` |
|      29173 | 1783 | `			 }else{` |
|          - | 1784 | `				 /* New */` |
|       4312 | 1785 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1786 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1787 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1788 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1789 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1790 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1791 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1792 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1793 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1794 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1795 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1796 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1797 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1798 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1799 | `					 }` |
|        ! 0 | 1800 | `					 return rc;` |
|          - | 1801 | `				 }` |
|       4317 | 1802 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       4317 | 1803 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       4312 | 1804 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         35 | 1805 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1806 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1807 | `						 /* Syntax error */` |
|        ! 0 | 1808 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1809 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1810 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1811 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1812 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1813 | `						 }` |
|        ! 0 | 1814 | `						 return rc;` |
|          - | 1815 | `					 }` |
|       2156 | 1816 | `				 }` |
|          - | 1817 | `			 }` |
|          - | 1818 | `			  /* Link the node to the tree */` |
|      62653 | 1819 | `			 pNode->pLeft = apNode[iLeft];` |
|      62653 | 1820 | `			 apNode[iLeft] = 0;` |
|      62653 | 1821 | `			 pNode->pRight = 0; /* Paranoid */` |
|      31324 | 1822 | `		 }` |
|   21056190 | 1823 | `	 }` |
|          - | 1824 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   16284603 | 1825 | `	 iLeft = -1;` |
|  122176329 | 1826 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105735961 | 1827 | `		 if( apNode[iCur] == 0 ){` |
|   63779361 | 1828 | `			 continue;` |
|          - | 1829 | `		 }` |
|   41956605 | 1830 | `		 pNode = apNode[iCur];` |
|   41956605 | 1831 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     373839 | 1832 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     186923 | 1833 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1834 | `					 /* Link the node to the tree */` |
|     202497 | 1835 | `					 pNode->pLeft = apNode[iLeft];` |
|     202497 | 1836 | `					 apNode[iLeft] = 0;` |
|     101246 | 1837 | `			 }` |
|     436138 | 1838 | `		  }` |
|   42112375 | 1839 | `		 iLeft = iCur;` |
|   21056190 | 1840 | `	  }` |
|   16440373 | 1841 | `	 iLeft = -1;` |
|  122332099 | 1842 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105891731 | 1843 | `		 if( apNode[iCur] == 0 ){` |
|   63981853 | 1844 | `			 continue;` |
|          - | 1845 | `		 }` |
|   41909883 | 1846 | `		 pNode = apNode[iCur];` |
|   41909883 | 1847 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15572 | 1848 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15577 | 1849 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1850 | `					 /* Syntax error */` |
|        ! 0 | 1851 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1852 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1853 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1854 | `					 }` |
|        ! 0 | 1855 | `					 return rc;` |
|          - | 1856 | `			 }` |
|          - | 1857 | `			 /* Link the node to the tree */` |
|      15577 | 1858 | `			 pNode->pLeft = apNode[iLeft];` |
|      15577 | 1859 | `			 apNode[iLeft] = 0;` |
|          - | 1860 | `			 /* Mark as pre-increment/decrement node */` |
|      15577 | 1861 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7786 | 1862 | `		  }` |
|   41909883 | 1863 | `		 iLeft = iCur;` |
|   20954944 | 1864 | `	 }` |
|          - | 1865 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1866 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1867 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1868 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1869 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1870 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1871 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1872 | `	  * pass below skips it (pLeft != 0). */` |
|   16440373 | 1873 | `	 iLeft = -1;` |
|  122332099 | 1874 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105891731 | 1875 | `		 if( apNode[iCur] == 0 ){` |
|   64079339 | 1876 | `			 continue;` |
|          - | 1877 | `		 }` |
|   41812397 | 1878 | `		 pNode = apNode[iCur];` |
|   41812397 | 1879 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      81919 | 1880 | `			 iRight = iCur + 1;` |
|      81919 | 1881 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1882 | `				 iRight++;` |
|        ! 0 | 1883 | `			 }` |
|      81919 | 1884 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1885 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1886 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1887 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1888 | `				 }` |
|        ! 0 | 1889 | `				 return rc;` |
|          - | 1890 | `			 }` |
|      81919 | 1891 | `			 pNode->pLeft = apNode[iLeft];` |
|      81919 | 1892 | `			 pNode->pRight = apNode[iRight];` |
|      81919 | 1893 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      40957 | 1894 | `		 }` |
|   41812397 | 1895 | `		 iLeft = iCur;` |
|   20906201 | 1896 | `	 }` |
|          - | 1897 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   16440373 | 1898 | `	  iLeft = 0;` |
|  122332093 | 1899 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105891727 | 1900 | `		  if( apNode[iCur] ){` |
|   41730479 | 1901 | `			  pNode = apNode[iCur];` |
|   41730479 | 1902 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1377527 | 1903 | `				  if( iLeft > 0 ){` |
|          - | 1904 | `					  /* Link the node to the tree */` |
|    1377525 | 1905 | `					  pNode->pLeft = apNode[iLeft];` |
|    1377525 | 1906 | `					  apNode[iLeft] = 0;` |
|    1377525 | 1907 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      54519 | 1908 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1909 | `							   /* Syntax error */` |
|        ! 0 | 1910 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1911 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 1912 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 1913 | `							  }` |
|        ! 0 | 1914 | `							  return rc;` |
|          - | 1915 | `						  }` |
|      27257 | 1916 | `					  }` |
|     688765 | 1917 | `				  }else{` |
|          - | 1918 | `					  /* Syntax error */` |
|          3 | 1919 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 1920 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 1921 | `						  rc = SXERR_SYNTAX;` |
|          1 | 1922 | `					  }` |
|          3 | 1923 | `					  return rc;` |
|          - | 1924 | `				  }` |
|     688760 | 1925 | `			  }` |
|          - | 1926 | `			  /* Save terminal position */` |
|   41730477 | 1927 | `			  iLeft = iCur;` |
|   20865236 | 1928 | `		  }` |
|   52945865 | 1929 | `	  }` |
|          - | 1930 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 1931 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 1932 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 1933 | `	  * yielding a right-leaning tree. */` |
|  122332091 | 1934 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  105891725 | 1935 | `		 if( apNode[iCur] == 0 ){` |
|   65538887 | 1936 | `			 continue;` |
|          - | 1937 | `		 }` |
|   40352843 | 1938 | `		 pNode = apNode[iCur];` |
|   40352843 | 1939 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 1940 | `			 sxi32 iL, iR;` |
|          - | 1941 | `			 /* Find the right operand */` |
|        115 | 1942 | `			 iR = -1;` |
|          - | 1943 | `			 {` |
|          - | 1944 | `				 sxi32 j;` |
|        127 | 1945 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 1946 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 1947 | `				 }` |
|          - | 1948 | `			 }` |
|          - | 1949 | `			 /* Find the left operand */` |
|        115 | 1950 | `			 iL = -1;` |
|          - | 1951 | `			 {` |
|          - | 1952 | `				 sxi32 j;` |
|        183 | 1953 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 1954 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 1955 | `				 }` |
|          - | 1956 | `			 }` |
|        115 | 1957 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 1958 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1959 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1960 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1961 | `				 }` |
|        ! 0 | 1962 | `				 return rc;` |
|          - | 1963 | `			 }` |
|        115 | 1964 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 1965 | `			 pNode->pRight = apNode[iR];` |
|        115 | 1966 | `			 apNode[iL] = 0;` |
|        115 | 1967 | `			 apNode[iR] = 0;` |
|          - | 1968 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 1969 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 1970 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 1971 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 1972 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 1973 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 1974 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 1975 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 1976 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 1977 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 1978 | `			  * operands are respected. */` |
|        114 | 1979 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 1980 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 1981 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 1982 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 1983 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 1984 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 1985 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 1986 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 1987 | `				 while( pTail->pLeft` |
|         34 | 1988 | `					 && pTail->pLeft->pOp` |
|         23 | 1989 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 1990 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 1991 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 1992 | `					 pTail = pTail->pLeft;` |
|          1 | 1993 | `				 }` |
|          - | 1994 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 1995 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 1996 | `				 pTail->pLeft = pNode;` |
|         27 | 1997 | `				 apNode[iCur] = pHead;` |
|         13 | 1998 | `			 }` |
|         57 | 1999 | `		 }` |
|   20176424 | 2000 | `	 }` |
|          - | 2001 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  180843945 | 2002 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  164403589 | 2003 | `		 iLeft = -1;` |
| 1223320495 | 2004 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 1058916921 | 2005 | `			 if( apNode[iCur] == 0 ){` |
|  724257757 | 2006 | `				 continue;` |
|          - | 2007 | `			 }` |
|  334659169 | 2008 | `			 pNode = apNode[iCur];` |
|  334659169 | 2009 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2010 | `				 /* Get the right node */` |
|    5725527 | 2011 | `				 iRight = iCur + 1;` |
|    8675007 | 2012 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2949485 | 2013 | `					 iRight++;` |
|          5 | 2014 | `				 }` |
|    5725527 | 2015 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2016 | `					 /* Syntax error */` |
|         10 | 2017 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 2018 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2019 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2020 | `					 }` |
|         10 | 2021 | `					 return rc;` |
|          - | 2022 | `				 }` |
|    5725519 | 2023 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2024 | `					 sxi32  iTmp;` |
|          - | 2025 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2026 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2027 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2028 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2029 | `					  * is swapped below. */` |
|         75 | 2030 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2031 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2032 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2033 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2034 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2035 | `						 }` |
|          3 | 2036 | `						 return rc;` |
|          - | 2037 | `					 }` |
|          - | 2038 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|          - | 2039 | `					  * reference target — ExprIsModifiableValue already accepts` |
|          - | 2040 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|          - | 2041 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|          - | 2042 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|          - | 2043 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|         73 | 2044 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2045 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2046 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2047 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2048 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2049 | `						 }` |
|        ! 0 | 2050 | `						 return rc;` |
|          - | 2051 | `					 }` |
|         73 | 2052 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         55 | 2053 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2054 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2055 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2056 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2057 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2058 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2059 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2060 | `									 }` |
|        ! 0 | 2061 | `									 return rc;` |
|          - | 2062 | `							 }` |
|        ! 0 | 2063 | `						 }` |
|         26 | 2064 | `					 }` |
|          - | 2065 | `					 /* Swap operands */` |
|         73 | 2066 | `					 iTmp = iRight;` |
|         73 | 2067 | `					 iRight = iLeft;` |
|         73 | 2068 | `					 iLeft = iTmp;` |
|         35 | 2069 | `				 }` |
|          - | 2070 | `				 /* Link the node to the tree */` |
|    5725517 | 2071 | `				 pNode->pLeft = apNode[iLeft];` |
|    5725517 | 2072 | `				 pNode->pRight = apNode[iRight];` |
|    5725517 | 2073 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2862756 | 2074 | `			 }` |
|  334659159 | 2075 | `			 iLeft = iCur;` |
|  167329582 | 2076 | `		 }` |
|   82201792 | 2077 | `	 }` |
|          - | 2078 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2079 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2080 | `	  * we are dealing with a single operator.` |
|          - | 2081 | `	  */` |
|   16440361 | 2082 | `	  iLeft = -1;` |
|  118167647 | 2083 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  102231845 | 2084 | `		  if( apNode[iCur] == 0 ){` |
|   74839903 | 2085 | `			  continue;` |
|          - | 2086 | `		  }` |
|   27391947 | 2087 | `		  pNode = apNode[iCur];` |
|   27391947 | 2088 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     504559 | 2089 | `			  sxi32 iNest = 1;` |
|     504559 | 2090 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2091 | `				  /* Missing condition */` |
|          3 | 2092 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2093 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2094 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2095 | `				  }` |
|          3 | 2096 | `				  return rc;` |
|          - | 2097 | `			  }` |
|          - | 2098 | `			  /* Get the right node */` |
|     504557 | 2099 | `			  iRight = iCur + 1;` |
|    2144237 | 2100 | `			  while( iRight < nToken  ){` |
|    2144237 | 2101 | `				  if( apNode[iRight] ){` |
|    1005153 | 2102 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2103 | `						  /* Increment nesting level */` |
|        ! 0 | 2104 | `						  ++iNest;` |
|    1005153 | 2105 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2106 | `						  /* Decrement nesting level */` |
|     504557 | 2107 | `						  --iNest;` |
|     504557 | 2108 | `						  if( iNest <= 0 ){` |
|     504557 | 2109 | `							  break;` |
|          - | 2110 | `						  }` |
|        ! 0 | 2111 | `					  }` |
|     250298 | 2112 | `				  }` |
|    1639685 | 2113 | `				  iRight++;` |
|          5 | 2114 | `			  }` |
|     504557 | 2115 | `			  if( iRight > iCur + 1 ){` |
|          - | 2116 | `				  /* Recurse and process the then expression */` |
|     500601 | 2117 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     500601 | 2118 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2119 | `					  return rc;` |
|          - | 2120 | `				  }` |
|          - | 2121 | `				  /* Link the node to the tree */` |
|     500601 | 2122 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     250298 | 2123 | `			  }else{` |
|          - | 2124 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2125 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2126 | `			  }` |
|     504557 | 2127 | `			  apNode[iCur + 1] = 0;` |
|     504557 | 2128 | `			  if( iRight + 1 < nToken ){` |
|          - | 2129 | `				  /* Recurse and process the else expression */` |
|     504557 | 2130 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     504557 | 2131 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2132 | `					  return rc;` |
|          - | 2133 | `				  }` |
|          - | 2134 | `				  /* Link the node to the tree */` |
|     504557 | 2135 | `				  pNode->pRight = apNode[iRight + 1];` |
|     504557 | 2136 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     252281 | 2137 | `			  }else{` |
|        ! 0 | 2138 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2139 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2140 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2141 | `				 }` |
|        ! 0 | 2142 | `				 return rc;` |
|          - | 2143 | `			  }` |
|          - | 2144 | `			  /* Point to the condition */` |
|     504557 | 2145 | `			  pNode->pCond  = apNode[iLeft];` |
|     504557 | 2146 | `			  apNode[iLeft] = 0;` |
|     504557 | 2147 | `			  break;` |
|          - | 2148 | `		  }` |
|   26887393 | 2149 | `		  iLeft = iCur;` |
|   13443699 | 2150 | `	  }` |
|          - | 2151 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2152 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2153 | `	  * so there is no need for a precedence loop here.` |
|          - | 2154 | `	  */` |
|   16440359 | 2155 | `	 iRight = -1;` |
|  122331895 | 2156 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  105891595 | 2157 | `		 if( apNode[iCur] == 0 ){` |
|   84227691 | 2158 | `			 continue;` |
|          - | 2159 | `		 }` |
|   21663909 | 2160 | `		 pNode = apNode[iCur];` |
|   21663909 | 2161 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2162 | `			 /* Get the left node */` |
|    5223485 | 2163 | `			 iLeft = iCur - 1;` |
|    7172597 | 2164 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1949117 | 2165 | `				 iLeft--;` |
|          5 | 2166 | `			 }` |
|    5223485 | 2167 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2168 | `				 /* Syntax error */` |
|         46 | 2169 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2170 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2171 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2172 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2173 | `				 }else{` |
|         42 | 2174 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2175 | `				 }` |
|         46 | 2176 | `				 if( rc != SXERR_ABORT ){` |
|         44 | 2177 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2178 | `				 }` |
|         46 | 2179 | `				 return rc;` |
|          - | 2180 | `			 }` |
|          - | 2181 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2182 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2183 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2184 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2185 | `			  * a write. */` |
|    5223443 | 2186 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2187 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2188 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2189 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2190 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2191 | `				 }` |
|         11 | 2192 | `				 return rc;` |
|          - | 2193 | `			 }` |
|          - | 2194 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2195 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2196 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2197 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2198 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    5223435 | 2199 | `			 pSuppress = 0;` |
|    5223430 | 2200 | `			 if( apNode[iLeft]->pOp` |
|    3360621 | 2201 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     748906 | 2202 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2203 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2204 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2205 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2206 | `			 }` |
|    5223435 | 2207 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2208 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2209 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2210 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2211 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2212 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2213 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        103 | 2214 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2215 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2216 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2217 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2218 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2219 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2220 | `					 }` |
|          8 | 2221 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2222 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2223 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2224 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2225 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2226 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2227 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2228 | `						 iRight = iCur;` |
|          9 | 2229 | `						 continue;` |
|          - | 2230 | `					 }` |
|        ! 0 | 2231 | `				 }` |
|        123 | 2232 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         88 | 2233 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2234 | `					 /* Left operand must be a modifiable l-value */` |
|          6 | 2235 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2236 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2237 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2238 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2239 | `					 }else{` |
|          4 | 2240 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          2 | 2241 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2242 | `					 }` |
|          6 | 2243 | `					 if( rc != SXERR_ABORT ){` |
|          6 | 2244 | `						 rc = SXERR_SYNTAX;` |
|          2 | 2245 | `					 }` |
|          6 | 2246 | `					 return rc;` |
|          - | 2247 | `				 }` |
|         43 | 2248 | `			 }` |
|          - | 2249 | `			 /* Link the node to the tree (Reverse) */` |
|    5223423 | 2250 | `			 pNode->pLeft = apNode[iRight];` |
|    5223423 | 2251 | `			 pNode->pRight = apNode[iLeft];` |
|    5223423 | 2252 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    5223423 | 2253 | `			 if( pSuppress ){` |
|          - | 2254 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2255 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2256 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2257 | `			 }` |
|    2611709 | 2258 | `		 }` |
|   21663847 | 2259 | `		 iRight = iCur;` |
|   10831926 | 2260 | `	 }` |
|          - | 2261 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   82201505 | 2262 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   65761205 | 2263 | `		 iLeft = -1;` |
|  489327293 | 2264 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  423566093 | 2265 | `			 if( apNode[iCur] == 0 ){` |
|  357804653 | 2266 | `				 continue;` |
|          - | 2267 | `			 }` |
|   65761445 | 2268 | `			 pNode = apNode[iCur];` |
|   65761445 | 2269 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2270 | `				 /* Get the right node */` |
|         47 | 2271 | `				 iRight = iCur + 1;` |
|         59 | 2272 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2273 | `					 iRight++;` |
|          1 | 2274 | `				 }` |
|         47 | 2275 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2276 | `					 /* Syntax error */` |
|        ! 0 | 2277 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2278 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2279 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2280 | `					 }` |
|        ! 0 | 2281 | `					 return rc;` |
|          - | 2282 | `				 }` |
|          - | 2283 | `				 /* Link the node to the tree */` |
|         47 | 2284 | `				 pNode->pLeft = apNode[iLeft];` |
|         47 | 2285 | `				 pNode->pRight = apNode[iRight];` |
|         47 | 2286 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         22 | 2287 | `			 }` |
|   65761445 | 2288 | `			 iLeft = iCur;` |
|   32880725 | 2289 | `		 }` |
|   32880605 | 2290 | `	 }` |
|          - | 2291 | `	 /* Point to the root of the expression tree */` |
|  105891499 | 2292 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   89451217 | 2293 | `		 if( apNode[iCur] ){` |
|   15638327 | 2294 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         23 | 2295 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|         23 | 2296 | `				  if( rc != SXERR_ABORT ){` |
|         23 | 2297 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2298 | `				  }` |
|         23 | 2299 | `				  return rc;` |
|          - | 2300 | `			 }` |
|   15638309 | 2301 | `			 apNode[0] = apNode[iCur];` |
|   15638309 | 2302 | `			 apNode[iCur] = 0;` |
|    7819152 | 2303 | `		 }` |
|   44725602 | 2304 | `	 }` |
|   16440287 | 2305 | `	 return SXRET_OK;` |
|   14725418 | 2306 | ` }` |
|          - | 2307 | ` /*` |
|          - | 2308 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2309 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2310 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2311 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2312 | `  */` |
|   17129434 | 2313 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2314 | `{` |
|          - | 2315 | `	ph7_expr_node **apNode;` |
|          - | 2316 | `	ph7_expr_node *pNode;` |
|          - | 2317 | `	sxi32 rc;` |
|          - | 2318 | `	/* Reset node container */` |
|   17129439 | 2319 | `	SySetReset(pExprNode);` |
|   17129439 | 2320 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2321 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2322 | `	{` |
|   17129439 | 2323 | `		int iLastWasTerm = 0;` |
|   17129439 | 2324 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  107373119 | 2325 | `		while( pGen->pIn < pGen->pEnd ){` |
|   90243717 | 2326 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   90243717 | 2327 | `			if( rc != SXRET_OK ){` |
|         36 | 2328 | `				return rc;` |
|          - | 2329 | `			}` |
|          - | 2330 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   90243685 | 2331 | `			if( pNode->xCode ){` |
|          - | 2332 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   45987177 | 2333 | `				iLastWasTerm = 1;` |
|   67250099 | 2334 | `			}else if( pNode->pOp ){` |
|          - | 2335 | `				/* Operator node */` |
|   25591373 | 2336 | `				iLastWasTerm = 0;` |
|   12795689 | 2337 | `			}else{` |
|          - | 2338 | `				/* Delimiter: ')' and ']' end terms */` |
|   18665145 | 2339 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2340 | `			}` |
|          - | 2341 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2342 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2343 | `			 * node kind, so this single test covers all branches. */` |
|   90243685 | 2344 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2345 | `			/* Save the extracted node */` |
|   90243685 | 2346 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2347 | `		}` |
|          - | 2348 | `	}` |
|   17129407 | 2349 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2350 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2351 | `		*ppRoot = 0;` |
|        ! 0 | 2352 | `		return SXRET_OK;` |
|          - | 2353 | `	}` |
|   17129407 | 2354 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2355 | `	/* Make sure we are dealing with valid nodes */` |
|   17129407 | 2356 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17129407 | 2357 | `	if( rc != SXRET_OK ){` |
|          - | 2358 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2359 | `		 * cleanup the mess left behind.` |
|          - | 2360 | `		 */` |
|         54 | 2361 | `		*ppRoot = 0;` |
|         54 | 2362 | `		return rc;` |
|          - | 2363 | `	}` |
|          - | 2364 | `	/* Build the tree */` |
|   17129357 | 2365 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17129357 | 2366 | `	if( rc != SXRET_OK ){` |
|          - | 2367 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        103 | 2368 | `		*ppRoot = 0;` |
|        103 | 2369 | `		return rc;` |
|          - | 2370 | `	}` |
|          - | 2371 | `	/* Point to the root of the tree */` |
|   17129259 | 2372 | `	*ppRoot = apNode[0];` |
|   17129259 | 2373 | `	return SXRET_OK;` |
|    8564722 | 2374 | `}` |
|          - | 2375 |  |
