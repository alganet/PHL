# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1304/1485 lines (87.81%)

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
|   34407850 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   34407855 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  543568903 |  278 | `	for(;;){` |
| 1087137811 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
| 1087137811 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  103668099 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   51834052 |  285 | `		}else{` |
|  983469717 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
| 1087137811 |  288 | `		if( rc == 0 ){` |
|   34807461 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   34266637 |  291 | `				return &aOpTable[n];` |
|          - |  292 | `			}` |
|          - |  293 | `			/* Handle ambiguity */` |
|     540829 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|          - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      50487 |  296 | `				return &aOpTable[n];` |
|          - |  297 | `			}` |
|     490347 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      90749 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|          - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      90749 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|          - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      90741 |  303 | `					return &aOpTable[n];` |
|          - |  304 | `				}` |
|          - |  305 |  |
|          4 |  306 | `			}` |
|     199803 |  307 | `		}` |
| 1052729961 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   17203930 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    9755080 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    9755085 |  320 | `	SyToken *pCur = pIn;` |
|    9755085 |  321 | `	sxi32 iNest = 1;` |
|  170446988 |  322 | `	for(;;){` |
|  340893981 |  323 | `		if( pCur >= pEnd ){` |
|      18707 |  324 | `			break;` |
|          - |  325 | `		}` |
|  340875279 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|   13054071 |  328 | `			iNest++;` |
|  334348246 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   22790449 |  331 | `			iNest--;` |
|   22790449 |  332 | `			if( iNest <= 0 ){` |
|    9736383 |  333 | `				break;` |
|          - |  334 | `			}` |
|    6527033 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  331138901 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    9755085 |  340 | `	*ppEnd = pCur;` |
|    9755085 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     631180 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     631180 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     631128 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        159 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     631031 |  358 | `	if( bCheckFunc ){` |
|      68462 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      68433 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      68389 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        101 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      34183 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     630935 |  366 | `	return FALSE;` |
|     315595 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   19957864 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   19957869 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       9167 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       9167 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       4581 |  383 | `	}` |
|   19957869 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  125950339 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  105992511 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     294759 |  388 | `			continue;` |
|          - |  389 | `		}` |
|  105697757 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|          - |  391 | `			/* A short-array literal is a SELF-CONTAINED node whose start token is '['` |
|          - |  392 | `			 * (its ']' was consumed), so the raw-token CSB test below can never see it —` |
|          - |  393 | ``			 * `[$obj, 'm']()` parsed the '(' as a grouping paren and silently DROPPED`` |
|          - |  394 | `			 * the call (the expression evaluated to the array). php invokes the literal` |
|          - |  395 | `			 * array callable exactly like the variable-held form. */` |
|    9136049 |  396 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     513222 |  397 | `				apNode[i-1]->xCode == PH7_CompileShortArray \|\|` |
|     513204 |  398 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  399 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis.` |
|          - |  400 | `					 * A self-contained short-array node is exempt: its start token is '[',` |
|          - |  401 | `					 * which carries PH7_TK_OP as the subscript operator, but the node is a` |
|          - |  402 | ``					 * complete array-literal TERM — `[$obj, 'm'](...)` is a call. */`` |
|    8265588 |  403 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0` |
|    4132808 |  404 | `					 \|\| apNode[i-1]->xCode == PH7_CompileShortArray ){` |
|          - |  405 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  406 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  407 | `						 */` |
|    8265593 |  408 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    8265593 |  409 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    8265593 |  410 | `						apNode[i]->pOp = &sFCallOp;` |
|    4132794 |  411 | `					}` |
|    4132794 |  412 | `			}` |
|    8879447 |  413 | `			iParen++;` |
|  101258036 |  414 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    8879451 |  415 | `			if( iParen <= 0 ){` |
|         15 |  416 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         15 |  417 | `				if( rc != SXERR_ABORT ){` |
|         15 |  418 | `					rc = SXERR_SYNTAX;` |
|          6 |  419 | `				}` |
|         15 |  420 | `				return rc;` |
|          - |  421 | `			}` |
|    8879439 |  422 | `			iParen--;` |
|   92378586 |  423 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    3527207 |  424 | `			iSquare++;` |
|   86175268 |  425 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    3527211 |  426 | `			if( iSquare <= 0 ){` |
|          8 |  427 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          8 |  428 | `				if( rc != SXERR_ABORT ){` |
|          8 |  429 | `					rc = SXERR_SYNTAX;` |
|          3 |  430 | `				}` |
|          8 |  431 | `				return rc;` |
|          - |  432 | `			}` |
|    3527205 |  433 | `			iSquare--;` |
|   82648061 |  434 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       4543 |  435 | `			iBraces++;` |
|       4543 |  436 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|          - |  437 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|          - |  438 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|          - |  439 | `				 * rejects outright. It is a parse error now, like php's. */` |
|          3 |  440 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|          3 |  441 | `				if( rc != SXERR_ABORT ){` |
|          3 |  442 | `					rc = SXERR_SYNTAX;` |
|          1 |  443 | `				}` |
|          3 |  444 | `				return rc;` |
|          5 |  445 | `			}` |
|   80882191 |  446 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       4553 |  447 | `			if( iBraces <= 0 ){` |
|         15 |  448 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         15 |  449 | `				if( rc != SXERR_ABORT ){` |
|         15 |  450 | `					rc = SXERR_SYNTAX;` |
|          6 |  451 | `				}` |
|         15 |  452 | `				return rc;` |
|          - |  453 | `			}` |
|       4541 |  454 | `			iBraces--;` |
|   80877643 |  455 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     601505 |  456 | `			if( iQuesty > 0 ){` |
|     601083 |  457 | `				iQuesty--;` |
|     300966 |  458 | `			}else if( iParen <= 0 ){` |
|          - |  459 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  460 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  461 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  462 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  463 | `				if( rc != SXERR_ABORT ){` |
|          6 |  464 | `					rc = SXERR_SYNTAX;` |
|          2 |  465 | `				}` |
|          6 |  466 | `				return rc;` |
|          5 |  467 | `			}` |
|   80574623 |  468 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   26632475 |  469 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   26632475 |  470 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     601085 |  471 | `				iQuesty++;` |
|   26331935 |  472 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     109421 |  473 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|          9 |  474 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|          9 |  475 | `					sxu32 n = 0;` |
|          9 |  476 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|          5 |  477 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|          2 |  478 | `					}` |
|          - |  479 | `					/*` |
|          - |  480 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|          - |  481 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|          - |  482 | `					 */` |
|        213 |  483 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|        205 |  484 | `						++n;` |
|          1 |  485 | `					}` |
|          9 |  486 | `					pOp = &aOpTable[n];` |
|          - |  487 | `					/* Mark as binary '+' or '-',not an unary */` |
|          9 |  488 | `					apNode[i]->pOp = pOp;` |
|          9 |  489 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|          4 |  490 | `				}` |
|      54708 |  491 | `			}` |
|   13316235 |  492 | `		}` |
|   52848863 |  493 | `	}` |
|   19957833 |  494 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         16 |  495 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         16 |  496 | `		if( rc != SXERR_ABORT ){` |
|         16 |  497 | `			rc = SXERR_SYNTAX;` |
|          6 |  498 | `		}` |
|         16 |  499 | `		return rc;` |
|          - |  500 | `	}` |
|   19957821 |  501 | `	return SXRET_OK;` |
|    9978937 |  502 | `}` |
|          - |  503 | `/*` |
|          - |  504 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  505 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  506 | ` */` |
|   16564936 |  507 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  508 | `{` |
|   16564941 |  509 | `	SyToken *pIn = *ppCur;` |
|          - |  510 | `	/* Jump the first literal seen */` |
|   16564941 |  511 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   16560313 |  512 | `		pIn++;` |
|    8280154 |  513 | `	}` |
|    8284868 |  514 | `	for(;;){` |
|   16569741 |  515 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       4805 |  516 | `			pIn++;` |
|       4805 |  517 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       4803 |  518 | `				pIn++;` |
|       2399 |  519 | `			}` |
|       2405 |  520 | `		}else{` |
|    8282473 |  521 | `			break;` |
|          - |  522 | `		}` |
|          5 |  523 | `	}` |
|          - |  524 | `	/* Synchronize pointers */` |
|   16564941 |  525 | `	*ppCur = pIn;` |
|   16564941 |  526 | `}` |
|          - |  527 | `/*` |
|          - |  528 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|          - |  529 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  530 | ` * Note on annonymous functions.` |
|          - |  531 | ` *  According to the PHP language reference manual:` |
|          - |  532 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|          - |  533 | ` *  which have no specified name. They are most useful as the value of callback` |
|          - |  534 | ` *  parameters, but they have many other uses.` |
|          - |  535 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|          - |  536 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|          - |  537 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|          - |  538 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|          - |  539 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|          - |  540 | ` *` |
|          - |  541 | ` * Some example:` |
|          - |  542 | ` *  $greet = function($name)` |
|          - |  543 | ` * {` |
|          - |  544 | ` *   printf("Hello %s\r\n", $name);` |
|          - |  545 | ` * };` |
|          - |  546 | ` *  $greet('World');` |
|          - |  547 | ` *  $greet('PHP');` |
|          - |  548 | ` *` |
|          - |  549 | ` * $double = function($a) {` |
|          - |  550 | ` *   return $a * 2;` |
|          - |  551 | ` * };` |
|          - |  552 | ` * // This is our range of numbers` |
|          - |  553 | ` * $numbers = range(1, 5);` |
|          - |  554 | ` * // Use the Annonymous function as a callback here to` |
|          - |  555 | ` * // double the size of each element in our` |
|          - |  556 | ` * // range` |
|          - |  557 | ` * $new_numbers = array_map($double, $numbers);` |
|          - |  558 | ` * print implode(' ', $new_numbers);` |
|          - |  559 | ` */` |
|          - |  560 | `/*` |
|          - |  561 | ` * Skip an optional return-type declaration at *ppIn:` |
|          - |  562 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|          - |  563 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|          - |  564 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|          - |  565 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|          - |  566 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|          - |  567 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|          - |  568 | ` * authoritative type parse, so this must accept every shape it does` |
|          - |  569 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|          - |  570 | ` */` |
|       3486 |  571 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|          5 |  572 | `{` |
|       3491 |  573 | `	SyToken *pIn = *ppIn;` |
|       3491 |  574 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|         35 |  575 | `		pIn++; /* Skip ':' */` |
|         16 |  576 | `		for(;;){` |
|          - |  577 | `			/* Optional '?' nullable prefix */` |
|         39 |  578 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|          6 |  579 | `				pIn++;` |
|          2 |  580 | `			}` |
|         39 |  581 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|          - |  582 | `				/* Parenthesized DNF group '(A&B)' */` |
|        ! 0 |  583 | `				pIn++;` |
|        ! 0 |  584 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        ! 0 |  585 | `				if( pIn < pEnd ){` |
|        ! 0 |  586 | `					pIn++; /* ')' */` |
|        ! 0 |  587 | `				}` |
|         36 |  588 | `			}else if( pIn < pEnd` |
|         39 |  589 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|          - |  590 | `				/* ['\']Name('\'Name)* */` |
|         39 |  591 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|         39 |  592 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|         39 |  593 | `					pIn++;` |
|         39 |  594 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|        ! 0 |  595 | `						pIn += 2;` |
|        ! 0 |  596 | `					}` |
|         18 |  597 | `				}` |
|         21 |  598 | `			}else{` |
|          - |  599 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|        ! 0 |  600 | `				break;` |
|          - |  601 | `			}` |
|          - |  602 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|         36 |  603 | `			if( pIn < pEnd` |
|         39 |  604 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|         36 |  605 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|          5 |  606 | `				pIn++;` |
|          5 |  607 | `				continue;` |
|          - |  608 | `			}` |
|         35 |  609 | `			break;` |
|        ! 0 |  610 | `		}` |
|         16 |  611 | `	}` |
|       3491 |  612 | `	*ppIn = pIn;` |
|       3491 |  613 | `}` |
|       1608 |  614 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  615 | `{` |
|       1613 |  616 | `	SyToken *pIn = *ppCur;` |
|          - |  617 | `	sxi32 rc;` |
|          - |  618 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|          - |  619 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|          - |  620 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|          - |  621 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|          - |  622 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|       1613 |  623 | `	pIn++;` |
|       1608 |  624 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|        831 |  625 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|         46 |  626 | `		pIn++;` |
|         21 |  627 | `	}` |
|       1613 |  628 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  629 | `		/* Syntax error */` |
|          6 |  630 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  631 | `		if( rc != SXERR_ABORT ){` |
|          6 |  632 | `			rc = SXERR_SYNTAX;` |
|          2 |  633 | `		}` |
|          6 |  634 | `		goto Synchronize;` |
|          - |  635 | `	}` |
|       1609 |  636 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       1609 |  637 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       1609 |  638 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|          - |  639 | `		/* Two different failures used to share this arm and both claimed the body was` |
|          - |  640 | `		 * missing. They are distinguishable: the delimiter search leaves pIn ON the` |
|          - |  641 | `		 * ')' when it found one, and AT pEnd when it did not.` |
|          - |  642 | ``		 *   pIn >= pEnd      the parameter list never closed (`function($x {`)`` |
|          - |  643 | `		 *                    -> php expects ')'` |
|          - |  644 | `		 *   &pIn[1] >= pEnd  ')' closed it but nothing follows -> php expects '{'` |
|          - |  645 | `		 * php names the token that actually comes next, which lives just past the` |
|          - |  646 | `		 * expression slice, still in the raw stream. */` |
|          6 |  647 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|          6 |  648 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,pIn >= pEnd ? "\")\"" : "\"{\"");` |
|          6 |  649 | `		if( rc != SXERR_ABORT ){` |
|          6 |  650 | `			rc = SXERR_SYNTAX;` |
|          2 |  651 | `		}` |
|          6 |  652 | `		goto Synchronize;` |
|          - |  653 | `	}` |
|       1605 |  654 | `	pIn++; /* Jump the trailing parenthesis */` |
|          - |  655 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       1605 |  656 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       1605 |  657 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|        217 |  658 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|          - |  659 | `		/* Check if we are dealing with a closure */` |
|        217 |  660 | `		if( nKey == PH7_TKWRD_USE ){` |
|        209 |  661 | `			pIn++; /* Jump the 'use' keyword */` |
|        209 |  662 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|          - |  663 | `				/* Syntax error */` |
|          6 |  664 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|          6 |  665 | `				if( rc != SXERR_ABORT ){` |
|          6 |  666 | `					rc = SXERR_SYNTAX;` |
|          2 |  667 | `				}` |
|          6 |  668 | `				goto Synchronize;` |
|          - |  669 | `			}` |
|        205 |  670 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|          - |  671 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|          - |  672 | `			 * has no nested structure to balance here, so the first token that is not` |
|          - |  673 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|          - |  674 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|          - |  675 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|          - |  676 | `			 * the first foreign token. */` |
|          - |  677 | `			{` |
|        205 |  678 | `				SyToken *pUse = pIn;` |
|        205 |  679 | `				int bClosed = 0;` |
|        817 |  680 | `				while( pUse < pEnd ){` |
|        817 |  681 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|        619 |  682 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        617 |  683 | `						pUse++;` |
|        617 |  684 | `						continue;` |
|          - |  685 | `					}` |
|          3 |  686 | `					break; /* foreign token: php names this one */` |
|        ! 0 |  687 | `				}` |
|        205 |  688 | `				if( !bClosed ){` |
|          - |  689 | `					/* php names the offending token and expects ')'; if the list simply` |
|          - |  690 | `					 * ran off the end of the slice, that token sits just past it. */` |
|          3 |  691 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|          3 |  692 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|          3 |  693 | `					if( rc != SXERR_ABORT ){` |
|          3 |  694 | `						rc = SXERR_SYNTAX;` |
|          1 |  695 | `					}` |
|          3 |  696 | `					goto Synchronize;` |
|          - |  697 | `				}` |
|        203 |  698 | `				pIn = pUse; /* on the ')' */` |
|          - |  699 | `			}` |
|        203 |  700 | `			if( &pIn[1] >= pEnd ){` |
|          - |  701 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|          3 |  702 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|          3 |  703 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|          3 |  704 | `				if( rc != SXERR_ABORT ){` |
|          3 |  705 | `					rc = SXERR_SYNTAX;` |
|          1 |  706 | `				}` |
|          3 |  707 | `				goto Synchronize;` |
|          - |  708 | `			}` |
|        201 |  709 | `			pIn++;` |
|          - |  710 | `			/* php 7.1+: the return type may also follow the use clause —` |
|          - |  711 | ``			 * `function (...) use (...) : int {` */`` |
|        201 |  712 | `			ExprSkipReturnType(&pIn,pEnd);` |
|        103 |  713 | `		}else{` |
|          - |  714 | `			/* Syntax error */` |
|         11 |  715 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|         11 |  716 | `			if( rc != SXERR_ABORT ){` |
|         11 |  717 | `				rc = SXERR_SYNTAX;` |
|          4 |  718 | `			}` |
|         11 |  719 | `			goto Synchronize;` |
|          - |  720 | `		}` |
|         98 |  721 | `	}` |
|          - |  722 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|          - |  723 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|          - |  724 | `	 * the type), and pEnd is one past the last token. */` |
|       1589 |  725 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       1589 |  726 | `		pIn++; /* Jump the leading curly '{' */` |
|       1589 |  727 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       1589 |  728 | `		if( pIn < pEnd ){` |
|       1589 |  729 | `			pIn++;` |
|        792 |  730 | `		}` |
|        797 |  731 | `	}else{` |
|          - |  732 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|          - |  733 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|          - |  734 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|          - |  735 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|        ! 0 |  736 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|        ! 0 |  737 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|        ! 0 |  738 | `		if( rc == SXERR_ABORT ){` |
|        ! 0 |  739 | `			return SXERR_ABORT;` |
|          - |  740 | `		}` |
|          - |  741 | `	}` |
|       1589 |  742 | `	rc = SXRET_OK;` |
|        804 |  743 | `Synchronize:` |
|          - |  744 | `	/* Synchronize pointers */` |
|       1613 |  745 | `	*ppCur = pIn;` |
|       1613 |  746 | `	return rc;` |
|        809 |  747 | `}` |
|          - |  748 | `/*` |
|          - |  749 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|          - |  750 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|          - |  751 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|          - |  752 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|          - |  753 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|          - |  754 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|          - |  755 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|          - |  756 | ` */` |
|         56 |  757 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  758 | `{` |
|         61 |  759 | `	SyToken *pIn = *ppCur;` |
|         61 |  760 | `	sxu32 nLine = pIn->nLine;` |
|          - |  761 | `	sxi32 rc;` |
|         61 |  762 | `	pIn++; /* Jump the 'class' keyword */` |
|          - |  763 | `	/* Optional constructor argument list */` |
|         61 |  764 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         10 |  765 | `		pIn++; /* Jump '(' */` |
|         10 |  766 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         10 |  767 | `		if( pIn < pEnd ){` |
|         10 |  768 | `			pIn++; /* Jump ')' */` |
|          4 |  769 | `		}` |
|          4 |  770 | `	}` |
|          - |  771 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|          - |  772 | `	 * (no braces appear between ')' and the class body). */` |
|        151 |  773 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|         95 |  774 | `		pIn++;` |
|          5 |  775 | `	}` |
|         61 |  776 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|          - |  777 | `		/* Syntax error: missing class body */` |
|        ! 0 |  778 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|          - |  779 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|        ! 0 |  780 | `		if( rc != SXERR_ABORT ){` |
|        ! 0 |  781 | `			rc = SXERR_SYNTAX;` |
|        ! 0 |  782 | `		}` |
|        ! 0 |  783 | `		*ppCur = pIn;` |
|        ! 0 |  784 | `		return rc;` |
|          - |  785 | `	}` |
|         61 |  786 | `	pIn++; /* Jump the leading '{' */` |
|         61 |  787 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|         61 |  788 | `	if( pIn < pEnd ){` |
|         61 |  789 | `		pIn++; /* Jump the trailing '}' */` |
|         28 |  790 | `	}` |
|         61 |  791 | `	*ppCur = pIn;` |
|         61 |  792 | `	return SXRET_OK;` |
|         33 |  793 | `}` |
|          - |  794 | `/*` |
|          - |  795 | ` * TRUE when a KEYWORD token actually OPENS an arrow function.` |
|          - |  796 | ` *` |
|          - |  797 | `` * `fn` is reserved, but it only ever introduces `[static] fn[&](…) => expr`.`` |
|          - |  798 | ``  * Everywhere php expects a NAME the same word is an ordinary identifier: `$fn` `` |
|          - |  799 | ` * (the lexer emits '$' plus the keyword, so the keyword IS the variable name),` |
|          - |  800 | ``  * `$fn(…)` calling that variable, `C::fn`, `$o->fn`, `\A\fn`, and the `fn:` `` |
|          - |  801 | ` * named-argument label. Every raw-token lookahead that steps over an arrow` |
|          - |  802 | ` * function has to make that distinction or it swallows a plain name and loses` |
|          - |  803 | `` * the '=>' that follows it (`[$fn => 1]` became `syntax error, unexpected token`` |
|          - |  804 | `` * "=>"`).`` |
|          - |  805 | ` *` |
|          - |  806 | `` * The test is POSITIONAL, never "is it well formed": a malformed `fn` (`fn $x`` |
|          - |  807 | `` * => $x`, or a bare `fn` used as a key) must still reach the arrow parser,`` |
|          - |  808 | `` * which is what reports php's `expecting "("`. Two name positions:`` |
|          - |  809 | ` *   - member/variable/namespace: '$', '->', '?->', '::' or '\' immediately` |
|          - |  810 | ` *     before the word;` |
|          - |  811 | ``  *   - a named-argument LABEL: a bare `fn` directly before ':' (`static fn:` `` |
|          - |  812 | `` *     and `fn&:` cannot be labels, so they stay the arrow parser's business).`` |
|          - |  813 | ` *     The argument list is re-parsed from the argument's own first token, so` |
|          - |  814 | ` *     there is no '(' to look back at — the label test cannot be scoped to` |
|          - |  815 | `` *     call context, and the degenerate `true ? fn : 0` (only reachable through`` |
|          - |  816 | ` *     define('fn',…), which php itself cannot parse) is accepted as a` |
|          - |  817 | ` *     constant instead of rejected. A recorded divergence; rejecting it` |
|          - |  818 | `` *     would cost the real `f(fn: 1)` spelling.`` |
|          - |  819 | `` * pStart bounds the look-back; pTok may point at `static`, which must then be`` |
|          - |  820 | `` * followed by `fn`.`` |
|          - |  821 | ` */` |
|      38206 |  822 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd)` |
|          5 |  823 | `{` |
|      38211 |  824 | `	int bStatic = FALSE;` |
|      38211 |  825 | `	if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|        ! 0 |  826 | `		return FALSE;` |
|          - |  827 | `	}` |
|      38211 |  828 | `	if( pTok > pStart ){` |
|      27273 |  829 | `		SyToken *pPrev = &pTok[-1];` |
|      27273 |  830 | `		if( pPrev->nType & (PH7_TK_DOLLAR\|PH7_TK_NSSEP) ){` |
|       9095 |  831 | `			return FALSE; /* $fn / \A\fn — the keyword IS the name */` |
|          - |  832 | `		}` |
|      18183 |  833 | `		if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData ){` |
|      18161 |  834 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)pPrev->pUserData;` |
|      18156 |  835 | `			if( pOp->iOp == EXPR_OP_ARROW \|\| pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|         42 |  836 | `			 \|\| pOp->iOp == EXPR_OP_DC ){` |
|      18137 |  837 | `				return FALSE; /* $o->fn, $o?->fn, C::fn — a member name */` |
|          - |  838 | `			}` |
|         12 |  839 | `		}` |
|         23 |  840 | `	}` |
|      10989 |  841 | `	if( SX_PTR_TO_INT(pTok->pUserData) == PH7_TKWRD_STATIC ){` |
|       9159 |  842 | `		bStatic = TRUE;` |
|       9159 |  843 | `		pTok++;` |
|       9159 |  844 | `		if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       9131 |  845 | `			return FALSE;` |
|          - |  846 | `		}` |
|         14 |  847 | `	}` |
|       1863 |  848 | `	if( SX_PTR_TO_INT(pTok->pUserData) != PH7_TKWRD_FN ){` |
|        128 |  849 | `		return FALSE;` |
|          - |  850 | `	}` |
|       1739 |  851 | `	if( !bStatic && &pTok[1] < pEnd && (pTok[1].nType & PH7_TK_COLON) ){` |
|          3 |  852 | `		return FALSE; /* f(fn: 1) — a named-argument label */` |
|          - |  853 | `	}` |
|       1737 |  854 | `	return TRUE;` |
|      19108 |  855 | `}` |
|          - |  856 | `/*` |
|          - |  857 | ` * Assemble a PHP 7.4 arrow function token range:` |
|          - |  858 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|          - |  859 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|          - |  860 | ` * past the body expression — the body ends at the first top-level comma,` |
|          - |  861 | ` * semicolon, or unbalanced closing delimiter.` |
|          - |  862 | ` */` |
|       1690 |  863 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  864 | `{` |
|       1695 |  865 | `	SyToken *pIn = *ppCur;` |
|          - |  866 | `	sxu32 nLine;` |
|          - |  867 | `	sxi32 rc;` |
|          - |  868 | `	int iNest;` |
|       1695 |  869 | `	nLine = pIn->nLine;` |
|          - |  870 | `	/* Optional 'static' prefix */` |
|       1690 |  871 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       1695 |  872 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         27 |  873 | `		pIn++;` |
|         13 |  874 | `	}` |
|          - |  875 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       1690 |  876 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       1695 |  877 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|        ! 0 |  878 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  879 | `		goto Synchronize;` |
|          - |  880 | `	}` |
|       1695 |  881 | `	pIn++; /* Jump 'fn' */` |
|        845 |  882 | `	SXUNUSED(nLine);` |
|        845 |  883 | `	SXUNUSED(pGen);` |
|          - |  884 | `	/* Optional '&' for return-by-reference */` |
|       1695 |  885 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        ! 0 |  886 | `		pIn++;` |
|        ! 0 |  887 | `	}` |
|          - |  888 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|          - |  889 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|          - |  890 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|          - |  891 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       1695 |  892 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       1693 |  893 | `		pIn++; /* '(' */` |
|       1693 |  894 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       1693 |  895 | `		if( pIn < pEnd ){` |
|       1691 |  896 | `			pIn++; /* ')' */` |
|        843 |  897 | `		}` |
|        844 |  898 | `	}` |
|          - |  899 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       1695 |  900 | `	ExprSkipReturnType(&pIn,pEnd);` |
|          - |  901 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       1695 |  902 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       1689 |  903 | `		pIn++;` |
|        842 |  904 | `	}` |
|          - |  905 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       1695 |  906 | `	iNest = 0;` |
|      14423 |  907 | `	while( pIn < pEnd ){` |
|      14185 |  908 | `		if( iNest == 0 && (pIn->nType &` |
|          - |  909 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       1457 |  910 | `			break;` |
|          - |  911 | `		}` |
|      12733 |  912 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       2211 |  913 | `			iNest++;` |
|      11630 |  914 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       2211 |  915 | `			iNest--;` |
|       1103 |  916 | `		}` |
|      12733 |  917 | `		pIn++;` |
|          5 |  918 | `	}` |
|       1695 |  919 | `	rc = SXRET_OK;` |
|        845 |  920 | `Synchronize:` |
|       1695 |  921 | `	*ppCur = pIn;` |
|       1695 |  922 | `	return rc;` |
|          5 |  923 | `}` |
|          - |  924 | `/*` |
|          - |  925 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|          - |  926 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|          - |  927 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|          - |  928 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|          - |  929 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|          - |  930 | ` */` |
|        104 |  931 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|          5 |  932 | `{` |
|        109 |  933 | `	SyToken *pIn = *ppCur;` |
|          - |  934 | `	sxi32 rc;` |
|         52 |  935 | `	SXUNUSED(pGen);` |
|          - |  936 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|        104 |  937 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        109 |  938 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|        ! 0 |  939 | `		rc = SXERR_SYNTAX;` |
|        ! 0 |  940 | `		goto Synchronize;` |
|          - |  941 | `	}` |
|        109 |  942 | `	pIn++; /* Jump 'match' */` |
|          - |  943 | `	/* Optional '(' subject ')' */` |
|        109 |  944 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        109 |  945 | `		pIn++;` |
|        109 |  946 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        109 |  947 | `		if( pIn < pEnd ){` |
|        109 |  948 | `			pIn++; /* ')' */` |
|         52 |  949 | `		}` |
|         52 |  950 | `	}` |
|          - |  951 | `	/* Optional '{' arms '}' */` |
|        109 |  952 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|        109 |  953 | `		pIn++;` |
|        109 |  954 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|        109 |  955 | `		if( pIn < pEnd ){` |
|        109 |  956 | `			pIn++; /* '}' */` |
|         52 |  957 | `		}` |
|         52 |  958 | `	}` |
|        109 |  959 | `	rc = SXRET_OK;` |
|         52 |  960 | `Synchronize:` |
|        109 |  961 | `	*ppCur = pIn;` |
|        109 |  962 | `	return rc;` |
|          5 |  963 | `}` |
|          - |  964 | `/*` |
|          - |  965 | ` * Extract a single expression node from the input.` |
|          - |  966 | ` * On success store the freshly extractd node in ppNode.` |
|          - |  967 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  968 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|          - |  969 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|          - |  970 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|          - |  971 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|          - |  972 | ` */` |
|  105997472 |  973 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  974 | `{` |
|          - |  975 | `	ph7_expr_node *pNode;` |
|          - |  976 | `	SyToken *pCur;` |
|          - |  977 | `	sxi32 rc;` |
|          - |  978 | `	/* Allocate a new node */` |
|  105997477 |  979 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  105997477 |  980 | `	if( pNode == 0 ){` |
|          - |  981 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  982 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  983 | `		 */` |
|        ! 0 |  984 | `		return SXERR_MEM;` |
|          - |  985 | `	}` |
|          - |  986 | `	/* Zero the structure */` |
|  105997477 |  987 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  105997477 |  988 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  989 | `	/* Point to the head of the token stream */` |
|  105997477 |  990 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  991 | `	/* Start collecting tokens */` |
|  105997477 |  992 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4903 |  993 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|          - |  994 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|          - |  995 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|          - |  996 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|          - |  997 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|        103 |  998 | `			pNode->pEnd = pCur;` |
|        103 |  999 | `			pCur++;` |
|        103 | 1000 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|        103 | 1001 | `			pNode->xCode = PH7_CompileFccMarker;` |
|        103 | 1002 | `			pGen->pIn = pCur;` |
|        103 | 1003 | `			*ppNode = pNode;` |
|        103 | 1004 | `			return SXRET_OK;` |
|          - | 1005 | `		}` |
|          - | 1006 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|          - | 1007 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       4803 | 1008 | `		pCur++;` |
|       4803 | 1009 | `		pGen->pIn = pCur;` |
|       4803 | 1010 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4803 | 1011 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4803 | 1012 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4803 | 1013 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2399 | 1014 | `		}` |
|       4803 | 1015 | `		return rc;` |
|          - | 1016 | `	}` |
|  105992579 | 1017 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - | 1018 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - | 1019 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - | 1020 | `		 */` |
|     294761 | 1021 | `		pCur++; /* Skip the opening '[' */` |
|     294761 | 1022 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     294761 | 1023 | `		if( pCur < pGen->pEnd ){` |
|     294761 | 1024 | `			pCur++; /* Skip past the closing ']' */` |
|     147383 | 1025 | `		}else{` |
|        ! 0 | 1026 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1027 | `				"Short array: Missing closing bracket ']'");` |
|        ! 0 | 1028 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 | 1029 | `				rc = SXERR_SYNTAX;` |
|        ! 0 | 1030 | `			}` |
|        ! 0 | 1031 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1032 | `			return rc;` |
|          - | 1033 | `		}` |
|          - | 1034 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|          - | 1035 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|          - | 1036 | `		 */` |
|     295187 | 1037 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        857 | 1038 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        857 | 1039 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        110 | 1040 | `				pNode->xCode = PH7_CompileShortList;` |
|         57 | 1041 | `			}else{` |
|        751 | 1042 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - | 1043 | `			}` |
|        431 | 1044 | `		}else{` |
|     293909 | 1045 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 | 1046 | `		}` |
|  105845201 | 1047 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|          - | 1048 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|          - | 1049 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|          - | 1050 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|          - | 1051 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|          - | 1052 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|          - | 1053 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|          - | 1054 | `		 * call, not the clone() intrinsic. */` |
|         25 | 1055 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         25 | 1056 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|         25 | 1057 | `		pNode->xCode = PH7_CompileLiteral;` |
|  105697807 | 1058 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   30159732 | 1059 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   15113868 | 1060 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|          - | 1061 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|          - | 1062 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|          - | 1063 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|          - | 1064 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|          - | 1065 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|          - | 1066 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|          - | 1067 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|          - | 1068 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|          - | 1069 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|          - | 1070 | `		 * operator (which would dereference the NULL pOp). */` |
|         24 | 1071 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|         24 | 1072 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|         24 | 1073 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         24 | 1074 | `		if( pCur < pGen->pEnd ){` |
|         24 | 1075 | `			pCur++; /* skip the closing ')' */` |
|         13 | 1076 | `		}else{` |
|        ! 0 | 1077 | `			rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|        ! 0 | 1078 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 | 1079 | `				rc = SXERR_SYNTAX;` |
|        ! 0 | 1080 | `			}` |
|        ! 0 | 1081 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1082 | `			return rc;` |
|          - | 1083 | `		}` |
|         24 | 1084 | `		pNode->xCode = PH7_CompileCloneCall;` |
|  105697788 | 1085 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - | 1086 | `		/* Point to the instance that describe this operator */` |
|   30159715 | 1087 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - | 1088 | `		/* Advance the stream cursor */` |
|   30159715 | 1089 | `		pCur++;` |
|   90617922 | 1090 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - | 1091 | `		/* Isolate variable */` |
|   50730795 | 1092 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   25365417 | 1093 | `			pCur++; /* Variable variable */` |
|          5 | 1094 | `		}` |
|   25365383 | 1095 | `		if( pCur < pGen->pEnd ){` |
|   25365383 | 1096 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - | 1097 | `				/* Variable name */` |
|   25365347 | 1098 | `				pCur++;` |
|   12682712 | 1099 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|         32 | 1100 | `				pCur++;` |
|          - | 1101 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|         32 | 1102 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|         32 | 1103 | `				if( pCur < pGen->pEnd ){` |
|         30 | 1104 | `					pCur++;` |
|         17 | 1105 | `				}else{` |
|          - | 1106 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|          - | 1107 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|          - | 1108 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|          - | 1109 | `					 * delimiter search stops at the slice end, so the token php names` |
|          - | 1110 | `					 * usually sits just past it, still inside the chunk stream. */` |
|          - | 1111 | `					{` |
|          3 | 1112 | `						SyToken *pBad = 0;` |
|          3 | 1113 | `						if( pGen->pTokenSet ){` |
|          3 | 1114 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|          3 | 1115 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|          3 | 1116 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|        ! 0 | 1117 | `								pBad = pCur;` |
|        ! 0 | 1118 | `							}` |
|          1 | 1119 | `						}` |
|          3 | 1120 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|          - | 1121 | `					}` |
|          3 | 1122 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1123 | `						rc = SXERR_SYNTAX;` |
|          1 | 1124 | `					}` |
|          3 | 1125 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1126 | `					return rc;` |
|          - | 1127 | `				}` |
|         17 | 1128 | `			}else{` |
|          - | 1129 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|          - | 1130 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|          - | 1131 | `				 * covered only the '$' and the offending token drifted into a later` |
|          - | 1132 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|          - | 1133 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|         10 | 1134 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|         10 | 1135 | `				if( rc != SXERR_ABORT ){` |
|         10 | 1136 | `					rc = SXERR_SYNTAX;` |
|          4 | 1137 | `				}` |
|         10 | 1138 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         10 | 1139 | `				return rc;` |
|          - | 1140 | `			}` |
|   12682684 | 1141 | `		}` |
|   25365373 | 1142 | `		pNode->xCode = PH7_CompileVariable;` |
|   62855373 | 1143 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1234489 | 1144 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    1234489 | 1145 | `		 if( bAfterMemberOp ){` |
|          - | 1146 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1147 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1148 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1149 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1150 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1151 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1152 | `			  * the word itself. */` |
|     158863 | 1153 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     158863 | 1154 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     158863 | 1155 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    1155060 | 1156 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1157 | `			 /* List/Array node */` |
|     490915 | 1158 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1159 | `				 /* Assume a literal */` |
|          3 | 1160 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|          3 | 1161 | `				 pNode->xCode = PH7_CompileLiteral;` |
|          2 | 1162 | `			 }else{` |
|     490913 | 1163 | `				 pCur += 2;` |
|          - | 1164 | `				 /* Collect array/list tokens */` |
|     490913 | 1165 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     490913 | 1166 | `				 if( pCur < pGen->pEnd ){` |
|     490911 | 1167 | `					 pCur++;` |
|     245458 | 1168 | `				 }else{` |
|          - | 1169 | `					 /* Syntax error */` |
|          - | 1170 | `					 /* php names the token it stopped on and says it expected ")". */` |
|          3 | 1171 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|          3 | 1172 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1173 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1174 | `					 }` |
|          3 | 1175 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1176 | `					 return rc;` |
|          - | 1177 | `				 }` |
|     490911 | 1178 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     490911 | 1179 | `				 if( pNode->xCode == PH7_CompileList ){` |
|         45 | 1180 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|         45 | 1181 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|          - | 1182 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|          - | 1183 | ``						  * not the `list` the construct started at. */`` |
|          3 | 1184 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|          3 | 1185 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1186 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1187 | `						 }` |
|          3 | 1188 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1189 | `						 return rc;` |
|          - | 1190 | `					 }` |
|         19 | 1191 | `				 }` |
|          5 | 1192 | `			 }` |
|     830174 | 1193 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1194 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      18569 | 1195 | `			 pCur++; /* Skip 'yield' keyword */` |
|      18569 | 1196 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1197 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1198 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      18569 | 1199 | `			 pNode->xCode = PH7_CompileYield;` |
|     575439 | 1200 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     565408 | 1201 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       9190 | 1202 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       4628 | 1203 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1204 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|       1613 | 1205 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1206 | `				 /* Assume a literal */` |
|        ! 0 | 1207 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1208 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1209 | `			 }else{` |
|          - | 1210 | `				 /* Assemble annonymous functions body */` |
|       1613 | 1211 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       1613 | 1212 | `				 if( rc != SXRET_OK ){` |
|         28 | 1213 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1214 | `					 return rc;` |
|          - | 1215 | `				 }` |
|       1589 | 1216 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1217 | `			  }` |
|     565341 | 1218 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         69 | 1219 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         43 | 1220 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         24 | 1221 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|         15 | 1222 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1223 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1224 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1225 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1226 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         61 | 1227 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         61 | 1228 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1229 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1230 | `				 return rc;` |
|          - | 1231 | `			 }` |
|         61 | 1232 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     564521 | 1233 | `		 }else if( (nKeyword == PH7_TKWRD_FN \|\| nKeyword == PH7_TKWRD_STATIC)` |
|     287657 | 1234 | `			&& PH7_TokenOpensArrowFunc(pGen->pIn,pCur,pGen->pEnd) ){` |
|          - | 1235 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       1695 | 1236 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       1695 | 1237 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1238 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1239 | `				 return rc;` |
|          - | 1240 | `			 }` |
|       1695 | 1241 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     563648 | 1242 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1243 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        109 | 1244 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        109 | 1245 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1246 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1247 | `				 return rc;` |
|          - | 1248 | `			 }` |
|        109 | 1249 | `			 pNode->xCode = PH7_CompileMatch;` |
|     562751 | 1250 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1251 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1252 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1253 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         44 | 1254 | `			 pCur++; /* Skip 'throw' */` |
|         44 | 1255 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1256 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1257 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         44 | 1258 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     562679 | 1259 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1260 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         91 | 1261 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         91 | 1262 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         48 | 1263 | `		 }else{` |
|          - | 1264 | `			 /* Assume a literal */` |
|     562573 | 1265 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     562573 | 1266 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1267 | `		 }` |
|   49555433 | 1268 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1269 | `		 /* Constants,function name,namespace path,class name... */` |
|   15843489 | 1270 | `		 if( bAfterMemberOp ){` |
|          - | 1271 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1272 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1273 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1274 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    6570797 | 1275 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    3285396 | 1276 | `		 }` |
|   15843489 | 1277 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   15843489 | 1278 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    7921747 | 1279 | `	 }else{` |
|   33094721 | 1280 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1281 | `			 /* Point to the code generator routine */` |
|   11198017 | 1282 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   11198017 | 1283 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1284 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1285 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1286 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1287 | `				 }` |
|          3 | 1288 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1289 | `				 return rc;` |
|          - | 1290 | `			 }` |
|    5599005 | 1291 | `		 }` |
|          - | 1292 | `		/* Advance the stream cursor */` |
|   33094719 | 1293 | `		pCur++;` |
|          - | 1294 | `	 }` |
|          - | 1295 | `	/* Point to the end of the token stream */` |
|  105992539 | 1296 | `	pNode->pEnd = pCur;` |
|          - | 1297 | `	/* Save the node for later processing */` |
|  105992539 | 1298 | `	*ppNode = pNode;` |
|          - | 1299 | `	/* Synchronize cursors */` |
|  105992539 | 1300 | `	pGen->pIn = pCur;` |
|  105992539 | 1301 | `	return SXRET_OK;` |
|   52998741 | 1302 | `}` |
|          - | 1303 | `/*` |
|          - | 1304 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1305 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1306 | ` * level is zero.` |
|          - | 1307 | ` */` |
|    2293246 | 1308 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1309 | `{` |
|    2293251 | 1310 | `	SyToken *pCur = pStart;` |
|    2293251 | 1311 | `	sxi32 iNest = 0;` |
|    2293251 | 1312 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1313 | `		/* Last expression */` |
|     836653 | 1314 | `		return SXERR_EOF;` |
|          - | 1315 | `	}` |
|    5526983 | 1316 | `	while( pCur < pEnd ){` |
|    5164849 | 1317 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    1094469 | 1318 | `			break;` |
|          - | 1319 | `		}` |
|    4070385 | 1320 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     291549 | 1321 | `			iNest++;` |
|    3924613 | 1322 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     291553 | 1323 | `			iNest--;` |
|     145774 | 1324 | `		}` |
|    4070385 | 1325 | `		pCur++;` |
|          5 | 1326 | `	}` |
|    1456603 | 1327 | `	*ppNext = pCur;` |
|    1456603 | 1328 | `	return SXRET_OK;` |
|    1146628 | 1329 | `}` |
|          - | 1330 | `/*` |
|          - | 1331 | ` * Free an expression tree.` |
|          - | 1332 | ` */` |
|   90712982 | 1333 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1334 | `{` |
|   90712987 | 1335 | `	if( pNode->pLeft ){` |
|          - | 1336 | `		/* Release the left tree */` |
|   35534749 | 1337 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   17767372 | 1338 | `	}` |
|   90712987 | 1339 | `	if( pNode->pRight ){` |
|          - | 1340 | `		/* Release the right tree */` |
|   20750639 | 1341 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   10375317 | 1342 | `	}` |
|   90712987 | 1343 | `	if( pNode->pCond ){` |
|          - | 1344 | `		/* Release the conditional tree used by the ternary operator */` |
|     601081 | 1345 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     300538 | 1346 | `	}` |
|   90712987 | 1347 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1348 | `		ph7_expr_node **apArg;` |
|          - | 1349 | `		sxu32 n;` |
|          - | 1350 | `		/* Release node arguments */` |
|    9744951 | 1351 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   22375655 | 1352 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   12630709 | 1353 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    6315357 | 1354 | `		}` |
|    9744951 | 1355 | `		SySetRelease(&pNode->aNodeArgs);` |
|    4872473 | 1356 | `	}` |
|          - | 1357 | `	/* Finally,release this node */` |
|   90712987 | 1358 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   90712987 | 1359 | `}` |
|          - | 1360 | `/*` |
|          - | 1361 | ` * Free an expression tree.` |
|          - | 1362 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1363 | ` */` |
|   19957900 | 1364 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1365 | `{` |
|          - | 1366 | `	ph7_expr_node **apNode;` |
|          - | 1367 | `	sxu32 n;` |
|   19957905 | 1368 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  125950509 | 1369 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  105992609 | 1370 | `		if( apNode[n] ){` |
|   19958233 | 1371 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    9979114 | 1372 | `		}` |
|   52996307 | 1373 | `	}` |
|   19957905 | 1374 | `	return SXRET_OK;` |
|          5 | 1375 | `}` |
|          - | 1376 | `/*` |
|          - | 1377 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1378 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1379 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1380 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1381 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1382 | ` */` |
|   24962682 | 1383 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1384 | `{` |
|   24962687 | 1385 | `	if( pNode == 0 ){` |
|   15556883 | 1386 | `		return 0;` |
|          - | 1387 | `	}` |
|    9405809 | 1388 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1389 | `		return 1;` |
|          - | 1390 | `	}` |
|    9405797 | 1391 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1392 | `		return 1;` |
|          - | 1393 | `	}` |
|    9405793 | 1394 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1395 | `		return 1;` |
|          - | 1396 | `	}` |
|    9405793 | 1397 | `	return 0;` |
|   12481346 | 1398 | `}` |
|          - | 1399 | `/*` |
|          - | 1400 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1401 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1402 | ` */` |
|    6123820 | 1403 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1404 | `{` |
|          - | 1405 | `	sxi32 iExprOp;` |
|    6123825 | 1406 | `	if( pNode->pOp == 0 ){` |
|    4382873 | 1407 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1408 | `	}` |
|    1740957 | 1409 | `	iExprOp = pNode->pOp->iOp;` |
|    1740957 | 1410 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    1119399 | 1411 | `			return TRUE;` |
|          - | 1412 | `	}` |
|     621563 | 1413 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     621549 | 1414 | `		if( pNode->pLeft->pOp ) {` |
|     149528 | 1415 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      63428 | 1416 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1417 | `				return FALSE;` |
|          5 | 1418 | `			}` |
|     546785 | 1419 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1420 | `			return FALSE;` |
|          - | 1421 | `		}` |
|     621549 | 1422 | `		return TRUE;` |
|          - | 1423 | `	}` |
|         16 | 1424 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1425 | `		return TRUE;` |
|          - | 1426 | `	}` |
|          - | 1427 | `	/* Not a modifiable l or r-value */` |
|          9 | 1428 | `	return FALSE;` |
|    3061915 | 1429 | `}` |
|          - | 1430 | `/* Forward declaration */` |
|          - | 1431 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|          - | 1432 | `/* Macro to check if the given node is a terminal.` |
|          - | 1433 | ` * A node is a term if it has no operator, or has already been linked into an` |
|          - | 1434 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|          - | 1435 | ` * linked ternary/elvis node). */` |
|          - | 1436 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|          - | 1437 | `/*` |
|          - | 1438 | ` * Buid an expression tree for each given function argument.` |
|          - | 1439 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1440 | ` */` |
|    6489942 | 1441 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1442 | `{` |
|          - | 1443 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1444 | `	sxi32 rc;` |
|          - | 1445 | ``	/* php: a stray token in a call argument is `... expecting ")"`. Each arg's`` |
|          - | 1446 | `	 * tree is built by the shared ExprMakeTree below, whose leftover-node error` |
|          - | 1447 | `	 * reads this. Saved/restored so a nested call or array element inside an arg` |
|          - | 1448 | `	 * gets its own closer. */` |
|    6489947 | 1449 | `	const char *zSaveArg = pGen->zClauseCloser;` |
|    6489947 | 1450 | `	pGen->zClauseCloser = "\")\"";` |
|          - | 1451 | `	/* Process function arguments from left to right */` |
|    6489947 | 1452 | `	iCur = 0;` |
|    7932808 | 1453 | `	for(;;){` |
|   15865621 | 1454 | `		if( iCur >= nToken ){` |
|          - | 1455 | `			/* No more arguments to process */` |
|    6489919 | 1456 | `			break;` |
|          - | 1457 | `		}` |
|    9375707 | 1458 | `		iNode = iCur;` |
|    9375707 | 1459 | `		iNest = 0;` |
|   30926613 | 1460 | `		while( iCur < nToken ){` |
|   24436697 | 1461 | `			if( apNode[iCur] ){` |
|   24381435 | 1462 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1442898 | 1463 | `					break;` |
|   21495644 | 1464 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   11514933 | 1465 | `					&& apNode[iCur]->pLeft == 0` |
|    1534215 | 1466 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1530747 | 1467 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1468 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1469 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1470 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1471 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1472 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1473 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|          - | 1474 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|          - | 1475 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|          - | 1476 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|          - | 1477 | `					 * and swallow the following argument separator. */` |
|    1527281 | 1478 | `					iNest++;` |
|   20732011 | 1479 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|   10747827 | 1480 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    1527281 | 1481 | `					iNest--;` |
|     763638 | 1482 | `				}` |
|   10747822 | 1483 | `			}` |
|   21550911 | 1484 | `			iCur++;` |
|          5 | 1485 | `		}` |
|    9375707 | 1486 | `		if( iCur > iNode ){` |
|    9375701 | 1487 | `			SyString sArgName = {0, 0};` |
|          - | 1488 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1489 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1490 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    9375696 | 1491 | `			if( (iCur - iNode) >= 2` |
|    6541608 | 1492 | `				&& apNode[iNode]` |
|    3707442 | 1493 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    2001011 | 1494 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     294258 | 1495 | `				&& apNode[iNode+1]` |
|     293863 | 1496 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1497 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        423 | 1498 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        423 | 1499 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        423 | 1500 | `				apNode[iNode] = 0;` |
|        423 | 1501 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        423 | 1502 | `				apNode[iNode+1] = 0;` |
|        423 | 1503 | `				iNode += 2;` |
|          - | 1504 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1505 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        423 | 1506 | `				if( iNode >= iCur ){` |
|          4 | 1507 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1508 | `						pOp->pStart->nLine,` |
|          - | 1509 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1510 | `						&sArgName);` |
|          3 | 1511 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1512 | `						rc = SXERR_SYNTAX;` |
|          1 | 1513 | `					}` |
|          3 | 1514 | `					pGen->zClauseCloser = zSaveArg;` |
|          3 | 1515 | `					return rc;` |
|          - | 1516 | `				}` |
|        208 | 1517 | `			}` |
|    9375694 | 1518 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1519 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1520 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1521 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1522 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1523 | `					apNode[iNode] = 0;` |
|        ! 0 | 1524 | `			}` |
|          - | 1525 | `			{` |
|          - | 1526 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1527 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1528 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1529 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1530 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1531 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1532 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1533 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1534 | `				 * NULL slots ahead of the flagged subtree. */` |
|    9375699 | 1535 | `				int bSpreadArg = 0;` |
|          - | 1536 | `				sxi32 iScan;` |
|    9375855 | 1537 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    9375855 | 1538 | `					if( apNode[iScan] ){` |
|    9375699 | 1539 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    9375699 | 1540 | `						break;` |
|          - | 1541 | `					}` |
|         83 | 1542 | `				}` |
|    9375699 | 1543 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    9375699 | 1544 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4733 | 1545 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2364 | 1546 | `				}` |
|          - | 1547 | `			}` |
|    9375699 | 1548 | `			if( apNode[iNode] ){` |
|    9375699 | 1549 | `				if( sArgName.nByte > 0 ){` |
|        421 | 1550 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        421 | 1551 | `					apNode[iNode]->sArgName = sArgName;` |
|        208 | 1552 | `				}` |
|          - | 1553 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    9375699 | 1554 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    4687852 | 1555 | `			}else{` |
|          - | 1556 | `				/* No expression before comma */` |
|        ! 0 | 1557 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1558 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1559 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1560 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1561 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1562 | `				}` |
|        ! 0 | 1563 | `				pGen->zClauseCloser = zSaveArg;` |
|        ! 0 | 1564 | `				return rc;` |
|          - | 1565 | `			}` |
|    4687852 | 1566 | `		}else{` |
|          - | 1567 | `			/* Comma with no preceding argument */` |
|          9 | 1568 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          9 | 1569 | `			if( rc != SXERR_ABORT ){` |
|          9 | 1570 | `				rc = SXERR_SYNTAX;` |
|          3 | 1571 | `			}` |
|          9 | 1572 | `			pGen->zClauseCloser = zSaveArg;` |
|          9 | 1573 | `			return rc;` |
|          - | 1574 | `		}` |
|          - | 1575 | `		/* Jump trailing comma */` |
|    9375699 | 1576 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2885785 | 1577 | `			iCur++;` |
|    2885785 | 1578 | `			if( iCur >= nToken ){` |
|          - | 1579 | `				/* Trailing comma after last argument */` |
|         21 | 1580 | `				break;` |
|          - | 1581 | `			}` |
|    1442880 | 1582 | `		}` |
|          5 | 1583 | `	}` |
|    6489939 | 1584 | `	pGen->zClauseCloser = zSaveArg;` |
|    6489939 | 1585 | `	return SXRET_OK;` |
|    3244976 | 1586 | `}` |
|          - | 1587 | ` /*` |
|          - | 1588 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|          - | 1589 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|          - | 1590 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|          - | 1591 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|          - | 1592 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|          - | 1593 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|          - | 1594 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|          - | 1595 | `  */` |
|         74 | 1596 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|          4 | 1597 | ` {` |
|          - | 1598 | `	 SyToken *pMin;` |
|          - | 1599 | `	 SyToken *pChild;` |
|         78 | 1600 | `	 if( pNode == 0 ){` |
|         50 | 1601 | `		 return 0;` |
|          - | 1602 | `	 }` |
|         32 | 1603 | `	 pMin = pNode->pStart;` |
|         32 | 1604 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|         32 | 1605 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|          6 | 1606 | `		 pMin = pChild;` |
|          2 | 1607 | `	 }` |
|         32 | 1608 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|         32 | 1609 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|        ! 0 | 1610 | `		 pMin = pChild;` |
|        ! 0 | 1611 | `	 }` |
|         32 | 1612 | `	 return pMin;` |
|         41 | 1613 | ` }` |
|          - | 1614 | `/*` |
|          - | 1615 | ` * The full RAW token extent of a linked subtree: minimum pStart / maximum pEnd` |
|          - | 1616 | ` * over every node (pLeft/pRight/pCond and postfix aNodeArgs children), widened` |
|          - | 1617 | ` * by one token on each side for a node whose group parens were consumed by` |
|          - | 1618 | ` * ExprMakeTree's paren pass (EXPR_NODE_PARENS — its '('/')' slots were nulled,` |
|          - | 1619 | ` * but token contiguity guarantees they sit exactly one token outside the inner` |
|          - | 1620 | ` * extent). Tokens live in one contiguous set, so pointer min/max IS source` |
|          - | 1621 | ` * order. Consumed by the assert() source-text capture, which` |
|          - | 1622 | ` * needs the argument's whole source span where a root's own pStart/pEnd name` |
|          - | 1623 | `` * only the operator token. Note: nested redundant groups `((x))` share the`` |
|          - | 1624 | ` * single PARENS bit, so only one paren layer is recovered — the renderer` |
|          - | 1625 | ` * strips redundant outermost parens anyway, matching php's export.` |
|          - | 1626 | ` */` |
|        536 | 1627 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax)` |
|          5 | 1628 | `{` |
|          - | 1629 | `	SyToken *pMin;` |
|          - | 1630 | `	SyToken *pMax;` |
|        541 | 1631 | `	SyToken *pCMin = 0;` |
|        541 | 1632 | `	SyToken *pCMax = 0;` |
|          - | 1633 | `	ph7_expr_node **apArg;` |
|          - | 1634 | `	sxu32 n;` |
|        541 | 1635 | `	if( pNode == 0 ){` |
|        385 | 1636 | `		return;` |
|          - | 1637 | `	}` |
|        161 | 1638 | `	pMin = pNode->pStart;` |
|        161 | 1639 | `	pMax = pNode->pEnd;` |
|        161 | 1640 | `	PH7_ExprSubtreeSpan(pNode->pLeft,&pCMin,&pCMax);` |
|        161 | 1641 | `	PH7_ExprSubtreeSpan(pNode->pRight,&pCMin,&pCMax);` |
|        161 | 1642 | `	PH7_ExprSubtreeSpan(pNode->pCond,&pCMin,&pCMax);` |
|        161 | 1643 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|        167 | 1644 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|          7 | 1645 | `		PH7_ExprSubtreeSpan(apArg[n],&pCMin,&pCMax);` |
|          4 | 1646 | `	}` |
|        161 | 1647 | `	if( pCMin && (pMin == 0 \|\| pCMin < pMin) ){` |
|         46 | 1648 | `		pMin = pCMin;` |
|         22 | 1649 | `	}` |
|        161 | 1650 | `	if( pCMax && (pMax == 0 \|\| pCMax > pMax) ){` |
|         50 | 1651 | `		pMax = pCMax;` |
|         24 | 1652 | `	}` |
|        161 | 1653 | `	if( (pNode->iFlags & EXPR_NODE_PARENS) && pMin && pMax ){` |
|          5 | 1654 | `		pMin--;` |
|          5 | 1655 | `		pMax++;` |
|          2 | 1656 | `	}` |
|        156 | 1657 | `	if( pNode->pOp && pMax` |
|         53 | 1658 | `	 && (pNode->pOp->iOp == EXPR_OP_FUNC_CALL \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT) ){` |
|          - | 1659 | `		/* A postfix call/subscript's extent stops AT its closing ')' / ']' (the` |
|          - | 1660 | `		 * closer's node was consumed building the postfix op); token contiguity` |
|          - | 1661 | `		 * puts the closer exactly at the extent, so widen one token past it. */` |
|          5 | 1662 | `		pMax++;` |
|          2 | 1663 | `	}` |
|        161 | 1664 | `	if( pMin && (*ppMin == 0 \|\| pMin < *ppMin) ){` |
|        117 | 1665 | `		*ppMin = pMin;` |
|         56 | 1666 | `	}` |
|        161 | 1667 | `	if( pMax && (*ppMax == 0 \|\| pMax > *ppMax) ){` |
|        159 | 1668 | `		*ppMax = pMax;` |
|         77 | 1669 | `	}` |
|        273 | 1670 | `}` |
|          - | 1671 | ` /*` |
|          - | 1672 | `  * Create an expression tree from an array of tokens.` |
|          - | 1673 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1674 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1675 | `  */` |
|   34590510 | 1676 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1677 | ` {` |
|          - | 1678 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1679 | `	 ph7_expr_node *pNode;` |
|          - | 1680 | `	 ph7_expr_node *pSuppress;` |
|          - | 1681 | `	 sxi32 iCur;` |
|          - | 1682 | `	 sxi32 rc;` |
|   34590515 | 1683 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1684 | `		 /* TICKET 1433-17: self evaluating node */` |
|   15190115 | 1685 | `		 return SXRET_OK;` |
|          - | 1686 | `	 }` |
|          - | 1687 | `	 /* Process expressions enclosed in parenthesis first */` |
|  138735559 | 1688 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1689 | `		 sxi32 iNest;` |
|          - | 1690 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1691 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1692 | `		  */` |
|  119335161 | 1693 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  118721315 | 1694 | `			 continue;` |
|          - | 1695 | `		 }` |
|     613851 | 1696 | `		 iNest = 1;` |
|     613851 | 1697 | `		 iLeft = iCur;` |
|          - | 1698 | `		 /* Find the closing parenthesis */` |
|     613851 | 1699 | `		 iCur++;` |
|    5528221 | 1700 | `		 while( iCur < nToken ){` |
|    5528221 | 1701 | `			 if( apNode[iCur] ){` |
|    5528221 | 1702 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1703 | `					 /* Decrement nesting level */` |
|     923231 | 1704 | `					 iNest--;` |
|     923231 | 1705 | `					 if( iNest <= 0 ){` |
|     613851 | 1706 | `						 break;` |
|          5 | 1707 | `					 }` |
|    4759685 | 1708 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1709 | `					 /* Increment nesting level */` |
|     309385 | 1710 | `					 iNest++;` |
|     154690 | 1711 | `				 }` |
|    2457185 | 1712 | `			 }` |
|    4914375 | 1713 | `			 iCur++;` |
|          5 | 1714 | `		 }` |
|     613851 | 1715 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1716 | `			 sxi32 j;` |
|          - | 1717 | `			 /* Recurse and process this expression */` |
|     613851 | 1718 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     613851 | 1719 | `			 if( rc != SXRET_OK ){` |
|          3 | 1720 | `				 return rc;` |
|          - | 1721 | `			 }` |
|          - | 1722 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1723 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1724 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1725 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1726 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1727 | `			  * group's free below silently drops the unpacking. */` |
|     613849 | 1728 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     613849 | 1729 | `				 if( apNode[j] ){` |
|     613849 | 1730 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     613844 | 1731 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     613849 | 1732 | `					 break;` |
|          - | 1733 | `				 }` |
|        ! 0 | 1734 | `			 }` |
|     306922 | 1735 | `		 }` |
|          - | 1736 | `		 /* Free the left and right nodes */` |
|     613849 | 1737 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     613849 | 1738 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     613849 | 1739 | `		 apNode[iLeft] = 0;` |
|     613849 | 1740 | `		 apNode[iCur] = 0;` |
|     306927 | 1741 | `	 }` |
|          - | 1742 | `	  /* Process expressions enclosed in braces */` |
|  143882577 | 1743 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1744 | `		 sxi32 iNest;` |
|          - | 1745 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1746 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1747 | `		  */` |
|  124854295 | 1748 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  124849759 | 1749 | `			 continue;` |
|          - | 1750 | `		 }` |
|       4541 | 1751 | `		 iNest = 1;` |
|       4541 | 1752 | `		 iLeft = iCur;` |
|          - | 1753 | `		 /* Find the closing parenthesis */` |
|       4541 | 1754 | `		 iCur++;` |
|       9075 | 1755 | `		 while( iCur < nToken ){` |
|       9075 | 1756 | `			 if( apNode[iCur] ){` |
|       9075 | 1757 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1758 | `					 /* Decrement nesting level */` |
|       4541 | 1759 | `					 iNest--;` |
|       4541 | 1760 | `					 if( iNest <= 0 ){` |
|       4541 | 1761 | `						 break;` |
|        ! 0 | 1762 | `					 }` |
|       4539 | 1763 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1764 | `					 /* Increment nesting level */` |
|        ! 0 | 1765 | `					 iNest++;` |
|        ! 0 | 1766 | `				 }` |
|       2267 | 1767 | `			 }` |
|       4539 | 1768 | `			 iCur++;` |
|          5 | 1769 | `		 }` |
|       4541 | 1770 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1771 | `			 /* Recurse and process this expression */` |
|       4539 | 1772 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       4539 | 1773 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1774 | `				 return rc;` |
|          - | 1775 | `			 }` |
|       2267 | 1776 | `		 }` |
|          - | 1777 | `		 /* Free the left and right nodes */` |
|       4541 | 1778 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       4541 | 1779 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       4541 | 1780 | `		 apNode[iLeft] = 0;` |
|       4541 | 1781 | `		 apNode[iCur] = 0;` |
|       2273 | 1782 | `	 }` |
|          - | 1783 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   19028287 | 1784 | `	 iLeft = -1;` |
|  143891607 | 1785 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  124863339 | 1786 | `		 if( apNode[iCur] == 0 ){` |
|   55171911 | 1787 | `			 continue;` |
|          - | 1788 | `		 }` |
|   69691433 | 1789 | `		 pNode = apNode[iCur];` |
|   69691433 | 1790 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   18803419 | 1791 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1792 | `				 /* Collect function arguments */` |
|    8265591 | 1793 | `				 sxi32 iPtr = 0;` |
|    8265591 | 1794 | `				 sxi32 nFuncTok = 0;` |
|   40967871 | 1795 | `				 while( nFuncTok + iCur < nToken ){` |
|   40967871 | 1796 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|          - | 1797 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|          - | 1798 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|          - | 1799 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|          - | 1800 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|          - | 1801 | `					  * nulled, so counting it here would over-count and never find` |
|          - | 1802 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   40967871 | 1803 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   40898851 | 1804 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    8832135 | 1805 | `							 iPtr++;` |
|   36482786 | 1806 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    8832135 | 1807 | `							 iPtr--;` |
|    8832135 | 1808 | `							 if( iPtr <= 0 ){` |
|    8265591 | 1809 | `								 break;` |
|          - | 1810 | `							 }` |
|     283272 | 1811 | `						 }` |
|   16316630 | 1812 | `					 }` |
|   32702285 | 1813 | `					 nFuncTok++;` |
|          5 | 1814 | `				 }` |
|    8265591 | 1815 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1816 | `					 /* Syntax error */` |
|        ! 0 | 1817 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1818 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1819 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1820 | `					 }` |
|        ! 0 | 1821 | `					 return rc;` |
|          - | 1822 | `				 }` |
|    8265591 | 1823 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1824 | `					 /* Syntax error */` |
|        ! 0 | 1825 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1826 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1827 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1828 | `					 }` |
|        ! 0 | 1829 | `					 return rc;` |
|          - | 1830 | `				 }` |
|    8265591 | 1831 | `				 if( nFuncTok > 1 ){` |
|          - | 1832 | `					 /* Process function arguments */` |
|    6489947 | 1833 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    6489947 | 1834 | `					 if( rc != SXRET_OK ){` |
|         11 | 1835 | `						 return rc;` |
|          - | 1836 | `					 }` |
|    3244967 | 1837 | `				 }` |
|          - | 1838 | `				 /* Link the node to the tree */` |
|    8265583 | 1839 | `				 pNode->pLeft = apNode[iLeft];` |
|    8265583 | 1840 | `				 apNode[iLeft] = 0;` |
|   40967839 | 1841 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   32702261 | 1842 | `					 apNode[iCur+iPtr] = 0;` |
|   16351133 | 1843 | `				 }` |
|          - | 1844 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1845 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1846 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1847 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1848 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1849 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1850 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1851 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1852 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1853 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1854 | `				 {` |
|    8265583 | 1855 | `					 sxi32 iNew = iLeft - 1;` |
|   10680053 | 1856 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    2414475 | 1857 | `						 iNew--;` |
|          5 | 1858 | `					 }` |
|    8265578 | 1859 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    4730449 | 1860 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2863992 | 1861 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    1013409 | 1862 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    1013409 | 1863 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    1013409 | 1864 | `						 apNode[iNew] = 0;` |
|    1013409 | 1865 | `						 pNode = apNode[iCur];` |
|     506707 | 1866 | `					 }` |
|          - | 1867 | `				 }` |
|   14670622 | 1868 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1869 | `				 /* Subscripting */` |
|    3527205 | 1870 | `				 sxi32 iArrTok = iCur + 1;` |
|    3527205 | 1871 | `				 sxi32 iNest = 1;` |
|    3527200 | 1872 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         58 | 1873 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         52 | 1874 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|          - | 1875 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|          - | 1876 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|          - | 1877 | `					  * subscripting a global constant raised "Invalid array name" while` |
|          - | 1878 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|          - | 1879 | `					  * worked. */` |
|         46 | 1880 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|         36 | 1881 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    3527200 | 1882 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1883 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1884 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     376147 | 1885 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1886 | `						 /* Syntax error */` |
|        ! 0 | 1887 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1888 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1889 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1890 | `						 }` |
|        ! 0 | 1891 | `						 return rc;` |
|          - | 1892 | `				 }` |
|          - | 1893 | `				 /* Collect index tokens */` |
|    7416305 | 1894 | `				 while( iArrTok < nToken ){` |
|    7416305 | 1895 | `					 if( apNode[iArrTok] ){` |
|    7416273 | 1896 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1897 | `							 /* Increment nesting level */` |
|      31701 | 1898 | `							 iNest++;` |
|    7400425 | 1899 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1900 | `							 /* Decrement nesting level */` |
|    3558901 | 1901 | `							 iNest--;` |
|    3558901 | 1902 | `							 if( iNest <= 0 ){` |
|    3527205 | 1903 | `								 break;` |
|          - | 1904 | `							 }` |
|      15848 | 1905 | `						 }` |
|    1944534 | 1906 | `					 }` |
|    3889105 | 1907 | `					 ++iArrTok;` |
|          5 | 1908 | `				 }` |
|    3527205 | 1909 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1910 | ``					 /* php: a stray token in a subscript index is `... expecting "]"`. */`` |
|    3255015 | 1911 | `					 const char *zSaveIdx = pGen->zClauseCloser;` |
|    3255015 | 1912 | `					 pGen->zClauseCloser = "\"]\"";` |
|          - | 1913 | `					 /* Recurse and process this expression */` |
|    3255015 | 1914 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    3255015 | 1915 | `					 pGen->zClauseCloser = zSaveIdx;` |
|    3255015 | 1916 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1917 | `						 return rc;` |
|          - | 1918 | `					 }` |
|          - | 1919 | `					 /* Link the node to it's index */` |
|    3255015 | 1920 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1627505 | 1921 | `				 }` |
|          - | 1922 | `				 /* Link the node to the tree */` |
|    3527205 | 1923 | `				 pNode->pLeft = apNode[iLeft];` |
|    3527205 | 1924 | `				 pNode->pRight = 0;` |
|    3527205 | 1925 | `				 apNode[iLeft] = 0;` |
|   10943505 | 1926 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    7416305 | 1927 | `					 apNode[iNest] = 0;` |
|    3708155 | 1928 | `				 }` |
|    1763605 | 1929 | `			 }else{` |
|          - | 1930 | `				 /* Member access operators [i.e: '->','::'] */` |
|    7010633 | 1931 | `				  iRight = iCur + 1;` |
|    7015167 | 1932 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       4539 | 1933 | `					 iRight++;` |
|          5 | 1934 | `				 }` |
|    7010633 | 1935 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1936 | `					 /* Syntax error */` |
|          5 | 1937 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1938 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1939 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1940 | `					 }` |
|          5 | 1941 | `					 return rc;` |
|          - | 1942 | `				 }` |
|          - | 1943 | `				 /* Validate the left operand BEFORE linking it. A node that is both` |
|          - | 1944 | `				  * still in apNode[] and already reachable as pNode->pLeft is freed` |
|          - | 1945 | `				  * TWICE by PH7_ExprFreeTree on the error path — a heap-use-after-free` |
|          - | 1946 | ``				  * that `1->x;`, `"s"->x;` and `[1]->x;` all reached. Ownership moves`` |
|          - | 1947 | `				  * out of the set only once the link is certain. */` |
|    7010624 | 1948 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    6765351 | 1949 | `					 && apNode[iLeft]->pOp == 0 &&` |
|    6414927 | 1950 | `					 apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|          - | 1951 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1952 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1953 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1954 | `					  * accepts. */` |
|          6 | 1955 | `					 apNode[iLeft]->xCode != PH7_CompileCloneCall ){` |
|          - | 1956 | `						 /* Syntax error */` |
|          4 | 1957 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          2 | 1958 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|          3 | 1959 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1960 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1961 | `						 }` |
|          3 | 1962 | `						 return rc;` |
|          - | 1963 | `				 }` |
|          - | 1964 | `				 /* Link the node to the tree */` |
|    7010627 | 1965 | `				 pNode->pLeft = apNode[iLeft];` |
|    7010627 | 1966 | `				 pNode->pRight = apNode[iRight];` |
|    7010627 | 1967 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1968 | `			 }` |
|    9401700 | 1969 | `		 }` |
|   69691419 | 1970 | `		 iLeft = iCur;` |
|   34845712 | 1971 | `	 }` |
|          - | 1972 | `	 /* Handle left associative (new, clone) operators */` |
|  143891571 | 1973 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  124863303 | 1974 | `		 if( apNode[iCur] == 0 ){` |
|   75062133 | 1975 | `			 continue;` |
|          - | 1976 | `		 }` |
|   49801175 | 1977 | `		 pNode = apNode[iCur];` |
|   49801175 | 1978 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1979 | `			 SyToken *pToken;` |
|          - | 1980 | `			 /* Get the left node */` |
|      73423 | 1981 | `			 iLeft = iCur + 1;` |
|      73431 | 1982 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1983 | `				 iLeft++;` |
|          1 | 1984 | `			 }` |
|      73423 | 1985 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1986 | `				  /* Syntax error */` |
|        ! 0 | 1987 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1988 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1989 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1990 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1991 | `				 }` |
|        ! 0 | 1992 | `				 return rc;` |
|          - | 1993 | `			 }` |
|          - | 1994 | `			 /* Make sure the operand are of a valid type */` |
|      73423 | 1995 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1996 | `				 /* Clone:` |
|          - | 1997 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1998 | `				  *  ++ function call (including annonymous)` |
|          - | 1999 | `				  *  ++ array member` |
|          - | 2000 | `				  *  ++ 'new' operator` |
|          - | 2001 | `				  * Example:` |
|          - | 2002 | `				  *   clone $pObj;` |
|          - | 2003 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 2004 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 2005 | `				  */` |
|      67977 | 2006 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      67971 | 2007 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 2008 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 2009 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 2010 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 2011 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2012 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2013 | `						 }` |
|        ! 0 | 2014 | `						 return rc;` |
|          - | 2015 | `					 }` |
|      33983 | 2016 | `				 }` |
|      33991 | 2017 | `			 }else{` |
|          - | 2018 | `				 /* New */` |
|       5446 | 2019 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 2020 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 2021 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 2022 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 2023 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 2024 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 2025 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 2026 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 2027 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 2028 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2029 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 2030 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 2031 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2032 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2033 | `					 }` |
|        ! 0 | 2034 | `					 return rc;` |
|          - | 2035 | `				 }` |
|       5451 | 2036 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       5451 | 2037 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       5446 | 2038 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         61 | 2039 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 2040 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 2041 | `						 /* Syntax error */` |
|        ! 0 | 2042 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2043 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 2044 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 2045 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2046 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2047 | `						 }` |
|        ! 0 | 2048 | `						 return rc;` |
|          - | 2049 | `					 }` |
|       2723 | 2050 | `				 }` |
|          - | 2051 | `			 }` |
|          - | 2052 | `			  /* Link the node to the tree */` |
|      73423 | 2053 | `			 pNode->pLeft = apNode[iLeft];` |
|      73423 | 2054 | `			 apNode[iLeft] = 0;` |
|      73423 | 2055 | `			 pNode->pRight = 0; /* Paranoid */` |
|      36709 | 2056 | `		 }` |
|   24900590 | 2057 | `	 }` |
|          - | 2058 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   19028273 | 2059 | `	 iLeft = -1;` |
|  143891571 | 2060 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  124677245 | 2061 | `		 if( apNode[iCur] == 0 ){` |
|   75062133 | 2062 | `			 continue;` |
|          - | 2063 | `		 }` |
|   49615117 | 2064 | `		 pNode = apNode[iCur];` |
|   49615117 | 2065 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     435691 | 2066 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     217849 | 2067 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 2068 | `					 /* Link the node to the tree */` |
|     236001 | 2069 | `					 pNode->pLeft = apNode[iLeft];` |
|     236001 | 2070 | `					 apNode[iLeft] = 0;` |
|     117998 | 2071 | `			 }` |
|     510556 | 2072 | `		  }` |
|   49801175 | 2073 | `		 iLeft = iCur;` |
|   24900590 | 2074 | `	  }` |
|   19214331 | 2075 | `	 iLeft = -1;` |
|  144077629 | 2076 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  124863303 | 2077 | `		 if( apNode[iCur] == 0 ){` |
|   75298129 | 2078 | `			 continue;` |
|          - | 2079 | `		 }` |
|   49565179 | 2080 | `		 pNode = apNode[iCur];` |
|   49565179 | 2081 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      13632 | 2082 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      13637 | 2083 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 2084 | `					 /* Syntax error */` |
|        ! 0 | 2085 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 2086 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2087 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2088 | `					 }` |
|        ! 0 | 2089 | `					 return rc;` |
|          - | 2090 | `			 }` |
|          - | 2091 | `			 /* Link the node to the tree */` |
|      13637 | 2092 | `			 pNode->pLeft = apNode[iLeft];` |
|      13637 | 2093 | `			 apNode[iLeft] = 0;` |
|          - | 2094 | `			 /* Mark as pre-increment/decrement node */` |
|      13637 | 2095 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       6816 | 2096 | `		  }` |
|   49565179 | 2097 | `		 iLeft = iCur;` |
|   24782592 | 2098 | `	 }` |
|          - | 2099 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 2100 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 2101 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 2102 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 2103 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 2104 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 2105 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 2106 | `	  * pass below skips it (pLeft != 0). */` |
|   19214331 | 2107 | `	 iLeft = -1;` |
|  144077629 | 2108 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  124863303 | 2109 | `		 if( apNode[iCur] == 0 ){` |
|   75407189 | 2110 | `			 continue;` |
|          - | 2111 | `		 }` |
|   49456119 | 2112 | `		 pNode = apNode[iCur];` |
|   49456119 | 2113 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      95433 | 2114 | `			 iRight = iCur + 1;` |
|      95433 | 2115 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 2116 | `				 iRight++;` |
|        ! 0 | 2117 | `			 }` |
|      95433 | 2118 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 2119 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2120 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2121 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2122 | `				 }` |
|        ! 0 | 2123 | `				 return rc;` |
|          - | 2124 | `			 }` |
|      95433 | 2125 | `			 pNode->pLeft = apNode[iLeft];` |
|      95433 | 2126 | `			 pNode->pRight = apNode[iRight];` |
|      95433 | 2127 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      47714 | 2128 | `		 }` |
|   49456119 | 2129 | `		 iLeft = iCur;` |
|   24728062 | 2130 | `	 }` |
|          - | 2131 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   19214331 | 2132 | `	  iLeft = 0;` |
|  144077623 | 2133 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  124863299 | 2134 | `		  if( apNode[iCur] ){` |
|   49360687 | 2135 | `			  pNode = apNode[iCur];` |
|   49360687 | 2136 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1659491 | 2137 | `				  if( iLeft > 0 ){` |
|          - | 2138 | `					  /* Link the node to the tree */` |
|    1659489 | 2139 | `					  pNode->pLeft = apNode[iLeft];` |
|    1659489 | 2140 | `					  apNode[iLeft] = 0;` |
|    1659489 | 2141 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      68041 | 2142 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 2143 | `							   /* Syntax error */` |
|        ! 0 | 2144 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2145 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2146 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 2147 | `							  }` |
|        ! 0 | 2148 | `							  return rc;` |
|          - | 2149 | `						  }` |
|      34018 | 2150 | `					  }` |
|     829747 | 2151 | `				  }else{` |
|          - | 2152 | `					  /* Syntax error */` |
|          3 | 2153 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 2154 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 2155 | `						  rc = SXERR_SYNTAX;` |
|          1 | 2156 | `					  }` |
|          3 | 2157 | `					  return rc;` |
|          - | 2158 | `				  }` |
|     829742 | 2159 | `			  }` |
|          - | 2160 | `			  /* Save terminal position */` |
|   49360685 | 2161 | `			  iLeft = iCur;` |
|   24680340 | 2162 | `		  }` |
|   62431651 | 2163 | `	  }` |
|          - | 2164 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 2165 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 2166 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 2167 | `	  * yielding a right-leaning tree. */` |
|  144077621 | 2168 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  124863297 | 2169 | `		 if( apNode[iCur] == 0 ){` |
|   77162217 | 2170 | `			 continue;` |
|          - | 2171 | `		 }` |
|   47701085 | 2172 | `		 pNode = apNode[iCur];` |
|   47701085 | 2173 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 2174 | `			 sxi32 iL, iR;` |
|          - | 2175 | `			 /* Find the right operand */` |
|        117 | 2176 | `			 iR = -1;` |
|          - | 2177 | `			 {` |
|          - | 2178 | `				 sxi32 j;` |
|        129 | 2179 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        129 | 2180 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 2181 | `				 }` |
|          - | 2182 | `			 }` |
|          - | 2183 | `			 /* Find the left operand */` |
|        117 | 2184 | `			 iL = -1;` |
|          - | 2185 | `			 {` |
|          - | 2186 | `				 sxi32 j;` |
|        185 | 2187 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        185 | 2188 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 2189 | `				 }` |
|          - | 2190 | `			 }` |
|        117 | 2191 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 2192 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2193 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2194 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2195 | `				 }` |
|        ! 0 | 2196 | `				 return rc;` |
|          - | 2197 | `			 }` |
|        117 | 2198 | `			 pNode->pLeft  = apNode[iL];` |
|        117 | 2199 | `			 pNode->pRight = apNode[iR];` |
|        117 | 2200 | `			 apNode[iL] = 0;` |
|        117 | 2201 | `			 apNode[iR] = 0;` |
|          - | 2202 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 2203 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 2204 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 2205 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 2206 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 2207 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 2208 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 2209 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 2210 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 2211 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 2212 | `			  * operands are respected. */` |
|        116 | 2213 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         76 | 2214 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 2215 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 2216 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 2217 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 2218 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 2219 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 2220 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 2221 | `				 while( pTail->pLeft` |
|         34 | 2222 | `					 && pTail->pLeft->pOp` |
|         23 | 2223 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 2224 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 2225 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 2226 | `					 pTail = pTail->pLeft;` |
|          1 | 2227 | `				 }` |
|          - | 2228 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 2229 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 2230 | `				 pTail->pLeft = pNode;` |
|         27 | 2231 | `				 apNode[iCur] = pHead;` |
|         13 | 2232 | `			 }` |
|         58 | 2233 | `		 }` |
|   23850545 | 2234 | `	 }` |
|          - | 2235 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  211357483 | 2236 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  192143169 | 2237 | `		 iLeft = -1;` |
| 1440775795 | 2238 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 1248632641 | 2239 | `			 if( apNode[iCur] == 0 ){` |
|  853535075 | 2240 | `				 continue;` |
|          - | 2241 | `			 }` |
|  395097571 | 2242 | `			 pNode = apNode[iCur];` |
|  395097571 | 2243 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2244 | `				 /* Get the right node */` |
|    6919733 | 2245 | `				 iRight = iCur + 1;` |
|   10478609 | 2246 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    3558881 | 2247 | `					 iRight++;` |
|          5 | 2248 | `				 }` |
|    6919733 | 2249 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2250 | `					 /* Syntax error */` |
|         10 | 2251 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 2252 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2253 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2254 | `					 }` |
|         10 | 2255 | `					 return rc;` |
|          - | 2256 | `				 }` |
|    6919725 | 2257 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2258 | `					 sxi32  iTmp;` |
|          - | 2259 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2260 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2261 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2262 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2263 | `					  * is swapped below. */` |
|        115 | 2264 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2265 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2266 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2267 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2268 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2269 | `						 }` |
|          3 | 2270 | `						 return rc;` |
|          - | 2271 | `					 }` |
|          - | 2272 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|          - | 2273 | `					  * reference target — ExprIsModifiableValue already accepts` |
|          - | 2274 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|          - | 2275 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|          - | 2276 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|          - | 2277 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|        113 | 2278 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2279 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2280 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2281 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2282 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2283 | `						 }` |
|        ! 0 | 2284 | `						 return rc;` |
|          - | 2285 | `					 }` |
|        113 | 2286 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         75 | 2287 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2288 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2289 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2290 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2291 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2292 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2293 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2294 | `									 }` |
|        ! 0 | 2295 | `									 return rc;` |
|          - | 2296 | `							 }` |
|        ! 0 | 2297 | `						 }` |
|         35 | 2298 | `					 }` |
|          - | 2299 | `					 /* Swap operands */` |
|        113 | 2300 | `					 iTmp = iRight;` |
|        113 | 2301 | `					 iRight = iLeft;` |
|        113 | 2302 | `					 iLeft = iTmp;` |
|         54 | 2303 | `				 }` |
|          - | 2304 | `				 /* Link the node to the tree */` |
|    6919723 | 2305 | `				 pNode->pLeft = apNode[iLeft];` |
|    6919723 | 2306 | `				 pNode->pRight = apNode[iRight];` |
|    6919723 | 2307 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    3459859 | 2308 | `			 }` |
|  395097561 | 2309 | `			 iLeft = iCur;` |
|  197548783 | 2310 | `		 }` |
|   96071582 | 2311 | `	 }` |
|          - | 2312 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2313 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2314 | `	  * we are dealing with a single operator.` |
|          - | 2315 | `	  */` |
|   19214319 | 2316 | `	  iLeft = -1;` |
|  139172397 | 2317 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  120559161 | 2318 | `		  if( apNode[iCur] == 0 ){` |
|   88496319 | 2319 | `			  continue;` |
|          - | 2320 | `		  }` |
|   32062847 | 2321 | `		  pNode = apNode[iCur];` |
|   32062847 | 2322 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     601083 | 2323 | `			  sxi32 iNest = 1;` |
|     601083 | 2324 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2325 | `				  /* Missing condition */` |
|          3 | 2326 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2327 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2328 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2329 | `				  }` |
|          3 | 2330 | `				  return rc;` |
|          - | 2331 | `			  }` |
|          - | 2332 | `			  /* Get the right node */` |
|     601081 | 2333 | `			  iRight = iCur + 1;` |
|    2524873 | 2334 | `			  while( iRight < nToken  ){` |
|    2524873 | 2335 | `				  if( apNode[iRight] ){` |
|    1197557 | 2336 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2337 | `						  /* Increment nesting level */` |
|        ! 0 | 2338 | `						  ++iNest;` |
|    1197557 | 2339 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2340 | `						  /* Decrement nesting level */` |
|     601081 | 2341 | `						  --iNest;` |
|     601081 | 2342 | `						  if( iNest <= 0 ){` |
|     601081 | 2343 | `							  break;` |
|          - | 2344 | `						  }` |
|        ! 0 | 2345 | `					  }` |
|     298238 | 2346 | `				  }` |
|    1923797 | 2347 | `				  iRight++;` |
|          5 | 2348 | `			  }` |
|     601081 | 2349 | `			  if( iRight > iCur + 1 ){` |
|          - | 2350 | `				  /* Recurse and process the then expression */` |
|     596481 | 2351 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     596481 | 2352 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2353 | `					  return rc;` |
|          - | 2354 | `				  }` |
|          - | 2355 | `				  /* Link the node to the tree */` |
|     596481 | 2356 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     298238 | 2357 | `			  }else{` |
|          - | 2358 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2359 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2360 | `			  }` |
|     601081 | 2361 | `			  apNode[iCur + 1] = 0;` |
|     601081 | 2362 | `			  if( iRight + 1 < nToken ){` |
|          - | 2363 | `				  /* Recurse and process the else expression */` |
|     601081 | 2364 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     601081 | 2365 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2366 | `					  return rc;` |
|          - | 2367 | `				  }` |
|          - | 2368 | `				  /* Link the node to the tree */` |
|     601081 | 2369 | `				  pNode->pRight = apNode[iRight + 1];` |
|     601081 | 2370 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     300543 | 2371 | `			  }else{` |
|        ! 0 | 2372 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2373 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2374 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2375 | `				 }` |
|        ! 0 | 2376 | `				 return rc;` |
|          - | 2377 | `			  }` |
|          - | 2378 | `			  /* Point to the condition */` |
|     601081 | 2379 | `			  pNode->pCond  = apNode[iLeft];` |
|     601081 | 2380 | `			  apNode[iLeft] = 0;` |
|     601081 | 2381 | `			  break;` |
|          - | 2382 | `		  }` |
|   31461769 | 2383 | `		  iLeft = iCur;` |
|   15730887 | 2384 | `	  }` |
|          - | 2385 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2386 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2387 | `	  * so there is no need for a precedence loop here.` |
|          - | 2388 | `	  */` |
|   19214317 | 2389 | `	 iRight = -1;` |
|  144077431 | 2390 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  124863171 | 2391 | `		 if( apNode[iCur] == 0 ){` |
|   99525091 | 2392 | `			 continue;` |
|          - | 2393 | `		 }` |
|   25338085 | 2394 | `		 pNode = apNode[iCur];` |
|   25338085 | 2395 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2396 | `			 /* Get the left node */` |
|    6123687 | 2397 | `			 iLeft = iCur - 1;` |
|    8390571 | 2398 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    2266889 | 2399 | `				 iLeft--;` |
|          5 | 2400 | `			 }` |
|    6123687 | 2401 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2402 | `				 /* Syntax error */` |
|         45 | 2403 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2404 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2405 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2406 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2407 | `				 }else{` |
|         41 | 2408 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2409 | `				 }` |
|         45 | 2410 | `				 if( rc != SXERR_ABORT ){` |
|         43 | 2411 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2412 | `				 }` |
|         45 | 2413 | `				 return rc;` |
|          - | 2414 | `			 }` |
|          - | 2415 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2416 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2417 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2418 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2419 | `			  * a write. */` |
|    6123645 | 2420 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2421 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2422 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2423 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2424 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2425 | `				 }` |
|         11 | 2426 | `				 return rc;` |
|          - | 2427 | `			 }` |
|          - | 2428 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2429 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2430 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2431 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2432 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    6123637 | 2433 | `			 pSuppress = 0;` |
|    6123632 | 2434 | `			 if( apNode[iLeft]->pOp` |
|    3932251 | 2435 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     870435 | 2436 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2437 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2438 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2439 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2440 | `			 }` |
|    6123637 | 2441 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2442 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2443 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2444 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2445 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2446 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2447 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        159 | 2448 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2449 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2450 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2451 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2452 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2453 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2454 | `					 }` |
|          8 | 2455 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2456 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2457 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2458 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2459 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2460 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2461 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2462 | `						 iRight = iCur;` |
|          9 | 2463 | `						 continue;` |
|          - | 2464 | `					 }` |
|        ! 0 | 2465 | `				 }` |
|        204 | 2466 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        144 | 2467 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2468 | `					 /* Left operand must be a modifiable l-value */` |
|          3 | 2469 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2470 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2471 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2472 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2473 | `					 }else{` |
|        ! 0 | 2474 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 2475 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2476 | `					 }` |
|          3 | 2477 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 2478 | `						 rc = SXERR_SYNTAX;` |
|          1 | 2479 | `					 }` |
|          3 | 2480 | `					 return rc;` |
|          - | 2481 | `				 }` |
|         72 | 2482 | `			 }` |
|          - | 2483 | `			 /* Link the node to the tree (Reverse) */` |
|    6123627 | 2484 | `			 pNode->pLeft = apNode[iRight];` |
|    6123627 | 2485 | `			 pNode->pRight = apNode[iLeft];` |
|    6123627 | 2486 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    6123627 | 2487 | `			 if( pSuppress ){` |
|          - | 2488 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2489 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2490 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2491 | `			 }` |
|    3061811 | 2492 | `		 }` |
|   25338025 | 2493 | `		 iRight = iCur;` |
|   12669015 | 2494 | `	 }` |
|          - | 2495 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   96071305 | 2496 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   76857045 | 2497 | `		 iLeft = -1;` |
|  576309453 | 2498 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  499452413 | 2499 | `			 if( apNode[iCur] == 0 ){` |
|  422595113 | 2500 | `				 continue;` |
|          - | 2501 | `			 }` |
|   76857305 | 2502 | `			 pNode = apNode[iCur];` |
|   76857305 | 2503 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2504 | `				 /* Get the right node */` |
|         56 | 2505 | `				 iRight = iCur + 1;` |
|         68 | 2506 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2507 | `					 iRight++;` |
|          1 | 2508 | `				 }` |
|         56 | 2509 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2510 | `					 /* Syntax error */` |
|        ! 0 | 2511 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2512 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2513 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2514 | `					 }` |
|        ! 0 | 2515 | `					 return rc;` |
|          - | 2516 | `				 }` |
|          - | 2517 | `				 /* Link the node to the tree */` |
|         56 | 2518 | `				 pNode->pLeft = apNode[iLeft];` |
|         56 | 2519 | `				 pNode->pRight = apNode[iRight];` |
|         56 | 2520 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         26 | 2521 | `			 }` |
|   76857305 | 2522 | `			 iLeft = iCur;` |
|   38428655 | 2523 | `		 }` |
|   38428525 | 2524 | `	 }` |
|          - | 2525 | `	 /* Point to the root of the expression tree */` |
|  124863079 | 2526 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  105648837 | 2527 | `		 if( apNode[iCur] ){` |
|   18279365 | 2528 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|          - | 2529 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|          - | 2530 | `				  * operator sitting at its slot. The "expecting" clause is the closer` |
|          - | 2531 | ``				  * the enclosing construct set (`;` after `return`, `,`/`;` after`` |
|          - | 2532 | ``				  * `echo`, `)` for a for() post clause …); a for() clause defaults to`` |
|          - | 2533 | ``				  * `;` when nothing more specific was set. php prints no clause for a`` |
|          - | 2534 | `				  * plain expression statement, so a NULL closer stays clauseless. */` |
|         22 | 2535 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|         22 | 2536 | `				 const char *zExpect = pGen->zClauseCloser;` |
|         22 | 2537 | `				 if( zExpect == 0 && pGen->nCommaExprOk > 0 ){` |
|        ! 0 | 2538 | `					 zExpect = "\";\"";` |
|        ! 0 | 2539 | `				 }` |
|         22 | 2540 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,zExpect);` |
|         22 | 2541 | `				  if( rc != SXERR_ABORT ){` |
|         22 | 2542 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2543 | `				  }` |
|         22 | 2544 | `				  return rc;` |
|          - | 2545 | `			 }` |
|   18279347 | 2546 | `			 apNode[0] = apNode[iCur];` |
|   18279347 | 2547 | `			 apNode[iCur] = 0;` |
|    9139671 | 2548 | `		 }` |
|   52824412 | 2549 | `	 }` |
|   19214247 | 2550 | `	 return SXRET_OK;` |
|   17202231 | 2551 | ` }` |
|          - | 2552 | ` /*` |
|          - | 2553 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2554 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2555 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2556 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2557 | `  */` |
|   19957904 | 2558 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2559 | `{` |
|          - | 2560 | `	ph7_expr_node **apNode;` |
|          - | 2561 | `	ph7_expr_node *pNode;` |
|          - | 2562 | `	sxi32 rc;` |
|          - | 2563 | `	/* Reset node container */` |
|   19957909 | 2564 | `	SySetReset(pExprNode);` |
|   19957909 | 2565 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2566 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2567 | `	{` |
|   19957909 | 2568 | `		int iLastWasTerm = 0;` |
|   19957909 | 2569 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  125950543 | 2570 | `		while( pGen->pIn < pGen->pEnd ){` |
|  105992679 | 2571 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  105992679 | 2572 | `			if( rc != SXRET_OK ){` |
|         44 | 2573 | `				return rc;` |
|          - | 2574 | `			}` |
|          - | 2575 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  105992639 | 2576 | `			if( pNode->xCode ){` |
|          - | 2577 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   53936225 | 2578 | `				iLastWasTerm = 1;` |
|   79024529 | 2579 | `			}else if( pNode->pOp ){` |
|          - | 2580 | `				/* Operator node */` |
|   30159715 | 2581 | `				iLastWasTerm = 0;` |
|   15079860 | 2582 | `			}else{` |
|          - | 2583 | `				/* Delimiter: ')' and ']' end terms */` |
|   21896709 | 2584 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2585 | `			}` |
|          - | 2586 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2587 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2588 | `			 * node kind, so this single test covers all branches. */` |
|  105992639 | 2589 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2590 | `			/* Save the extracted node */` |
|  105992639 | 2591 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2592 | `		}` |
|          - | 2593 | `	}` |
|   19957869 | 2594 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2595 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2596 | `		*ppRoot = 0;` |
|        ! 0 | 2597 | `		return SXRET_OK;` |
|          - | 2598 | `	}` |
|   19957869 | 2599 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2600 | `	/* Make sure we are dealing with valid nodes */` |
|   19957869 | 2601 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   19957869 | 2602 | `	if( rc != SXRET_OK ){` |
|          - | 2603 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2604 | `		 * cleanup the mess left behind.` |
|          - | 2605 | `		 */` |
|         52 | 2606 | `		*ppRoot = 0;` |
|         52 | 2607 | `		return rc;` |
|          - | 2608 | `	}` |
|          - | 2609 | `	/* Build the tree */` |
|   19957821 | 2610 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   19957821 | 2611 | `	if( rc != SXRET_OK ){` |
|          - | 2612 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        103 | 2613 | `		*ppRoot = 0;` |
|        103 | 2614 | `		return rc;` |
|          - | 2615 | `	}` |
|          - | 2616 | `	/* Point to the root of the tree */` |
|   19957723 | 2617 | `	*ppRoot = apNode[0];` |
|   19957723 | 2618 | `	return SXRET_OK;` |
|    9978957 | 2619 | `}` |
|          - | 2620 |  |
