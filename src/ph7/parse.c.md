# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1412/1568 lines (90.05%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `/*` |
|         - |    8 | ` * This file implement a hand-coded, thread-safe, full-reentrant and highly-efficient` |
|         - |    9 | ` * expression parser for the PH7 engine.` |
|         - |   10 | ` * Besides from the one introudced by PHP (Over 60), the PH7 engine have introduced three new` |
|         - |   11 | ` * operators. These are 'eq', 'ne' and the comma operator ','.` |
|         - |   12 | ` * The eq and ne operators are borrowed from the Perl world. They are used for strict` |
|         - |   13 | ` * string comparison. The reason why they have been implemented in the PH7 engine` |
|         - |   14 | ` * and introduced as an extension to the PHP programming language is due to the confusion` |
|         - |   15 | ` * introduced by the standard PHP comparison operators ('==' or '===') especially if you` |
|         - |   16 | ` * are comparing strings with numbers.` |
|         - |   17 | ` * Take the following example:` |
|         - |   18 | ` * var_dump( 0xFF == '255' ); // bool(true) ???` |
|         - |   19 | ` * // use the type equal operator by adding a single space to one of the operand` |
|         - |   20 | ` * var_dump( '255  ' === '255' ); //bool(true) depending on the PHP version` |
|         - |   21 | ` * That is, if one of the operand looks like a number (either integer or float) then PHP` |
|         - |   22 | ` * will internally convert the two operands to numbers and then a numeric comparison is performed.` |
|         - |   23 | ` * This is what the PHP language reference manual says:` |
|         - |   24 | ` * If you compare a number with a string or the comparison involves numerical strings, then each` |
|         - |   25 | ` * string is converted to a number and the comparison performed numerically.` |
|         - |   26 | ` * Bummer, if you ask me,this is broken, badly broken. I mean,the programmer cannot dictate` |
|         - |   27 | ` * it's comparison rule, it's the underlying engine who decides in it's place and perform` |
|         - |   28 | ` * the internal conversion. In most cases,PHP developers wants simple string comparison and they` |
|         - |   29 | ` * are stuck to use the ugly and inefficient strcmp() function and it's variants instead.` |
|         - |   30 | ` * This is the big reason why we have introduced these two operators.` |
|         - |   31 | ` * The eq operator is used to compare two strings byte per byte. If you came from the C/C++ world` |
|         - |   32 | ` * think of this operator as a barebone implementation of the memcmp() C standard library function.` |
|         - |   33 | ` * Keep in mind that if you are comparing two ASCII strings then the capital letters and their lowercase` |
|         - |   34 | ` * letters are completely different and so this example will output false.` |
|         - |   35 | ` * var_dump('allo' eq 'Allo'); //bool(FALSE)` |
|         - |   36 | ` * The ne operator perform the opposite operation of the eq operator and is used to test for string` |
|         - |   37 | ` * inequality. This example will output true` |
|         - |   38 | ` * var_dump('allo' ne 'Allo'); //bool(TRUE) unequal strings` |
|         - |   39 | ` * The eq operator return a Boolean true if and only if the two strings are identical while the` |
|         - |   40 | ` * ne operator return a Boolean true if and only if the two strings are different. Otherwise` |
|         - |   41 | ` * a Boolean false is returned (equal strings).` |
|         - |   42 | ` * Note that the comparison is performed only if the two strings are of the same length.` |
|         - |   43 | ` * Otherwise the eq and ne operators return a Boolean false without performing any comparison` |
|         - |   44 | ` * and avoid us wasting CPU time for nothing.` |
|         - |   45 | ` * Again remember that we talk about a low level byte per byte comparison and nothing else.` |
|         - |   46 | ` * Also remember that zero length strings are always equal.` |
|         - |   47 | ` *` |
|         - |   48 | ` * Again, another powerful mechanism borrowed from the C/C++ world and introduced as an extension` |
|         - |   49 | ` * to the PHP programming language.` |
|         - |   50 | ` * A comma expression contains two operands of any type separated by a comma and has left-to-right` |
|         - |   51 | ` * associativity. The left operand is fully evaluated, possibly producing side effects, and its` |
|         - |   52 | ` * value, if there is one, is discarded. The right operand is then evaluated. The type and value` |
|         - |   53 | ` * of the result of a comma expression are those of its right operand, after the usual unary conversions.` |
|         - |   54 | ` * Any number of expressions separated by commas can form a single expression because the comma operator` |
|         - |   55 | ` * is associative. The use of the comma operator guarantees that the sub-expressions will be evaluated` |
|         - |   56 | ` * in left-to-right order, and the value of the last becomes the value of the entire expression.` |
|         - |   57 | ` * The following example assign the value 25 to the variable $a, multiply the value of $a with 2` |
|         - |   58 | ` * and assign the result to variable $b and finally we call a test function to output the value` |
|         - |   59 | ` * of $a and $b. Keep-in mind that all theses operations are done in a single expression using` |
|         - |   60 | ` * the comma operator to create side effect.` |
|         - |   61 | ` * $a = 25,$b = $a << 1 ,test();` |
|         - |   62 | ` * //Output the value of $a and $b` |
|         - |   63 | ` * function test(){` |
|         - |   64 | ` *	 global $a,$b;` |
|         - |   65 | ` *	 echo "\$a = $a \$b= $b\n"; // You should see: $a = 25 $b = 50` |
|         - |   66 | ` * }` |
|         - |   67 | ` *` |
|         - |   68 | ` * For a full discussions on these extensions, please refer to  offical` |
|         - |   69 | ` * documentation(http://ph7.symisc.net/features.html) or visit the offical forums` |
|         - |   70 | ` * (http://forums.symisc.net/) if you want to share your point of view.` |
|         - |   71 | ` *` |
|         - |   72 | ` * Exprressions: According to the PHP language reference manual` |
|         - |   73 | ` *` |
|         - |   74 | ` * Expressions are the most important building stones of PHP. In PHP, almost anything you write is an expression.` |
|         - |   75 | ` * The simplest yet most accurate way to define an expression is "anything that has a value".` |
|         - |   76 | ` * The most basic forms of expressions are constants and variables. When you type "$a = 5", you're assigning` |
|         - |   77 | ` * '5' into $a. '5', obviously, has the value 5, or in other words '5' is an expression with the value of 5` |
|         - |   78 | ` * (in this case, '5' is an integer constant).` |
|         - |   79 | ` * After this assignment, you'd expect $a's value to be 5 as well, so if you wrote $b = $a, you'd expect` |
|         - |   80 | ` * it to behave just as if you wrote $b = 5. In other words, $a is an expression with the value of 5 as well.` |
|         - |   81 | ` * If everything works right, this is exactly what will happen.` |
|         - |   82 | ` * Slightly more complex examples for expressions are functions. For instance, consider the following function:` |
|         - |   83 | ` * <?php` |
|         - |   84 | ` * function foo ()` |
|         - |   85 | ` * {` |
|         - |   86 | ` *   return 5;` |
|         - |   87 | ` * }` |
|         - |   88 | ` * ?>` |
|         - |   89 | ` * Assuming you're familiar with the concept of functions (if you're not, take a look at the chapter about functions)` |
|         - |   90 | ` * you'd assume that typing $c = foo() is essentially just like writing $c = 5, and you're right.` |
|         - |   91 | ` * Functions are expressions with the value of their return value. Since foo() returns 5, the value of the expression` |
|         - |   92 | ` * 'foo()' is 5. Usually functions don't just return a static value but compute something.` |
|         - |   93 | ` * Of course, values in PHP don't have to be integers, and very often they aren't.` |
|         - |   94 | ` * PHP supports four scalar value types: integer values, floating point values (float), string values and boolean values` |
|         - |   95 | ` * (scalar values are values that you can't 'break' into smaller pieces, unlike arrays, for instance).` |
|         - |   96 | ` * PHP also supports two composite (non-scalar) types: arrays and objects. Each of these value types can be assigned` |
|         - |   97 | ` * into variables or returned from functions.` |
|         - |   98 | ` * PHP takes expressions much further, in the same way many other languages do. PHP is an expression-oriented language` |
|         - |   99 | ` * in the sense that almost everything is an expression. Consider the example we've already dealt with, '$a = 5'.` |
|         - |  100 | ` * It's easy to see that there are two values involved here, the value of the integer constant '5', and the value` |
|         - |  101 | ` * of $a which is being updated to 5 as well. But the truth is that there's one additional value involved here` |
|         - |  102 | ` * and that's the value of the assignment itself. The assignment itself evaluates to the assigned value, in this case 5.` |
|         - |  103 | ` * In practice, it means that '$a = 5', regardless of what it does, is an expression with the value 5. Thus, writing` |
|         - |  104 | ` * something like '$b = ($a = 5)' is like writing '$a = 5; $b = 5;' (a semicolon marks the end of a statement).` |
|         - |  105 | ` * Since assignments are parsed in a right to left order, you can also write '$b = $a = 5'.` |
|         - |  106 | ` * Another good example of expression orientation is pre- and post-increment and decrement.` |
|         - |  107 | ` * Users of PHP and many other languages may be familiar with the notation of variable++ and variable--.` |
|         - |  108 | ` * These are increment and decrement operators. In PHP, like in C, there are two types of increment - pre-increment` |
|         - |  109 | ` * and post-increment. Both pre-increment and post-increment essentially increment the variable, and the effect` |
|         - |  110 | ` * on the variable is identical. The difference is with the value of the increment expression. Pre-increment, which is written` |
|         - |  111 | ` * '++$variable', evaluates to the incremented value (PHP increments the variable before reading its value, thus the name 'pre-increment').` |
|         - |  112 | ` * Post-increment, which is written '$variable++' evaluates to the original value of $variable, before it was incremented` |
|         - |  113 | ` * (PHP increments the variable after reading its value, thus the name 'post-increment').` |
|         - |  114 | ` * A very common type of expressions are comparison expressions. These expressions evaluate to either FALSE or TRUE.` |
|         - |  115 | ` * PHP supports > (bigger than), >= (bigger than or equal to), == (equal), != (not equal), < (smaller than) and <= (smaller than or equal to).` |
|         - |  116 | ` * The language also supports a set of strict equivalence operators: === (equal to and same type) and !== (not equal to or not same type).` |
|         - |  117 | ` * These expressions are most commonly used inside conditional execution, such as if statements.` |
|         - |  118 | ` * The last example of expressions we'll deal with here is combined operator-assignment expressions.` |
|         - |  119 | ` * You already know that if you want to increment $a by 1, you can simply write '$a++' or '++$a'.` |
|         - |  120 | ` * But what if you want to add more than one to it, for instance 3? You could write '$a++' multiple times, but this is obviously not a very` |
|         - |  121 | ` * efficient or comfortable way. A much more common practice is to write '$a = $a + 3'. '$a + 3' evaluates to the value of $a plus 3` |
|         - |  122 | ` * and is assigned back into $a, which results in incrementing $a by 3. In PHP, as in several other languages like C, you can write` |
|         - |  123 | ` * this in a shorter way, which with time would become clearer and quicker to understand as well. Adding 3 to the current value of $a` |
|         - |  124 | ` * can be written '$a += 3'. This means exactly "take the value of $a, add 3 to it, and assign it back into $a".` |
|         - |  125 | ` * In addition to being shorter and clearer, this also results in faster execution. The value of '$a += 3', like the value of a regular` |
|         - |  126 | ` * assignment, is the assigned value. Notice that it is NOT 3, but the combined value of $a plus 3 (this is the value that's assigned into $a).` |
|         - |  127 | ` * Any two-place operator can be used in this operator-assignment mode, for example '$a -= 5' (subtract 5 from the value of $a), '$b *= 7'` |
|         - |  128 | ` * (multiply the value of $b by 7), etc.` |
|         - |  129 | ` * There is one more expression that may seem odd if you haven't seen it in other languages, the ternary conditional operator:` |
|         - |  130 | ` * <?php` |
|         - |  131 | ` * $first ? $second : $third` |
|         - |  132 | ` * ?>` |
|         - |  133 | ` * If the value of the first subexpression is TRUE (non-zero), then the second subexpression is evaluated, and that is the result` |
|         - |  134 | ` * of the conditional expression. Otherwise, the third subexpression is evaluated, and that is the value.` |
|         - |  135 | ` */` |
|         - |  136 | `/* Operators associativity */` |
|         - |  137 | `#define EXPR_OP_ASSOC_LEFT   0x01 /* Left associative operator */` |
|         - |  138 | `#define EXPR_OP_ASSOC_RIGHT  0x02 /* Right associative operator */` |
|         - |  139 | `#define EXPR_OP_NON_ASSOC    0x04 /* Non-associative operator */` |
|         - |  140 | `/*` |
|         - |  141 | ` * Operators table` |
|         - |  142 | ` * This table is sorted by operators priority (highest to lowest) according` |
|         - |  143 | ` * the PHP language reference manual.` |
|         - |  144 | ` * PH7 implements all the 60 PHP operators and have introduced the eq and ne operators.` |
|         - |  145 | ` * The operators precedence table have been improved dramatically so that you can do same` |
|         - |  146 | ` * amazing things now such as array dereferencing,on the fly function call,anonymous function` |
|         - |  147 | ` * as array values,class member access on instantiation and so on.` |
|         - |  148 | ` * Refer to the following page for a full discussion on these improvements:` |
|         - |  149 | ` * http://ph7.symisc.net/features.html#improved_precedence` |
|         - |  150 | ` */` |
|         - |  151 | `static const ph7_expr_op aOpTable[] = {` |
|         - |  152 | `	/* Precedence 1: non-associative */` |
|         - |  153 | `	{ {"new",sizeof("new")-1},     EXPR_OP_NEW,   1, EXPR_OP_NON_ASSOC, PH7_OP_NEW  },` |
|         - |  154 | `	{ {"clone",sizeof("clone")-1}, EXPR_OP_CLONE, 1, EXPR_OP_NON_ASSOC, PH7_OP_CLONE},` |
|         - |  155 | `	                              /* Postfix operators */` |
|         - |  156 | `	/* Precedence 2(Highest),left-associative */` |
|         - |  157 | `	{ {"->",sizeof(char)*2}, EXPR_OP_ARROW,     2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|         - |  158 | `	{ {"?->",sizeof(char)*3},EXPR_OP_NULLSAFE_ARROW, 2, EXPR_OP_ASSOC_LEFT, PH7_OP_MEMBER},` |
|         - |  159 | `	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|         - |  160 | `	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},` |
|         - |  161 | `	/* Precedence 3,non-associative  */` |
|         - |  162 | `	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},` |
|         - |  163 | `	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},` |
|         - |  164 | `	                              /* Unary operators */` |
|         - |  165 | `	/* Precedence 4,right-associative  */` |
|         - |  166 | `	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },` |
|         - |  167 | `	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },` |
|         - |  168 | `	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },` |
|         - |  169 | `	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },` |
|         - |  170 | `	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},` |
|         - |  171 | `	                             /* Cast operators */` |
|         - |  172 | `	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },` |
|         - |  173 | `	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },` |
|         - |  174 | `	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },` |
|         - |  175 | `	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|         - |  176 | `	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},` |
|         - |  177 | `	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },` |
|         - |  178 | `	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },` |
|         - |  179 | ``	/* php 8.5's `(void)`: not a conversion at all — it converts nothing and answers`` |
|         - |  180 | `	 * the operand — but it sits exactly where a cast sits and binds exactly as` |
|         - |  181 | ``	 * tightly. Only the codegen's `for`-clause pass ever hands a token this row`` |
|         - |  182 | ``	 * (GenStateEnableClauseVoidCasts): php's grammar takes `(void)` at the head of`` |
|         - |  183 | ``	 * an expression STATEMENT and at the head of each `for` clause element, and`` |
|         - |  184 | `	 * NOWHERE else, so the token is otherwise left unrecognized on purpose. */` |
|         - |  185 | `	{ {"(void)",   sizeof("(void)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_NOOP     },` |
|         - |  186 | `	                           /* Binary operators */` |
|         - |  187 | `	/* Precedence 5,right-associative: exponentiation (PHP 5.6) */` |
|         - |  188 | `	{ {"**",sizeof(char)*2}, EXPR_OP_POW, 5, EXPR_OP_ASSOC_RIGHT, PH7_OP_POW},` |
|         - |  189 | `	/* Precedence 7,left-associative */` |
|         - |  190 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|         - |  191 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|         - |  192 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|         - |  193 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|         - |  194 | `	/* Precedence 8,left-associative */` |
|         - |  195 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|         - |  196 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|         - |  197 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|         - |  198 | `	/* Precedence 9,left-associative */` |
|         - |  199 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|         - |  200 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|         - |  201 | ``	/* PHP 8.5 pipe operator: `$x \|> f(...)` desugars to `f($x)`. It binds`` |
|         - |  202 | `	 * looser than shift/arithmetic and tighter than comparison — PHP places it` |
|         - |  203 | `	 * between precedence 9 and 10. We share level 9 (left-associative) so the` |
|         - |  204 | `	 * generic binary tree-builder links it correctly; the actual codegen is` |
|         - |  205 | `	 * custom (a one-argument call of the RHS callable), handled in` |
|         - |  206 | `	 * GenStateEmitExprCode. iVmOp is 0 like the other codegen-only operators. */` |
|         - |  207 | `	{ {"\|>",sizeof(char)*2}, EXPR_OP_PIPE, 9, EXPR_OP_ASSOC_LEFT, 0},` |
|         - |  208 | `	/* Precedence 10,non-associative */` |
|         - |  209 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|         - |  210 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|         - |  211 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|         - |  212 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|         - |  213 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|         - |  214 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|         - |  215 | `	/* Precedence 11,non-associative */` |
|         - |  216 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|         - |  217 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|         - |  218 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|         - |  219 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|         - |  220 | `	/* Precedence 12,left-associative */` |
|         - |  221 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|         - |  222 | `	/* Precedence 12,left-associative */` |
|         - |  223 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|         - |  224 | `	                         /* Binary operators */` |
|         - |  225 | `	/* Precedence 13,left-associative */` |
|         - |  226 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|         - |  227 | `	/* Precedence 14,left-associative */` |
|         - |  228 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|         - |  229 | `	/* Precedence 15,left-associative */` |
|         - |  230 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|         - |  231 | `	/* Precedence 16,left-associative */` |
|         - |  232 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|         - |  233 | `	                      /* Null coalescing operator */` |
|         - |  234 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|         - |  235 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|         - |  236 | `	                      /* Ternary operator */` |
|         - |  237 | `	/* Precedence 17,left-associative */` |
|         - |  238 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|         - |  239 | `	                     /* Combined binary operators */` |
|         - |  240 | `	/* Precedence 18,right-associative */` |
|         - |  241 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|         - |  242 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|         - |  243 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|         - |  244 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|         - |  245 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|         - |  246 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|         - |  247 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|         - |  248 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|         - |  249 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|         - |  250 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|         - |  251 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|         - |  252 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|         - |  253 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|         - |  254 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|         - |  255 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|         - |  256 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|         - |  257 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|         - |  258 | `	 * in this file: keep one of the question marks escaped. */` |
|         - |  259 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|         - |  260 | `	/* Precedence 19,left-associative */` |
|         - |  261 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|         - |  262 | `	/* Precedence 20,left-associative */` |
|         - |  263 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|         - |  264 | `	/* Precedence 21,left-associative */` |
|         - |  265 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|         - |  266 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|         - |  267 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|         - |  268 | `};` |
|         - |  269 | `/* Function call operator need special handling */` |
|         - |  270 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|         - |  271 | `/*` |
|         - |  272 | ` * Check if the given token is a potential operator or not.` |
|         - |  273 | ` * This function is called by the lexer each time it extract a token that may` |
|         - |  274 | ` * look like an operator.` |
|         - |  275 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|         - |  276 | ` * Otherwise NULL.` |
|         - |  277 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|         - |  278 | ` * a binary minus or unary minus.]` |
|         - |  279 | ` */` |
|   3530406 |  280 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  281 | `{` |
|   3530411 |  282 | `	sxu32 n = 0;` |
|         - |  283 | `	sxi32 rc;` |
|         - |  284 | `	/* Do a linear lookup on the operators table */` |
|  71633082 |  285 | `	for(;;){` |
| 143266169 |  286 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  287 | `			break;` |
|         - |  288 | `		}` |
| 143266169 |  289 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  290 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  11737427 |  291 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   5868716 |  292 | `		}else{` |
| 131528747 |  293 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  294 | `		}` |
| 143266169 |  295 | `		if( rc == 0 ){` |
|   3624719 |  296 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  297 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   3481653 |  298 | `				return &aOpTable[n];` |
|         - |  299 | `			}` |
|         - |  300 | `			/* Handle ambiguity */` |
|    143071 |  301 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  302 | `				/* Unary opertors have prcedence here over binary operators */` |
|     19179 |  303 | `				return &aOpTable[n];` |
|         - |  304 | `			}` |
|    123897 |  305 | `			if( pLast->nType & PH7_TK_OP ){` |
|     29597 |  306 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  307 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     29597 |  308 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  309 | `					/* Unary opertors have prcedence here over binary operators */` |
|     29589 |  310 | `					return &aOpTable[n];` |
|         - |  311 | `				}` |
|         - |  312 |  |
|         4 |  313 | `			}` |
|     47154 |  314 | `		}` |
| 139735763 |  315 | `		++n; /* Next operator in the table */` |
|         5 |  316 | `	}` |
|         - |  317 | `	/* No such operator */` |
|       ! 0 |  318 | `	return 0;` |
|   1765208 |  319 | `}` |
|         - |  320 | `/*` |
|         - |  321 | ` * Delimit a set of token stream.` |
|         - |  322 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  323 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  324 | ` */` |
|    903432 |  325 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  326 | `{` |
|    903437 |  327 | `	SyToken *pCur = pIn;` |
|    903437 |  328 | `	sxi32 iNest = 1;` |
|   3794326 |  329 | `	for(;;){` |
|   7588657 |  330 | `		if( pCur >= pEnd ){` |
|        23 |  331 | `			break;` |
|         - |  332 | `		}` |
|   7588639 |  333 | `		if( pCur->nType & nTokStart ){` |
|         - |  334 | `			/* Increment nesting level */` |
|    408555 |  335 | `			iNest++;` |
|   7384364 |  336 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  337 | `			/* Decrement nesting level */` |
|   1311969 |  338 | `			iNest--;` |
|   1311969 |  339 | `			if( iNest <= 0 ){` |
|    903419 |  340 | `				break;` |
|         - |  341 | `			}` |
|    204275 |  342 | `		}` |
|         - |  343 | `		/* Advance cursor */` |
|   6685225 |  344 | `		pCur++;` |
|         5 |  345 | `	}` |
|         - |  346 | `	/* Point to the end of the chunk */` |
|    903437 |  347 | `	*ppEnd = pCur;` |
|    903437 |  348 | `}` |
|         - |  349 | `/*` |
|         - |  350 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  351 | ` * Note on reserved keywords.` |
|         - |  352 | ` *  According to the PHP language reference manual:` |
|         - |  353 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  354 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  355 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  356 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  357 | ` */` |
|     14222 |  358 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  359 | `{` |
|     14222 |  360 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     14143 |  361 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  362 | `		){` |
|       327 |  363 | `			return TRUE;` |
|         - |  364 | `	}` |
|     13905 |  365 | `	if( bCheckFunc ){` |
|       688 |  366 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       652 |  367 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       599 |  368 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       119 |  369 | `				return TRUE;` |
|         - |  370 | `		}` |
|       287 |  371 | `	}` |
|         - |  372 | `	/* Not a language construct */` |
|     13791 |  373 | `	return FALSE;` |
|      7116 |  374 | `}` |
|         - |  375 | `/*` |
|         - |  376 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  377 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  378 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  379 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  380 | ` */` |
|   2326664 |  381 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  382 | `{` |
|         - |  383 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  384 | `	sxi32 i,rc;` |
|         - |  385 |  |
|   2326669 |  386 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  387 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       138 |  388 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       138 |  389 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        67 |  390 | `	}` |
|   2326669 |  391 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  13650523 |  392 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  11323897 |  393 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  394 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     14819 |  395 | `			continue;` |
|         - |  396 | `		}` |
|  11309083 |  397 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|         - |  398 | `			/* A short-array literal is a SELF-CONTAINED node whose start token is '['` |
|         - |  399 | `			 * (its ']' was consumed), so the raw-token CSB test below can never see it —` |
|         - |  400 | ``			 * `[$obj, 'm']()` parsed the '(' as a grouping paren and silently DROPPED`` |
|         - |  401 | `			 * the call (the expression evaluated to the array). php invokes the literal` |
|         - |  402 | `			 * array callable exactly like the variable-held form. */` |
|   1080892 |  403 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    116528 |  404 | `				apNode[i-1]->xCode == PH7_CompileShortArray \|\|` |
|    116510 |  405 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  406 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis.` |
|         - |  407 | `					 * A self-contained short-array node is exempt: its start token is '[',` |
|         - |  408 | `					 * which carries PH7_TK_OP as the subscript operator, but the node is a` |
|         - |  409 | ``					 * complete array-literal TERM — `[$obj, 'm'](...)` is a call. */`` |
|    853670 |  410 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0` |
|    426855 |  411 | `					 \|\| apNode[i-1]->xCode == PH7_CompileShortArray ){` |
|         - |  412 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  413 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  414 | `						 */` |
|    853663 |  415 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    853663 |  416 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    853663 |  417 | `						apNode[i]->pOp = &sFCallOp;` |
|    426829 |  418 | `					}` |
|    426835 |  419 | `			}` |
|   1022637 |  420 | `			iParen++;` |
|  10797767 |  421 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   1022637 |  422 | `			if( iParen <= 0 ){` |
|        18 |  423 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        18 |  424 | `				if( rc != SXERR_ABORT ){` |
|        18 |  425 | `					rc = SXERR_SYNTAX;` |
|         7 |  426 | `				}` |
|        18 |  427 | `				return rc;` |
|         - |  428 | `			}` |
|   1022623 |  429 | `			iParen--;` |
|   9775128 |  430 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    249437 |  431 | `			iSquare++;` |
|   9139103 |  432 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    249441 |  433 | `			if( iSquare <= 0 ){` |
|         8 |  434 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         8 |  435 | `				if( rc != SXERR_ABORT ){` |
|         8 |  436 | `					rc = SXERR_SYNTAX;` |
|         3 |  437 | `				}` |
|         8 |  438 | `				return rc;` |
|         - |  439 | `			}` |
|    249435 |  440 | `			iSquare--;` |
|   8889666 |  441 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|        77 |  442 | `			iBraces++;` |
|        77 |  443 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  444 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  445 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  446 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  447 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  448 | `				if( rc != SXERR_ABORT ){` |
|         3 |  449 | `					rc = SXERR_SYNTAX;` |
|         1 |  450 | `				}` |
|         3 |  451 | `				return rc;` |
|         2 |  452 | `			}` |
|   8764913 |  453 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|        89 |  454 | `			if( iBraces <= 0 ){` |
|        16 |  455 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        16 |  456 | `				if( rc != SXERR_ABORT ){` |
|        16 |  457 | `					rc = SXERR_SYNTAX;` |
|         6 |  458 | `				}` |
|        16 |  459 | `				return rc;` |
|         - |  460 | `			}` |
|        74 |  461 | `			iBraces--;` |
|   8764829 |  462 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     44857 |  463 | `			if( iQuesty > 0 ){` |
|     44227 |  464 | `				iQuesty--;` |
|     22746 |  465 | `			}else if( iParen <= 0 ){` |
|         - |  466 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  467 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  468 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  469 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  470 | `				if( rc != SXERR_ABORT ){` |
|         6 |  471 | `					rc = SXERR_SYNTAX;` |
|         2 |  472 | `				}` |
|         6 |  473 | `				return rc;` |
|         5 |  474 | `			}` |
|   8742365 |  475 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   2902101 |  476 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   2902101 |  477 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     44229 |  478 | `				iQuesty++;` |
|   2879989 |  479 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     36901 |  480 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|         9 |  481 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|         9 |  482 | `					sxu32 n = 0;` |
|         9 |  483 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|         5 |  484 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|         2 |  485 | `					}` |
|         - |  486 | `					/*` |
|         - |  487 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|         - |  488 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|         - |  489 | `					 */` |
|       221 |  490 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|       213 |  491 | `						++n;` |
|         1 |  492 | `					}` |
|         9 |  493 | `					pOp = &aOpTable[n];` |
|         - |  494 | `					/* Mark as binary '+' or '-',not an unary */` |
|         9 |  495 | `					apNode[i]->pOp = pOp;` |
|         9 |  496 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|         4 |  497 | `				}` |
|     18448 |  498 | `			}` |
|   1451048 |  499 | `		}` |
|   5654525 |  500 | `	}` |
|   2326631 |  501 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        18 |  502 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        18 |  503 | `		if( rc != SXERR_ABORT ){` |
|        18 |  504 | `			rc = SXERR_SYNTAX;` |
|         7 |  505 | `		}` |
|        18 |  506 | `		return rc;` |
|         - |  507 | `	}` |
|   2326617 |  508 | `	return SXRET_OK;` |
|   1163337 |  509 | `}` |
|         - |  510 | `/*` |
|         - |  511 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  512 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  513 | ` */` |
|   1330318 |  514 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  515 | `{` |
|   1330323 |  516 | `	SyToken *pIn = *ppCur;` |
|         - |  517 | `	/* Jump the first literal seen */` |
|   1330323 |  518 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   1330193 |  519 | `		pIn++;` |
|    665094 |  520 | `	}` |
|    665452 |  521 | `	for(;;){` |
|   1330909 |  522 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       591 |  523 | `			pIn++;` |
|       591 |  524 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       589 |  525 | `				pIn++;` |
|       292 |  526 | `			}` |
|       298 |  527 | `		}else{` |
|    665164 |  528 | `			break;` |
|         - |  529 | `		}` |
|         5 |  530 | `	}` |
|         - |  531 | `	/* Synchronize pointers */` |
|   1330323 |  532 | `	*ppCur = pIn;` |
|   1330323 |  533 | `}` |
|         - |  534 | `/*` |
|         - |  535 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|         - |  536 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  537 | ` * Note on annonymous functions.` |
|         - |  538 | ` *  According to the PHP language reference manual:` |
|         - |  539 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  540 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  541 | ` *  parameters, but they have many other uses.` |
|         - |  542 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|         - |  543 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|         - |  544 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|         - |  545 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|         - |  546 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|         - |  547 | ` *` |
|         - |  548 | ` * Some example:` |
|         - |  549 | ` *  $greet = function($name)` |
|         - |  550 | ` * {` |
|         - |  551 | ` *   printf("Hello %s\r\n", $name);` |
|         - |  552 | ` * };` |
|         - |  553 | ` *  $greet('World');` |
|         - |  554 | ` *  $greet('PHP');` |
|         - |  555 | ` *` |
|         - |  556 | ` * $double = function($a) {` |
|         - |  557 | ` *   return $a * 2;` |
|         - |  558 | ` * };` |
|         - |  559 | ` * // This is our range of numbers` |
|         - |  560 | ` * $numbers = range(1, 5);` |
|         - |  561 | ` * // Use the Annonymous function as a callback here to` |
|         - |  562 | ` * // double the size of each element in our` |
|         - |  563 | ` * // range` |
|         - |  564 | ` * $new_numbers = array_map($double, $numbers);` |
|         - |  565 | ` * print implode(' ', $new_numbers);` |
|         - |  566 | ` */` |
|         - |  567 | `/*` |
|         - |  568 | ` * Skip an optional return-type declaration at *ppIn:` |
|         - |  569 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|         - |  570 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|         - |  571 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|         - |  572 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|         - |  573 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|         - |  574 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|         - |  575 | ` * authoritative type parse, so this must accept every shape it does` |
|         - |  576 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|         - |  577 | ` */` |
|     11820 |  578 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  579 | `{` |
|     11825 |  580 | `	SyToken *pIn = *ppIn;` |
|     11825 |  581 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        92 |  582 | `		pIn++; /* Skip ':' */` |
|        44 |  583 | `		for(;;){` |
|         - |  584 | `			/* Optional '?' nullable prefix */` |
|        96 |  585 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|         6 |  586 | `				pIn++;` |
|         2 |  587 | `			}` |
|        96 |  588 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  589 | `				/* Parenthesized DNF group '(A&B)' */` |
|       ! 0 |  590 | `				pIn++;` |
|       ! 0 |  591 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       ! 0 |  592 | `				if( pIn < pEnd ){` |
|       ! 0 |  593 | `					pIn++; /* ')' */` |
|       ! 0 |  594 | `				}` |
|        92 |  595 | `			}else if( pIn < pEnd` |
|        96 |  596 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  597 | `				/* ['\']Name('\'Name)* */` |
|        96 |  598 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|        96 |  599 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        96 |  600 | `					pIn++;` |
|        96 |  601 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  602 | `						pIn += 2;` |
|       ! 0 |  603 | `					}` |
|        46 |  604 | `				}` |
|        50 |  605 | `			}else{` |
|         - |  606 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  607 | `				break;` |
|         - |  608 | `			}` |
|         - |  609 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|        92 |  610 | `			if( pIn < pEnd` |
|        96 |  611 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|        92 |  612 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|         5 |  613 | `				pIn++;` |
|         5 |  614 | `				continue;` |
|         - |  615 | `			}` |
|        92 |  616 | `			break;` |
|       ! 0 |  617 | `		}` |
|        44 |  618 | `	}` |
|     11825 |  619 | `	*ppIn = pIn;` |
|     11825 |  620 | `}` |
|      4770 |  621 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  622 | `{` |
|      4775 |  623 | `	SyToken *pIn = *ppCur;` |
|         - |  624 | `	sxi32 rc;` |
|         - |  625 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  626 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  627 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  628 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  629 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|      4775 |  630 | `	pIn++;` |
|      4770 |  631 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      2548 |  632 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|       319 |  633 | `		pIn++;` |
|       157 |  634 | `	}` |
|      4775 |  635 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  636 | `		/* Syntax error */` |
|         6 |  637 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  638 | `		if( rc != SXERR_ABORT ){` |
|         6 |  639 | `			rc = SXERR_SYNTAX;` |
|         2 |  640 | `		}` |
|         6 |  641 | `		goto Synchronize;` |
|         - |  642 | `	}` |
|      4771 |  643 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      4771 |  644 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      4771 |  645 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  646 | `		/* Two different failures used to share this arm and both claimed the body was` |
|         - |  647 | `		 * missing. They are distinguishable: the delimiter search leaves pIn ON the` |
|         - |  648 | `		 * ')' when it found one, and AT pEnd when it did not.` |
|         - |  649 | ``		 *   pIn >= pEnd      the parameter list never closed (`function($x {`)`` |
|         - |  650 | `		 *                    -> php expects ')'` |
|         - |  651 | `		 *   &pIn[1] >= pEnd  ')' closed it but nothing follows -> php expects '{'` |
|         - |  652 | `		 * php names the token that actually comes next, which lives just past the` |
|         - |  653 | `		 * expression slice, still in the raw stream. */` |
|         6 |  654 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         6 |  655 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,pIn >= pEnd ? "\")\"" : "\"{\"");` |
|         6 |  656 | `		if( rc != SXERR_ABORT ){` |
|         6 |  657 | `			rc = SXERR_SYNTAX;` |
|         2 |  658 | `		}` |
|         6 |  659 | `		goto Synchronize;` |
|         - |  660 | `	}` |
|      4767 |  661 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  662 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      4767 |  663 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      4767 |  664 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|      1259 |  665 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  666 | `		/* Check if we are dealing with a closure */` |
|      1259 |  667 | `		if( nKey == PH7_TKWRD_USE ){` |
|      1251 |  668 | `			pIn++; /* Jump the 'use' keyword */` |
|      1251 |  669 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  670 | `				/* Syntax error */` |
|         6 |  671 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  672 | `				if( rc != SXERR_ABORT ){` |
|         6 |  673 | `					rc = SXERR_SYNTAX;` |
|         2 |  674 | `				}` |
|         6 |  675 | `				goto Synchronize;` |
|         - |  676 | `			}` |
|      1247 |  677 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|         - |  678 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|         - |  679 | `			 * has no nested structure to balance here, so the first token that is not` |
|         - |  680 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|         - |  681 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|         - |  682 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|         - |  683 | `			 * the first foreign token. */` |
|         - |  684 | `			{` |
|      1247 |  685 | `				SyToken *pUse = pIn;` |
|      1247 |  686 | `				int bClosed = 0;` |
|      4471 |  687 | `				while( pUse < pEnd ){` |
|      4471 |  688 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|      3231 |  689 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      3229 |  690 | `						pUse++;` |
|      3229 |  691 | `						continue;` |
|         - |  692 | `					}` |
|         3 |  693 | `					break; /* foreign token: php names this one */` |
|       ! 0 |  694 | `				}` |
|      1247 |  695 | `				if( !bClosed ){` |
|         - |  696 | `					/* php names the offending token and expects ')'; if the list simply` |
|         - |  697 | `					 * ran off the end of the slice, that token sits just past it. */` |
|         3 |  698 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|         3 |  699 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|         3 |  700 | `					if( rc != SXERR_ABORT ){` |
|         3 |  701 | `						rc = SXERR_SYNTAX;` |
|         1 |  702 | `					}` |
|         3 |  703 | `					goto Synchronize;` |
|         - |  704 | `				}` |
|      1245 |  705 | `				pIn = pUse; /* on the ')' */` |
|         - |  706 | `			}` |
|      1245 |  707 | `			if( &pIn[1] >= pEnd ){` |
|         - |  708 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|         3 |  709 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         3 |  710 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         3 |  711 | `				if( rc != SXERR_ABORT ){` |
|         3 |  712 | `					rc = SXERR_SYNTAX;` |
|         1 |  713 | `				}` |
|         3 |  714 | `				goto Synchronize;` |
|         - |  715 | `			}` |
|      1243 |  716 | `			pIn++;` |
|         - |  717 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  718 | ``			 * `function (...) use (...) : int {` */`` |
|      1243 |  719 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       624 |  720 | `		}else{` |
|         - |  721 | `			/* Syntax error */` |
|        11 |  722 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        11 |  723 | `			if( rc != SXERR_ABORT ){` |
|        11 |  724 | `				rc = SXERR_SYNTAX;` |
|         4 |  725 | `			}` |
|        11 |  726 | `			goto Synchronize;` |
|         - |  727 | `		}` |
|       619 |  728 | `	}` |
|         - |  729 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  730 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  731 | `	 * the type), and pEnd is one past the last token. */` |
|      4751 |  732 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      4751 |  733 | `		pIn++; /* Jump the leading curly '{' */` |
|      4751 |  734 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      4751 |  735 | `		if( pIn < pEnd ){` |
|      4751 |  736 | `			pIn++;` |
|      2373 |  737 | `		}` |
|      2378 |  738 | `	}else{` |
|         - |  739 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|         - |  740 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|         - |  741 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|         - |  742 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|       ! 0 |  743 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|       ! 0 |  744 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|       ! 0 |  745 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  746 | `			return SXERR_ABORT;` |
|         - |  747 | `		}` |
|         - |  748 | `	}` |
|      4751 |  749 | `	rc = SXRET_OK;` |
|      2385 |  750 | `Synchronize:` |
|         - |  751 | `	/* Synchronize pointers */` |
|      4775 |  752 | `	*ppCur = pIn;` |
|      4775 |  753 | `	return rc;` |
|      2390 |  754 | `}` |
|         - |  755 | `/*` |
|         - |  756 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  757 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  758 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  759 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  760 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  761 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  762 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  763 | ` */` |
|        80 |  764 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  765 | `{` |
|        85 |  766 | `	SyToken *pIn = *ppCur;` |
|        85 |  767 | `	sxu32 nLine = pIn->nLine;` |
|         - |  768 | `	sxi32 rc;` |
|        85 |  769 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  770 | `	/* Optional constructor argument list */` |
|        85 |  771 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        20 |  772 | `		pIn++; /* Jump '(' */` |
|        20 |  773 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        20 |  774 | `		if( pIn < pEnd ){` |
|        20 |  775 | `			pIn++; /* Jump ')' */` |
|         9 |  776 | `		}` |
|         9 |  777 | `	}` |
|         - |  778 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  779 | `	 * (no braces appear between ')' and the class body). */` |
|       215 |  780 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       135 |  781 | `		pIn++;` |
|         5 |  782 | `	}` |
|        85 |  783 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  784 | `		/* Syntax error: missing class body */` |
|       ! 0 |  785 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  786 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  787 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  788 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  789 | `		}` |
|       ! 0 |  790 | `		*ppCur = pIn;` |
|       ! 0 |  791 | `		return rc;` |
|         - |  792 | `	}` |
|        85 |  793 | `	pIn++; /* Jump the leading '{' */` |
|        85 |  794 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        85 |  795 | `	if( pIn < pEnd ){` |
|        85 |  796 | `		pIn++; /* Jump the trailing '}' */` |
|        40 |  797 | `	}` |
|        85 |  798 | `	*ppCur = pIn;` |
|        85 |  799 | `	return SXRET_OK;` |
|        45 |  800 | `}` |
|         - |  801 | `/*` |
|         - |  802 | ` * TRUE when a KEYWORD token actually OPENS an arrow function.` |
|         - |  803 | ` *` |
|         - |  804 | `` * `fn` is reserved, but it only ever introduces `[static] fn[&](…) => expr`.`` |
|         - |  805 | ``  * Everywhere php expects a NAME the same word is an ordinary identifier: `$fn` `` |
|         - |  806 | ` * (the lexer emits '$' plus the keyword, so the keyword IS the variable name),` |
|         - |  807 | ``  * `$fn(…)` calling that variable, `C::fn`, `$o->fn`, `\A\fn`, and the `fn:` `` |
|         - |  808 | ` * named-argument label. Every raw-token lookahead that steps over an arrow` |
|         - |  809 | ` * function has to make that distinction or it swallows a plain name and loses` |
|         - |  810 | `` * the '=>' that follows it (`[$fn => 1]` became `syntax error, unexpected token`` |
|         - |  811 | `` * "=>"`).`` |
|         - |  812 | ` *` |
|         - |  813 | `` * The test is POSITIONAL, never "is it well formed": a malformed `fn` (`fn $x`` |
|         - |  814 | `` * => $x`, or a bare `fn` used as a key) must still reach the arrow parser,`` |
|         - |  815 | `` * which is what reports php's `expecting "("`. Two name positions:`` |
|         - |  816 | ` *   - member/variable/namespace: '$', '->', '?->', '::' or '\' immediately` |
|         - |  817 | ` *     before the word;` |
|         - |  818 | ``  *   - a named-argument LABEL: a bare `fn` directly before ':' (`static fn:` `` |
|         - |  819 | `` *     and `fn&:` cannot be labels, so they stay the arrow parser's business).`` |
|         - |  820 | ` *     The argument list is re-parsed from the argument's own first token, so` |
|         - |  821 | ` *     there is no '(' to look back at — the label test cannot be scoped to` |
|         - |  822 | `` *     call context, and the degenerate `true ? fn : 0` (only reachable through`` |
|         - |  823 | ` *     define('fn',…), which php itself cannot parse) is accepted as a` |
|         - |  824 | ` *     constant instead of rejected. A recorded divergence; rejecting it` |
|         - |  825 | `` *     would cost the real `f(fn: 1)` spelling.`` |
|         - |  826 | `` * pStart bounds the look-back; pTok may point at `static`, which must then be`` |
|         - |  827 | `` * followed by `fn`.`` |
|         - |  828 | ` */` |
|      6734 |  829 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd)` |
|         5 |  830 | `{` |
|      6739 |  831 | `	int bStatic = FALSE;` |
|      6739 |  832 | `	if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  833 | `		return FALSE;` |
|         - |  834 | `	}` |
|      6739 |  835 | `	if( pTok > pStart ){` |
|       401 |  836 | `		SyToken *pPrev = &pTok[-1];` |
|       401 |  837 | `		if( pPrev->nType & (PH7_TK_DOLLAR\|PH7_TK_NSSEP) ){` |
|        80 |  838 | `			return FALSE; /* $fn / \A\fn — the keyword IS the name */` |
|         - |  839 | `		}` |
|       325 |  840 | `		if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData ){` |
|       153 |  841 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)pPrev->pUserData;` |
|       148 |  842 | `			if( pOp->iOp == EXPR_OP_ARROW \|\| pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       144 |  843 | `			 \|\| pOp->iOp == EXPR_OP_DC ){` |
|        60 |  844 | `				return FALSE; /* $o->fn, $o?->fn, C::fn — a member name */` |
|         - |  845 | `			}` |
|        46 |  846 | `		}` |
|       132 |  847 | `	}` |
|      6607 |  848 | `	if( SX_PTR_TO_INT(pTok->pUserData) == PH7_TKWRD_STATIC ){` |
|       166 |  849 | `		bStatic = TRUE;` |
|       166 |  850 | `		pTok++;` |
|       166 |  851 | `		if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       102 |  852 | `			return FALSE;` |
|         - |  853 | `		}` |
|        32 |  854 | `	}` |
|      6509 |  855 | `	if( SX_PTR_TO_INT(pTok->pUserData) != PH7_TKWRD_FN ){` |
|       381 |  856 | `		return FALSE;` |
|         - |  857 | `	}` |
|      6133 |  858 | `	if( !bStatic && &pTok[1] < pEnd && (pTok[1].nType & PH7_TK_COLON) ){` |
|       ! 0 |  859 | `		return FALSE; /* f(fn: 1) — a named-argument label */` |
|         - |  860 | `	}` |
|      6133 |  861 | `	return TRUE;` |
|      3372 |  862 | `}` |
|         - |  863 | `/*` |
|         - |  864 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  865 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  866 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  867 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  868 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  869 | ` */` |
|      5820 |  870 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  871 | `{` |
|      5825 |  872 | `	SyToken *pIn = *ppCur;` |
|         - |  873 | `	sxu32 nLine;` |
|         - |  874 | `	sxi32 rc;` |
|         - |  875 | `	int iNest;` |
|      5825 |  876 | `	nLine = pIn->nLine;` |
|         - |  877 | `	/* Optional 'static' prefix */` |
|      5820 |  878 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      5825 |  879 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        61 |  880 | `		pIn++;` |
|        30 |  881 | `	}` |
|         - |  882 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      5820 |  883 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      5825 |  884 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  885 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  886 | `		goto Synchronize;` |
|         - |  887 | `	}` |
|      5825 |  888 | `	pIn++; /* Jump 'fn' */` |
|      2910 |  889 | `	SXUNUSED(nLine);` |
|      2910 |  890 | `	SXUNUSED(pGen);` |
|         - |  891 | `	/* Optional '&' for return-by-reference */` |
|      5825 |  892 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  893 | `		pIn++;` |
|       ! 0 |  894 | `	}` |
|         - |  895 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  896 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  897 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  898 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      5825 |  899 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      5823 |  900 | `		pIn++; /* '(' */` |
|      5823 |  901 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      5823 |  902 | `		if( pIn < pEnd ){` |
|      5821 |  903 | `			pIn++; /* ')' */` |
|      2908 |  904 | `		}` |
|      2909 |  905 | `	}` |
|         - |  906 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      5825 |  907 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  908 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      5825 |  909 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      5819 |  910 | `		pIn++;` |
|      2907 |  911 | `	}` |
|         - |  912 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      5825 |  913 | `	iNest = 0;` |
|     59725 |  914 | `	while( pIn < pEnd ){` |
|     58995 |  915 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  916 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      5095 |  917 | `			break;` |
|         - |  918 | `		}` |
|     53905 |  919 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      9709 |  920 | `			iNest++;` |
|     49053 |  921 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      9709 |  922 | `			iNest--;` |
|      4852 |  923 | `		}` |
|     53905 |  924 | `		pIn++;` |
|         5 |  925 | `	}` |
|      5825 |  926 | `	rc = SXRET_OK;` |
|      2910 |  927 | `Synchronize:` |
|      5825 |  928 | `	*ppCur = pIn;` |
|      5825 |  929 | `	return rc;` |
|         5 |  930 | `}` |
|         - |  931 | `/*` |
|         - |  932 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - |  933 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - |  934 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - |  935 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - |  936 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - |  937 | ` */` |
|       124 |  938 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         4 |  939 | `{` |
|       128 |  940 | `	SyToken *pIn = *ppCur;` |
|         - |  941 | `	sxi32 rc;` |
|        62 |  942 | `	SXUNUSED(pGen);` |
|         - |  943 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       124 |  944 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       128 |  945 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 |  946 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  947 | `		goto Synchronize;` |
|         - |  948 | `	}` |
|       128 |  949 | `	pIn++; /* Jump 'match' */` |
|         - |  950 | `	/* Optional '(' subject ')' */` |
|       128 |  951 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       128 |  952 | `		pIn++;` |
|       128 |  953 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       128 |  954 | `		if( pIn < pEnd ){` |
|       128 |  955 | `			pIn++; /* ')' */` |
|        62 |  956 | `		}` |
|        62 |  957 | `	}` |
|         - |  958 | `	/* Optional '{' arms '}' */` |
|       128 |  959 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       128 |  960 | `		pIn++;` |
|       128 |  961 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       128 |  962 | `		if( pIn < pEnd ){` |
|       128 |  963 | `			pIn++; /* '}' */` |
|        62 |  964 | `		}` |
|        62 |  965 | `	}` |
|       128 |  966 | `	rc = SXRET_OK;` |
|        62 |  967 | `Synchronize:` |
|       128 |  968 | `	*ppCur = pIn;` |
|       128 |  969 | `	return rc;` |
|         4 |  970 | `}` |
|         - |  971 | `/*` |
|         - |  972 | `` * PHP 8.5 `clone (`: tell the clone() CALL form from the clone OPERATOR applied to a`` |
|         - |  973 | ` * parenthesised operand. php's grammar has both, and its parser resolves the conflict` |
|         - |  974 | ` * by continuing the parenthesised expression whenever the token after the ')' can` |
|         - |  975 | ``  * dereference it — so `clone ($a)->b()` clones what `b()` returns, `clone ($c)[0]` `` |
|         - |  976 | `` * clones the ELEMENT and `clone ($f)()` clones the call's result, while a plain`` |
|         - |  977 | `` * `clone ($a)` (nothing dereferencing) is the one-argument call, which means the same`` |
|         - |  978 | `` * thing either way. PHL took the call form for every `clone (`, so the receiver was`` |
|         - |  979 | ` * cloned and the member access ran on the ORIGINAL — a silent wrong answer with no` |
|         - |  980 | `` * diagnostic, out of `clone (new A)->b()`.`` |
|         - |  981 | ` *` |
|         - |  982 | ` * pClone points at the 'clone' token and pClone[1] at its '('. Returns TRUE when the` |
|         - |  983 | ` * call-form branch should take the tokens (including the unterminated case, which that` |
|         - |  984 | ` * branch reports), FALSE to leave them to the precedence-1 operator path.` |
|         - |  985 | ` */` |
|        68 |  986 | `static int CloneCallFormFollows(SyToken *pClone,SyToken *pEnd)` |
|         2 |  987 | `{` |
|        70 |  988 | `	SyToken *pNext = &pClone[2]; /* first token inside the '(' */` |
|        70 |  989 | `	PH7_DelimitNestedTokens(pNext,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pNext);` |
|        70 |  990 | `	if( pNext >= pEnd ){` |
|       ! 0 |  991 | `		return TRUE; /* unterminated '(' — the call-form branch raises php's ')' error */` |
|         - |  992 | `	}` |
|        70 |  993 | `	pNext++; /* step past the matching ')' */` |
|        70 |  994 | `	if( pNext >= pEnd ){` |
|        52 |  995 | `		return TRUE;` |
|         - |  996 | `	}` |
|        19 |  997 | `	if( pNext->nType & (PH7_TK_OSB /*'['*/\|PH7_TK_LPAREN /*'('*/) ){` |
|         5 |  998 | `		return FALSE;` |
|         - |  999 | `	}` |
|        15 | 1000 | `	if( (pNext->nType & PH7_TK_OP) && pNext->pUserData ){` |
|         9 | 1001 | `		sxi32 iOp = ((const ph7_expr_op *)pNext->pUserData)->iOp;` |
|         9 | 1002 | `		if( iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW \|\| iOp == EXPR_OP_DC ){` |
|         9 | 1003 | `			return FALSE;` |
|         - | 1004 | `		}` |
|       ! 0 | 1005 | `	}` |
|         7 | 1006 | `	return TRUE;` |
|        36 | 1007 | `}` |
|         - | 1008 | `/*` |
|         - | 1009 | ` * Extract a single expression node from the input.` |
|         - | 1010 | ` * On success store the freshly extractd node in ppNode.` |
|         - | 1011 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1012 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - | 1013 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - | 1014 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - | 1015 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - | 1016 | ` */` |
|         - | 1017 | `/*` |
|         - | 1018 | `` * Where a KEYWORD-headed operand ends: `yield <expr>`, `throw <expr>` and the`` |
|         - | 1019 | `` * one-operand language constructs (`print`, `include`, `require`, …) each take`` |
|         - | 1020 | ` * the rest of the enclosing group, so PH7_DelimitNestedTokens is the right shape` |
|         - | 1021 | `` * for them — EXCEPT that php's grammar gives them `expr`, and a top-level COMMA`` |
|         - | 1022 | `` * is not part of an `expr`. In an ARGUMENT LIST that comma is the separator, so`` |
|         - | 1023 | `` * `f(yield 1, 2)` and `f(print "p", 2)` are two arguments in php; here the`` |
|         - | 1024 | `` * operand ran straight past it and the leftover `, 2` came back as`` |
|         - | 1025 | `` * `syntax error, unexpected token ","` on source php runs. (An ARRAY literal`` |
|         - | 1026 | ` * never showed it: its body is re-split on commas before these nodes are ever` |
|         - | 1027 | ` * extracted.) At statement level nothing legitimate follows such an operand with` |
|         - | 1028 | `` * a comma, so stopping is php's answer there too — `yield 1, 2;` and`` |
|         - | 1029 | `` * `print "a", "b";` stay the parse error both engines already gave.`` |
|         - | 1030 | ` */` |
|       758 | 1031 | `static void ExprDelimitKeywordOperand(SyToken *pIn,SyToken *pEnd,SyToken **ppEnd)` |
|         5 | 1032 | `{` |
|       763 | 1033 | `	SyToken *pCur = pIn;` |
|       763 | 1034 | `	sxi32 iNest = 1;` |
|      1399 | 1035 | `	for(;;){` |
|      2803 | 1036 | `		if( pCur >= pEnd ){` |
|       713 | 1037 | `			break;` |
|         - | 1038 | `		}` |
|      2095 | 1039 | `		if( (pCur->nType & PH7_TK_COMMA) && iNest <= 1 ){` |
|        11 | 1040 | `			break;` |
|         - | 1041 | `		}` |
|      2085 | 1042 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB) ){` |
|       173 | 1043 | `			iNest++;` |
|      2001 | 1044 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|       213 | 1045 | `			iNest--;` |
|       213 | 1046 | `			if( iNest <= 0 ){` |
|        44 | 1047 | `				break;` |
|         - | 1048 | `			}` |
|        84 | 1049 | `		}` |
|      2045 | 1050 | `		pCur++;` |
|         5 | 1051 | `	}` |
|       763 | 1052 | `	*ppEnd = pCur;` |
|       763 | 1053 | `}` |
|  11324444 | 1054 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 | 1055 | `{` |
|         - | 1056 | `	ph7_expr_node *pNode;` |
|         - | 1057 | `	SyToken *pCur;` |
|         - | 1058 | `	sxi32 rc;` |
|         - | 1059 | `	/* Allocate a new node */` |
|  11324449 | 1060 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  11324449 | 1061 | `	if( pNode == 0 ){` |
|         - | 1062 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1063 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1064 | `		 */` |
|       ! 0 | 1065 | `		return SXERR_MEM;` |
|         - | 1066 | `	}` |
|         - | 1067 | `	/* Zero the structure */` |
|  11324449 | 1068 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  11324449 | 1069 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - | 1070 | `	/* Point to the head of the token stream */` |
|  11324449 | 1071 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - | 1072 | `	/* Start collecting tokens */` |
|  11324449 | 1073 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       627 | 1074 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - | 1075 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - | 1076 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - | 1077 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - | 1078 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       258 | 1079 | `			pNode->pEnd = pCur;` |
|       258 | 1080 | `			pCur++;` |
|       258 | 1081 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       258 | 1082 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       258 | 1083 | `			pGen->pIn = pCur;` |
|       258 | 1084 | `			*ppNode = pNode;` |
|       258 | 1085 | `			return SXRET_OK;` |
|         - | 1086 | `		}` |
|         - | 1087 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - | 1088 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       373 | 1089 | `		pCur++;` |
|       373 | 1090 | `		pGen->pIn = pCur;` |
|       373 | 1091 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       373 | 1092 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       373 | 1093 | `		if( rc == SXRET_OK && *ppNode ){` |
|       373 | 1094 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       184 | 1095 | `		}` |
|       373 | 1096 | `		return rc;` |
|         - | 1097 | `	}` |
|  11323827 | 1098 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - | 1099 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - | 1100 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - | 1101 | `		 */` |
|     14821 | 1102 | `		pCur++; /* Skip the opening '[' */` |
|     14821 | 1103 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     14821 | 1104 | `		if( pCur < pGen->pEnd ){` |
|     14821 | 1105 | `			pCur++; /* Skip past the closing ']' */` |
|      7413 | 1106 | `		}else{` |
|       ! 0 | 1107 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1108 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 | 1109 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 1110 | `				rc = SXERR_SYNTAX;` |
|       ! 0 | 1111 | `			}` |
|       ! 0 | 1112 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1113 | `			return rc;` |
|         - | 1114 | `		}` |
|         - | 1115 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - | 1116 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - | 1117 | `		 */` |
|     15563 | 1118 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      1489 | 1119 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      1489 | 1120 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       199 | 1121 | `				pNode->xCode = PH7_CompileShortList;` |
|       102 | 1122 | `			}else{` |
|      1295 | 1123 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - | 1124 | `			}` |
|       747 | 1125 | `		}else{` |
|     13337 | 1126 | `			pNode->xCode = PH7_CompileShortArray;` |
|         - | 1127 | `		}` |
|  11316419 | 1128 | `	}else if( !bAfterMemberOp && (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID))` |
|   6376886 | 1129 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_COLON) ){` |
|         - | 1130 | `		/* A RESERVED WORD immediately followed by a single ':' is a named-argument` |
|         - | 1131 | `		 * LABEL — php accepts all 73 of them there, since a parameter may be called` |
|         - | 1132 | ``		 * anything (`function f($new, $print, $and)`). Sixteen of them were claimed`` |
|         - | 1133 | ``		 * by their own construct branch below instead, so `f(new: 1)`, `f(print: 2)`,`` |
|         - | 1134 | ``		 * `f(match: $m)` and thirteen more were a compile fatal on source php runs.`` |
|         - | 1135 | ``		 * The lexer gives `::` its own operator token, so the only other shape this`` |
|         - | 1136 | `		 * can see — a bare word before a colon — is already a literal on the` |
|         - | 1137 | ``		 * fallthrough path; a `?:` cannot reach here at all, its '?' being neither`` |
|         - | 1138 | `		 * an identifier nor a keyword. The argument list is re-parsed from each` |
|         - | 1139 | `		 * argument's own first token, so there is no '(' to look back at and no way` |
|         - | 1140 | `		 * to scope this to call context; ExprProcessFuncArguments makes the` |
|         - | 1141 | `		 * POSITIONAL test that decides whether the label is really one. */` |
|      6427 | 1142 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|      6427 | 1143 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      6427 | 1144 | `		pNode->xCode = PH7_CompileLiteral;` |
|  11305800 | 1145 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - | 1146 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - | 1147 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - | 1148 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - | 1149 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - | 1150 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - | 1151 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - | 1152 | `		 * call, not the clone() intrinsic. */` |
|        25 | 1153 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        25 | 1154 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        25 | 1155 | `		pNode->xCode = PH7_CompileLiteral;` |
|  11302573 | 1156 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   3151624 | 1157 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   1575974 | 1158 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN)` |
|       201 | 1159 | `		&& CloneCallFormFollows(pCur,pGen->pEnd) ){` |
|         - | 1160 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - | 1161 | ``		 * `clone` is a real internal FUNCTION in php 8.5, so this spelling is an`` |
|         - | 1162 | `		 * ordinary call — and every property the call machinery owns comes with` |
|         - | 1163 | ``		 * it: named arguments, spread, the first-class-callable `clone(...)`, and`` |
|         - | 1164 | `		 * the runtime ArgumentCountError/TypeError php raises for a degenerate` |
|         - | 1165 | ``		 * argument list (PHL used to refuse `clone()` and a three-argument call at`` |
|         - | 1166 | ``		 * COMPILE time, and had no FCC form at all). `clone` is an alpha-stream`` |
|         - | 1167 | `` 		 * operator token, so `clone(` is not auto-marked as a call the way `foo(` `` |
|         - | 1168 | `		 * is: clear PH7_TK_OP and leave a plain name TERM behind, and the postfix` |
|         - | 1169 | `		 * pass then binds the '(' to it. The bare operator/statement form` |
|         - | 1170 | ``		 * `clone $obj` (no immediately-following '(') keeps the precedence-1`` |
|         - | 1171 | `		 * operator path below. */` |
|        58 | 1172 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        58 | 1173 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        58 | 1174 | `		pNode->xCode = PH7_CompileLiteral;` |
|  11302537 | 1175 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1176 | `		/* Point to the instance that describe this operator */` |
|   3151573 | 1177 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1178 | `		/* Advance the stream cursor */` |
|   3151573 | 1179 | `		pCur++;` |
|   9726725 | 1180 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1181 | `		/* Isolate variable */` |
|   6218747 | 1182 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   3109395 | 1183 | `			pCur++; /* Variable variable */` |
|         5 | 1184 | `		}` |
|   3109357 | 1185 | `		if( pCur < pGen->pEnd ){` |
|   3109357 | 1186 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1187 | `				/* Variable name */` |
|   3109319 | 1188 | `				pCur++;` |
|   1554700 | 1189 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        34 | 1190 | `				pCur++;` |
|         - | 1191 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        34 | 1192 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        34 | 1193 | `				if( pCur < pGen->pEnd ){` |
|        31 | 1194 | `					pCur++;` |
|        17 | 1195 | `				}else{` |
|         - | 1196 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|         - | 1197 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|         - | 1198 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|         - | 1199 | `					 * delimiter search stops at the slice end, so the token php names` |
|         - | 1200 | `					 * usually sits just past it, still inside the chunk stream. */` |
|         - | 1201 | `					{` |
|         3 | 1202 | `						SyToken *pBad = 0;` |
|         3 | 1203 | `						if( pGen->pTokenSet ){` |
|         3 | 1204 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         3 | 1205 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         3 | 1206 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|       ! 0 | 1207 | `								pBad = pCur;` |
|       ! 0 | 1208 | `							}` |
|         1 | 1209 | `						}` |
|         3 | 1210 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|         - | 1211 | `					}` |
|         3 | 1212 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1213 | `						rc = SXERR_SYNTAX;` |
|         1 | 1214 | `					}` |
|         3 | 1215 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1216 | `					return rc;` |
|         - | 1217 | `				}` |
|        17 | 1218 | `			}else{` |
|         - | 1219 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|         - | 1220 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|         - | 1221 | `				 * covered only the '$' and the offending token drifted into a later` |
|         - | 1222 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|         - | 1223 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|        10 | 1224 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|        10 | 1225 | `				if( rc != SXERR_ABORT ){` |
|        10 | 1226 | `					rc = SXERR_SYNTAX;` |
|         4 | 1227 | `				}` |
|        10 | 1228 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        10 | 1229 | `				return rc;` |
|         - | 1230 | `			}` |
|   1554671 | 1231 | `		}` |
|   3109347 | 1232 | `		pNode->xCode = PH7_CompileVariable;` |
|   6596260 | 1233 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    119377 | 1234 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    119377 | 1235 | `		 if( bAfterMemberOp ){` |
|         - | 1236 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1237 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1238 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1239 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|         - | 1240 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|         - | 1241 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|         - | 1242 | `			  * the word itself. */` |
|       495 | 1243 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|       495 | 1244 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       495 | 1245 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    119132 | 1246 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1247 | `			 /* List/Array node */` |
|     94119 | 1248 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1249 | `				 /* Assume a literal */` |
|       ! 0 | 1250 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1251 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1252 | `			 }else{` |
|     94119 | 1253 | `				 pCur += 2;` |
|         - | 1254 | `				 /* Collect array/list tokens */` |
|     94119 | 1255 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     94119 | 1256 | `				 if( pCur < pGen->pEnd ){` |
|     94117 | 1257 | `					 pCur++;` |
|     47061 | 1258 | `				 }else{` |
|         - | 1259 | `					 /* Syntax error */` |
|         - | 1260 | `					 /* php names the token it stopped on and says it expected ")". */` |
|         3 | 1261 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|         3 | 1262 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1263 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1264 | `					 }` |
|         3 | 1265 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1266 | `					 return rc;` |
|         - | 1267 | `				 }` |
|     94117 | 1268 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     94117 | 1269 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        49 | 1270 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        49 | 1271 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1272 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|         - | 1273 | ``						  * not the `list` the construct started at. */`` |
|         3 | 1274 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|         3 | 1275 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1276 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1277 | `						 }` |
|         3 | 1278 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1279 | `						 return rc;` |
|         - | 1280 | `					 }` |
|        21 | 1281 | `				 }` |
|         5 | 1282 | `			 }` |
|     71828 | 1283 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1284 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       523 | 1285 | `			 pCur++; /* Skip 'yield' keyword */` |
|       523 | 1286 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       523 | 1287 | `			 pNode->xCode = PH7_CompileYield;` |
|     24514 | 1288 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     22214 | 1289 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       462 | 1290 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       417 | 1291 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1292 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|      4775 | 1293 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1294 | `				 /* Assume a literal */` |
|       ! 0 | 1295 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1296 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1297 | `			 }else{` |
|         - | 1298 | `				 /* Assemble annonymous functions body */` |
|      4775 | 1299 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      4775 | 1300 | `				 if( rc != SXRET_OK ){` |
|        28 | 1301 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1302 | `					 return rc;` |
|         - | 1303 | `				 }` |
|      4751 | 1304 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1305 | `			  }` |
|     21858 | 1306 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|        92 | 1307 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        57 | 1308 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        34 | 1309 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        24 | 1310 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1311 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1312 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1313 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1314 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|        85 | 1315 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|        85 | 1316 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1317 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1318 | `				 return rc;` |
|         - | 1319 | `			 }` |
|        85 | 1320 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     19445 | 1321 | `		 }else if( (nKeyword == PH7_TKWRD_FN \|\| nKeyword == PH7_TKWRD_STATIC)` |
|     12660 | 1322 | `			&& PH7_TokenOpensArrowFunc(pGen->pIn,pCur,pGen->pEnd) ){` |
|         - | 1323 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      5825 | 1324 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      5825 | 1325 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1326 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1327 | `				 return rc;` |
|         - | 1328 | `			 }` |
|      5825 | 1329 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     16495 | 1330 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1331 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       128 | 1332 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       128 | 1333 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1334 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1335 | `				 return rc;` |
|         - | 1336 | `			 }` |
|       128 | 1337 | `			 pNode->xCode = PH7_CompileMatch;` |
|     13523 | 1338 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1339 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1340 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1341 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        48 | 1342 | `			 pCur++; /* Skip 'throw' */` |
|        48 | 1343 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|        48 | 1344 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     13439 | 1345 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1346 | `			 /* Language constructs [i.e: print,echo,die...] require special handling.` |
|         - | 1347 | `			  * Each of the six that reach here takes exactly ONE operand. */` |
|       201 | 1348 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       201 | 1349 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       103 | 1350 | `		 }else{` |
|         - | 1351 | `			 /* Assume a literal */` |
|     13221 | 1352 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     13221 | 1353 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1354 | `		 }` |
|   4981889 | 1355 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1356 | `		 /* Constants,function name,namespace path,class name... */` |
|   1310115 | 1357 | `		 if( bAfterMemberOp ){` |
|         - | 1358 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|         - | 1359 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|         - | 1360 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|         - | 1361 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|     46157 | 1362 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     23076 | 1363 | `		 }` |
|   1310115 | 1364 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   1310115 | 1365 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    655060 | 1366 | `	 }else{` |
|   3612107 | 1367 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1368 | `			 /* Point to the code generator routine */` |
|   1272371 | 1369 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   1272371 | 1370 | `			 if( pNode->xCode == 0 ){` |
|        12 | 1371 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|        12 | 1372 | `				 if( rc != SXERR_ABORT ){` |
|        12 | 1373 | `					 rc = SXERR_SYNTAX;` |
|         5 | 1374 | `				 }` |
|        12 | 1375 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        12 | 1376 | `				 return rc;` |
|         - | 1377 | `			 }` |
|    636178 | 1378 | `		 }` |
|         - | 1379 | `		/* Advance the stream cursor */` |
|   3612097 | 1380 | `		pCur++;` |
|         - | 1381 | `	 }` |
|         - | 1382 | `	/* Point to the end of the token stream */` |
|  11323779 | 1383 | `	pNode->pEnd = pCur;` |
|         - | 1384 | `	/* Save the node for later processing */` |
|  11323779 | 1385 | `	*ppNode = pNode;` |
|         - | 1386 | `	/* Synchronize cursors */` |
|  11323779 | 1387 | `	pGen->pIn = pCur;` |
|  11323779 | 1388 | `	return SXRET_OK;` |
|   5662227 | 1389 | `}` |
|         - | 1390 | `/*` |
|         - | 1391 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1392 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1393 | ` * level is zero.` |
|         - | 1394 | ` */` |
|    325084 | 1395 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1396 | `{` |
|    325089 | 1397 | `	SyToken *pCur = pStart;` |
|    325089 | 1398 | `	sxi32 iNest = 0;` |
|    325089 | 1399 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1400 | `		/* Last expression */` |
|    140631 | 1401 | `		return SXERR_EOF;` |
|         - | 1402 | `	}` |
|    596179 | 1403 | `	while( pCur < pEnd ){` |
|    565965 | 1404 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    154249 | 1405 | `			break;` |
|         - | 1406 | `		}` |
|    411721 | 1407 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     35087 | 1408 | `			iNest++;` |
|    394180 | 1409 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     35091 | 1410 | `			iNest--;` |
|     17543 | 1411 | `		}` |
|    411721 | 1412 | `		pCur++;` |
|         5 | 1413 | `	}` |
|    184463 | 1414 | `	*ppNext = pCur;` |
|    184463 | 1415 | `	return SXRET_OK;` |
|    162547 | 1416 | `}` |
|         - | 1417 | `/*` |
|         - | 1418 | ` * Free an expression tree.` |
|         - | 1419 | ` */` |
|   9774410 | 1420 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1421 | `{` |
|   9774415 | 1422 | `	if( pNode->pLeft ){` |
|         - | 1423 | `		/* Release the left tree */` |
|   3602599 | 1424 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   1801297 | 1425 | `	}` |
|   9774415 | 1426 | `	if( pNode->pRight ){` |
|         - | 1427 | `		/* Release the right tree */` |
|   2057301 | 1428 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   1028648 | 1429 | `	}` |
|   9774415 | 1430 | `	if( pNode->pCond ){` |
|         - | 1431 | `		/* Release the conditional tree used by the ternary operator */` |
|     44223 | 1432 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     22109 | 1433 | `	}` |
|   9774415 | 1434 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1435 | `		ph7_expr_node **apArg;` |
|         - | 1436 | `		sxu32 n;` |
|         - | 1437 | `		/* Release node arguments */` |
|   1001621 | 1438 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   2405519 | 1439 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   1403903 | 1440 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    701954 | 1441 | `		}` |
|   1001621 | 1442 | `		SySetRelease(&pNode->aNodeArgs);` |
|    500808 | 1443 | `	}` |
|         - | 1444 | `	/* Finally,release this node */` |
|   9774415 | 1445 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   9774415 | 1446 | `}` |
|         - | 1447 | `/*` |
|         - | 1448 | ` * Free an expression tree.` |
|         - | 1449 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1450 | ` */` |
|   2326708 | 1451 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1452 | `{` |
|         - | 1453 | `	ph7_expr_node **apNode;` |
|         - | 1454 | `	sxu32 n;` |
|   2326713 | 1455 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  13650711 | 1456 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  11324003 | 1457 | `		if( apNode[n] ){` |
|   2327097 | 1458 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   1163546 | 1459 | `		}` |
|   5662004 | 1460 | `	}` |
|   2326713 | 1461 | `	return SXRET_OK;` |
|         5 | 1462 | `}` |
|         - | 1463 | `/*` |
|         - | 1464 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1465 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1466 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1467 | ` * (PHP 8.0 makes this a compile fatal:` |
|         - | 1468 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1469 | ` */` |
|   6138044 | 1470 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1471 | `{` |
|   6138049 | 1472 | `	if( pNode == 0 ){` |
|   3983283 | 1473 | `		return 0;` |
|         - | 1474 | `	}` |
|   2154771 | 1475 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        24 | 1476 | `		return 1;` |
|         - | 1477 | `	}` |
|   2154751 | 1478 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1479 | `		return 1;` |
|         - | 1480 | `	}` |
|   2154747 | 1481 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1482 | `		return 1;` |
|         - | 1483 | `	}` |
|   2154747 | 1484 | `	return 0;` |
|   3069027 | 1485 | `}` |
|         - | 1486 | `/*` |
|         - | 1487 | `` * TRUE when a `::` node names a class CONSTANT (`A::K`, `A::class`) rather than`` |
|         - | 1488 | `` * a static PROPERTY (`A::$s`, `A::$$name`).`` |
|         - | 1489 | ` *` |
|         - | 1490 | ` * php's grammar puts the two in different rules and only the property is a` |
|         - | 1491 | `` * `variable`: a class constant is a `constant`, which nothing but a DEREFERENCE`` |
|         - | 1492 | `` * (`A::K[0]`, `A::K->p`) can turn into one — and that dereference answers a`` |
|         - | 1493 | `` * TEMPORARY. So `A::K = 5`, `A::K++` and `unset(A::K)` are php PARSE errors`` |
|         - | 1494 | `` * while `A::K[0] = 5` is its "Cannot use temporary expression in write`` |
|         - | 1495 | `` * context". PHL treated every `::` as class-level STORAGE, so the first three`` |
|         - | 1496 | `` * were reported at runtime with a PH7-ism (or, for `A::K++` and `--A::K`, ran in`` |
|         - | 1497 | ` * silence) and the fourth wrote into a discarded copy of the constant.` |
|         - | 1498 | ` *` |
|         - | 1499 | `` * The token after `::` decides it: a static property always spells a `$`.`` |
|         - | 1500 | ` */` |
|       474 | 1501 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode)` |
|         5 | 1502 | `{` |
|       479 | 1503 | `	if( pNode == 0 \|\| pNode->pOp == 0 \|\| pNode->pOp->iOp != EXPR_OP_DC ){` |
|       205 | 1504 | `		return FALSE;` |
|         - | 1505 | `	}` |
|       279 | 1506 | `	if( pNode->pRight == 0 \|\| pNode->pRight->pStart == 0 ){` |
|         - | 1507 | `		/* Not linked yet / nothing to look at: keep the old permissive answer. */` |
|       ! 0 | 1508 | `		return FALSE;` |
|         - | 1509 | `	}` |
|       279 | 1510 | `	return (pNode->pRight->pStart->nType & PH7_TK_DOLLAR) == 0;` |
|       242 | 1511 | `}` |
|         - | 1512 | `/*` |
|         - | 1513 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1514 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1515 | ` *` |
|         - | 1516 | ` * This is the SHAPE question only: is the target an access chain at all?` |
|         - | 1517 | ` * Whether the chain's BASE may be written through is php's separate rule, made` |
|         - | 1518 | ` * at codegen by GenStateWriteTargetCheck (php: zend_compile_var_inner) so every` |
|         - | 1519 | `` * write kind — `=`, `+=`, `=&`, `++`, `unset()`, a foreach target — reaches it`` |
|         - | 1520 | ` * and reports php's own two refusals instead of a message naming the operator.` |
|         - | 1521 | ` */` |
|    831100 | 1522 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode)` |
|         5 | 1523 | `{` |
|         - | 1524 | `	sxi32 iExprOp;` |
|    831105 | 1525 | `	if( pNode->pOp == 0 ){` |
|    671369 | 1526 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1527 | `	}` |
|    159741 | 1528 | `	iExprOp = pNode->pOp->iOp;` |
|    159741 | 1529 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ ){` |
|      1879 | 1530 | `			return TRUE;` |
|         - | 1531 | `	}` |
|    157867 | 1532 | `	if( iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|         - | 1533 | ``		/* `C::$s` is storage; `C::K` is a constant, and php's grammar will not`` |
|         - | 1534 | `		 * take one as a write target at all. */` |
|        85 | 1535 | `		return PH7_ExprNodeIsClassConst(pNode) ? FALSE : TRUE;` |
|         - | 1536 | `	}` |
|    157787 | 1537 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|         - | 1538 | `		/* A subscript is a writable shape whatever it subscripts. php compiles` |
|         - | 1539 | ``		 * and RUNS `f()[0] = 5` and `str_split("ab")[0] = "z"` — the write lands`` |
|         - | 1540 | `		 * on the temporary the call answered and is discarded — and refuses a` |
|         - | 1541 | ``		 * literal/cast/computed/`new` base with a wording of its own. Screening`` |
|         - | 1542 | ``		 * the base here rejected both alike with `'=': Left operand must be a`` |
|         - | 1543 | ``		 * modifiable l-value`, and did it BEFORE the codegen check that knows`` |
|         - | 1544 | `		 * php's rules could speak. */` |
|    157753 | 1545 | `		return TRUE;` |
|         - | 1546 | `	}` |
|        38 | 1547 | `	if( iExprOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1548 | ``		/* A call is a shape both ways: as a reference SOURCE (`$r =& f()`) it is`` |
|         - | 1549 | `		 * php-legal, and as a write TARGET it is php's own compile fatal naming` |
|         - | 1550 | `		 * the kind of call, which GenStateWriteTargetCheck raises. */` |
|        26 | 1551 | `		return TRUE;` |
|         - | 1552 | `	}` |
|         - | 1553 | `	/* Not a modifiable l or r-value */` |
|        15 | 1554 | `	return FALSE;` |
|    415555 | 1555 | `}` |
|         - | 1556 | `/*` |
|         - | 1557 | `` * php refuses a write to something that is not a `variable` in its GRAMMAR, so`` |
|         - | 1558 | ` * what comes out is a SYNTAX error naming a token — never a sentence about the` |
|         - | 1559 | ` * operator, which is all PHL had ("'=': Left operand must be a modifiable` |
|         - | 1560 | `` * l-value", "'++' operator needs l-value", and two more for `=&` and `unset()`).`` |
|         - | 1561 | ` * Which token php names depends on which side of the operator the offending` |
|         - | 1562 | ` * operand sits, and both shapes are here:` |
|         - | 1563 | ` *` |
|         - | 1564 | `` *   the operand LEFT of the operator (`5 = 1`, `A::K += 1`, `(1+2)++`) —  php`` |
|         - | 1565 | ` *   has already shifted it and stops AT the operator, so that is what it names.` |
|         - | 1566 | ` */` |
|        10 | 1567 | `static sxi32 ExprWriteTargetNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOpNode)` |
|         3 | 1568 | `{` |
|         - | 1569 | `	sxi32 rc;` |
|        13 | 1570 | `	if( pOpNode->pOp && pOpNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1571 | ``		/* php lexes `=&` as `=` then `&`, and stops on the `=`. */`` |
|       ! 0 | 1572 | `		rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       ! 0 | 1573 | `			pOpNode->pStart ? pOpNode->pStart->nLine : 0,` |
|         - | 1574 | `			"syntax error, unexpected token \"=\"");` |
|       ! 0 | 1575 | `	}else{` |
|        13 | 1576 | `		rc = PH7_GenSyntaxError(pGen,pOpNode->pStart,0);` |
|         - | 1577 | `	}` |
|        13 | 1578 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         3 | 1579 | `}` |
|         - | 1580 | `/*` |
|         - | 1581 | ` * A token pointer one PAST the last token of a subtree is only a real token` |
|         - | 1582 | `` * while the subtree is not the last thing in the file. `<?php --A::K` (no`` |
|         - | 1583 | ` * terminator) walked off the end of the stream and named a garbage token on` |
|         - | 1584 | ` * line 0; php says "unexpected end of file" there, which is what` |
|         - | 1585 | ` * PH7_GenSyntaxError answers for a NULL. Returns pTok, or 0 when it is outside` |
|         - | 1586 | ` * the chunk's token stream.` |
|         - | 1587 | ` */` |
|        12 | 1588 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok)` |
|         3 | 1589 | `{` |
|         - | 1590 | `	SyToken *pBase, *pStreamEnd;` |
|        15 | 1591 | `	if( pTok == 0 \|\| pGen->pTokenSet == 0 ){` |
|       ! 0 | 1592 | `		return pTok;` |
|         - | 1593 | `	}` |
|        15 | 1594 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        15 | 1595 | `	pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        15 | 1596 | `	return (pTok >= pBase && pTok < pStreamEnd) ? pTok : 0;` |
|         9 | 1597 | `}` |
|         - | 1598 | `/*` |
|         - | 1599 | `` *   the operand RIGHT of the operator (`--A::K`, `$r =& "s"`, `unset(GK)`) — it`` |
|         - | 1600 | `` *   was shifted as a `constant`/`dereferencable_scalar`, so php runs past it and`` |
|         - | 1601 | ` *   stops on whatever FOLLOWS, still expecting the dereference that would have` |
|         - | 1602 | ` *   made it a variable. A bare integer/float is the exception: nothing in php's` |
|         - | 1603 | ` *   grammar dereferences one, so the literal itself is named.` |
|         - | 1604 | ` */` |
|        10 | 1605 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand)` |
|         2 | 1606 | `{` |
|        12 | 1607 | `	SyToken *pMin = 0, *pMax = 0;` |
|         - | 1608 | `	sxi32 rc;` |
|        10 | 1609 | `	if( pOperand && pOperand->pOp == 0 && pOperand->pStart` |
|         6 | 1610 | `	 && (pOperand->pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL)) ){` |
|       ! 0 | 1611 | `		rc = PH7_GenSyntaxError(pGen,pOperand->pStart,0);` |
|       ! 0 | 1612 | `	}else{` |
|         - | 1613 | `		/* The whole SUBTREE has to be stepped over, not just the node's own` |
|         - | 1614 | ``		 * tokens: `A::K` and `(1+2)` each named an inner token otherwise. The`` |
|         - | 1615 | `		 * span's max is already one past the last token. */` |
|        12 | 1616 | `		PH7_ExprSubtreeSpan(pOperand,&pMin,&pMax);` |
|        12 | 1617 | `		rc = PH7_GenSyntaxError(pGen,PH7_ExprTokenInStream(pGen,pMax),` |
|         - | 1618 | `			"\"->\" or \"?->\" or \"[\"");` |
|         - | 1619 | `	}` |
|        12 | 1620 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         2 | 1621 | `}` |
|         - | 1622 | `/* Forward declaration */` |
|         - | 1623 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1624 | `/* Macro to check if the given node is a terminal.` |
|         - | 1625 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1626 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1627 | ` * linked ternary/elvis node). */` |
|         - | 1628 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1629 | `/*` |
|         - | 1630 | ` * Buid an expression tree for each given function argument.` |
|         - | 1631 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1632 | ` */` |
|    810812 | 1633 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1634 | `{` |
|         - | 1635 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1636 | `	sxi32 rc;` |
|         - | 1637 | ``	/* php: a stray token in a call argument is `... expecting ")"`. Each arg's`` |
|         - | 1638 | `	 * tree is built by the shared ExprMakeTree below, whose leftover-node error` |
|         - | 1639 | `	 * reads this. Saved/restored so a nested call or array element inside an arg` |
|         - | 1640 | `	 * gets its own closer. */` |
|    810817 | 1641 | `	const char *zSaveArg = pGen->zClauseCloser;` |
|    810817 | 1642 | `	pGen->zClauseCloser = "\")\"";` |
|         - | 1643 | `	/* Process function arguments from left to right */` |
|    810817 | 1644 | `	iCur = 0;` |
|   1011939 | 1645 | `	for(;;){` |
|   2023883 | 1646 | `		if( iCur >= nToken ){` |
|         - | 1647 | `			/* No more arguments to process */` |
|    810787 | 1648 | `			break;` |
|         - | 1649 | `		}` |
|   1213101 | 1650 | `		iNode = iCur;` |
|   1213101 | 1651 | `		iNest = 0;` |
|   3181729 | 1652 | `		while( iCur < nToken ){` |
|   2370945 | 1653 | `			if( apNode[iCur] ){` |
|   2315547 | 1654 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    201161 | 1655 | `					break;` |
|   1913230 | 1656 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   1011374 | 1657 | `					&& apNode[iCur]->pLeft == 0` |
|    109507 | 1658 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    106646 | 1659 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1660 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1661 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1662 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1663 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1664 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1665 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|         - | 1666 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|         - | 1667 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|         - | 1668 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|         - | 1669 | `					 * and swallow the following argument separator. */` |
|    103791 | 1670 | `					iNest++;` |
|   1861342 | 1671 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    956620 | 1672 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    103791 | 1673 | `					iNest--;` |
|     51893 | 1674 | `				}` |
|    956615 | 1675 | `			}` |
|   1968633 | 1676 | `			iCur++;` |
|         5 | 1677 | `		}` |
|   1213101 | 1678 | `		if( iCur > iNode ){` |
|   1213095 | 1679 | `			SyString sArgName = {0, 0};` |
|         - | 1680 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1681 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1682 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   1213090 | 1683 | `			if( (iCur - iNode) >= 2` |
|    714960 | 1684 | `				&& apNode[iNode]` |
|    213708 | 1685 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    135885 | 1686 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     60195 | 1687 | `				&& apNode[iNode+1]` |
|     59211 | 1688 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1689 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       631 | 1690 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       631 | 1691 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       631 | 1692 | `				apNode[iNode] = 0;` |
|       631 | 1693 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       631 | 1694 | `				apNode[iNode+1] = 0;` |
|       631 | 1695 | `				iNode += 2;` |
|         - | 1696 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1697 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       631 | 1698 | `				if( iNode >= iCur ){` |
|         4 | 1699 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1700 | `						pOp->pStart->nLine,` |
|         - | 1701 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1702 | `						&sArgName);` |
|         3 | 1703 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1704 | `						rc = SXERR_SYNTAX;` |
|         1 | 1705 | `					}` |
|         3 | 1706 | `					pGen->zClauseCloser = zSaveArg;` |
|         3 | 1707 | `					return rc;` |
|         - | 1708 | `				}` |
|       312 | 1709 | `			}` |
|   1213088 | 1710 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1711 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1712 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1713 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1714 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1715 | `					apNode[iNode] = 0;` |
|       ! 0 | 1716 | `			}` |
|         - | 1717 | `			{` |
|         - | 1718 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1719 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1720 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1721 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1722 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1723 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1724 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1725 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1726 | `				 * NULL slots ahead of the flagged subtree. */` |
|   1213093 | 1727 | `				int bSpreadArg = 0;` |
|         - | 1728 | `				sxi32 iScan;` |
|   1219339 | 1729 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   1219339 | 1730 | `					if( apNode[iScan] ){` |
|   1213093 | 1731 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   1213093 | 1732 | `						break;` |
|         - | 1733 | `					}` |
|      3128 | 1734 | `				}` |
|   1213093 | 1735 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   1213093 | 1736 | `				if( bSpreadArg && apNode[iNode] ){` |
|       296 | 1737 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       146 | 1738 | `				}` |
|         - | 1739 | `			}` |
|   1213093 | 1740 | `			if( apNode[iNode] ){` |
|   1213093 | 1741 | `				if( sArgName.nByte > 0 ){` |
|       629 | 1742 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       629 | 1743 | `					apNode[iNode]->sArgName = sArgName;` |
|       312 | 1744 | `				}` |
|         - | 1745 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   1213093 | 1746 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    606549 | 1747 | `			}else{` |
|         - | 1748 | `				/* No expression before comma */` |
|       ! 0 | 1749 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1750 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1751 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1752 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1753 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1754 | `				}` |
|       ! 0 | 1755 | `				pGen->zClauseCloser = zSaveArg;` |
|       ! 0 | 1756 | `				return rc;` |
|         - | 1757 | `			}` |
|    606549 | 1758 | `		}else{` |
|         - | 1759 | `			/* Comma with no preceding argument */` |
|         9 | 1760 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         9 | 1761 | `			if( rc != SXERR_ABORT ){` |
|         9 | 1762 | `				rc = SXERR_SYNTAX;` |
|         3 | 1763 | `			}` |
|         9 | 1764 | `			pGen->zClauseCloser = zSaveArg;` |
|         9 | 1765 | `			return rc;` |
|         - | 1766 | `		}` |
|         - | 1767 | `		/* Jump trailing comma */` |
|   1213093 | 1768 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    402311 | 1769 | `			iCur++;` |
|    402311 | 1770 | `			if( iCur >= nToken ){` |
|         - | 1771 | `				/* Trailing comma after last argument */` |
|        23 | 1772 | `				break;` |
|         - | 1773 | `			}` |
|    201142 | 1774 | `		}` |
|         5 | 1775 | `	}` |
|    810809 | 1776 | `	pGen->zClauseCloser = zSaveArg;` |
|    810809 | 1777 | `	return SXRET_OK;` |
|    405411 | 1778 | `}` |
|         - | 1779 | ` /*` |
|         - | 1780 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|         - | 1781 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|         - | 1782 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|         - | 1783 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|         - | 1784 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|         - | 1785 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|         - | 1786 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|         - | 1787 | `  */` |
|        74 | 1788 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|         5 | 1789 | ` {` |
|         - | 1790 | `	 SyToken *pMin;` |
|         - | 1791 | `	 SyToken *pChild;` |
|        79 | 1792 | `	 if( pNode == 0 ){` |
|        51 | 1793 | `		 return 0;` |
|         - | 1794 | `	 }` |
|        33 | 1795 | `	 pMin = pNode->pStart;` |
|        33 | 1796 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|        33 | 1797 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|         6 | 1798 | `		 pMin = pChild;` |
|         2 | 1799 | `	 }` |
|        33 | 1800 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|        33 | 1801 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|       ! 0 | 1802 | `		 pMin = pChild;` |
|       ! 0 | 1803 | `	 }` |
|        33 | 1804 | `	 return pMin;` |
|        42 | 1805 | ` }` |
|         - | 1806 | `/*` |
|         - | 1807 | ` * The full RAW token extent of a linked subtree: minimum pStart / maximum pEnd` |
|         - | 1808 | ` * over every node (pLeft/pRight/pCond and postfix aNodeArgs children), widened` |
|         - | 1809 | ` * by one token on each side for a node whose group parens were consumed by` |
|         - | 1810 | ` * ExprMakeTree's paren pass (EXPR_NODE_PARENS — its '('/')' slots were nulled,` |
|         - | 1811 | ` * but token contiguity guarantees they sit exactly one token outside the inner` |
|         - | 1812 | ` * extent). Tokens live in one contiguous set, so pointer min/max IS source` |
|         - | 1813 | ` * order. Consumed by the assert() source-text capture, which` |
|         - | 1814 | ` * needs the argument's whole source span where a root's own pStart/pEnd name` |
|         - | 1815 | `` * only the operator token. Note: nested redundant groups `((x))` share the`` |
|         - | 1816 | ` * single PARENS bit, so only one paren layer is recovered — the renderer` |
|         - | 1817 | ` * strips redundant outermost parens anyway, matching php's export.` |
|         - | 1818 | ` */` |
|       626 | 1819 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax)` |
|         5 | 1820 | `{` |
|         - | 1821 | `	SyToken *pMin;` |
|         - | 1822 | `	SyToken *pMax;` |
|       631 | 1823 | `	SyToken *pCMin = 0;` |
|       631 | 1824 | `	SyToken *pCMax = 0;` |
|         - | 1825 | `	ph7_expr_node **apArg;` |
|         - | 1826 | `	sxu32 n;` |
|       631 | 1827 | `	if( pNode == 0 ){` |
|       449 | 1828 | `		return;` |
|         - | 1829 | `	}` |
|       187 | 1830 | `	pMin = pNode->pStart;` |
|       187 | 1831 | `	pMax = pNode->pEnd;` |
|       187 | 1832 | `	PH7_ExprSubtreeSpan(pNode->pLeft,&pCMin,&pCMax);` |
|       187 | 1833 | `	PH7_ExprSubtreeSpan(pNode->pRight,&pCMin,&pCMax);` |
|       187 | 1834 | `	PH7_ExprSubtreeSpan(pNode->pCond,&pCMin,&pCMax);` |
|       187 | 1835 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       193 | 1836 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|         7 | 1837 | `		PH7_ExprSubtreeSpan(apArg[n],&pCMin,&pCMax);` |
|         4 | 1838 | `	}` |
|       187 | 1839 | `	if( pCMin && (pMin == 0 \|\| pCMin < pMin) ){` |
|        54 | 1840 | `		pMin = pCMin;` |
|        25 | 1841 | `	}` |
|       187 | 1842 | `	if( pCMax && (pMax == 0 \|\| pCMax > pMax) ){` |
|        60 | 1843 | `		pMax = pCMax;` |
|        28 | 1844 | `	}` |
|       187 | 1845 | `	if( (pNode->iFlags & EXPR_NODE_PARENS) && pMin && pMax ){` |
|         5 | 1846 | `		pMin--;` |
|         5 | 1847 | `		pMax++;` |
|         2 | 1848 | `	}` |
|       182 | 1849 | `	if( pNode->pOp && pMax` |
|        61 | 1850 | `	 && (pNode->pOp->iOp == EXPR_OP_FUNC_CALL \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT) ){` |
|         - | 1851 | `		/* A postfix call/subscript's extent stops AT its closing ')' / ']' (the` |
|         - | 1852 | `		 * closer's node was consumed building the postfix op); token contiguity` |
|         - | 1853 | `		 * puts the closer exactly at the extent, so widen one token past it. */` |
|         5 | 1854 | `		pMax++;` |
|         2 | 1855 | `	}` |
|       187 | 1856 | `	if( pMin && (*ppMin == 0 \|\| pMin < *ppMin) ){` |
|       137 | 1857 | `		*ppMin = pMin;` |
|        66 | 1858 | `	}` |
|       187 | 1859 | `	if( pMax && (*ppMax == 0 \|\| pMax > *ppMax) ){` |
|       185 | 1860 | `		*ppMax = pMax;` |
|        90 | 1861 | `	}` |
|       318 | 1862 | `}` |
|         - | 1863 | ` /*` |
|         - | 1864 | `  * Create an expression tree from an array of tokens.` |
|         - | 1865 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1866 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1867 | `  */` |
|   3987854 | 1868 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1869 | ` {` |
|         - | 1870 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1871 | `	 ph7_expr_node *pNode;` |
|         - | 1872 | `	 ph7_expr_node *pSuppress;` |
|   3987859 | 1873 | `	 ph7_expr_node *pUnOuter = 0;` |
|         - | 1874 | `	 sxi32 iCur;` |
|         - | 1875 | `	 sxi32 rc;` |
|   3987859 | 1876 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1877 | `		 /* TICKET 1433-17: self evaluating node */` |
|   1858063 | 1878 | `		 return SXRET_OK;` |
|         - | 1879 | `	 }` |
|         - | 1880 | `	 /* Process expressions enclosed in parenthesis first */` |
|  13798729 | 1881 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1882 | `		 sxi32 iNest;` |
|         - | 1883 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1884 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1885 | `		  */` |
|  11668937 | 1886 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  11499975 | 1887 | `			 continue;` |
|         - | 1888 | `		 }` |
|    168967 | 1889 | `		 iNest = 1;` |
|    168967 | 1890 | `		 iLeft = iCur;` |
|         - | 1891 | `		 /* Find the closing parenthesis */` |
|    168967 | 1892 | `		 iCur++;` |
|   1112671 | 1893 | `		 while( iCur < nToken ){` |
|   1112671 | 1894 | `			 if( apNode[iCur] ){` |
|   1112671 | 1895 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1896 | `					 /* Decrement nesting level */` |
|    252359 | 1897 | `					 iNest--;` |
|    252359 | 1898 | `					 if( iNest <= 0 ){` |
|    168967 | 1899 | `						 break;` |
|         5 | 1900 | `					 }` |
|    902013 | 1901 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1902 | `					 /* Increment nesting level */` |
|     83397 | 1903 | `					 iNest++;` |
|     41696 | 1904 | `				 }` |
|    471852 | 1905 | `			 }` |
|    943709 | 1906 | `			 iCur++;` |
|         5 | 1907 | `		 }` |
|    168967 | 1908 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1909 | `			 sxi32 j;` |
|         - | 1910 | `			 /* Recurse and process this expression */` |
|    168967 | 1911 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    168967 | 1912 | `			 if( rc != SXRET_OK ){` |
|         6 | 1913 | `				 return rc;` |
|         - | 1914 | `			 }` |
|         - | 1915 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1916 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1917 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1918 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1919 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1920 | `			  * group's free below silently drops the unpacking. */` |
|    168963 | 1921 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    168963 | 1922 | `				 if( apNode[j] ){` |
|    168963 | 1923 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    168958 | 1924 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    168963 | 1925 | `					 break;` |
|         - | 1926 | `				 }` |
|       ! 0 | 1927 | `			 }` |
|     84479 | 1928 | `		 }` |
|         - | 1929 | `		 /* Free the left and right nodes */` |
|    168963 | 1930 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    168963 | 1931 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    168963 | 1932 | `		 apNode[iLeft] = 0;` |
|    168963 | 1933 | `		 apNode[iCur] = 0;` |
|     84484 | 1934 | `	 }` |
|         - | 1935 | `	  /* Process expressions enclosed in braces */` |
|  14911231 | 1936 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1937 | `		 sxi32 iNest;` |
|         - | 1938 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1939 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1940 | `		  */` |
|  12781439 | 1941 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  12781367 | 1942 | `			 continue;` |
|         - | 1943 | `		 }` |
|        74 | 1944 | `		 iNest = 1;` |
|        74 | 1945 | `		 iLeft = iCur;` |
|         - | 1946 | `		 /* Find the closing parenthesis */` |
|        74 | 1947 | `		 iCur++;` |
|       144 | 1948 | `		 while( iCur < nToken ){` |
|       144 | 1949 | `			 if( apNode[iCur] ){` |
|       144 | 1950 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1951 | `					 /* Decrement nesting level */` |
|        74 | 1952 | `					 iNest--;` |
|        74 | 1953 | `					 if( iNest <= 0 ){` |
|        74 | 1954 | `						 break;` |
|       ! 0 | 1955 | `					 }` |
|        71 | 1956 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1957 | `					 /* Increment nesting level */` |
|       ! 0 | 1958 | `					 iNest++;` |
|       ! 0 | 1959 | `				 }` |
|        35 | 1960 | `			 }` |
|        71 | 1961 | `			 iCur++;` |
|         1 | 1962 | `		 }` |
|        74 | 1963 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1964 | `			 /* Recurse and process this expression */` |
|        71 | 1965 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        71 | 1966 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1967 | `				 return rc;` |
|         - | 1968 | `			 }` |
|        35 | 1969 | `		 }` |
|         - | 1970 | `		 /* Free the left and right nodes */` |
|        74 | 1971 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        74 | 1972 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        74 | 1973 | `		 apNode[iLeft] = 0;` |
|        74 | 1974 | `		 apNode[iCur] = 0;` |
|        38 | 1975 | `	 }` |
|         - | 1976 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   2129797 | 1977 | `	 iLeft = -1;` |
|  14911333 | 1978 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  12781555 | 1979 | `		 if( apNode[iCur] == 0 ){` |
|   4950465 | 1980 | `			 continue;` |
|         - | 1981 | `		 }` |
|   7831095 | 1982 | `		 pNode = apNode[iCur];` |
|   7831095 | 1983 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   1150353 | 1984 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1985 | `				 /* Collect function arguments */` |
|    853659 | 1986 | `				 sxi32 iPtr = 0;` |
|    853659 | 1987 | `				 sxi32 nFuncTok = 0;` |
|   4078255 | 1988 | `				 while( nFuncTok + iCur < nToken ){` |
|   4078255 | 1989 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|         - | 1990 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|         - | 1991 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|         - | 1992 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|         - | 1993 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|         - | 1994 | `					  * nulled, so counting it here would over-count and never find` |
|         - | 1995 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   4078255 | 1996 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   4016489 | 1997 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    931897 | 1998 | `							 iPtr++;` |
|   3550543 | 1999 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    931897 | 2000 | `							 iPtr--;` |
|    931897 | 2001 | `							 if( iPtr <= 0 ){` |
|    853659 | 2002 | `								 break;` |
|         - | 2003 | `							 }` |
|     39119 | 2004 | `						 }` |
|   1581415 | 2005 | `					 }` |
|   3224601 | 2006 | `					 nFuncTok++;` |
|         5 | 2007 | `				 }` |
|    853659 | 2008 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 2009 | `					 /* Syntax error */` |
|       ! 0 | 2010 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 2011 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2012 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2013 | `					 }` |
|       ! 0 | 2014 | `					 return rc;` |
|         - | 2015 | `				 }` |
|    853659 | 2016 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 2017 | `					 /* Syntax error */` |
|       ! 0 | 2018 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 2019 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2020 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2021 | `					 }` |
|       ! 0 | 2022 | `					 return rc;` |
|         - | 2023 | `				 }` |
|    853659 | 2024 | `				 if( nFuncTok > 1 ){` |
|         - | 2025 | `					 /* Process function arguments */` |
|    810817 | 2026 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    810817 | 2027 | `					 if( rc != SXRET_OK ){` |
|        12 | 2028 | `						 return rc;` |
|         - | 2029 | `					 }` |
|    405402 | 2030 | `				 }` |
|         - | 2031 | `				 /* Link the node to the tree */` |
|    853651 | 2032 | `				 pNode->pLeft = apNode[iLeft];` |
|    853651 | 2033 | `				 apNode[iLeft] = 0;` |
|   4078223 | 2034 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   3224577 | 2035 | `					 apNode[iCur+iPtr] = 0;` |
|   1612291 | 2036 | `				 }` |
|         - | 2037 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 2038 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 2039 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 2040 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 2041 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 2042 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 2043 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 2044 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 2045 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 2046 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 2047 | `				 {` |
|    853651 | 2048 | `					 sxi32 iNew = iLeft - 1;` |
|    904657 | 2049 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     51011 | 2050 | `						 iNew--;` |
|         5 | 2051 | `					 }` |
|    853646 | 2052 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    524022 | 2053 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    304596 | 2054 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     88165 | 2055 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     88165 | 2056 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     88165 | 2057 | `						 apNode[iNew] = 0;` |
|     88165 | 2058 | `						 pNode = apNode[iCur];` |
|     44085 | 2059 | `					 }` |
|         - | 2060 | `				 }` |
|    723522 | 2061 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 2062 | `				 /* Subscripting */` |
|    249435 | 2063 | `				 sxi32 iArrTok = iCur + 1;` |
|    249435 | 2064 | `				 sxi32 iNest = 1;` |
|         - | 2065 | ``				 /* php's `dereferencable` list has `'(' expr ')'` in it, so WHATEVER a`` |
|         - | 2066 | `				  * parenthesised group evaluates to may be subscripted:` |
|         - | 2067 | ``				  * `((array)$o)['k']`, `((string)$s)[1]`, `(clone $o)[0]`, `(1+2)[0]`,`` |
|         - | 2068 | ``				  * `(1)[0]`. The base test below is a whitelist of node SHAPES, and no`` |
|         - | 2069 | `				  * shape describes "the user wrote parentheses", so every such group` |
|         - | 2070 | `				  * whose root was not already a term or a postfix chain was refused as` |
|         - | 2071 | `` 				  * `Invalid array name` — a compile fatal on source php runs. The `->` `` |
|         - | 2072 | `				  * branch further down reads the same flag for the same reason. */` |
|    249430 | 2073 | `				 if( !(iLeft >= 0 && apNode[iLeft] && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS))` |
|    249407 | 2074 | `					 && ( iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        56 | 2075 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        50 | 2076 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         - | 2077 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|         - | 2078 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|         - | 2079 | `					  * subscripting a global constant raised "Invalid array name" while` |
|         - | 2080 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|         - | 2081 | `					  * worked. */` |
|        50 | 2082 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        32 | 2083 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    249372 | 2084 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 2085 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 2086 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|       740 | 2087 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ) ){` |
|         - | 2088 | `						 /* Syntax error */` |
|       ! 0 | 2089 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 2090 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2091 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2092 | `						 }` |
|       ! 0 | 2093 | `						 return rc;` |
|         - | 2094 | `				 }` |
|         - | 2095 | `				 /* Collect index tokens */` |
|    452095 | 2096 | `				 while( iArrTok < nToken ){` |
|    452095 | 2097 | `					 if( apNode[iArrTok] ){` |
|    452063 | 2098 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 2099 | `							 /* Increment nesting level */` |
|       ! 0 | 2100 | `							 iNest++;` |
|    452063 | 2101 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 2102 | `							 /* Decrement nesting level */` |
|    249435 | 2103 | `							 iNest--;` |
|    249435 | 2104 | `							 if( iNest <= 0 ){` |
|    249435 | 2105 | `								 break;` |
|         - | 2106 | `							 }` |
|       ! 0 | 2107 | `						 }` |
|    101314 | 2108 | `					 }` |
|    202665 | 2109 | `					 ++iArrTok;` |
|         5 | 2110 | `				 }` |
|    249435 | 2111 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 2112 | ``					 /* php: a stray token in a subscript index is `... expecting "]"`. */`` |
|    190815 | 2113 | `					 const char *zSaveIdx = pGen->zClauseCloser;` |
|    190815 | 2114 | `					 pGen->zClauseCloser = "\"]\"";` |
|         - | 2115 | `					 /* Recurse and process this expression */` |
|    190815 | 2116 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    190815 | 2117 | `					 pGen->zClauseCloser = zSaveIdx;` |
|    190815 | 2118 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 2119 | `						 return rc;` |
|         - | 2120 | `					 }` |
|         - | 2121 | `					 /* Link the node to it's index */` |
|    190815 | 2122 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|     95405 | 2123 | `				 }` |
|         - | 2124 | `				 /* Link the node to the tree */` |
|    249435 | 2125 | `				 pNode->pLeft = apNode[iLeft];` |
|    249435 | 2126 | `				 pNode->pRight = 0;` |
|    249435 | 2127 | `				 apNode[iLeft] = 0;` |
|    701525 | 2128 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    452095 | 2129 | `					 apNode[iNest] = 0;` |
|    226050 | 2130 | `				 }` |
|    124720 | 2131 | `			 }else{` |
|         - | 2132 | `				 /* Member access operators [i.e: '->','::'] */` |
|     47269 | 2133 | `				  iRight = iCur + 1;` |
|     47339 | 2134 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        71 | 2135 | `					 iRight++;` |
|         1 | 2136 | `				 }` |
|     47269 | 2137 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2138 | `					 /* Syntax error */` |
|         5 | 2139 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 2140 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 2141 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2142 | `					 }` |
|         5 | 2143 | `					 return rc;` |
|         - | 2144 | `				 }` |
|         - | 2145 | `				 /* Validate the left operand BEFORE linking it. A node that is both` |
|         - | 2146 | `				  * still in apNode[] and already reachable as pNode->pLeft is freed` |
|         - | 2147 | `				  * TWICE by PH7_ExprFreeTree on the error path — a heap-use-after-free` |
|         - | 2148 | ``				  * that `1->x;`, `"s"->x;` and `[1]->x;` all reached. Ownership moves`` |
|         - | 2149 | `				  * out of the set only once the link is certain. */` |
|     47260 | 2150 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|     45451 | 2151 | `					 && apNode[iLeft]->pOp == 0 &&` |
|     39320 | 2152 | `					 apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         - | 2153 | ``					 /* A PARENTHESISED group is php's `( expr )` dereferencable: whatever it`` |
|         - | 2154 | ``					  * evaluates to may be reached through `->`, which is how the closure`` |
|         - | 2155 | ``					  * idioms are written — `(function(){ … })->bindTo($o)`,`` |
|         - | 2156 | ``					  * `(fn() => …)->call($o)`, `(match($k){ … })->m()`. PHL refused all of`` |
|         - | 2157 | `					  * them as "Expecting a variable as left operand", a compile fatal on` |
|         - | 2158 | `					  * valid php, because a literal TERM carries no operator. */` |
|        36 | 2159 | `					 (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 &&` |
|         - | 2160 | ``					 /* php's `dereferencable` also covers the SCALAR forms — a quoted`` |
|         - | 2161 | `					  * string (interpolated or not) and an array literal — plus a` |
|         - | 2162 | ``					  * CONSTANT, and it runs them: `"s"->p` warns `Attempt to read`` |
|         - | 2163 | ``					  * property "p" on string` and yields null, `"s"->m()` is the`` |
|         - | 2164 | `					  * member-function Error. Refusing them at COMPILE time killed the` |
|         - | 2165 | `					  * whole file instead. A NUMBER literal and a heredoc stay refused` |
|         - | 2166 | ``					  * — those are php's own parse error for `1->x`. */`` |
|        22 | 2167 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString &&` |
|        20 | 2168 | `					 apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 2169 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        11 | 2170 | `					 apNode[iLeft]->xCode != PH7_CompileArray &&` |
|         4 | 2171 | `					 apNode[iLeft]->xCode != PH7_CompileShortArray ){` |
|         - | 2172 | `						 /* Syntax error */` |
|         4 | 2173 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2174 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|         3 | 2175 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2176 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2177 | `						 }` |
|         3 | 2178 | `						 return rc;` |
|         - | 2179 | `				 }` |
|         - | 2180 | `				 /* Link the node to the tree */` |
|     47263 | 2181 | `				 pNode->pLeft = apNode[iLeft];` |
|     47263 | 2182 | `				 pNode->pRight = apNode[iRight];` |
|     47263 | 2183 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 2184 | `			 }` |
|    575167 | 2185 | `		 }` |
|   7831081 | 2186 | `		 iLeft = iCur;` |
|   3915543 | 2187 | `	 }` |
|         - | 2188 | `	 /* Handle the prefix (new, clone) operators. Walk RIGHT to LEFT: both take the` |
|         - | 2189 | `	  * operand on their right, so a nested one has to be linked before the outer` |
|         - | 2190 | ``	  * sees it. Left-to-right, `clone new Q` reached the still-unlinked `new` node —`` |
|         - | 2191 | `	  * not a term yet — and answered php's own valid source with the compile fatal` |
|         - | 2192 | ``	  * "'clone': Expecting class constructor call". `clone new Q()` worked only`` |
|         - | 2193 | `	  * because the postfix pass folds a constructor CALL into its new-node early. */` |
|  14911297 | 2194 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  12781519 | 2195 | `		 if( apNode[iCur] == 0 ){` |
|   6188959 | 2196 | `			 continue;` |
|         - | 2197 | `		 }` |
|   6592565 | 2198 | `		 pNode = apNode[iCur];` |
|   6592565 | 2199 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 2200 | `			 SyToken *pToken;` |
|         - | 2201 | `			 /* Get the left node */` |
|      2645 | 2202 | `			 iLeft = iCur + 1;` |
|      2783 | 2203 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|       139 | 2204 | `				 iLeft++;` |
|         1 | 2205 | `			 }` |
|      2645 | 2206 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2207 | `				  /* Syntax error */` |
|       ! 0 | 2208 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 2209 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 2210 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2211 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2212 | `				 }` |
|       ! 0 | 2213 | `				 return rc;` |
|         - | 2214 | `			 }` |
|         - | 2215 | `			 /* Make sure the operand are of a valid type. CLONE takes ANY expression —` |
|         - | 2216 | ``			  * php's grammar is `clone expr`, and what that expression evaluates to is a`` |
|         - | 2217 | `			  * RUNTIME question: a non-object operand is php's catchable` |
|         - | 2218 | ``			  * `clone(): Argument #1 ($object) must be of type object, %s given`, which`` |
|         - | 2219 | `			  * OP_CLONE already raises. The whitelist that used to sit here (a variable,` |
|         - | 2220 | ``			  * or any operator node) refused `clone 5`, `clone []`, `clone null` and`` |
|         - | 2221 | ``			  * `clone match(…){…}` at COMPILE time — the first three with a diagnostic php`` |
|         - | 2222 | `			  * never prints, the last on source php runs. NEW keeps its own, because its` |
|         - | 2223 | `			  * operand is a class-name REFERENCE, not a value. */` |
|      2645 | 2224 | `			 if( pNode->pOp->iOp != EXPR_OP_CLONE ){` |
|         - | 2225 | `				 /* New */` |
|      2372 | 2226 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 2227 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 2228 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 2229 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 2230 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 2231 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 2232 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 2233 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 2234 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 2235 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2236 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2237 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2238 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2239 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2240 | `					 }` |
|       ! 0 | 2241 | `					 return rc;` |
|         - | 2242 | `				 }` |
|      2377 | 2243 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      2377 | 2244 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      2372 | 2245 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        85 | 2246 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 2247 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 2248 | `						 /* Syntax error */` |
|       ! 0 | 2249 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2250 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2251 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2252 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2253 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2254 | `						 }` |
|       ! 0 | 2255 | `						 return rc;` |
|         - | 2256 | `					 }` |
|      1186 | 2257 | `				 }` |
|      1186 | 2258 | `			 }` |
|         - | 2259 | `			  /* Link the node to the tree */` |
|      2645 | 2260 | `			 pNode->pLeft = apNode[iLeft];` |
|      2645 | 2261 | `			 apNode[iLeft] = 0;` |
|      2645 | 2262 | `			 pNode->pRight = 0; /* Paranoid */` |
|      1320 | 2263 | `		 }` |
|   3296285 | 2264 | `	 }` |
|         - | 2265 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   2129783 | 2266 | `	 iLeft = -1;` |
|  14911297 | 2267 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  12781519 | 2268 | `		 if( apNode[iCur] == 0 ){` |
|   6191599 | 2269 | `			 continue;` |
|         - | 2270 | `		 }` |
|   6589925 | 2271 | `		 pNode = apNode[iCur];` |
|   6589925 | 2272 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     52726 | 2273 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */` |
|         - | 2274 | ``					 /* …but `A::K++` is php's parse error, not an increment of`` |
|         - | 2275 | `					  * class-level storage: a class CONSTANT is not a variable. */` |
|       197 | 2276 | `					 && !PH7_ExprNodeIsClassConst(apNode[iLeft]))` |
|     46732 | 2277 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 2278 | `					 /* Link the node to the tree */` |
|     46825 | 2279 | `					 pNode->pLeft = apNode[iLeft];` |
|     46825 | 2280 | `					 apNode[iLeft] = 0;` |
|     23410 | 2281 | `			 }` |
|     26312 | 2282 | `		  }` |
|   6589925 | 2283 | `		 iLeft = iCur;` |
|   3294965 | 2284 | `	  }` |
|   2129783 | 2285 | `	 iLeft = -1;` |
|  14911287 | 2286 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  12781513 | 2287 | `		 if( apNode[iCur] == 0 ){` |
|   6238415 | 2288 | `			 continue;` |
|         - | 2289 | `		 }` |
|   6543103 | 2290 | `		 pNode = apNode[iCur];` |
|   6543103 | 2291 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      5804 | 2292 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      5807 | 2293 | `				 \|\| ( apNode[iLeft]->pOp && (apNode[iLeft]->pOp->iPrec != 2 /* Postfix */` |
|        24 | 2294 | `					 \|\| PH7_ExprNodeIsClassConst(apNode[iLeft]))) ){` |
|         - | 2295 | `					 /* Not a variable. Nothing to the right at all means the operator` |
|         - | 2296 | `					  * was POSTFIX and its target (already passed over) was refused,` |
|         - | 2297 | `					  * which is where php stops; otherwise this is a PREFIX operator` |
|         - | 2298 | `					  * over a non-variable and php stops past that operand. */` |
|         6 | 2299 | `					 if( iLeft < 0 ){` |
|         3 | 2300 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2301 | `					 }` |
|         3 | 2302 | `					 return PH7_ExprOperandNotAVariable(pGen,apNode[iLeft]);` |
|         - | 2303 | `			 }` |
|         - | 2304 | `			 /* Link the node to the tree */` |
|      5805 | 2305 | `			 pNode->pLeft = apNode[iLeft];` |
|      5805 | 2306 | `			 apNode[iLeft] = 0;` |
|         - | 2307 | `			 /* Mark as pre-increment/decrement node */` |
|      5805 | 2308 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      2900 | 2309 | `		  }` |
|   6543099 | 2310 | `		 iLeft = iCur;` |
|   3271552 | 2311 | `	 }` |
|         - | 2312 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|         - | 2313 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|         - | 2314 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|         - | 2315 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|         - | 2316 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|         - | 2317 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|         - | 2318 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|         - | 2319 | `	  * pass below skips it (pLeft != 0). */` |
|   2129779 | 2320 | `	 iLeft = -1;` |
|  14911277 | 2321 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  12781503 | 2322 | `		 if( apNode[iCur] == 0 ){` |
|   6256195 | 2323 | `			 continue;` |
|         - | 2324 | `		 }` |
|   6525313 | 2325 | `		 pNode = apNode[iCur];` |
|   6525313 | 2326 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|     11989 | 2327 | `			 iRight = iCur + 1;` |
|     11989 | 2328 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       ! 0 | 2329 | `				 iRight++;` |
|       ! 0 | 2330 | `			 }` |
|     11989 | 2331 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|       ! 0 | 2332 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2333 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2334 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2335 | `				 }` |
|       ! 0 | 2336 | `				 return rc;` |
|         - | 2337 | `			 }` |
|     11989 | 2338 | `			 pNode->pLeft = apNode[iLeft];` |
|     11989 | 2339 | `			 pNode->pRight = apNode[iRight];` |
|     11989 | 2340 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      5992 | 2341 | `		 }` |
|   6525313 | 2342 | `		 iLeft = iCur;` |
|   3262659 | 2343 | `	 }` |
|         - | 2344 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   2129779 | 2345 | `	  iLeft = 0;` |
|  14911271 | 2346 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  12781499 | 2347 | `		  if( apNode[iCur] ){` |
|   6513325 | 2348 | `			  pNode = apNode[iCur];` |
|   6513325 | 2349 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    298935 | 2350 | `				  if( iLeft > 0 ){` |
|         - | 2351 | `					  /* Link the node to the tree */` |
|    298933 | 2352 | `					  pNode->pLeft = apNode[iLeft];` |
|    298933 | 2353 | `					  apNode[iLeft] = 0;` |
|    298933 | 2354 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|         - | 2355 | `						  /* "Is the operand a finished subtree?" — a binary node fills` |
|         - | 2356 | `						   * pLeft+pRight and a full ternary fills all three, but a SHORT` |
|         - | 2357 | ``						   * ternary (`a ?: b`, which is what `!($a ?: $b)` hands here)`` |
|         - | 2358 | `						   * fills pCond+pRight and leaves pLeft NULL on purpose. Reading` |
|         - | 2359 | `						   * pLeft alone called it unfinished and refused source php` |
|         - | 2360 | `						   * compiles — every unary and cast over a parenthesised elvis. */` |
|      5932 | 2361 | `						  if( pNode->pLeft->pRight == 0` |
|      5937 | 2362 | `							  \|\| (pNode->pLeft->pLeft == 0 && pNode->pLeft->pCond == 0) ){` |
|         - | 2363 | `							   /* Syntax error */` |
|       ! 0 | 2364 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2365 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2366 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 2367 | `							  }` |
|       ! 0 | 2368 | `							  return rc;` |
|         - | 2369 | `						  }` |
|      2966 | 2370 | `					  }` |
|    149469 | 2371 | `				  }else{` |
|         - | 2372 | `					  /* Syntax error */` |
|         3 | 2373 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 2374 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 2375 | `						  rc = SXERR_SYNTAX;` |
|         1 | 2376 | `					  }` |
|         3 | 2377 | `					  return rc;` |
|         - | 2378 | `				  }` |
|    149464 | 2379 | `			  }` |
|         - | 2380 | `			  /* Save terminal position */` |
|   6513323 | 2381 | `			  iLeft = iCur;` |
|   3256659 | 2382 | `		  }` |
|   6390751 | 2383 | `	  }` |
|         - | 2384 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 2385 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 2386 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 2387 | `	  * yielding a right-leaning tree. */` |
|  14911269 | 2388 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  12781497 | 2389 | `		 if( apNode[iCur] == 0 ){` |
|   6567709 | 2390 | `			 continue;` |
|         - | 2391 | `		 }` |
|   6213793 | 2392 | `		 pNode = apNode[iCur];` |
|   6213793 | 2393 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 2394 | `			 sxi32 iL, iR;` |
|         - | 2395 | `			 /* Find the right operand */` |
|       604 | 2396 | `			 iR = -1;` |
|         - | 2397 | `			 {` |
|         - | 2398 | `				 sxi32 j;` |
|      1026 | 2399 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      1026 | 2400 | `					 if( apNode[j] ){ iR = j; break; }` |
|       212 | 2401 | `				 }` |
|         - | 2402 | `			 }` |
|         - | 2403 | `			 /* Find the left operand */` |
|       604 | 2404 | `			 iL = -1;` |
|         - | 2405 | `			 {` |
|         - | 2406 | `				 sxi32 j;` |
|      1176 | 2407 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      1176 | 2408 | `					 if( apNode[j] ){ iL = j; break; }` |
|       287 | 2409 | `				 }` |
|         - | 2410 | `			 }` |
|       604 | 2411 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 2412 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2413 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2414 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2415 | `				 }` |
|       ! 0 | 2416 | `				 return rc;` |
|         - | 2417 | `			 }` |
|       604 | 2418 | `			 pNode->pLeft  = apNode[iL];` |
|       604 | 2419 | `			 pNode->pRight = apNode[iR];` |
|       604 | 2420 | `			 apNode[iL] = 0;` |
|       604 | 2421 | `			 apNode[iR] = 0;` |
|         - | 2422 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 2423 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 2424 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 2425 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 2426 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 2427 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 2428 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 2429 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 2430 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 2431 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 2432 | `			  * operands are respected. */` |
|       602 | 2433 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       354 | 2434 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        98 | 2435 | `				 && pNode->pLeft->pLeft != 0` |
|        92 | 2436 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 2437 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 2438 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 2439 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 2440 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 2441 | `				 while( pTail->pLeft` |
|        34 | 2442 | `					 && pTail->pLeft->pOp` |
|        23 | 2443 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 2444 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 2445 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 2446 | `					 pTail = pTail->pLeft;` |
|         1 | 2447 | `				 }` |
|         - | 2448 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 2449 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 2450 | `				 pTail->pLeft = pNode;` |
|        27 | 2451 | `				 apNode[iCur] = pHead;` |
|        13 | 2452 | `			 }` |
|       301 | 2453 | `		 }` |
|   3106899 | 2454 | `	 }` |
|         - | 2455 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  23427371 | 2456 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  21297617 | 2457 | `		 iLeft = -1;` |
| 149112091 | 2458 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 127814497 | 2459 | `			 if( apNode[iCur] == 0 ){` |
|  78627501 | 2460 | `				 continue;` |
|         - | 2461 | `			 }` |
|  49187001 | 2462 | `			 pNode = apNode[iCur];` |
|  49187001 | 2463 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|   1122919 | 2464 | ``				 ph7_expr_node *pRefUn = 0;      /* `=&` under a prefix unary */`` |
|   1122919 | 2465 | `				 ph7_expr_node *pRefUnOuter = 0;` |
|         - | 2466 | `				 /* Get the right node */` |
|   1122919 | 2467 | `				 iRight = iCur + 1;` |
|   1497351 | 2468 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    374437 | 2469 | `					 iRight++;` |
|         5 | 2470 | `				 }` |
|   1122919 | 2471 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2472 | `					 /* Syntax error */` |
|        11 | 2473 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        11 | 2474 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 2475 | `						 rc = SXERR_SYNTAX;` |
|         4 | 2476 | `					 }` |
|        11 | 2477 | `					 return rc;` |
|         - | 2478 | `				 }` |
|   1122911 | 2479 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2480 | `					 sxi32  iTmp;` |
|         - | 2481 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 2482 | ``					 /* A prefix unary covers the whole BIND too — `@$a[0] =& $x` is`` |
|         - | 2483 | ``					  * `@($a[0] =& $x)`, and so are its `-`/`+`/`!`/`~`/cast spellings,`` |
|         - | 2484 | `					  * every one of which php runs. Same hoist the assignment path makes` |
|         - | 2485 | `					  * below, re-wrapped after the operands are swapped and linked. */` |
|         - | 2486 | `					 {` |
|       269 | 2487 | `						 ph7_expr_node *pUn = apNode[iLeft];` |
|       375 | 2488 | `						 while( pUn->pOp && pUn->pLeft` |
|       110 | 2489 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|       248 | 2490 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|       109 | 2491 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|         3 | 2492 | `							 pRefUn = pUn;      /* innermost unary over the bind target */` |
|         3 | 2493 | `							 pUn = pUn->pLeft;` |
|         1 | 2494 | `						 }` |
|       269 | 2495 | `						 if( pRefUn ){` |
|         3 | 2496 | `							 pRefUnOuter = apNode[iLeft]; /* the chain's result node */` |
|         3 | 2497 | `							 apNode[iLeft] = pUn;` |
|         1 | 2498 | `						 }` |
|         - | 2499 | `					 }` |
|         - | 2500 | `					 /* PHP 8.0: a reference and a nullsafe chain do not mix, and php` |
|         - | 2501 | `					  * has a different sentence for each SIDE — the bind target is a` |
|         - | 2502 | `					  * write like any other, while the SOURCE gets a wording of its` |
|         - | 2503 | `					  * own. Both operands are still in written order here; the swap` |
|         - | 2504 | `					  * below turns them over. */` |
|       264 | 2505 | `					 if( PH7_ExprContainsNullsafe(apNode[iLeft])` |
|       269 | 2506 | `					  \|\| PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         8 | 2507 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         4 | 2508 | `							 PH7_ExprContainsNullsafe(apNode[iLeft])` |
|         - | 2509 | `								 ? "Can't use nullsafe operator in write context"` |
|         - | 2510 | `								 : "Cannot take reference of a nullsafe chain");` |
|         6 | 2511 | `						 if( rc != SXERR_ABORT ){` |
|         6 | 2512 | `							 rc = SXERR_SYNTAX;` |
|         2 | 2513 | `						 }` |
|         6 | 2514 | `						 return rc;` |
|         - | 2515 | `					 }` |
|         - | 2516 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|         - | 2517 | `					  * reference target — PH7_ExprIsModifiableValue accepts` |
|         - | 2518 | ``					  * EXPR_OP_ARROW (`->`) and a static-PROPERTY `::`, and rejects`` |
|         - | 2519 | ``					  * both a class CONSTANT and the nullsafe `?->` form, so no extra`` |
|         - | 2520 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|         - | 2521 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|       265 | 2522 | `					 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2523 | ``						 /* The bind TARGET is not a variable: php stops at the `=`. */`` |
|       ! 0 | 2524 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2525 | `					 }` |
|       265 | 2526 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       207 | 2527 | `						 if(  PH7_ExprIsModifiableValue(apNode[iRight]) == FALSE ){` |
|         - | 2528 | ``							 /* The SOURCE has to be a variable too, and php's `&new` /`` |
|         - | 2529 | ``							  * `&clone` legacy productions each stop somewhere of their`` |
|         - | 2530 | ``							  * own: `$r =& clone $o` names the `clone` keyword (nothing`` |
|         - | 2531 | `` 							  * in a `variable` may start with it), while `$r =& new A` `` |
|         - | 2532 | ``							  * enters php 4's `&new` rule, which wants the ARGUMENT`` |
|         - | 2533 | ``							  * list — `expecting "("` — unless one was written, in`` |
|         - | 2534 | `							  * which case the rule completes and php asks for the` |
|         - | 2535 | `							  * dereference like everything else. PHL accepted BOTH` |
|         - | 2536 | `							  * spellings and silently bound a copy. */` |
|         6 | 2537 | `							 if( apNode[iRight]->pOp` |
|         8 | 2538 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_CLONE ){` |
|         3 | 2539 | `								 rc = PH7_GenSyntaxError(pGen,apNode[iRight]->pStart,0);` |
|         3 | 2540 | `								 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2541 | `							 }` |
|         4 | 2542 | `							 if( apNode[iRight]->pOp` |
|         5 | 2543 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_NEW ){` |
|         3 | 2544 | `								 SyToken *pNMin = 0, *pNMax = 0;` |
|         3 | 2545 | `								 PH7_ExprSubtreeSpan(apNode[iRight],&pNMin,&pNMax);` |
|         2 | 2546 | `								 if( pNMax == 0 \|\| pNMax <= apNode[iRight]->pStart` |
|         3 | 2547 | `								  \|\| (pNMax[-1].nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 2548 | `									 rc = PH7_GenSyntaxError(pGen,` |
|         1 | 2549 | `										 PH7_ExprTokenInStream(pGen,pNMax),"\"(\"");` |
|         3 | 2550 | `									 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2551 | `								 }` |
|       ! 0 | 2552 | `							 }` |
|         3 | 2553 | `							 return PH7_ExprOperandNotAVariable(pGen,apNode[iRight]);` |
|         - | 2554 | `						 }` |
|        98 | 2555 | `					 }` |
|         - | 2556 | `					 /* Swap operands */` |
|       259 | 2557 | `					 iTmp = iRight;` |
|       259 | 2558 | `					 iRight = iLeft;` |
|       259 | 2559 | `					 iLeft = iTmp;` |
|       127 | 2560 | `				 }` |
|         - | 2561 | `				 /* Link the node to the tree */` |
|   1122901 | 2562 | `				 pNode->pLeft = apNode[iLeft];` |
|   1122901 | 2563 | `				 pNode->pRight = apNode[iRight];` |
|   1122901 | 2564 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   1122901 | 2565 | `				 if( pRefUn ){` |
|         - | 2566 | `					 /* Re-wrap: the unary chain now covers the whole bind. */` |
|         3 | 2567 | `					 pRefUn->pLeft = pNode;` |
|         3 | 2568 | `					 apNode[iCur] = pRefUnOuter;` |
|         1 | 2569 | `				 }` |
|    561448 | 2570 | `			 }` |
|  49186983 | 2571 | `			 iLeft = iCur;` |
|  24593494 | 2572 | `		 }` |
|  10648802 | 2573 | `	 }` |
|         - | 2574 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2575 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2576 | `	  * we are dealing with a single operator.` |
|         - | 2577 | `	  */` |
|   2129759 | 2578 | `	  iLeft = -1;` |
|  14620651 | 2579 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  12535119 | 2580 | `		  if( apNode[iCur] == 0 ){` |
|   8700347 | 2581 | `			  continue;` |
|         - | 2582 | `		  }` |
|   3834777 | 2583 | `		  pNode = apNode[iCur];` |
|         - | 2584 | ``		  /* `pLeft == 0` alone does NOT mean "not linked yet" for this operator: a`` |
|         - | 2585 | ``		   * SHORT ternary (`a ?: b`) leaves pLeft NULL on purpose and records its`` |
|         - | 2586 | `		   * operands in pCond/pRight. A completed elvis node sitting in this slot —` |
|         - | 2587 | ``		   * which is what a parenthesised group leaves behind, `($a ?: $b)` — was`` |
|         - | 2588 | `		   * therefore re-entered here, and the term to its left is whatever the` |
|         - | 2589 | ``		   * enclosing expression put there (the `=` of `$x = ($a ?: $b);`), so the`` |
|         - | 2590 | `		   * "missing condition" branch fired on source php compiles. pCond is the` |
|         - | 2591 | `		   * real linked/not-linked marker, and the nesting scan below already` |
|         - | 2592 | `		   * reads it that way. */` |
|   3834772 | 2593 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0` |
|     44372 | 2594 | `			  && pNode->pCond == 0 ){` |
|     44227 | 2595 | `			  sxi32 iNest = 1;` |
|     44227 | 2596 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2597 | `				  /* Missing condition */` |
|         6 | 2598 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         6 | 2599 | `				  if( rc != SXERR_ABORT ){` |
|         6 | 2600 | `					  rc = SXERR_SYNTAX;` |
|         2 | 2601 | `				  }` |
|         6 | 2602 | `				  return rc;` |
|         - | 2603 | `			  }` |
|         - | 2604 | `			  /* Get the right node */` |
|     44223 | 2605 | `			  iRight = iCur + 1;` |
|    159485 | 2606 | `			  while( iRight < nToken  ){` |
|    159485 | 2607 | `				  if( apNode[iRight] ){` |
|     88317 | 2608 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2609 | `						  /* Increment nesting level */` |
|       ! 0 | 2610 | `						  ++iNest;` |
|     88317 | 2611 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2612 | `						  /* Decrement nesting level */` |
|     44223 | 2613 | `						  --iNest;` |
|     44223 | 2614 | `						  if( iNest <= 0 ){` |
|     44223 | 2615 | `							  break;` |
|         - | 2616 | `						  }` |
|       ! 0 | 2617 | `					  }` |
|     22047 | 2618 | `				  }` |
|    115267 | 2619 | `				  iRight++;` |
|         5 | 2620 | `			  }` |
|     44223 | 2621 | `			  if( iRight > iCur + 1 ){` |
|         - | 2622 | `				  /* Recurse and process the then expression */` |
|     44099 | 2623 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     44099 | 2624 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2625 | `					  return rc;` |
|         - | 2626 | `				  }` |
|         - | 2627 | `				  /* Link the node to the tree */` |
|     44099 | 2628 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     22047 | 2629 | `			  }else{` |
|         - | 2630 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2631 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2632 | `			  }` |
|     44223 | 2633 | `			  apNode[iCur + 1] = 0;` |
|     44223 | 2634 | `			  if( iRight + 1 < nToken ){` |
|         - | 2635 | `				  /* Recurse and process the else expression */` |
|     44223 | 2636 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     44223 | 2637 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2638 | `					  return rc;` |
|         - | 2639 | `				  }` |
|         - | 2640 | `				  /* Link the node to the tree */` |
|     44223 | 2641 | `				  pNode->pRight = apNode[iRight + 1];` |
|     44223 | 2642 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     22114 | 2643 | `			  }else{` |
|       ! 0 | 2644 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2645 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2646 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2647 | `				 }` |
|       ! 0 | 2648 | `				 return rc;` |
|         - | 2649 | `			  }` |
|         - | 2650 | `			  /* Point to the condition */` |
|     44223 | 2651 | `			  pNode->pCond  = apNode[iLeft];` |
|     44223 | 2652 | `			  apNode[iLeft] = 0;` |
|     44223 | 2653 | `			  break;` |
|         - | 2654 | `		  }` |
|   3790555 | 2655 | `		  iLeft = iCur;` |
|   1895280 | 2656 | `	  }` |
|         - | 2657 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2658 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2659 | `	  * so there is no need for a precedence loop here.` |
|         - | 2660 | `	  */` |
|   2129755 | 2661 | `	 iRight = -1;` |
|  14910995 | 2662 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  12781307 | 2663 | `		 if( apNode[iCur] == 0 ){` |
|   9821107 | 2664 | `			 continue;` |
|         - | 2665 | `		 }` |
|   2960205 | 2666 | `		 pNode = apNode[iCur];` |
|   2960205 | 2667 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2668 | `			 /* Get the left node */` |
|    830353 | 2669 | `			 iLeft = iCur - 1;` |
|   1088723 | 2670 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    258375 | 2671 | `				 iLeft--;` |
|         5 | 2672 | `			 }` |
|    830353 | 2673 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2674 | `				 /* Syntax error */` |
|        46 | 2675 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2676 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2677 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2678 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2679 | `				 }else{` |
|        41 | 2680 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2681 | `				 }` |
|        46 | 2682 | `				 if( rc != SXERR_ABORT ){` |
|        44 | 2683 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2684 | `				 }` |
|        46 | 2685 | `				 return rc;` |
|         - | 2686 | `			 }` |
|         - | 2687 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2688 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2689 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2690 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2691 | `			  * a write. */` |
|    830311 | 2692 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        16 | 2693 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2694 | `					 "Can't use nullsafe operator in write context");` |
|        16 | 2695 | `				 if( rc != SXERR_ABORT ){` |
|        16 | 2696 | `					 rc = SXERR_SYNTAX;` |
|         6 | 2697 | `				 }` |
|        16 | 2698 | `				 return rc;` |
|         - | 2699 | `			 }` |
|         - | 2700 | `			 /* Every PREFIX unary in php covers the whole assignment rather than just` |
|         - | 2701 | ``			  * its target: `@$x = expr` is `@($x = expr)`, and so are `-$x = 5`,`` |
|         - | 2702 | ``			  * `+$x = 5`, `!$x = 5`, `~$x = 5`, `(int)$x = 5` and `clone $o = 5` —`` |
|         - | 2703 | `			  * every one of them RUNS in php, writing $x and then applying the` |
|         - | 2704 | `			  * operator to the result. The unary phase has already bound the operator` |
|         - | 2705 | `			  * to the LHS, which leaves the assignment staring at a non-lvalue, so` |
|         - | 2706 | `			  * walk down to the innermost operand, let the assignment bind THERE, and` |
|         - | 2707 | ``			  * re-wrap below. Only `@` was handled here, so the other eight spellings`` |
|         - | 2708 | ``			  * did not compile at all. A PARENTHESISED operand (`(-$x) = 5`) is a`` |
|         - | 2709 | ``			  * genuine non-lvalue and stays refused, and `new` keeps its own`` |
|         - | 2710 | `			  * production, where php refuses too.` |
|         - | 2711 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    830299 | 2712 | `			 pSuppress = 0;` |
|    830299 | 2713 | `			 pUnOuter = 0;` |
|         - | 2714 | `			 {` |
|    830299 | 2715 | `				 ph7_expr_node *pUn = apNode[iLeft];` |
|    989533 | 2716 | `				 while( pUn->pOp && pUn->pLeft` |
|    159220 | 2717 | `					 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|    574391 | 2718 | `					 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|    159201 | 2719 | `					  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        39 | 2720 | `					 pSuppress = pUn;  /* innermost unary seen so far */` |
|        39 | 2721 | `					 pUn = pUn->pLeft;` |
|         1 | 2722 | `				 }` |
|    830299 | 2723 | `				 if( pSuppress ){` |
|        33 | 2724 | `					 pUnOuter = apNode[iLeft]; /* the chain's result node */` |
|        33 | 2725 | `					 apNode[iLeft] = pUn;` |
|        16 | 2726 | `				 }` |
|         - | 2727 | `			 }` |
|    830299 | 2728 | `			 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2729 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|         - | 2730 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|         - | 2731 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|         - | 2732 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|         - | 2733 | `				  * assignment there, leaving the binary operator as the outer node.` |
|         - | 2734 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|       257 | 2735 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|        15 | 2736 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|        15 | 2737 | `					 ph7_expr_node *pParent = pHost;` |
|        19 | 2738 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|        11 | 2739 | `						 && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|       ! 0 | 2740 | `						 pParent = pParent->pRight;` |
|       ! 0 | 2741 | `					 }` |
|        12 | 2742 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight)` |
|        13 | 2743 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|         9 | 2744 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|         9 | 2745 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|         9 | 2746 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|         9 | 2747 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|         9 | 2748 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|         9 | 2749 | `						 iRight = iCur;` |
|         9 | 2750 | `						 continue;` |
|         - | 2751 | `					 }` |
|         2 | 2752 | `				 }` |
|       348 | 2753 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       240 | 2754 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2755 | ``					 /* The target is not a `variable` in php's grammar: php stops at`` |
|         - | 2756 | `					  * the assignment operator itself, whatever the target was. */` |
|        11 | 2757 | `					 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2758 | `				 }` |
|       118 | 2759 | `			 }` |
|         - | 2760 | `			 /* Link the node to the tree (Reverse) */` |
|    830283 | 2761 | `			 pNode->pLeft = apNode[iRight];` |
|    830283 | 2762 | `			 pNode->pRight = apNode[iLeft];` |
|    830283 | 2763 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    830283 | 2764 | `			 if( pSuppress ){` |
|         - | 2765 | `				 /* Re-wrap: the unary chain now covers the whole assignment. */` |
|        33 | 2766 | `				 pSuppress->pLeft = pNode;` |
|        33 | 2767 | `				 apNode[iCur] = pUnOuter;` |
|        16 | 2768 | `			 }` |
|    415139 | 2769 | `		 }` |
|   2960135 | 2770 | `		 iRight = iCur;` |
|   1480070 | 2771 | `	 }` |
|         - | 2772 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  10648445 | 2773 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   8518757 | 2774 | `		 iLeft = -1;` |
|  59643629 | 2775 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  51124877 | 2776 | `			 if( apNode[iCur] == 0 ){` |
|  42605809 | 2777 | `				 continue;` |
|         - | 2778 | `			 }` |
|   8519073 | 2779 | `			 pNode = apNode[iCur];` |
|   8519073 | 2780 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2781 | `				 /* Get the right node */` |
|        64 | 2782 | `				 iRight = iCur + 1;` |
|        82 | 2783 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        20 | 2784 | `					 iRight++;` |
|         2 | 2785 | `				 }` |
|        64 | 2786 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2787 | `					 /* Syntax error */` |
|       ! 0 | 2788 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2789 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2790 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2791 | `					 }` |
|       ! 0 | 2792 | `					 return rc;` |
|         - | 2793 | `				 }` |
|         - | 2794 | `				 /* Link the node to the tree */` |
|        64 | 2795 | `				 pNode->pLeft = apNode[iLeft];` |
|        64 | 2796 | `				 pNode->pRight = apNode[iRight];` |
|        64 | 2797 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        30 | 2798 | `			 }` |
|   8519073 | 2799 | `			 iLeft = iCur;` |
|   4259539 | 2800 | `		 }` |
|   4259381 | 2801 | `	 }` |
|         - | 2802 | `	 /* Point to the root of the expression tree */` |
|  12781195 | 2803 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  10651525 | 2804 | `		 if( apNode[iCur] ){` |
|   2003559 | 2805 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         - | 2806 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|         - | 2807 | `				  * operator sitting at its slot. The "expecting" clause is the closer` |
|         - | 2808 | ``				  * the enclosing construct set (`;` after `return`, `,`/`;` after`` |
|         - | 2809 | ``				  * `echo`, `)` for a for() post clause …); a for() clause defaults to`` |
|         - | 2810 | ``				  * `;` when nothing more specific was set. php prints no clause for a`` |
|         - | 2811 | `				  * plain expression statement, so a NULL closer stays clauseless. */` |
|        23 | 2812 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|        23 | 2813 | `				 const char *zExpect = pGen->zClauseCloser;` |
|        23 | 2814 | `				 if( zExpect == 0 && pGen->nCommaExprOk > 0 ){` |
|       ! 0 | 2815 | `					 zExpect = "\";\"";` |
|       ! 0 | 2816 | `				 }` |
|        23 | 2817 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,zExpect);` |
|        23 | 2818 | `				  if( rc != SXERR_ABORT ){` |
|        23 | 2819 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2820 | `				  }` |
|        23 | 2821 | `				  return rc;` |
|         - | 2822 | `			 }` |
|   2003541 | 2823 | `			 apNode[0] = apNode[iCur];` |
|   2003541 | 2824 | `			 apNode[iCur] = 0;` |
|   1001768 | 2825 | `		 }` |
|   5325756 | 2826 | `	 }` |
|   2129675 | 2827 | `	 return SXRET_OK;` |
|   1993932 | 2828 | ` }` |
|         - | 2829 | ` /*` |
|         - | 2830 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2831 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2832 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2833 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2834 | `  */` |
|   2326712 | 2835 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2836 | `{` |
|         - | 2837 | `	ph7_expr_node **apNode;` |
|         - | 2838 | `	ph7_expr_node *pNode;` |
|         - | 2839 | `	sxi32 rc;` |
|         - | 2840 | `	/* Reset node container */` |
|   2326717 | 2841 | `	SySetReset(pExprNode);` |
|   2326717 | 2842 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2843 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2844 | `	{` |
|   2326717 | 2845 | `		int iLastWasTerm = 0;` |
|   2326717 | 2846 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  13650745 | 2847 | `		while( pGen->pIn < pGen->pEnd ){` |
|  11324081 | 2848 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  11324081 | 2849 | `			if( rc != SXRET_OK ){` |
|        52 | 2850 | `				return rc;` |
|         - | 2851 | `			}` |
|         - | 2852 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  11324033 | 2853 | `			if( pNode->xCode ){` |
|         - | 2854 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   5832729 | 2855 | `				iLastWasTerm = 1;` |
|   8407671 | 2856 | `			}else if( pNode->pOp ){` |
|         - | 2857 | `				/* Operator node */` |
|   3151573 | 2858 | `				iLastWasTerm = 0;` |
|   1575789 | 2859 | `			}else{` |
|         - | 2860 | `				/* Delimiter: ')' and ']' end terms */` |
|   2339741 | 2861 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2862 | `			}` |
|         - | 2863 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2864 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2865 | `			 * node kind, so this single test covers all branches. */` |
|  11324033 | 2866 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2867 | `			/* Save the extracted node */` |
|  11324033 | 2868 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2869 | `		}` |
|         - | 2870 | `	}` |
|   2326669 | 2871 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2872 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2873 | `		*ppRoot = 0;` |
|       ! 0 | 2874 | `		return SXRET_OK;` |
|         - | 2875 | `	}` |
|   2326669 | 2876 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2877 | `	/* Make sure we are dealing with valid nodes */` |
|   2326669 | 2878 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   2326669 | 2879 | `	if( rc != SXRET_OK ){` |
|         - | 2880 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2881 | `		 * cleanup the mess left behind.` |
|         - | 2882 | `		 */` |
|        57 | 2883 | `		*ppRoot = 0;` |
|        57 | 2884 | `		return rc;` |
|         - | 2885 | `	}` |
|         - | 2886 | `	/* Build the tree */` |
|   2326617 | 2887 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   2326617 | 2888 | `	if( rc != SXRET_OK ){` |
|         - | 2889 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       127 | 2890 | `		*ppRoot = 0;` |
|       127 | 2891 | `		return rc;` |
|         - | 2892 | `	}` |
|         - | 2893 | `	/* Point to the root of the tree */` |
|   2326495 | 2894 | `	*ppRoot = apNode[0];` |
|   2326495 | 2895 | `	return SXRET_OK;` |
|   1163361 | 2896 | `}` |
|         - | 2897 |  |
