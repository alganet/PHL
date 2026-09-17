# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1223/1409 lines (86.80%)

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
|   29022554 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   29022559 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  457930308 |  278 | `	for(;;){` |
|  915860621 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  915860621 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   87533711 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   43766858 |  285 | `		}else{` |
|  828326915 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  915860621 |  288 | `		if( rc == 0 ){` |
|   29355649 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   28902203 |  291 | `				return &aOpTable[n];` |
|          - |  292 | `			}` |
|          - |  293 | `			/* Handle ambiguity */` |
|     453451 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|          - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      42989 |  296 | `				return &aOpTable[n];` |
|          - |  297 | `			}` |
|     410467 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      77385 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|          - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      77385 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|          - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      77377 |  303 | `					return &aOpTable[n];` |
|          - |  304 | `				}` |
|          - |  305 |  |
|          4 |  306 | `			}` |
|     166545 |  307 | `		}` |
|  886838067 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   14511282 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    7890554 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    7890559 |  320 | `	SyToken *pCur = pIn;` |
|    7890559 |  321 | `	sxi32 iNest = 1;` |
|   84624921 |  322 | `	for(;;){` |
|  169249847 |  323 | `		if( pCur >= pEnd ){` |
|      15957 |  324 | `			break;` |
|          - |  325 | `		}` |
|  169233895 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    6523321 |  328 | `			iNest++;` |
|  165972237 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   14397923 |  331 | `			iNest--;` |
|   14397923 |  332 | `			if( iNest <= 0 ){` |
|    7874607 |  333 | `				break;` |
|          - |  334 | `			}` |
|    3261658 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  161359293 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    7890559 |  340 | `	*ppEnd = pCur;` |
|    7890559 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     538240 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     538240 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     538203 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        131 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     538119 |  358 | `	if( bCheckFunc ){` |
|      58370 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      58358 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      58334 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|         61 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      29157 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     538063 |  366 | `	return FALSE;` |
|     269125 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   17024068 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   17024073 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7763 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7763 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3879 |  383 | `	}` |
|   17024073 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  106711939 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   89687907 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     242085 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   89445827 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    7514271 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     406802 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    7025495 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    7025495 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    7025495 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    7025495 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3512745 |  401 | `					}` |
|    3512745 |  402 | `			}` |
|    7514271 |  403 | `			iParen++;` |
|   85688694 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    7514275 |  405 | `			if( iParen <= 0 ){` |
|         16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         16 |  407 | `				if( rc != SXERR_ABORT ){` |
|         16 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         16 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    7514263 |  412 | `			iParen--;` |
|   78174420 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    3012065 |  414 | `			iSquare++;` |
|   72911261 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    3012069 |  416 | `			if( iSquare <= 0 ){` |
|          8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          8 |  418 | `				if( rc != SXERR_ABORT ){` |
|          8 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          8 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    3012063 |  423 | `			iSquare--;` |
|   69899196 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       3877 |  425 | `			iBraces++;` |
|       3877 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|          - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|          - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|          - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|          3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|          3 |  431 | `				if( rc != SXERR_ABORT ){` |
|          3 |  432 | `					rc = SXERR_SYNTAX;` |
|          1 |  433 | `				}` |
|          3 |  434 | `				return rc;` |
|          5 |  435 | `			}` |
|   68391230 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3887 |  437 | `			if( iBraces <= 0 ){` |
|         15 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         15 |  439 | `				if( rc != SXERR_ABORT ){` |
|         15 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         15 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3875 |  444 | `			iBraces--;` |
|   68387348 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     501781 |  446 | `			if( iQuesty > 0 ){` |
|     501479 |  447 | `				iQuesty--;` |
|     251044 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   68134523 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   22421619 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   22421619 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     501481 |  461 | `				iQuesty++;` |
|   22170881 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      93247 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      46621 |  481 | `			}` |
|   11210807 |  482 | `		}` |
|   44722898 |  483 | `	}` |
|   17024037 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         15 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         15 |  486 | `		if( rc != SXERR_ABORT ){` |
|         15 |  487 | `			rc = SXERR_SYNTAX;` |
|          6 |  488 | `		}` |
|         15 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   17024025 |  491 | `	return SXRET_OK;` |
|    8512039 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   14037588 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   14037593 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   14037593 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   14033661 |  502 | `		pIn++;` |
|    7016828 |  503 | `	}` |
|    7020801 |  504 | `	for(;;){` |
|   14041607 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       4019 |  506 | `			pIn++;` |
|       4019 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       4017 |  508 | `				pIn++;` |
|       2006 |  509 | `			}` |
|       2012 |  510 | `		}else{` |
|    7018799 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   14037593 |  515 | `	*ppCur = pIn;` |
|   14037593 |  516 | `}` |
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
|          5 |  637 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|          5 |  638 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,pIn >= pEnd ? "\")\"" : "\"{\"");` |
|          5 |  639 | `		if( rc != SXERR_ABORT ){` |
|          5 |  640 | `			rc = SXERR_SYNTAX;` |
|          2 |  641 | `		}` |
|          5 |  642 | `		goto Synchronize;` |
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
|          5 |  654 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          5 |  655 | `				if( rc != SXERR_ABORT ){` |
|          5 |  656 | `					rc = SXERR_SYNTAX;` |
|          2 |  657 | `				}` |
|          5 |  658 | `				goto Synchronize;` |
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
|   89692200 |  901 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  902 | `{` |
|          - |  903 | `	ph7_expr_node *pNode;` |
|          - |  904 | `	SyToken *pCur;` |
|          - |  905 | `	sxi32 rc;` |
|          - |  906 | `	/* Allocate a new node */` |
|   89692205 |  907 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   89692205 |  908 | `	if( pNode == 0 ){` |
|          - |  909 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  910 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  911 | `		 */` |
|        ! 0 |  912 | `		return SXERR_MEM;` |
|          - |  913 | `	}` |
|          - |  914 | `	/* Zero the structure */` |
|   89692205 |  915 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   89692205 |  916 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  917 | `	/* Point to the head of the token stream */` |
|   89692205 |  918 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  919 | `	/* Start collecting tokens */` |
|   89692205 |  920 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4217 |  921 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
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
|       4135 |  936 | `		pCur++;` |
|       4135 |  937 | `		pGen->pIn = pCur;` |
|       4135 |  938 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4135 |  939 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4135 |  940 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4135 |  941 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2065 |  942 | `		}` |
|       4135 |  943 | `		return rc;` |
|          - |  944 | `	}` |
|   89687993 |  945 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - |  946 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - |  947 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - |  948 | `		 */` |
|     242087 |  949 | `		pCur++; /* Skip the opening '[' */` |
|     242087 |  950 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     242087 |  951 | `		if( pCur < pGen->pEnd ){` |
|     242087 |  952 | `			pCur++; /* Skip past the closing ']' */` |
|     121046 |  953 | `		}else{` |
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
|     242301 |  965 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        433 |  966 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        433 |  967 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|         67 |  968 | `				pNode->xCode = PH7_CompileShortList;` |
|         36 |  969 | `			}else{` |
|        370 |  970 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - |  971 | `			}` |
|        219 |  972 | `		}else{` |
|     241659 |  973 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 |  974 | `		}` |
|   89566952 |  975 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|   89445898 |  986 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   25433734 |  987 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   12745871 |  988 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|   89445882 | 1013 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - | 1014 | `		/* Point to the instance that describe this operator */` |
|   25433717 | 1015 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - | 1016 | `		/* Advance the stream cursor */` |
|   25433717 | 1017 | `		pCur++;` |
|   76729015 | 1018 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - | 1019 | `		/* Isolate variable */` |
|   43023989 | 1020 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   21512003 | 1021 | `			pCur++; /* Variable variable */` |
|          5 | 1022 | `		}` |
|   21511991 | 1023 | `		if( pCur < pGen->pEnd ){` |
|   21511991 | 1024 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - | 1025 | `				/* Variable name */` |
|   21511965 | 1026 | `				pCur++;` |
|   10756010 | 1027 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|         11 | 1062 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|         11 | 1063 | `				if( rc != SXERR_ABORT ){` |
|         11 | 1064 | `					rc = SXERR_SYNTAX;` |
|          4 | 1065 | `				}` |
|         11 | 1066 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         11 | 1067 | `				return rc;` |
|          - | 1068 | `			}` |
|   10755988 | 1069 | `		}` |
|   21511981 | 1070 | `		pNode->xCode = PH7_CompileVariable;` |
|   53256161 | 1071 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1051375 | 1072 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    1051375 | 1073 | `		 if( bAfterMemberOp ){` |
|          - | 1074 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1075 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1076 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1077 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1078 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1079 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1080 | `			  * the word itself. */` |
|     127701 | 1081 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     127701 | 1082 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     127701 | 1083 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     987527 | 1084 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1085 | `			 /* List/Array node */` |
|     426699 | 1086 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1087 | `				 /* Assume a literal */` |
|        ! 0 | 1088 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1089 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1090 | `			 }else{` |
|     426699 | 1091 | `				 pCur += 2;` |
|          - | 1092 | `				 /* Collect array/list tokens */` |
|     426699 | 1093 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     426699 | 1094 | `				 if( pCur < pGen->pEnd ){` |
|     426697 | 1095 | `					 pCur++;` |
|     213351 | 1096 | `				 }else{` |
|          - | 1097 | `					 /* Syntax error */` |
|          - | 1098 | `					 /* php names the token it stopped on and says it expected ")". */` |
|          3 | 1099 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|          3 | 1100 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1101 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1102 | `					 }` |
|          3 | 1103 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1104 | `					 return rc;` |
|          - | 1105 | `				 }` |
|     426697 | 1106 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     426697 | 1107 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|     710330 | 1121 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1122 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15841 | 1123 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15841 | 1124 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1125 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1126 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15841 | 1127 | `			 pNode->xCode = PH7_CompileYield;` |
|     489067 | 1128 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     480856 | 1129 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7816 | 1130 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3923 | 1131 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
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
|     480816 | 1146 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
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
|     480491 | 1161 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     480212 | 1162 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7792 | 1163 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3899 | 1164 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1165 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        551 | 1166 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        551 | 1167 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1168 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1169 | `				 return rc;` |
|          - | 1170 | `			 }` |
|        551 | 1171 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     480204 | 1172 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1173 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         78 | 1174 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         78 | 1175 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1176 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1177 | `				 return rc;` |
|          - | 1178 | `			 }` |
|         78 | 1179 | `			 pNode->xCode = PH7_CompileMatch;` |
|     479894 | 1180 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1181 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1182 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1183 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1184 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1185 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1186 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1187 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1188 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     479839 | 1189 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1190 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         73 | 1191 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         73 | 1192 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         39 | 1193 | `		 }else{` |
|          - | 1194 | `			 /* Assume a literal */` |
|     479753 | 1195 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     479753 | 1196 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1197 | `		 }` |
|   41974474 | 1198 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1199 | `		 /* Constants,function name,namespace path,class name... */` |
|   13430131 | 1200 | `		 if( bAfterMemberOp ){` |
|          - | 1201 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1202 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1203 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1204 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    5596287 | 1205 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    2798141 | 1206 | `		 }` |
|   13430131 | 1207 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   13430131 | 1208 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    6715068 | 1209 | `	 }else{` |
|   28018677 | 1210 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1211 | `			 /* Point to the code generator routine */` |
|    9468523 | 1212 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    9468523 | 1213 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1214 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1215 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1216 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1217 | `				 }` |
|          3 | 1218 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1219 | `				 return rc;` |
|          - | 1220 | `			 }` |
|    4734258 | 1221 | `		 }` |
|          - | 1222 | `		/* Advance the stream cursor */` |
|   28018675 | 1223 | `		pCur++;` |
|          - | 1224 | `	 }` |
|          - | 1225 | `	/* Point to the end of the token stream */` |
|   89687953 | 1226 | `	pNode->pEnd = pCur;` |
|          - | 1227 | `	/* Save the node for later processing */` |
|   89687953 | 1228 | `	*ppNode = pNode;` |
|          - | 1229 | `	/* Synchronize cursors */` |
|   89687953 | 1230 | `	pGen->pIn = pCur;` |
|   89687953 | 1231 | `	return SXRET_OK;` |
|   44846105 | 1232 | `}` |
|          - | 1233 | `/*` |
|          - | 1234 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1235 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1236 | ` * level is zero.` |
|          - | 1237 | ` */` |
|    1929248 | 1238 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1239 | `{` |
|    1929253 | 1240 | `	SyToken *pCur = pStart;` |
|    1929253 | 1241 | `	sxi32 iNest = 0;` |
|    1929253 | 1242 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1243 | `		/* Last expression */` |
|     712401 | 1244 | `		return SXERR_EOF;` |
|          - | 1245 | `	}` |
|    4618321 | 1246 | `	while( pCur < pEnd ){` |
|    4318015 | 1247 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     916551 | 1248 | `			break;` |
|          - | 1249 | `		}` |
|    3401469 | 1250 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     240455 | 1251 | `			iNest++;` |
|    3281244 | 1252 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     240459 | 1253 | `			iNest--;` |
|     120227 | 1254 | `		}` |
|    3401469 | 1255 | `		pCur++;` |
|          5 | 1256 | `	}` |
|    1216857 | 1257 | `	*ppNext = pCur;` |
|    1216857 | 1258 | `	return SXRET_OK;` |
|     964629 | 1259 | `}` |
|          - | 1260 | `/*` |
|          - | 1261 | ` * Free an expression tree.` |
|          - | 1262 | ` */` |
|   76655958 | 1263 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1264 | `{` |
|   76655963 | 1265 | `	if( pNode->pLeft ){` |
|          - | 1266 | `		/* Release the left tree */` |
|   29962059 | 1267 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   14981027 | 1268 | `	}` |
|   76655963 | 1269 | `	if( pNode->pRight ){` |
|          - | 1270 | `		/* Release the right tree */` |
|   17428131 | 1271 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    8714063 | 1272 | `	}` |
|   76655963 | 1273 | `	if( pNode->pCond ){` |
|          - | 1274 | `		/* Release the conditional tree used by the ternary operator */` |
|     501477 | 1275 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     250736 | 1276 | `	}` |
|   76655963 | 1277 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1278 | `		ph7_expr_node **apArg;` |
|          - | 1279 | `		sxu32 n;` |
|          - | 1280 | `		/* Release node arguments */` |
|    8260993 | 1281 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   19015003 | 1282 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   10754015 | 1283 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    5377010 | 1284 | `		}` |
|    8260993 | 1285 | `		SySetRelease(&pNode->aNodeArgs);` |
|    4130494 | 1286 | `	}` |
|          - | 1287 | `	/* Finally,release this node */` |
|   76655963 | 1288 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   76655963 | 1289 | `}` |
|          - | 1290 | `/*` |
|          - | 1291 | ` * Free an expression tree.` |
|          - | 1292 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1293 | ` */` |
|   17024104 | 1294 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1295 | `{` |
|          - | 1296 | `	ph7_expr_node **apNode;` |
|          - | 1297 | `	sxu32 n;` |
|   17024109 | 1298 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  106712109 | 1299 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   89688005 | 1300 | `		if( apNode[n] ){` |
|   17024433 | 1301 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    8512214 | 1302 | `		}` |
|   44844005 | 1303 | `	}` |
|   17024109 | 1304 | `	return SXRET_OK;` |
|          5 | 1305 | `}` |
|          - | 1306 | `/*` |
|          - | 1307 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1308 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1309 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1310 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1311 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1312 | ` */` |
|   21203368 | 1313 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1314 | `{` |
|   21203373 | 1315 | `	if( pNode == 0 ){` |
|   13209001 | 1316 | `		return 0;` |
|          - | 1317 | `	}` |
|    7994377 | 1318 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1319 | `		return 1;` |
|          - | 1320 | `	}` |
|    7994365 | 1321 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1322 | `		return 1;` |
|          - | 1323 | `	}` |
|    7994361 | 1324 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1325 | `		return 1;` |
|          - | 1326 | `	}` |
|    7994361 | 1327 | `	return 0;` |
|   10601689 | 1328 | `}` |
|          - | 1329 | `/*` |
|          - | 1330 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1331 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1332 | ` */` |
|    5191404 | 1333 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1334 | `{` |
|          - | 1335 | `	sxi32 iExprOp;` |
|    5191409 | 1336 | `	if( pNode->pOp == 0 ){` |
|    3702793 | 1337 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1338 | `	}` |
|    1488621 | 1339 | `	iExprOp = pNode->pOp->iOp;` |
|    1488621 | 1340 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     954761 | 1341 | `			return TRUE;` |
|          - | 1342 | `	}` |
|     533865 | 1343 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     533851 | 1344 | `		if( pNode->pLeft->pOp ) {` |
|     127530 | 1345 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      54104 | 1346 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1347 | `				return FALSE;` |
|          5 | 1348 | `			}` |
|     470086 | 1349 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1350 | `			return FALSE;` |
|          - | 1351 | `		}` |
|     533851 | 1352 | `		return TRUE;` |
|          - | 1353 | `	}` |
|         16 | 1354 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1355 | `		return TRUE;` |
|          - | 1356 | `	}` |
|          - | 1357 | `	/* Not a modifiable l or r-value */` |
|          9 | 1358 | `	return FALSE;` |
|    2595707 | 1359 | `}` |
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
|    5484964 | 1371 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1372 | `{` |
|          - | 1373 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1374 | `	sxi32 rc;` |
|          - | 1375 | `	/* Process function arguments from left to right */` |
|    5484969 | 1376 | `	iCur = 0;` |
|    6731462 | 1377 | `	for(;;){` |
|   13462929 | 1378 | `		if( iCur >= nToken ){` |
|          - | 1379 | `			/* No more arguments to process */` |
|    5484941 | 1380 | `			break;` |
|          - | 1381 | `		}` |
|    7977993 | 1382 | `		iNode = iCur;` |
|    7977993 | 1383 | `		iNest = 0;` |
|   26136643 | 1384 | `		while( iCur < nToken ){` |
|   20651705 | 1385 | `			if( apNode[iCur] ){` |
|   20605055 | 1386 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1246530 | 1387 | `					break;` |
|   18112000 | 1388 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    9697675 | 1389 | `					&& apNode[iCur]->pLeft == 0` |
|    1283343 | 1390 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1280765 | 1391 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
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
|    1278189 | 1402 | `					iNest++;` |
|   17472913 | 1403 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    9056005 | 1404 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    1278189 | 1405 | `					iNest--;` |
|     639092 | 1406 | `				}` |
|    9056000 | 1407 | `			}` |
|   18158655 | 1408 | `			iCur++;` |
|          5 | 1409 | `		}` |
|    7977993 | 1410 | `		if( iCur > iNode ){` |
|    7977987 | 1411 | `			SyString sArgName = {0, 0};` |
|          - | 1412 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1413 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1414 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    7977982 | 1415 | `			if( (iCur - iNode) >= 2` |
|    5550009 | 1416 | `				&& apNode[iNode]` |
|    3122013 | 1417 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1684190 | 1418 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     246115 | 1419 | `				&& apNode[iNode+1]` |
|     245845 | 1420 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
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
|    7977980 | 1441 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
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
|    7977985 | 1458 | `				int bSpreadArg = 0;` |
|          - | 1459 | `				sxi32 iScan;` |
|    7978031 | 1460 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    7978031 | 1461 | `					if( apNode[iScan] ){` |
|    7977985 | 1462 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    7977985 | 1463 | `						break;` |
|          - | 1464 | `					}` |
|         26 | 1465 | `				}` |
|    7977985 | 1466 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    7977985 | 1467 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4065 | 1468 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2030 | 1469 | `				}` |
|          - | 1470 | `			}` |
|    7977985 | 1471 | `			if( apNode[iNode] ){` |
|    7977985 | 1472 | `				if( sArgName.nByte > 0 ){` |
|        300 | 1473 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        300 | 1474 | `					apNode[iNode]->sArgName = sArgName;` |
|        148 | 1475 | `				}` |
|          - | 1476 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    7977985 | 1477 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    3988995 | 1478 | `			}else{` |
|          - | 1479 | `				/* No expression before comma */` |
|        ! 0 | 1480 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1481 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1482 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1483 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1484 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1485 | `				}` |
|        ! 0 | 1486 | `				return rc;` |
|          - | 1487 | `			}` |
|    3988995 | 1488 | `		}else{` |
|          - | 1489 | `			/* Comma with no preceding argument */` |
|          8 | 1490 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1491 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1492 | `				rc = SXERR_SYNTAX;` |
|          3 | 1493 | `			}` |
|          8 | 1494 | `			return rc;` |
|          - | 1495 | `		}` |
|          - | 1496 | `		/* Jump trailing comma */` |
|    7977985 | 1497 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2493049 | 1498 | `			iCur++;` |
|    2493049 | 1499 | `			if( iCur >= nToken ){` |
|          - | 1500 | `				/* Trailing comma after last argument */` |
|         21 | 1501 | `				break;` |
|          - | 1502 | `			}` |
|    1246512 | 1503 | `		}` |
|          5 | 1504 | `	}` |
|    5484961 | 1505 | `	return SXRET_OK;` |
|    2742487 | 1506 | `}` |
|          - | 1507 | ` /*` |
|          - | 1508 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|          - | 1509 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|          - | 1510 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|          - | 1511 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|          - | 1512 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|          - | 1513 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|          - | 1514 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|          - | 1515 | `  */` |
|         74 | 1516 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|          5 | 1517 | ` {` |
|          - | 1518 | `	 SyToken *pMin;` |
|          - | 1519 | `	 SyToken *pChild;` |
|         79 | 1520 | `	 if( pNode == 0 ){` |
|         51 | 1521 | `		 return 0;` |
|          - | 1522 | `	 }` |
|         33 | 1523 | `	 pMin = pNode->pStart;` |
|         33 | 1524 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|         33 | 1525 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|          6 | 1526 | `		 pMin = pChild;` |
|          2 | 1527 | `	 }` |
|         33 | 1528 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|         33 | 1529 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|        ! 0 | 1530 | `		 pMin = pChild;` |
|        ! 0 | 1531 | `	 }` |
|         33 | 1532 | `	 return pMin;` |
|         42 | 1533 | ` }` |
|          - | 1534 | ` /*` |
|          - | 1535 | `  * Create an expression tree from an array of tokens.` |
|          - | 1536 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1537 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1538 | `  */` |
|   29424490 | 1539 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1540 | ` {` |
|          - | 1541 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1542 | `	 ph7_expr_node *pNode;` |
|          - | 1543 | `	 ph7_expr_node *pSuppress;` |
|          - | 1544 | `	 sxi32 iCur;` |
|          - | 1545 | `	 sxi32 rc;` |
|   29424495 | 1546 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1547 | `		 /* TICKET 1433-17: self evaluating node */` |
|   12930583 | 1548 | `		 return SXRET_OK;` |
|          - | 1549 | `	 }` |
|          - | 1550 | `	 /* Process expressions enclosed in parenthesis first */` |
|  117377015 | 1551 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1552 | `		 sxi32 iNest;` |
|          - | 1553 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1554 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1555 | `		  */` |
|  100883105 | 1556 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  100394337 | 1557 | `			 continue;` |
|          - | 1558 | `		 }` |
|     488773 | 1559 | `		 iNest = 1;` |
|     488773 | 1560 | `		 iLeft = iCur;` |
|          - | 1561 | `		 /* Find the closing parenthesis */` |
|     488773 | 1562 | `		 iCur++;` |
|    4356529 | 1563 | `		 while( iCur < nToken ){` |
|    4356529 | 1564 | `			 if( apNode[iCur] ){` |
|    4356529 | 1565 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1566 | `					 /* Decrement nesting level */` |
|     714043 | 1567 | `					 iNest--;` |
|     714043 | 1568 | `					 if( iNest <= 0 ){` |
|     488773 | 1569 | `						 break;` |
|          5 | 1570 | `					 }` |
|    3755126 | 1571 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1572 | `					 /* Increment nesting level */` |
|     225275 | 1573 | `					 iNest++;` |
|     112635 | 1574 | `				 }` |
|    1933878 | 1575 | `			 }` |
|    3867761 | 1576 | `			 iCur++;` |
|          5 | 1577 | `		 }` |
|     488773 | 1578 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1579 | `			 sxi32 j;` |
|          - | 1580 | `			 /* Recurse and process this expression */` |
|     488773 | 1581 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     488773 | 1582 | `			 if( rc != SXRET_OK ){` |
|          3 | 1583 | `				 return rc;` |
|          - | 1584 | `			 }` |
|          - | 1585 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1586 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1587 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1588 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1589 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1590 | `			  * group's free below silently drops the unpacking. */` |
|     488771 | 1591 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     488771 | 1592 | `				 if( apNode[j] ){` |
|     488771 | 1593 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     488766 | 1594 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     488771 | 1595 | `					 break;` |
|          - | 1596 | `				 }` |
|        ! 0 | 1597 | `			 }` |
|     244383 | 1598 | `		 }` |
|          - | 1599 | `		 /* Free the left and right nodes */` |
|     488771 | 1600 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     488771 | 1601 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     488771 | 1602 | `		 apNode[iLeft] = 0;` |
|     488771 | 1603 | `		 apNode[iCur] = 0;` |
|     244388 | 1604 | `	 }` |
|          - | 1605 | `	  /* Process expressions enclosed in braces */` |
|  121416165 | 1606 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1607 | `		 sxi32 iNest;` |
|          - | 1608 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1609 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1610 | `		  */` |
|  105231879 | 1611 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  105228009 | 1612 | `			 continue;` |
|          - | 1613 | `		 }` |
|       3875 | 1614 | `		 iNest = 1;` |
|       3875 | 1615 | `		 iLeft = iCur;` |
|          - | 1616 | `		 /* Find the closing parenthesis */` |
|       3875 | 1617 | `		 iCur++;` |
|       7743 | 1618 | `		 while( iCur < nToken ){` |
|       7743 | 1619 | `			 if( apNode[iCur] ){` |
|       7743 | 1620 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1621 | `					 /* Decrement nesting level */` |
|       3875 | 1622 | `					 iNest--;` |
|       3875 | 1623 | `					 if( iNest <= 0 ){` |
|       3875 | 1624 | `						 break;` |
|        ! 0 | 1625 | `					 }` |
|       3873 | 1626 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1627 | `					 /* Increment nesting level */` |
|        ! 0 | 1628 | `					 iNest++;` |
|        ! 0 | 1629 | `				 }` |
|       1934 | 1630 | `			 }` |
|       3873 | 1631 | `			 iCur++;` |
|          5 | 1632 | `		 }` |
|       3875 | 1633 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1634 | `			 /* Recurse and process this expression */` |
|       3873 | 1635 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3873 | 1636 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1637 | `				 return rc;` |
|          - | 1638 | `			 }` |
|       1934 | 1639 | `		 }` |
|          - | 1640 | `		 /* Free the left and right nodes */` |
|       3875 | 1641 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3875 | 1642 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3875 | 1643 | `		 apNode[iLeft] = 0;` |
|       3875 | 1644 | `		 apNode[iCur] = 0;` |
|       1940 | 1645 | `	 }` |
|          - | 1646 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   16184291 | 1647 | `	 iLeft = -1;` |
|  121423867 | 1648 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105239593 | 1649 | `		 if( apNode[iCur] == 0 ){` |
|   46470741 | 1650 | `			 continue;` |
|          - | 1651 | `		 }` |
|   58768857 | 1652 | `		 pNode = apNode[iCur];` |
|   58768857 | 1653 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   16001103 | 1654 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1655 | `				 /* Collect function arguments */` |
|    7025493 | 1656 | `				 sxi32 iPtr = 0;` |
|    7025493 | 1657 | `				 sxi32 nFuncTok = 0;` |
|   34702683 | 1658 | `				 while( nFuncTok + iCur < nToken ){` |
|   34702683 | 1659 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|          - | 1660 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|          - | 1661 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|          - | 1662 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|          - | 1663 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|          - | 1664 | `					  * nulled, so counting it here would over-count and never find` |
|          - | 1665 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   34702683 | 1666 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   34644397 | 1667 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    7484299 | 1668 | `							 iPtr++;` |
|   30902250 | 1669 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    7484299 | 1670 | `							 iPtr--;` |
|    7484299 | 1671 | `							 if( iPtr <= 0 ){` |
|    7025493 | 1672 | `								 break;` |
|          - | 1673 | `							 }` |
|     229403 | 1674 | `						 }` |
|   13809452 | 1675 | `					 }` |
|   27677195 | 1676 | `					 nFuncTok++;` |
|          5 | 1677 | `				 }` |
|    7025493 | 1678 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1679 | `					 /* Syntax error */` |
|        ! 0 | 1680 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1681 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1682 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1683 | `					 }` |
|        ! 0 | 1684 | `					 return rc;` |
|          - | 1685 | `				 }` |
|    7025493 | 1686 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1687 | `					 /* Syntax error */` |
|        ! 0 | 1688 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1689 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1690 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1691 | `					 }` |
|        ! 0 | 1692 | `					 return rc;` |
|          - | 1693 | `				 }` |
|    7025493 | 1694 | `				 if( nFuncTok > 1 ){` |
|          - | 1695 | `					 /* Process function arguments */` |
|    5484969 | 1696 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    5484969 | 1697 | `					 if( rc != SXRET_OK ){` |
|         11 | 1698 | `						 return rc;` |
|          - | 1699 | `					 }` |
|    2742478 | 1700 | `				 }` |
|          - | 1701 | `				 /* Link the node to the tree */` |
|    7025485 | 1702 | `				 pNode->pLeft = apNode[iLeft];` |
|    7025485 | 1703 | `				 apNode[iLeft] = 0;` |
|   34702651 | 1704 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   27677171 | 1705 | `					 apNode[iCur+iPtr] = 0;` |
|   13838588 | 1706 | `				 }` |
|          - | 1707 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1708 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1709 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1710 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1711 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1712 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1713 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1714 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1715 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1716 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1717 | `				 {` |
|    7025485 | 1718 | `					 sxi32 iNew = iLeft - 1;` |
|    9080311 | 1719 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    2054831 | 1720 | `						 iNew--;` |
|          5 | 1721 | `					 }` |
|    7025480 | 1722 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3961200 | 1723 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2400059 | 1724 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     852447 | 1725 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     852447 | 1726 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     852447 | 1727 | `						 apNode[iNew] = 0;` |
|     852447 | 1728 | `						 pNode = apNode[iCur];` |
|     426226 | 1729 | `					 }` |
|          - | 1730 | `				 }` |
|   12488355 | 1731 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1732 | `				 /* Subscripting */` |
|    3012063 | 1733 | `				 sxi32 iArrTok = iCur + 1;` |
|    3012063 | 1734 | `				 sxi32 iNest = 1;` |
|    3012058 | 1735 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         34 | 1736 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         28 | 1737 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|          - | 1738 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|          - | 1739 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|          - | 1740 | `					  * subscripting a global constant raised "Invalid array name" while` |
|          - | 1741 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|          - | 1742 | `					  * worked. */` |
|         22 | 1743 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|         16 | 1744 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    3012058 | 1745 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1746 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1747 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     320830 | 1748 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1749 | `						 /* Syntax error */` |
|        ! 0 | 1750 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1751 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1752 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1753 | `						 }` |
|        ! 0 | 1754 | `						 return rc;` |
|          - | 1755 | `				 }` |
|          - | 1756 | `				 /* Collect index tokens */` |
|    6328923 | 1757 | `				 while( iArrTok < nToken ){` |
|    6328923 | 1758 | `					 if( apNode[iArrTok] ){` |
|    6328891 | 1759 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1760 | `							 /* Increment nesting level */` |
|      27039 | 1761 | `							 iNest++;` |
|    6315374 | 1762 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1763 | `							 /* Decrement nesting level */` |
|    3039097 | 1764 | `							 iNest--;` |
|    3039097 | 1765 | `							 if( iNest <= 0 ){` |
|    3012063 | 1766 | `								 break;` |
|          - | 1767 | `							 }` |
|      13517 | 1768 | `						 }` |
|    1658414 | 1769 | `					 }` |
|    3316865 | 1770 | `					 ++iArrTok;` |
|          5 | 1771 | `				 }` |
|    3012063 | 1772 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1773 | `					 /* Recurse and process this expression */` |
|    2776035 | 1774 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2776035 | 1775 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1776 | `						 return rc;` |
|          - | 1777 | `					 }` |
|          - | 1778 | `					 /* Link the node to it's index */` |
|    2776035 | 1779 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1388015 | 1780 | `				 }` |
|          - | 1781 | `				 /* Link the node to the tree */` |
|    3012063 | 1782 | `				 pNode->pLeft = apNode[iLeft];` |
|    3012063 | 1783 | `				 pNode->pRight = 0;` |
|    3012063 | 1784 | `				 apNode[iLeft] = 0;` |
|    9340981 | 1785 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6328923 | 1786 | `					 apNode[iNest] = 0;` |
|    3164464 | 1787 | `				 }` |
|    1506034 | 1788 | `			 }else{` |
|          - | 1789 | `				 /* Member access operators [i.e: '->','::'] */` |
|    5963557 | 1790 | `				  iRight = iCur + 1;` |
|    5967425 | 1791 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3873 | 1792 | `					 iRight++;` |
|          5 | 1793 | `				 }` |
|    5963557 | 1794 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1795 | `					 /* Syntax error */` |
|          5 | 1796 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1797 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1798 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1799 | `					 }` |
|          5 | 1800 | `					 return rc;` |
|          - | 1801 | `				 }` |
|          - | 1802 | `				 /* Link the node to the tree */` |
|    5963553 | 1803 | `				 pNode->pLeft = apNode[iLeft];` |
|    5963548 | 1804 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    5754463 | 1805 | `					 && pNode->pLeft->pOp == 0 &&` |
|    5455777 | 1806 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1807 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1808 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1809 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1810 | `					  * accepts. */` |
|          4 | 1811 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1812 | `						 /* Syntax error */` |
|        ! 0 | 1813 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1814 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1815 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1816 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1817 | `						 }` |
|        ! 0 | 1818 | `						 return rc;` |
|          - | 1819 | `				 }` |
|    5963553 | 1820 | `				 pNode->pRight = apNode[iRight];` |
|    5963553 | 1821 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1822 | `			 }` |
|    8000543 | 1823 | `		 }` |
|   58768845 | 1824 | `		 iLeft = iCur;` |
|   29384425 | 1825 | `	 }` |
|          - | 1826 | `	 /* Handle left associative (new, clone) operators */` |
|  121423835 | 1827 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105239561 | 1828 | `		 if( apNode[iCur] == 0 ){` |
|   63386531 | 1829 | `			 continue;` |
|          - | 1830 | `		 }` |
|   41853035 | 1831 | `		 pNode = apNode[iCur];` |
|   41853035 | 1832 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1833 | `			 SyToken *pToken;` |
|          - | 1834 | `			 /* Get the left node */` |
|      62267 | 1835 | `			 iLeft = iCur + 1;` |
|      62275 | 1836 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1837 | `				 iLeft++;` |
|          1 | 1838 | `			 }` |
|      62267 | 1839 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1840 | `				  /* Syntax error */` |
|        ! 0 | 1841 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1842 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1843 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1844 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1845 | `				 }` |
|        ! 0 | 1846 | `				 return rc;` |
|          - | 1847 | `			 }` |
|          - | 1848 | `			 /* Make sure the operand are of a valid type */` |
|      62267 | 1849 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1850 | `				 /* Clone:` |
|          - | 1851 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1852 | `				  *  ++ function call (including annonymous)` |
|          - | 1853 | `				  *  ++ array member` |
|          - | 1854 | `				  *  ++ 'new' operator` |
|          - | 1855 | `				  * Example:` |
|          - | 1856 | `				  *   clone $pObj;` |
|          - | 1857 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1858 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1859 | `				  */` |
|      57981 | 1860 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      57975 | 1861 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1862 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1863 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1864 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1865 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1866 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1867 | `						 }` |
|        ! 0 | 1868 | `						 return rc;` |
|          - | 1869 | `					 }` |
|      28985 | 1870 | `				 }` |
|      28993 | 1871 | `			 }else{` |
|          - | 1872 | `				 /* New */` |
|       4286 | 1873 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1874 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1875 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1876 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1877 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1878 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1879 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1880 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1881 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1882 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1883 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1884 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1885 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1886 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1887 | `					 }` |
|        ! 0 | 1888 | `					 return rc;` |
|          - | 1889 | `				 }` |
|       4291 | 1890 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       4291 | 1891 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       4286 | 1892 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         35 | 1893 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1894 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1895 | `						 /* Syntax error */` |
|        ! 0 | 1896 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1897 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1898 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1899 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1900 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1901 | `						 }` |
|        ! 0 | 1902 | `						 return rc;` |
|          - | 1903 | `					 }` |
|       2143 | 1904 | `				 }` |
|          - | 1905 | `			 }` |
|          - | 1906 | `			  /* Link the node to the tree */` |
|      62267 | 1907 | `			 pNode->pLeft = apNode[iLeft];` |
|      62267 | 1908 | `			 apNode[iLeft] = 0;` |
|      62267 | 1909 | `			 pNode->pRight = 0; /* Paranoid */` |
|      31131 | 1910 | `		 }` |
|   20926520 | 1911 | `	 }` |
|          - | 1912 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   16184279 | 1913 | `	 iLeft = -1;` |
|  121423835 | 1914 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105084749 | 1915 | `		 if( apNode[iCur] == 0 ){` |
|   63386531 | 1916 | `			 continue;` |
|          - | 1917 | `		 }` |
|   41698223 | 1918 | `		 pNode = apNode[iCur];` |
|   41698223 | 1919 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     371539 | 1920 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     185773 | 1921 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1922 | `					 /* Link the node to the tree */` |
|     201251 | 1923 | `					 pNode->pLeft = apNode[iLeft];` |
|     201251 | 1924 | `					 apNode[iLeft] = 0;` |
|     100623 | 1925 | `			 }` |
|     433455 | 1926 | `		  }` |
|   41853035 | 1927 | `		 iLeft = iCur;` |
|   20926520 | 1928 | `	  }` |
|   16339091 | 1929 | `	 iLeft = -1;` |
|  121578647 | 1930 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105239561 | 1931 | `		 if( apNode[iCur] == 0 ){` |
|   63587777 | 1932 | `			 continue;` |
|          - | 1933 | `		 }` |
|   41651789 | 1934 | `		 pNode = apNode[iCur];` |
|   41651789 | 1935 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15476 | 1936 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15481 | 1937 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1938 | `					 /* Syntax error */` |
|        ! 0 | 1939 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1940 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1941 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1942 | `					 }` |
|        ! 0 | 1943 | `					 return rc;` |
|          - | 1944 | `			 }` |
|          - | 1945 | `			 /* Link the node to the tree */` |
|      15481 | 1946 | `			 pNode->pLeft = apNode[iLeft];` |
|      15481 | 1947 | `			 apNode[iLeft] = 0;` |
|          - | 1948 | `			 /* Mark as pre-increment/decrement node */` |
|      15481 | 1949 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7738 | 1950 | `		  }` |
|   41651789 | 1951 | `		 iLeft = iCur;` |
|   20825897 | 1952 | `	 }` |
|          - | 1953 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1954 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1955 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1956 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1957 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1958 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1959 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1960 | `	  * pass below skips it (pLeft != 0). */` |
|   16339091 | 1961 | `	 iLeft = -1;` |
|  121578647 | 1962 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105239561 | 1963 | `		 if( apNode[iCur] == 0 ){` |
|   63684663 | 1964 | `			 continue;` |
|          - | 1965 | `		 }` |
|   41554903 | 1966 | `		 pNode = apNode[iCur];` |
|   41554903 | 1967 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      81415 | 1968 | `			 iRight = iCur + 1;` |
|      81415 | 1969 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1970 | `				 iRight++;` |
|        ! 0 | 1971 | `			 }` |
|      81415 | 1972 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1973 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1974 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1975 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1976 | `				 }` |
|        ! 0 | 1977 | `				 return rc;` |
|          - | 1978 | `			 }` |
|      81415 | 1979 | `			 pNode->pLeft = apNode[iLeft];` |
|      81415 | 1980 | `			 pNode->pRight = apNode[iRight];` |
|      81415 | 1981 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      40705 | 1982 | `		 }` |
|   41554903 | 1983 | `		 iLeft = iCur;` |
|   20777454 | 1984 | `	 }` |
|          - | 1985 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   16339091 | 1986 | `	  iLeft = 0;` |
|  121578641 | 1987 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105239557 | 1988 | `		  if( apNode[iCur] ){` |
|   41473489 | 1989 | `			  pNode = apNode[iCur];` |
|   41473489 | 1990 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1368905 | 1991 | `				  if( iLeft > 0 ){` |
|          - | 1992 | `					  /* Link the node to the tree */` |
|    1368903 | 1993 | `					  pNode->pLeft = apNode[iLeft];` |
|    1368903 | 1994 | `					  apNode[iLeft] = 0;` |
|    1368903 | 1995 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      54183 | 1996 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1997 | `							   /* Syntax error */` |
|        ! 0 | 1998 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1999 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2000 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 2001 | `							  }` |
|        ! 0 | 2002 | `							  return rc;` |
|          - | 2003 | `						  }` |
|      27089 | 2004 | `					  }` |
|     684454 | 2005 | `				  }else{` |
|          - | 2006 | `					  /* Syntax error */` |
|          3 | 2007 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 2008 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 2009 | `						  rc = SXERR_SYNTAX;` |
|          1 | 2010 | `					  }` |
|          3 | 2011 | `					  return rc;` |
|          - | 2012 | `				  }` |
|     684449 | 2013 | `			  }` |
|          - | 2014 | `			  /* Save terminal position */` |
|   41473487 | 2015 | `			  iLeft = iCur;` |
|   20736741 | 2016 | `		  }` |
|   52619780 | 2017 | `	  }` |
|          - | 2018 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 2019 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 2020 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 2021 | `	  * yielding a right-leaning tree. */` |
|  121578639 | 2022 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  105239555 | 2023 | `		 if( apNode[iCur] == 0 ){` |
|   65135085 | 2024 | `			 continue;` |
|          - | 2025 | `		 }` |
|   40104475 | 2026 | `		 pNode = apNode[iCur];` |
|   40104475 | 2027 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 2028 | `			 sxi32 iL, iR;` |
|          - | 2029 | `			 /* Find the right operand */` |
|        115 | 2030 | `			 iR = -1;` |
|          - | 2031 | `			 {` |
|          - | 2032 | `				 sxi32 j;` |
|        127 | 2033 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 2034 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 2035 | `				 }` |
|          - | 2036 | `			 }` |
|          - | 2037 | `			 /* Find the left operand */` |
|        115 | 2038 | `			 iL = -1;` |
|          - | 2039 | `			 {` |
|          - | 2040 | `				 sxi32 j;` |
|        183 | 2041 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 2042 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 2043 | `				 }` |
|          - | 2044 | `			 }` |
|        115 | 2045 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 2046 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2047 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2048 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2049 | `				 }` |
|        ! 0 | 2050 | `				 return rc;` |
|          - | 2051 | `			 }` |
|        115 | 2052 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 2053 | `			 pNode->pRight = apNode[iR];` |
|        115 | 2054 | `			 apNode[iL] = 0;` |
|        115 | 2055 | `			 apNode[iR] = 0;` |
|          - | 2056 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 2057 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 2058 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 2059 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 2060 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 2061 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 2062 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 2063 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 2064 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 2065 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 2066 | `			  * operands are respected. */` |
|        114 | 2067 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 2068 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 2069 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 2070 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 2071 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 2072 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 2073 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 2074 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 2075 | `				 while( pTail->pLeft` |
|         34 | 2076 | `					 && pTail->pLeft->pOp` |
|         23 | 2077 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 2078 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 2079 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 2080 | `					 pTail = pTail->pLeft;` |
|          1 | 2081 | `				 }` |
|          - | 2082 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 2083 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 2084 | `				 pTail->pLeft = pNode;` |
|         27 | 2085 | `				 apNode[iCur] = pHead;` |
|         13 | 2086 | `			 }` |
|         57 | 2087 | `		 }` |
|   20052240 | 2088 | `	 }` |
|          - | 2089 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  179729843 | 2090 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  163390769 | 2091 | `		 iLeft = -1;` |
| 1215785975 | 2092 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 1052395221 | 2093 | `			 if( apNode[iCur] == 0 ){` |
|  719796601 | 2094 | `				 continue;` |
|          - | 2095 | `			 }` |
|  332598625 | 2096 | `			 pNode = apNode[iCur];` |
|  332598625 | 2097 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2098 | `				 /* Get the right node */` |
|    5690291 | 2099 | `				 iRight = iCur + 1;` |
|    8621603 | 2100 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2931317 | 2101 | `					 iRight++;` |
|          5 | 2102 | `				 }` |
|    5690291 | 2103 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2104 | `					 /* Syntax error */` |
|         10 | 2105 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 2106 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2107 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2108 | `					 }` |
|         10 | 2109 | `					 return rc;` |
|          - | 2110 | `				 }` |
|    5690283 | 2111 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2112 | `					 sxi32  iTmp;` |
|          - | 2113 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2114 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2115 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2116 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2117 | `					  * is swapped below. */` |
|         75 | 2118 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2119 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2120 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2121 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2122 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2123 | `						 }` |
|          3 | 2124 | `						 return rc;` |
|          - | 2125 | `					 }` |
|          - | 2126 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|          - | 2127 | `					  * reference target — ExprIsModifiableValue already accepts` |
|          - | 2128 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|          - | 2129 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|          - | 2130 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|          - | 2131 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|         73 | 2132 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2133 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2134 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2135 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2136 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2137 | `						 }` |
|        ! 0 | 2138 | `						 return rc;` |
|          - | 2139 | `					 }` |
|         73 | 2140 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         55 | 2141 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2142 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2143 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2144 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2145 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2146 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2147 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2148 | `									 }` |
|        ! 0 | 2149 | `									 return rc;` |
|          - | 2150 | `							 }` |
|        ! 0 | 2151 | `						 }` |
|         26 | 2152 | `					 }` |
|          - | 2153 | `					 /* Swap operands */` |
|         73 | 2154 | `					 iTmp = iRight;` |
|         73 | 2155 | `					 iRight = iLeft;` |
|         73 | 2156 | `					 iLeft = iTmp;` |
|         35 | 2157 | `				 }` |
|          - | 2158 | `				 /* Link the node to the tree */` |
|    5690281 | 2159 | `				 pNode->pLeft = apNode[iLeft];` |
|    5690281 | 2160 | `				 pNode->pRight = apNode[iRight];` |
|    5690281 | 2161 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2845138 | 2162 | `			 }` |
|  332598615 | 2163 | `			 iLeft = iCur;` |
|  166299310 | 2164 | `		 }` |
|   81695382 | 2165 | `	 }` |
|          - | 2166 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2167 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2168 | `	  * we are dealing with a single operator.` |
|          - | 2169 | `	  */` |
|   16339079 | 2170 | `	  iLeft = -1;` |
|  117439763 | 2171 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  101602163 | 2172 | `		  if( apNode[iCur] == 0 ){` |
|   74378901 | 2173 | `			  continue;` |
|          - | 2174 | `		  }` |
|   27223267 | 2175 | `		  pNode = apNode[iCur];` |
|   27223267 | 2176 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     501479 | 2177 | `			  sxi32 iNest = 1;` |
|     501479 | 2178 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2179 | `				  /* Missing condition */` |
|          3 | 2180 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2181 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2182 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2183 | `				  }` |
|          3 | 2184 | `				  return rc;` |
|          - | 2185 | `			  }` |
|          - | 2186 | `			  /* Get the right node */` |
|     501477 | 2187 | `			  iRight = iCur + 1;` |
|    2131069 | 2188 | `			  while( iRight < nToken  ){` |
|    2131069 | 2189 | `				  if( apNode[iRight] ){` |
|     999017 | 2190 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2191 | `						  /* Increment nesting level */` |
|        ! 0 | 2192 | `						  ++iNest;` |
|     999017 | 2193 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2194 | `						  /* Decrement nesting level */` |
|     501477 | 2195 | `						  --iNest;` |
|     501477 | 2196 | `						  if( iNest <= 0 ){` |
|     501477 | 2197 | `							  break;` |
|          - | 2198 | `						  }` |
|        ! 0 | 2199 | `					  }` |
|     248770 | 2200 | `				  }` |
|    1629597 | 2201 | `				  iRight++;` |
|          5 | 2202 | `			  }` |
|     501477 | 2203 | `			  if( iRight > iCur + 1 ){` |
|          - | 2204 | `				  /* Recurse and process the then expression */` |
|     497545 | 2205 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     497545 | 2206 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2207 | `					  return rc;` |
|          - | 2208 | `				  }` |
|          - | 2209 | `				  /* Link the node to the tree */` |
|     497545 | 2210 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     248770 | 2211 | `			  }else{` |
|          - | 2212 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2213 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2214 | `			  }` |
|     501477 | 2215 | `			  apNode[iCur + 1] = 0;` |
|     501477 | 2216 | `			  if( iRight + 1 < nToken ){` |
|          - | 2217 | `				  /* Recurse and process the else expression */` |
|     501477 | 2218 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     501477 | 2219 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2220 | `					  return rc;` |
|          - | 2221 | `				  }` |
|          - | 2222 | `				  /* Link the node to the tree */` |
|     501477 | 2223 | `				  pNode->pRight = apNode[iRight + 1];` |
|     501477 | 2224 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     250741 | 2225 | `			  }else{` |
|        ! 0 | 2226 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2227 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2228 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2229 | `				 }` |
|        ! 0 | 2230 | `				 return rc;` |
|          - | 2231 | `			  }` |
|          - | 2232 | `			  /* Point to the condition */` |
|     501477 | 2233 | `			  pNode->pCond  = apNode[iLeft];` |
|     501477 | 2234 | `			  apNode[iLeft] = 0;` |
|     501477 | 2235 | `			  break;` |
|          - | 2236 | `		  }` |
|   26721793 | 2237 | `		  iLeft = iCur;` |
|   13360899 | 2238 | `	  }` |
|          - | 2239 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2240 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2241 | `	  * so there is no need for a precedence loop here.` |
|          - | 2242 | `	  */` |
|   16339077 | 2243 | `	 iRight = -1;` |
|  121578449 | 2244 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  105239429 | 2245 | `		 if( apNode[iCur] == 0 ){` |
|   83708965 | 2246 | `			 continue;` |
|          - | 2247 | `		 }` |
|   21530469 | 2248 | `		 pNode = apNode[iCur];` |
|   21530469 | 2249 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2250 | `			 /* Get the left node */` |
|    5191327 | 2251 | `			 iLeft = iCur - 1;` |
|    7128423 | 2252 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1937101 | 2253 | `				 iLeft--;` |
|          5 | 2254 | `			 }` |
|    5191327 | 2255 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2256 | `				 /* Syntax error */` |
|         46 | 2257 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2258 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2259 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2260 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2261 | `				 }else{` |
|         41 | 2262 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2263 | `				 }` |
|         46 | 2264 | `				 if( rc != SXERR_ABORT ){` |
|         44 | 2265 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2266 | `				 }` |
|         46 | 2267 | `				 return rc;` |
|          - | 2268 | `			 }` |
|          - | 2269 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2270 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2271 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2272 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2273 | `			  * a write. */` |
|    5191285 | 2274 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2275 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2276 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2277 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2278 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2279 | `				 }` |
|         11 | 2280 | `				 return rc;` |
|          - | 2281 | `			 }` |
|          - | 2282 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2283 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2284 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2285 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2286 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    5191277 | 2287 | `			 pSuppress = 0;` |
|    5191272 | 2288 | `			 if( apNode[iLeft]->pOp` |
|    3339923 | 2289 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     744287 | 2290 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2291 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2292 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2293 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2294 | `			 }` |
|    5191277 | 2295 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2296 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2297 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2298 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2299 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2300 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2301 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        109 | 2302 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2303 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2304 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2305 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2306 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2307 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2308 | `					 }` |
|          8 | 2309 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2310 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2311 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2312 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2313 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2314 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2315 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2316 | `						 iRight = iCur;` |
|          9 | 2317 | `						 continue;` |
|          - | 2318 | `					 }` |
|        ! 0 | 2319 | `				 }` |
|        132 | 2320 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         94 | 2321 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2322 | `					 /* Left operand must be a modifiable l-value */` |
|          3 | 2323 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2324 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2325 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2326 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2327 | `					 }else{` |
|        ! 0 | 2328 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 2329 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2330 | `					 }` |
|          3 | 2331 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 2332 | `						 rc = SXERR_SYNTAX;` |
|          1 | 2333 | `					 }` |
|          3 | 2334 | `					 return rc;` |
|          - | 2335 | `				 }` |
|         47 | 2336 | `			 }` |
|          - | 2337 | `			 /* Link the node to the tree (Reverse) */` |
|    5191267 | 2338 | `			 pNode->pLeft = apNode[iRight];` |
|    5191267 | 2339 | `			 pNode->pRight = apNode[iLeft];` |
|    5191267 | 2340 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    5191267 | 2341 | `			 if( pSuppress ){` |
|          - | 2342 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2343 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2344 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2345 | `			 }` |
|    2595631 | 2346 | `		 }` |
|   21530409 | 2347 | `		 iRight = iCur;` |
|   10765207 | 2348 | `	 }` |
|          - | 2349 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   81695105 | 2350 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   65356085 | 2351 | `		 iLeft = -1;` |
|  486313525 | 2352 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  420957445 | 2353 | `			 if( apNode[iCur] == 0 ){` |
|  355601125 | 2354 | `				 continue;` |
|          - | 2355 | `			 }` |
|   65356325 | 2356 | `			 pNode = apNode[iCur];` |
|   65356325 | 2357 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2358 | `				 /* Get the right node */` |
|         48 | 2359 | `				 iRight = iCur + 1;` |
|         60 | 2360 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2361 | `					 iRight++;` |
|          1 | 2362 | `				 }` |
|         48 | 2363 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2364 | `					 /* Syntax error */` |
|        ! 0 | 2365 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2366 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2367 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2368 | `					 }` |
|        ! 0 | 2369 | `					 return rc;` |
|          - | 2370 | `				 }` |
|          - | 2371 | `				 /* Link the node to the tree */` |
|         48 | 2372 | `				 pNode->pLeft = apNode[iLeft];` |
|         48 | 2373 | `				 pNode->pRight = apNode[iRight];` |
|         48 | 2374 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         22 | 2375 | `			 }` |
|   65356325 | 2376 | `			 iLeft = iCur;` |
|   32678165 | 2377 | `		 }` |
|   32678045 | 2378 | `	 }` |
|          - | 2379 | `	 /* Point to the root of the expression tree */` |
|  105239337 | 2380 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   88900335 | 2381 | `		 if( apNode[iCur] ){` |
|   15542093 | 2382 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|          - | 2383 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|          - | 2384 | `				  * operator sitting at its slot. A for()-clause also names the closer` |
|          - | 2385 | ``				  * it wants (`;` for init/condition, `)` for the post clause);`` |
|          - | 2386 | `				  * zClauseCloser carries it. */` |
|         23 | 2387 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|         32 | 2388 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,` |
|         18 | 2389 | `					 pGen->nCommaExprOk > 0 ? (pGen->zClauseCloser ? pGen->zClauseCloser : "\";\"") : 0);` |
|         23 | 2390 | `				  if( rc != SXERR_ABORT ){` |
|         23 | 2391 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2392 | `				  }` |
|         23 | 2393 | `				  return rc;` |
|          - | 2394 | `			 }` |
|   15542075 | 2395 | `			 apNode[0] = apNode[iCur];` |
|   15542075 | 2396 | `			 apNode[iCur] = 0;` |
|    7771035 | 2397 | `		 }` |
|   44450161 | 2398 | `	 }` |
|   16339007 | 2399 | `	 return SXRET_OK;` |
|   14634844 | 2400 | ` }` |
|          - | 2401 | ` /*` |
|          - | 2402 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2403 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2404 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2405 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2406 | `  */` |
|   17024108 | 2407 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2408 | `{` |
|          - | 2409 | `	ph7_expr_node **apNode;` |
|          - | 2410 | `	ph7_expr_node *pNode;` |
|          - | 2411 | `	sxi32 rc;` |
|          - | 2412 | `	/* Reset node container */` |
|   17024113 | 2413 | `	SySetReset(pExprNode);` |
|   17024113 | 2414 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2415 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2416 | `	{` |
|   17024113 | 2417 | `		int iLastWasTerm = 0;` |
|   17024113 | 2418 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  106712143 | 2419 | `		while( pGen->pIn < pGen->pEnd ){` |
|   89688075 | 2420 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   89688075 | 2421 | `			if( rc != SXRET_OK ){` |
|         44 | 2422 | `				return rc;` |
|          - | 2423 | `			}` |
|          - | 2424 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   89688035 | 2425 | `			if( pNode->xCode ){` |
|          - | 2426 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   45704169 | 2427 | `				iLastWasTerm = 1;` |
|   66835953 | 2428 | `			}else if( pNode->pOp ){` |
|          - | 2429 | `				/* Operator node */` |
|   25433717 | 2430 | `				iLastWasTerm = 0;` |
|   12716861 | 2431 | `			}else{` |
|          - | 2432 | `				/* Delimiter: ')' and ']' end terms */` |
|   18550159 | 2433 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2434 | `			}` |
|          - | 2435 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2436 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2437 | `			 * node kind, so this single test covers all branches. */` |
|   89688035 | 2438 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2439 | `			/* Save the extracted node */` |
|   89688035 | 2440 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2441 | `		}` |
|          - | 2442 | `	}` |
|   17024073 | 2443 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2444 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2445 | `		*ppRoot = 0;` |
|        ! 0 | 2446 | `		return SXRET_OK;` |
|          - | 2447 | `	}` |
|   17024073 | 2448 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2449 | `	/* Make sure we are dealing with valid nodes */` |
|   17024073 | 2450 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17024073 | 2451 | `	if( rc != SXRET_OK ){` |
|          - | 2452 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2453 | `		 * cleanup the mess left behind.` |
|          - | 2454 | `		 */` |
|         52 | 2455 | `		*ppRoot = 0;` |
|         52 | 2456 | `		return rc;` |
|          - | 2457 | `	}` |
|          - | 2458 | `	/* Build the tree */` |
|   17024025 | 2459 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17024025 | 2460 | `	if( rc != SXRET_OK ){` |
|          - | 2461 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        101 | 2462 | `		*ppRoot = 0;` |
|        101 | 2463 | `		return rc;` |
|          - | 2464 | `	}` |
|          - | 2465 | `	/* Point to the root of the tree */` |
|   17023929 | 2466 | `	*ppRoot = apNode[0];` |
|   17023929 | 2467 | `	return SXRET_OK;` |
|    8512059 | 2468 | `}` |
|          - | 2469 |  |
