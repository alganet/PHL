# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1182/1361 lines (86.85%)

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
|   29232390 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   29232395 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  461235379 |  278 | `	for(;;){` |
|  922470763 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  922470763 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   88165705 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   44082855 |  285 | `		}else{` |
|  834305063 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  922470763 |  288 | `		if( rc == 0 ){` |
|   29567893 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   29111171 |  291 | `				return &aOpTable[n];` |
|          - |  292 | `			}` |
|          - |  293 | `			/* Handle ambiguity */` |
|     456727 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|          - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      43297 |  296 | `				return &aOpTable[n];` |
|          - |  297 | `			}` |
|     413435 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      77945 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|          - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      77945 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|          - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      77937 |  303 | `					return &aOpTable[n];` |
|          - |  304 | `				}` |
|          - |  305 |  |
|          4 |  306 | `			}` |
|     167749 |  307 | `		}` |
|  893238373 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   14616200 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    7947848 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    7947853 |  320 | `	SyToken *pCur = pIn;` |
|    7947853 |  321 | `	sxi32 iNest = 1;` |
|   85238502 |  322 | `	for(;;){` |
|  170477009 |  323 | `		if( pCur >= pEnd ){` |
|      16075 |  324 | `			break;` |
|          - |  325 | `		}` |
|  170460939 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    6570733 |  328 | `			iNest++;` |
|  167175575 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   14502511 |  331 | `			iNest--;` |
|   14502511 |  332 | `			if( iNest <= 0 ){` |
|    7931783 |  333 | `				break;` |
|          - |  334 | `			}` |
|    3285364 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  162529161 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    7947853 |  340 | `	*ppEnd = pCur;` |
|    7947853 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     542124 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     542124 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     542087 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        131 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     542003 |  358 | `	if( bCheckFunc ){` |
|      58790 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      58778 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      58754 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|         61 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      29367 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     541947 |  366 | `	return FALSE;` |
|     271067 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   17147110 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   17147115 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7819 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7819 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3907 |  383 | `	}` |
|   17147115 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  107483843 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   90336769 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     243805 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   90092969 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    7568653 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     409742 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    7076349 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    7076349 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    7076349 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    7076349 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3538172 |  401 | `					}` |
|    3538172 |  402 | `			}` |
|    7568653 |  403 | `			iParen++;` |
|   86308645 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    7568655 |  405 | `			if( iParen <= 0 ){` |
|         16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         16 |  407 | `				if( rc != SXERR_ABORT ){` |
|         16 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         16 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    7568643 |  412 | `			iParen--;` |
|   78739990 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    3033877 |  414 | `			iSquare++;` |
|   73438735 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    3033881 |  416 | `			if( iSquare <= 0 ){` |
|          9 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          9 |  418 | `				if( rc != SXERR_ABORT ){` |
|          9 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          9 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    3033875 |  423 | `			iSquare--;` |
|   70404858 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       3905 |  425 | `			iBraces++;` |
|       3905 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|          - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|          - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|          - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|          3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|          3 |  431 | `				if( rc != SXERR_ABORT ){` |
|          3 |  432 | `					rc = SXERR_SYNTAX;` |
|          1 |  433 | `				}` |
|          3 |  434 | `				return rc;` |
|          5 |  435 | `			}` |
|   68885972 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3915 |  437 | `			if( iBraces <= 0 ){` |
|         16 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         16 |  439 | `				if( rc != SXERR_ABORT ){` |
|         16 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         16 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3903 |  444 | `			iBraces--;` |
|   68882062 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     505385 |  446 | `			if( iQuesty > 0 ){` |
|     505083 |  447 | `				iQuesty--;` |
|     252846 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   68627421 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   22583903 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   22583903 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     505085 |  461 | `				iQuesty++;` |
|   22331363 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      93919 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      46957 |  481 | `			}` |
|   11291949 |  482 | `		}` |
|   45046469 |  483 | `	}` |
|   17147079 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         17 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         17 |  486 | `		if( rc != SXERR_ABORT ){` |
|         17 |  487 | `			rc = SXERR_SYNTAX;` |
|          7 |  488 | `		}` |
|         17 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   17147065 |  491 | `	return SXRET_OK;` |
|    8573560 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   14139200 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   14139205 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   14139205 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   14135245 |  502 | `		pIn++;` |
|    7067620 |  503 | `	}` |
|    7071621 |  504 | `	for(;;){` |
|   14143247 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       4047 |  506 | `			pIn++;` |
|       4047 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       4045 |  508 | `				pIn++;` |
|       2020 |  509 | `			}` |
|       2026 |  510 | `		}else{` |
|    7069605 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   14139205 |  515 | `	*ppCur = pIn;` |
|   14139205 |  516 | `}` |
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
|         12 |  671 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|         12 |  672 | `			if( rc != SXERR_ABORT ){` |
|         12 |  673 | `				rc = SXERR_SYNTAX;` |
|          4 |  674 | `			}` |
|         12 |  675 | `			goto Synchronize;` |
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
|        546 |  807 | `			iNest++;` |
|       3412 |  808 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        546 |  809 | `			iNest--;` |
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
|   90341074 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  868 | `{` |
|          - |  869 | `	ph7_expr_node *pNode;` |
|          - |  870 | `	SyToken *pCur;` |
|          - |  871 | `	sxi32 rc;` |
|          - |  872 | `	/* Allocate a new node */` |
|   90341079 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   90341079 |  874 | `	if( pNode == 0 ){` |
|          - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  877 | `		 */` |
|        ! 0 |  878 | `		return SXERR_MEM;` |
|          - |  879 | `	}` |
|          - |  880 | `	/* Zero the structure */` |
|   90341079 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   90341079 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  883 | `	/* Point to the head of the token stream */` |
|   90341079 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  885 | `	/* Start collecting tokens */` |
|   90341079 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4245 |  887 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
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
|       4163 |  902 | `		pCur++;` |
|       4163 |  903 | `		pGen->pIn = pCur;` |
|       4163 |  904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4163 |  905 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4163 |  906 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4163 |  907 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2079 |  908 | `		}` |
|       4163 |  909 | `		return rc;` |
|          - |  910 | `	}` |
|   90336839 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - |  912 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - |  913 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - |  914 | `		 */` |
|     243807 |  915 | `		pCur++; /* Skip the opening '[' */` |
|     243807 |  916 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     243807 |  917 | `		if( pCur < pGen->pEnd ){` |
|     243807 |  918 | `			pCur++; /* Skip past the closing ']' */` |
|     121906 |  919 | `		}else{` |
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
|     244014 |  931 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        419 |  932 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        419 |  933 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|         57 |  934 | `				pNode->xCode = PH7_CompileShortList;` |
|         30 |  935 | `			}else{` |
|        365 |  936 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - |  937 | `			}` |
|        212 |  938 | `		}else{` |
|     243393 |  939 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 |  940 | `		}` |
|   90214938 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|   90093024 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   25617826 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   12838127 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|        ! 0 |  971 | `			rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|        ! 0 |  972 | `			if( rc != SXERR_ABORT ){` |
|        ! 0 |  973 | `				rc = SXERR_SYNTAX;` |
|        ! 0 |  974 | `			}` |
|        ! 0 |  975 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 |  976 | `			return rc;` |
|          - |  977 | `		}` |
|         24 |  978 | `		pNode->xCode = PH7_CompileCloneCall;` |
|   90093008 |  979 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - |  980 | `		/* Point to the instance that describe this operator */` |
|   25617809 |  981 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - |  982 | `		/* Advance the stream cursor */` |
|   25617809 |  983 | `		pCur++;` |
|   77284095 |  984 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - |  985 | `		/* Isolate variable */` |
|   43335301 |  986 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   21667659 |  987 | `			pCur++; /* Variable variable */` |
|          5 |  988 | `		}` |
|   21667647 |  989 | `		if( pCur < pGen->pEnd ){` |
|   21667647 |  990 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - |  991 | `				/* Variable name */` |
|   21667621 |  992 | `				pCur++;` |
|   10833839 |  993 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|         22 |  994 | `				pCur++;` |
|          - |  995 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|         22 |  996 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|         22 |  997 | `				if( pCur < pGen->pEnd ){` |
|         19 |  998 | `					pCur++;` |
|         11 |  999 | `				}else{` |
|          3 | 1000 | `					rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1001 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1002 | `						rc = SXERR_SYNTAX;` |
|          1 | 1003 | `					}` |
|          3 | 1004 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1005 | `					return rc;` |
|          - | 1006 | `				}` |
|          8 | 1007 | `			}` |
|   10833820 | 1008 | `		}` |
|   21667645 | 1009 | `		pNode->xCode = PH7_CompileVariable;` |
|   53641371 | 1010 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1058955 | 1011 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    1058955 | 1012 | `		 if( bAfterMemberOp ){` |
|          - | 1013 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1014 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1015 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1016 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1017 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1018 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1019 | `			  * the word itself. */` |
|     128625 | 1020 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     128625 | 1021 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     128625 | 1022 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     994645 | 1023 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1024 | `			 /* List/Array node */` |
|     429779 | 1025 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1026 | `				 /* Assume a literal */` |
|        ! 0 | 1027 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1028 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1029 | `			 }else{` |
|     429779 | 1030 | `				 pCur += 2;` |
|          - | 1031 | `				 /* Collect array/list tokens */` |
|     429779 | 1032 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     429779 | 1033 | `				 if( pCur < pGen->pEnd ){` |
|     429777 | 1034 | `					 pCur++;` |
|     214891 | 1035 | `				 }else{` |
|          - | 1036 | `					 /* Syntax error */` |
|          - | 1037 | `					 /* php names the token it stopped on and says it expected ")". */` |
|          3 | 1038 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|          3 | 1039 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1040 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1041 | `					 }` |
|          3 | 1042 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1043 | `					 return rc;` |
|          - | 1044 | `				 }` |
|     429777 | 1045 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     429777 | 1046 | `				 if( pNode->xCode == PH7_CompileList ){` |
|         39 | 1047 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|         39 | 1048 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|          - | 1049 | `						 /* Syntax error */` |
|          3 | 1050 | `						 rc = PH7_GenSyntaxError(pGen,pNode->pStart,"\"=\"");` |
|          3 | 1051 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1052 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1053 | `						 }` |
|          3 | 1054 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1055 | `						 return rc;` |
|          - | 1056 | `					 }` |
|         16 | 1057 | `				 }` |
|          5 | 1058 | `			 }` |
|     715446 | 1059 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1060 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15953 | 1061 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15953 | 1062 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1063 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1064 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15953 | 1065 | `			 pNode->xCode = PH7_CompileYield;` |
|     492587 | 1066 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     484320 | 1067 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7872 | 1068 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3951 | 1069 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1070 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|        647 | 1071 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1072 | `				 /* Assume a literal */` |
|        ! 0 | 1073 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1074 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1075 | `			 }else{` |
|          - | 1076 | `				 /* Assemble annonymous functions body */` |
|        647 | 1077 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|        647 | 1078 | `				 if( rc != SXRET_OK ){` |
|         28 | 1079 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1080 | `					 return rc;` |
|          - | 1081 | `				 }` |
|        623 | 1082 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1083 | `			  }` |
|     484280 | 1084 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         43 | 1085 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         24 | 1086 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         12 | 1087 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|          9 | 1088 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1089 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1090 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1091 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1092 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         34 | 1093 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         34 | 1094 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1095 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1096 | `				 return rc;` |
|          - | 1097 | `			 }` |
|         34 | 1098 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     483955 | 1099 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     483676 | 1100 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7848 | 1101 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3927 | 1102 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1103 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        551 | 1104 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        551 | 1105 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1106 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1107 | `				 return rc;` |
|          - | 1108 | `			 }` |
|        551 | 1109 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     483668 | 1110 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1111 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         78 | 1112 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         78 | 1113 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1114 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1115 | `				 return rc;` |
|          - | 1116 | `			 }` |
|         78 | 1117 | `			 pNode->xCode = PH7_CompileMatch;` |
|     483358 | 1118 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1119 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1120 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1121 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1122 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1123 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1124 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1125 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1126 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     483303 | 1127 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1128 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         73 | 1129 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         73 | 1130 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         39 | 1131 | `		 }else{` |
|          - | 1132 | `			 /* Assume a literal */` |
|     483217 | 1133 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     483217 | 1134 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1135 | `		 }` |
|   42278062 | 1136 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1137 | `		 /* Constants,function name,namespace path,class name... */` |
|   13527355 | 1138 | `		 if( bAfterMemberOp ){` |
|          - | 1139 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1140 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1141 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1142 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    5636801 | 1143 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    2818398 | 1144 | `		 }` |
|   13527355 | 1145 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   13527355 | 1146 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    6763680 | 1147 | `	 }else{` |
|   28221251 | 1148 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1149 | `			 /* Point to the code generator routine */` |
|    9536863 | 1150 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    9536863 | 1151 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1152 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1153 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1154 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1155 | `				 }` |
|          3 | 1156 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1157 | `				 return rc;` |
|          - | 1158 | `			 }` |
|    4768428 | 1159 | `		 }` |
|          - | 1160 | `		/* Advance the stream cursor */` |
|   28221249 | 1161 | `		pCur++;` |
|          - | 1162 | `	 }` |
|          - | 1163 | `	/* Point to the end of the token stream */` |
|   90336807 | 1164 | `	pNode->pEnd = pCur;` |
|          - | 1165 | `	/* Save the node for later processing */` |
|   90336807 | 1166 | `	*ppNode = pNode;` |
|          - | 1167 | `	/* Synchronize cursors */` |
|   90336807 | 1168 | `	pGen->pIn = pCur;` |
|   90336807 | 1169 | `	return SXRET_OK;` |
|   45170542 | 1170 | `}` |
|          - | 1171 | `/*` |
|          - | 1172 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1173 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1174 | ` * level is zero.` |
|          - | 1175 | ` */` |
|    1942838 | 1176 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1177 | `{` |
|    1942843 | 1178 | `	SyToken *pCur = pStart;` |
|    1942843 | 1179 | `	sxi32 iNest = 0;` |
|    1942843 | 1180 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1181 | `		/* Last expression */` |
|     717473 | 1182 | `		return SXERR_EOF;` |
|          - | 1183 | `	}` |
|    4650213 | 1184 | `	while( pCur < pEnd ){` |
|    4347807 | 1185 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     922969 | 1186 | `			break;` |
|          - | 1187 | `		}` |
|    3424843 | 1188 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     242069 | 1189 | `			iNest++;` |
|    3303811 | 1190 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     242073 | 1191 | `			iNest--;` |
|     121034 | 1192 | `		}` |
|    3424843 | 1193 | `		pCur++;` |
|          5 | 1194 | `	}` |
|    1225375 | 1195 | `	*ppNext = pCur;` |
|    1225375 | 1196 | `	return SXRET_OK;` |
|     971424 | 1197 | `}` |
|          - | 1198 | `/*` |
|          - | 1199 | ` * Free an expression tree.` |
|          - | 1200 | ` */` |
|   77210564 | 1201 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1202 | `{` |
|   77210569 | 1203 | `	if( pNode->pLeft ){` |
|          - | 1204 | `		/* Release the left tree */` |
|   30178995 | 1205 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   15089495 | 1206 | `	}` |
|   77210569 | 1207 | `	if( pNode->pRight ){` |
|          - | 1208 | `		/* Release the right tree */` |
|   17554217 | 1209 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    8777106 | 1210 | `	}` |
|   77210569 | 1211 | `	if( pNode->pCond ){` |
|          - | 1212 | `		/* Release the conditional tree used by the ternary operator */` |
|     505081 | 1213 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     252538 | 1214 | `	}` |
|   77210569 | 1215 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1216 | `		ph7_expr_node **apArg;` |
|          - | 1217 | `		sxu32 n;` |
|          - | 1218 | `		/* Release node arguments */` |
|    8320835 | 1219 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   19152667 | 1220 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   10831837 | 1221 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    5415921 | 1222 | `		}` |
|    8320835 | 1223 | `		SySetRelease(&pNode->aNodeArgs);` |
|    4160415 | 1224 | `	}` |
|          - | 1225 | `	/* Finally,release this node */` |
|   77210569 | 1226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   77210569 | 1227 | `}` |
|          - | 1228 | `/*` |
|          - | 1229 | ` * Free an expression tree.` |
|          - | 1230 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1231 | ` */` |
|   17147138 | 1232 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1233 | `{` |
|          - | 1234 | `	ph7_expr_node **apNode;` |
|          - | 1235 | `	sxu32 n;` |
|   17147143 | 1236 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  107483997 | 1237 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   90336859 | 1238 | `		if( apNode[n] ){` |
|   17147479 | 1239 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    8573737 | 1240 | `		}` |
|   45168432 | 1241 | `	}` |
|   17147143 | 1242 | `	return SXRET_OK;` |
|          5 | 1243 | `}` |
|          - | 1244 | `/*` |
|          - | 1245 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1246 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1247 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1248 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1249 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1250 | ` */` |
|   21356768 | 1251 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1252 | `{` |
|   21356773 | 1253 | `	if( pNode == 0 ){` |
|   13304557 | 1254 | `		return 0;` |
|          - | 1255 | `	}` |
|    8052221 | 1256 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1257 | `		return 1;` |
|          - | 1258 | `	}` |
|    8052209 | 1259 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1260 | `		return 1;` |
|          - | 1261 | `	}` |
|    8052205 | 1262 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1263 | `		return 1;` |
|          - | 1264 | `	}` |
|    8052205 | 1265 | `	return 0;` |
|   10678389 | 1266 | `}` |
|          - | 1267 | `/*` |
|          - | 1268 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1269 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1270 | ` */` |
|    5228948 | 1271 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1272 | `{` |
|          - | 1273 | `	sxi32 iExprOp;` |
|    5228953 | 1274 | `	if( pNode->pOp == 0 ){` |
|    3729557 | 1275 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1276 | `	}` |
|    1499401 | 1277 | `	iExprOp = pNode->pOp->iOp;` |
|    1499401 | 1278 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     961677 | 1279 | `			return TRUE;` |
|          - | 1280 | `	}` |
|     537729 | 1281 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     537715 | 1282 | `		if( pNode->pLeft->pOp ) {` |
|     128454 | 1283 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      54496 | 1284 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1285 | `				return FALSE;` |
|          5 | 1286 | `			}` |
|     473488 | 1287 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1288 | `			return FALSE;` |
|          - | 1289 | `		}` |
|     537715 | 1290 | `		return TRUE;` |
|          - | 1291 | `	}` |
|         16 | 1292 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1293 | `		return TRUE;` |
|          - | 1294 | `	}` |
|          - | 1295 | `	/* Not a modifiable l or r-value */` |
|          9 | 1296 | `	return FALSE;` |
|    2614479 | 1297 | `}` |
|          - | 1298 | `/* Forward declaration */` |
|          - | 1299 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|          - | 1300 | `/* Macro to check if the given node is a terminal.` |
|          - | 1301 | ` * A node is a term if it has no operator, or has already been linked into an` |
|          - | 1302 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|          - | 1303 | ` * linked ternary/elvis node). */` |
|          - | 1304 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|          - | 1305 | `/*` |
|          - | 1306 | ` * Buid an expression tree for each given function argument.` |
|          - | 1307 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1308 | ` */` |
|    5524702 | 1309 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1310 | `{` |
|          - | 1311 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1312 | `	sxi32 rc;` |
|          - | 1313 | `	/* Process function arguments from left to right */` |
|    5524707 | 1314 | `	iCur = 0;` |
|    6780190 | 1315 | `	for(;;){` |
|   13560385 | 1316 | `		if( iCur >= nToken ){` |
|          - | 1317 | `			/* No more arguments to process */` |
|    5524679 | 1318 | `			break;` |
|          - | 1319 | `		}` |
|    8035711 | 1320 | `		iNode = iCur;` |
|    8035711 | 1321 | `		iNest = 0;` |
|   26325731 | 1322 | `		while( iCur < nToken ){` |
|   20801055 | 1323 | `			if( apNode[iCur] ){` |
|   20754069 | 1324 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1255520 | 1325 | `					break;` |
|   18243034 | 1326 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    9767820 | 1327 | `					&& apNode[iCur]->pLeft == 0` |
|    1292599 | 1328 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1290012 | 1329 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1330 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1331 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1332 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1333 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1334 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1335 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|          - | 1336 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|          - | 1337 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|          - | 1338 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|          - | 1339 | `					 * and swallow the following argument separator. */` |
|    1287427 | 1340 | `					iNest++;` |
|   17599328 | 1341 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    9121522 | 1342 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    1287427 | 1343 | `					iNest--;` |
|     643711 | 1344 | `				}` |
|    9121517 | 1345 | `			}` |
|   18290025 | 1346 | `			iCur++;` |
|          5 | 1347 | `		}` |
|    8035711 | 1348 | `		if( iCur > iNode ){` |
|    8035705 | 1349 | `			SyString sArgName = {0, 0};` |
|          - | 1350 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1351 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1352 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    8035700 | 1353 | `			if( (iCur - iNode) >= 2` |
|    5590164 | 1354 | `				&& apNode[iNode]` |
|    3144605 | 1355 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1696367 | 1356 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     247877 | 1357 | `				&& apNode[iNode+1]` |
|     247607 | 1358 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1359 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        303 | 1360 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        303 | 1361 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        303 | 1362 | `				apNode[iNode] = 0;` |
|        303 | 1363 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        303 | 1364 | `				apNode[iNode+1] = 0;` |
|        303 | 1365 | `				iNode += 2;` |
|          - | 1366 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1367 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        303 | 1368 | `				if( iNode >= iCur ){` |
|          4 | 1369 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1370 | `						pOp->pStart->nLine,` |
|          - | 1371 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1372 | `						&sArgName);` |
|          3 | 1373 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1374 | `						rc = SXERR_SYNTAX;` |
|          1 | 1375 | `					}` |
|          3 | 1376 | `					return rc;` |
|          - | 1377 | `				}` |
|        148 | 1378 | `			}` |
|    8035698 | 1379 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1380 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1381 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1382 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1383 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1384 | `					apNode[iNode] = 0;` |
|        ! 0 | 1385 | `			}` |
|          - | 1386 | `			{` |
|          - | 1387 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1388 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1389 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1390 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1391 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1392 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1393 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1394 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1395 | `				 * NULL slots ahead of the flagged subtree. */` |
|    8035703 | 1396 | `				int bSpreadArg = 0;` |
|          - | 1397 | `				sxi32 iScan;` |
|    8035749 | 1398 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    8035749 | 1399 | `					if( apNode[iScan] ){` |
|    8035703 | 1400 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    8035703 | 1401 | `						break;` |
|          - | 1402 | `					}` |
|         26 | 1403 | `				}` |
|    8035703 | 1404 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    8035703 | 1405 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4093 | 1406 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2044 | 1407 | `				}` |
|          - | 1408 | `			}` |
|    8035703 | 1409 | `			if( apNode[iNode] ){` |
|    8035703 | 1410 | `				if( sArgName.nByte > 0 ){` |
|        300 | 1411 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        300 | 1412 | `					apNode[iNode]->sArgName = sArgName;` |
|        148 | 1413 | `				}` |
|          - | 1414 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    8035703 | 1415 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    4017854 | 1416 | `			}else{` |
|          - | 1417 | `				/* No expression before comma */` |
|        ! 0 | 1418 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1419 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1420 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1421 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1422 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1423 | `				}` |
|        ! 0 | 1424 | `				return rc;` |
|          - | 1425 | `			}` |
|    4017854 | 1426 | `		}else{` |
|          - | 1427 | `			/* Comma with no preceding argument */` |
|          8 | 1428 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1429 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1430 | `				rc = SXERR_SYNTAX;` |
|          3 | 1431 | `			}` |
|          8 | 1432 | `			return rc;` |
|          - | 1433 | `		}` |
|          - | 1434 | `		/* Jump trailing comma */` |
|    8035703 | 1435 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2511029 | 1436 | `			iCur++;` |
|    2511029 | 1437 | `			if( iCur >= nToken ){` |
|          - | 1438 | `				/* Trailing comma after last argument */` |
|         21 | 1439 | `				break;` |
|          - | 1440 | `			}` |
|    1255502 | 1441 | `		}` |
|          5 | 1442 | `	}` |
|    5524699 | 1443 | `	return SXRET_OK;` |
|    2762356 | 1444 | `}` |
|          - | 1445 | ` /*` |
|          - | 1446 | `  * Create an expression tree from an array of tokens.` |
|          - | 1447 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1448 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1449 | `  */` |
|   29637208 | 1450 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1451 | ` {` |
|          - | 1452 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1453 | `	 ph7_expr_node *pNode;` |
|          - | 1454 | `	 ph7_expr_node *pSuppress;` |
|          - | 1455 | `	 sxi32 iCur;` |
|          - | 1456 | `	 sxi32 rc;` |
|   29637213 | 1457 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1458 | `		 /* TICKET 1433-17: self evaluating node */` |
|   13023955 | 1459 | `		 return SXRET_OK;` |
|          - | 1460 | `	 }` |
|          - | 1461 | `	 /* Process expressions enclosed in parenthesis first */` |
|  118226401 | 1462 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1463 | `		 sxi32 iNest;` |
|          - | 1464 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1465 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1466 | `		  */` |
|  101613145 | 1467 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  101120849 | 1468 | `			 continue;` |
|          - | 1469 | `		 }` |
|     492301 | 1470 | `		 iNest = 1;` |
|     492301 | 1471 | `		 iLeft = iCur;` |
|          - | 1472 | `		 /* Find the closing parenthesis */` |
|     492301 | 1473 | `		 iCur++;` |
|    4388001 | 1474 | `		 while( iCur < nToken ){` |
|    4388001 | 1475 | `			 if( apNode[iCur] ){` |
|    4388001 | 1476 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1477 | `					 /* Decrement nesting level */` |
|     719195 | 1478 | `					 iNest--;` |
|     719195 | 1479 | `					 if( iNest <= 0 ){` |
|     492301 | 1480 | `						 break;` |
|          5 | 1481 | `					 }` |
|    3782258 | 1482 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1483 | `					 /* Increment nesting level */` |
|     226899 | 1484 | `					 iNest++;` |
|     113447 | 1485 | `				 }` |
|    1947850 | 1486 | `			 }` |
|    3895705 | 1487 | `			 iCur++;` |
|          5 | 1488 | `		 }` |
|     492301 | 1489 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1490 | `			 sxi32 j;` |
|          - | 1491 | `			 /* Recurse and process this expression */` |
|     492301 | 1492 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     492301 | 1493 | `			 if( rc != SXRET_OK ){` |
|          3 | 1494 | `				 return rc;` |
|          - | 1495 | `			 }` |
|          - | 1496 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1497 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1498 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1499 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1500 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1501 | `			  * group's free below silently drops the unpacking. */` |
|     492299 | 1502 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     492299 | 1503 | `				 if( apNode[j] ){` |
|     492299 | 1504 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     492294 | 1505 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     492299 | 1506 | `					 break;` |
|          - | 1507 | `				 }` |
|        ! 0 | 1508 | `			 }` |
|     246147 | 1509 | `		 }` |
|          - | 1510 | `		 /* Free the left and right nodes */` |
|     492299 | 1511 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     492299 | 1512 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     492299 | 1513 | `		 apNode[iLeft] = 0;` |
|     492299 | 1514 | `		 apNode[iCur] = 0;` |
|     246152 | 1515 | `	 }` |
|          - | 1516 | `	  /* Process expressions enclosed in braces */` |
|  122294727 | 1517 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1518 | `		 sxi32 iNest;` |
|          - | 1519 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1520 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1521 | `		  */` |
|  105993335 | 1522 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  105989437 | 1523 | `			 continue;` |
|          - | 1524 | `		 }` |
|       3903 | 1525 | `		 iNest = 1;` |
|       3903 | 1526 | `		 iLeft = iCur;` |
|          - | 1527 | `		 /* Find the closing parenthesis */` |
|       3903 | 1528 | `		 iCur++;` |
|       7799 | 1529 | `		 while( iCur < nToken ){` |
|       7799 | 1530 | `			 if( apNode[iCur] ){` |
|       7799 | 1531 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1532 | `					 /* Decrement nesting level */` |
|       3903 | 1533 | `					 iNest--;` |
|       3903 | 1534 | `					 if( iNest <= 0 ){` |
|       3903 | 1535 | `						 break;` |
|        ! 0 | 1536 | `					 }` |
|       3901 | 1537 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1538 | `					 /* Increment nesting level */` |
|        ! 0 | 1539 | `					 iNest++;` |
|        ! 0 | 1540 | `				 }` |
|       1948 | 1541 | `			 }` |
|       3901 | 1542 | `			 iCur++;` |
|          5 | 1543 | `		 }` |
|       3903 | 1544 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1545 | `			 /* Recurse and process this expression */` |
|       3901 | 1546 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3901 | 1547 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1548 | `				 return rc;` |
|          - | 1549 | `			 }` |
|       1948 | 1550 | `		 }` |
|          - | 1551 | `		 /* Free the left and right nodes */` |
|       3903 | 1552 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3903 | 1553 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3903 | 1554 | `		 apNode[iLeft] = 0;` |
|       3903 | 1555 | `		 apNode[iCur] = 0;` |
|       1954 | 1556 | `	 }` |
|          - | 1557 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   16301397 | 1558 | `	 iLeft = -1;` |
|  122302485 | 1559 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  106001105 | 1560 | `		 if( apNode[iCur] == 0 ){` |
|   46806965 | 1561 | `			 continue;` |
|          - | 1562 | `		 }` |
|   59194145 | 1563 | `		 pNode = apNode[iCur];` |
|   59194145 | 1564 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   16116941 | 1565 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1566 | `				 /* Collect function arguments */` |
|    7076345 | 1567 | `				 sxi32 iPtr = 0;` |
|    7076345 | 1568 | `				 sxi32 nFuncTok = 0;` |
|   34953737 | 1569 | `				 while( nFuncTok + iCur < nToken ){` |
|   34953737 | 1570 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|          - | 1571 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|          - | 1572 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|          - | 1573 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|          - | 1574 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|          - | 1575 | `					  * nulled, so counting it here would over-count and never find` |
|          - | 1576 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   34953737 | 1577 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   34895031 | 1578 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    7538453 | 1579 | `							 iPtr++;` |
|   31125807 | 1580 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    7538453 | 1581 | `							 iPtr--;` |
|    7538453 | 1582 | `							 if( iPtr <= 0 ){` |
|    7076345 | 1583 | `								 break;` |
|          - | 1584 | `							 }` |
|     231054 | 1585 | `						 }` |
|   13909343 | 1586 | `					 }` |
|   27877397 | 1587 | `					 nFuncTok++;` |
|          5 | 1588 | `				 }` |
|    7076345 | 1589 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1590 | `					 /* Syntax error */` |
|        ! 0 | 1591 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1592 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1593 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1594 | `					 }` |
|        ! 0 | 1595 | `					 return rc;` |
|          - | 1596 | `				 }` |
|    7076345 | 1597 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1598 | `					 /* Syntax error */` |
|        ! 0 | 1599 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1600 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1601 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1602 | `					 }` |
|        ! 0 | 1603 | `					 return rc;` |
|          - | 1604 | `				 }` |
|    7076345 | 1605 | `				 if( nFuncTok > 1 ){` |
|          - | 1606 | `					 /* Process function arguments */` |
|    5524707 | 1607 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    5524707 | 1608 | `					 if( rc != SXRET_OK ){` |
|         11 | 1609 | `						 return rc;` |
|          - | 1610 | `					 }` |
|    2762347 | 1611 | `				 }` |
|          - | 1612 | `				 /* Link the node to the tree */` |
|    7076337 | 1613 | `				 pNode->pLeft = apNode[iLeft];` |
|    7076337 | 1614 | `				 apNode[iLeft] = 0;` |
|   34953705 | 1615 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   27877373 | 1616 | `					 apNode[iCur+iPtr] = 0;` |
|   13938689 | 1617 | `				 }` |
|          - | 1618 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1619 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1620 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1621 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1622 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1623 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1624 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1625 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1626 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1627 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1628 | `				 {` |
|    7076337 | 1629 | `					 sxi32 iNew = iLeft - 1;` |
|    9146001 | 1630 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    2069669 | 1631 | `						 iNew--;` |
|          5 | 1632 | `					 }` |
|    7076332 | 1633 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3989986 | 1634 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2417483 | 1635 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     858607 | 1636 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     858607 | 1637 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     858607 | 1638 | `						 apNode[iNew] = 0;` |
|     858607 | 1639 | `						 pNode = apNode[iCur];` |
|     429306 | 1640 | `					 }` |
|          - | 1641 | `				 }` |
|   12578767 | 1642 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1643 | `				 /* Subscripting */` |
|    3033875 | 1644 | `				 sxi32 iArrTok = iCur + 1;` |
|    3033875 | 1645 | `				 sxi32 iNest = 1;` |
|    3033870 | 1646 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         34 | 1647 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         28 | 1648 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|          - | 1649 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|          - | 1650 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|          - | 1651 | `					  * subscripting a global constant raised "Invalid array name" while` |
|          - | 1652 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|          - | 1653 | `					  * worked. */` |
|         22 | 1654 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|         16 | 1655 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    3033870 | 1656 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1657 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1658 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     323154 | 1659 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1660 | `						 /* Syntax error */` |
|        ! 0 | 1661 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1662 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1663 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1664 | `						 }` |
|        ! 0 | 1665 | `						 return rc;` |
|          - | 1666 | `				 }` |
|          - | 1667 | `				 /* Collect index tokens */` |
|    6374759 | 1668 | `				 while( iArrTok < nToken ){` |
|    6374759 | 1669 | `					 if( apNode[iArrTok] ){` |
|    6374727 | 1670 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1671 | `							 /* Increment nesting level */` |
|      27235 | 1672 | `							 iNest++;` |
|    6361112 | 1673 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1674 | `							 /* Decrement nesting level */` |
|    3061105 | 1675 | `							 iNest--;` |
|    3061105 | 1676 | `							 if( iNest <= 0 ){` |
|    3033875 | 1677 | `								 break;` |
|          - | 1678 | `							 }` |
|      13615 | 1679 | `						 }` |
|    1670426 | 1680 | `					 }` |
|    3340889 | 1681 | `					 ++iArrTok;` |
|          5 | 1682 | `				 }` |
|    3033875 | 1683 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1684 | `					 /* Recurse and process this expression */` |
|    2796139 | 1685 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2796139 | 1686 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1687 | `						 return rc;` |
|          - | 1688 | `					 }` |
|          - | 1689 | `					 /* Link the node to it's index */` |
|    2796139 | 1690 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1398067 | 1691 | `				 }` |
|          - | 1692 | `				 /* Link the node to the tree */` |
|    3033875 | 1693 | `				 pNode->pLeft = apNode[iLeft];` |
|    3033875 | 1694 | `				 pNode->pRight = 0;` |
|    3033875 | 1695 | `				 apNode[iLeft] = 0;` |
|    9408629 | 1696 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6374759 | 1697 | `					 apNode[iNest] = 0;` |
|    3187382 | 1698 | `				 }` |
|    1516940 | 1699 | `			 }else{` |
|          - | 1700 | `				 /* Member access operators [i.e: '->','::'] */` |
|    6006731 | 1701 | `				  iRight = iCur + 1;` |
|    6010627 | 1702 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3901 | 1703 | `					 iRight++;` |
|          5 | 1704 | `				 }` |
|    6006731 | 1705 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1706 | `					 /* Syntax error */` |
|          5 | 1707 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1708 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1709 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1710 | `					 }` |
|          5 | 1711 | `					 return rc;` |
|          - | 1712 | `				 }` |
|          - | 1713 | `				 /* Link the node to the tree */` |
|    6006727 | 1714 | `				 pNode->pLeft = apNode[iLeft];` |
|    6006722 | 1715 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    5796125 | 1716 | `					 && pNode->pLeft->pOp == 0 &&` |
|    5495283 | 1717 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1718 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1719 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1720 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1721 | `					  * accepts. */` |
|          4 | 1722 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1723 | `						 /* Syntax error */` |
|        ! 0 | 1724 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1725 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1726 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1727 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1728 | `						 }` |
|        ! 0 | 1729 | `						 return rc;` |
|          - | 1730 | `				 }` |
|    6006727 | 1731 | `				 pNode->pRight = apNode[iRight];` |
|    6006727 | 1732 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1733 | `			 }` |
|    8058462 | 1734 | `		 }` |
|   59194133 | 1735 | `		 iLeft = iCur;` |
|   29597069 | 1736 | `	 }` |
|          - | 1737 | `	 /* Handle left associative (new, clone) operators */` |
|  122302453 | 1738 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  106001073 | 1739 | `		 if( apNode[iCur] == 0 ){` |
|   63845201 | 1740 | `			 continue;` |
|          - | 1741 | `		 }` |
|   42155877 | 1742 | `		 pNode = apNode[iCur];` |
|   42155877 | 1743 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1744 | `			 SyToken *pToken;` |
|          - | 1745 | `			 /* Get the left node */` |
|      62715 | 1746 | `			 iLeft = iCur + 1;` |
|      62723 | 1747 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1748 | `				 iLeft++;` |
|          1 | 1749 | `			 }` |
|      62715 | 1750 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1751 | `				  /* Syntax error */` |
|        ! 0 | 1752 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1753 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1754 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1755 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1756 | `				 }` |
|        ! 0 | 1757 | `				 return rc;` |
|          - | 1758 | `			 }` |
|          - | 1759 | `			 /* Make sure the operand are of a valid type */` |
|      62715 | 1760 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1761 | `				 /* Clone:` |
|          - | 1762 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1763 | `				  *  ++ function call (including annonymous)` |
|          - | 1764 | `				  *  ++ array member` |
|          - | 1765 | `				  *  ++ 'new' operator` |
|          - | 1766 | `				  * Example:` |
|          - | 1767 | `				  *   clone $pObj;` |
|          - | 1768 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1769 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1770 | `				  */` |
|      58401 | 1771 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      58395 | 1772 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1773 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1774 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1775 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1776 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1777 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1778 | `						 }` |
|        ! 0 | 1779 | `						 return rc;` |
|          - | 1780 | `					 }` |
|      29195 | 1781 | `				 }` |
|      29203 | 1782 | `			 }else{` |
|          - | 1783 | `				 /* New */` |
|       4314 | 1784 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1785 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1786 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1787 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1788 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1789 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1790 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1791 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1792 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1793 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1794 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1795 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1796 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1797 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1798 | `					 }` |
|        ! 0 | 1799 | `					 return rc;` |
|          - | 1800 | `				 }` |
|       4319 | 1801 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       4319 | 1802 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       4314 | 1803 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         35 | 1804 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1805 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1806 | `						 /* Syntax error */` |
|        ! 0 | 1807 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1808 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1809 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1810 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1811 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1812 | `						 }` |
|        ! 0 | 1813 | `						 return rc;` |
|          - | 1814 | `					 }` |
|       2157 | 1815 | `				 }` |
|          - | 1816 | `			 }` |
|          - | 1817 | `			  /* Link the node to the tree */` |
|      62715 | 1818 | `			 pNode->pLeft = apNode[iLeft];` |
|      62715 | 1819 | `			 apNode[iLeft] = 0;` |
|      62715 | 1820 | `			 pNode->pRight = 0; /* Paranoid */` |
|      31355 | 1821 | `		 }` |
|   21077941 | 1822 | `	 }` |
|          - | 1823 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   16301385 | 1824 | `	 iLeft = -1;` |
|  122302453 | 1825 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105845141 | 1826 | `		 if( apNode[iCur] == 0 ){` |
|   63845201 | 1827 | `			 continue;` |
|          - | 1828 | `		 }` |
|   41999945 | 1829 | `		 pNode = apNode[iCur];` |
|   41999945 | 1830 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     374227 | 1831 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     187117 | 1832 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1833 | `					 /* Link the node to the tree */` |
|     202707 | 1834 | `					 pNode->pLeft = apNode[iLeft];` |
|     202707 | 1835 | `					 apNode[iLeft] = 0;` |
|     101351 | 1836 | `			 }` |
|     436591 | 1837 | `		  }` |
|   42155877 | 1838 | `		 iLeft = iCur;` |
|   21077941 | 1839 | `	  }` |
|   16457317 | 1840 | `	 iLeft = -1;` |
|  122458385 | 1841 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  106001073 | 1842 | `		 if( apNode[iCur] == 0 ){` |
|   64047903 | 1843 | `			 continue;` |
|          - | 1844 | `		 }` |
|   41953175 | 1845 | `		 pNode = apNode[iCur];` |
|   41953175 | 1846 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15588 | 1847 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15593 | 1848 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1849 | `					 /* Syntax error */` |
|        ! 0 | 1850 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1851 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1852 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1853 | `					 }` |
|        ! 0 | 1854 | `					 return rc;` |
|          - | 1855 | `			 }` |
|          - | 1856 | `			 /* Link the node to the tree */` |
|      15593 | 1857 | `			 pNode->pLeft = apNode[iLeft];` |
|      15593 | 1858 | `			 apNode[iLeft] = 0;` |
|          - | 1859 | `			 /* Mark as pre-increment/decrement node */` |
|      15593 | 1860 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7794 | 1861 | `		  }` |
|   41953175 | 1862 | `		 iLeft = iCur;` |
|   20976590 | 1863 | `	 }` |
|          - | 1864 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1865 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1866 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1867 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1868 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1869 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1870 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1871 | `	  * pass below skips it (pLeft != 0). */` |
|   16457317 | 1872 | `	 iLeft = -1;` |
|  122458385 | 1873 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  106001073 | 1874 | `		 if( apNode[iCur] == 0 ){` |
|   64145489 | 1875 | `			 continue;` |
|          - | 1876 | `		 }` |
|   41855589 | 1877 | `		 pNode = apNode[iCur];` |
|   41855589 | 1878 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      82003 | 1879 | `			 iRight = iCur + 1;` |
|      82003 | 1880 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1881 | `				 iRight++;` |
|        ! 0 | 1882 | `			 }` |
|      82003 | 1883 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1884 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1885 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1886 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1887 | `				 }` |
|        ! 0 | 1888 | `				 return rc;` |
|          - | 1889 | `			 }` |
|      82003 | 1890 | `			 pNode->pLeft = apNode[iLeft];` |
|      82003 | 1891 | `			 pNode->pRight = apNode[iRight];` |
|      82003 | 1892 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      40999 | 1893 | `		 }` |
|   41855589 | 1894 | `		 iLeft = iCur;` |
|   20927797 | 1895 | `	 }` |
|          - | 1896 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   16457317 | 1897 | `	  iLeft = 0;` |
|  122458379 | 1898 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  106001069 | 1899 | `		  if( apNode[iCur] ){` |
|   41773587 | 1900 | `			  pNode = apNode[iCur];` |
|   41773587 | 1901 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1378943 | 1902 | `				  if( iLeft > 0 ){` |
|          - | 1903 | `					  /* Link the node to the tree */` |
|    1378941 | 1904 | `					  pNode->pLeft = apNode[iLeft];` |
|    1378941 | 1905 | `					  apNode[iLeft] = 0;` |
|    1378941 | 1906 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      54575 | 1907 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1908 | `							   /* Syntax error */` |
|        ! 0 | 1909 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1910 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 1911 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 1912 | `							  }` |
|        ! 0 | 1913 | `							  return rc;` |
|          - | 1914 | `						  }` |
|      27285 | 1915 | `					  }` |
|     689473 | 1916 | `				  }else{` |
|          - | 1917 | `					  /* Syntax error */` |
|          3 | 1918 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 1919 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 1920 | `						  rc = SXERR_SYNTAX;` |
|          1 | 1921 | `					  }` |
|          3 | 1922 | `					  return rc;` |
|          - | 1923 | `				  }` |
|     689468 | 1924 | `			  }` |
|          - | 1925 | `			  /* Save terminal position */` |
|   41773585 | 1926 | `			  iLeft = iCur;` |
|   20886790 | 1927 | `		  }` |
|   53000536 | 1928 | `	  }` |
|          - | 1929 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 1930 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 1931 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 1932 | `	  * yielding a right-leaning tree. */` |
|  122458377 | 1933 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  106001067 | 1934 | `		 if( apNode[iCur] == 0 ){` |
|   65606537 | 1935 | `			 continue;` |
|          - | 1936 | `		 }` |
|   40394535 | 1937 | `		 pNode = apNode[iCur];` |
|   40394535 | 1938 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 1939 | `			 sxi32 iL, iR;` |
|          - | 1940 | `			 /* Find the right operand */` |
|        115 | 1941 | `			 iR = -1;` |
|          - | 1942 | `			 {` |
|          - | 1943 | `				 sxi32 j;` |
|        127 | 1944 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 1945 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 1946 | `				 }` |
|          - | 1947 | `			 }` |
|          - | 1948 | `			 /* Find the left operand */` |
|        115 | 1949 | `			 iL = -1;` |
|          - | 1950 | `			 {` |
|          - | 1951 | `				 sxi32 j;` |
|        183 | 1952 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 1953 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 1954 | `				 }` |
|          - | 1955 | `			 }` |
|        115 | 1956 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 1957 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1958 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1959 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1960 | `				 }` |
|        ! 0 | 1961 | `				 return rc;` |
|          - | 1962 | `			 }` |
|        115 | 1963 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 1964 | `			 pNode->pRight = apNode[iR];` |
|        115 | 1965 | `			 apNode[iL] = 0;` |
|        115 | 1966 | `			 apNode[iR] = 0;` |
|          - | 1967 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 1968 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 1969 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 1970 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 1971 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 1972 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 1973 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 1974 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 1975 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 1976 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 1977 | `			  * operands are respected. */` |
|        114 | 1978 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 1979 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 1980 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 1981 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 1982 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 1983 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 1984 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 1985 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 1986 | `				 while( pTail->pLeft` |
|         34 | 1987 | `					 && pTail->pLeft->pOp` |
|         23 | 1988 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 1989 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 1990 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 1991 | `					 pTail = pTail->pLeft;` |
|          1 | 1992 | `				 }` |
|          - | 1993 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 1994 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 1995 | `				 pTail->pLeft = pNode;` |
|         27 | 1996 | `				 apNode[iCur] = pHead;` |
|         13 | 1997 | `			 }` |
|         57 | 1998 | `		 }` |
|   20197270 | 1999 | `	 }` |
|          - | 2000 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  181030329 | 2001 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  164573029 | 2002 | `		 iLeft = -1;` |
| 1224583355 | 2003 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 1060010341 | 2004 | `			 if( apNode[iCur] == 0 ){` |
|  725005955 | 2005 | `				 continue;` |
|          - | 2006 | `			 }` |
|  335004391 | 2007 | `			 pNode = apNode[iCur];` |
|  335004391 | 2008 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2009 | `				 /* Get the right node */` |
|    5731469 | 2010 | `				 iRight = iCur + 1;` |
|    8684005 | 2011 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2952541 | 2012 | `					 iRight++;` |
|          5 | 2013 | `				 }` |
|    5731469 | 2014 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2015 | `					 /* Syntax error */` |
|         10 | 2016 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 2017 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2018 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2019 | `					 }` |
|         10 | 2020 | `					 return rc;` |
|          - | 2021 | `				 }` |
|    5731461 | 2022 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2023 | `					 sxi32  iTmp;` |
|          - | 2024 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2025 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2026 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2027 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2028 | `					  * is swapped below. */` |
|         75 | 2029 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2030 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2031 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2032 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2033 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2034 | `						 }` |
|          3 | 2035 | `						 return rc;` |
|          - | 2036 | `					 }` |
|          - | 2037 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|          - | 2038 | `					  * reference target — ExprIsModifiableValue already accepts` |
|          - | 2039 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|          - | 2040 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|          - | 2041 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|          - | 2042 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|         73 | 2043 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2044 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2045 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2046 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2047 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2048 | `						 }` |
|        ! 0 | 2049 | `						 return rc;` |
|          - | 2050 | `					 }` |
|         73 | 2051 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         55 | 2052 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2053 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2054 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2055 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2056 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2057 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2058 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2059 | `									 }` |
|        ! 0 | 2060 | `									 return rc;` |
|          - | 2061 | `							 }` |
|        ! 0 | 2062 | `						 }` |
|         26 | 2063 | `					 }` |
|          - | 2064 | `					 /* Swap operands */` |
|         73 | 2065 | `					 iTmp = iRight;` |
|         73 | 2066 | `					 iRight = iLeft;` |
|         73 | 2067 | `					 iLeft = iTmp;` |
|         35 | 2068 | `				 }` |
|          - | 2069 | `				 /* Link the node to the tree */` |
|    5731459 | 2070 | `				 pNode->pLeft = apNode[iLeft];` |
|    5731459 | 2071 | `				 pNode->pRight = apNode[iRight];` |
|    5731459 | 2072 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2865727 | 2073 | `			 }` |
|  335004381 | 2074 | `			 iLeft = iCur;` |
|  167502193 | 2075 | `		 }` |
|   82286512 | 2076 | `	 }` |
|          - | 2077 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2078 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2079 | `	  * we are dealing with a single operator.` |
|          - | 2080 | `	  */` |
|   16457305 | 2081 | `	  iLeft = -1;` |
|  118289629 | 2082 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  102337407 | 2083 | `		  if( apNode[iCur] == 0 ){` |
|   74917225 | 2084 | `			  continue;` |
|          - | 2085 | `		  }` |
|   27420187 | 2086 | `		  pNode = apNode[iCur];` |
|   27420187 | 2087 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     505083 | 2088 | `			  sxi32 iNest = 1;` |
|     505083 | 2089 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2090 | `				  /* Missing condition */` |
|          3 | 2091 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2092 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2093 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2094 | `				  }` |
|          3 | 2095 | `				  return rc;` |
|          - | 2096 | `			  }` |
|          - | 2097 | `			  /* Get the right node */` |
|     505081 | 2098 | `			  iRight = iCur + 1;` |
|    2146453 | 2099 | `			  while( iRight < nToken  ){` |
|    2146453 | 2100 | `				  if( apNode[iRight] ){` |
|    1006197 | 2101 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2102 | `						  /* Increment nesting level */` |
|        ! 0 | 2103 | `						  ++iNest;` |
|    1006197 | 2104 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2105 | `						  /* Decrement nesting level */` |
|     505081 | 2106 | `						  --iNest;` |
|     505081 | 2107 | `						  if( iNest <= 0 ){` |
|     505081 | 2108 | `							  break;` |
|          - | 2109 | `						  }` |
|        ! 0 | 2110 | `					  }` |
|     250558 | 2111 | `				  }` |
|    1641377 | 2112 | `				  iRight++;` |
|          5 | 2113 | `			  }` |
|     505081 | 2114 | `			  if( iRight > iCur + 1 ){` |
|          - | 2115 | `				  /* Recurse and process the then expression */` |
|     501121 | 2116 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     501121 | 2117 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2118 | `					  return rc;` |
|          - | 2119 | `				  }` |
|          - | 2120 | `				  /* Link the node to the tree */` |
|     501121 | 2121 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     250558 | 2122 | `			  }else{` |
|          - | 2123 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2124 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2125 | `			  }` |
|     505081 | 2126 | `			  apNode[iCur + 1] = 0;` |
|     505081 | 2127 | `			  if( iRight + 1 < nToken ){` |
|          - | 2128 | `				  /* Recurse and process the else expression */` |
|     505081 | 2129 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     505081 | 2130 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2131 | `					  return rc;` |
|          - | 2132 | `				  }` |
|          - | 2133 | `				  /* Link the node to the tree */` |
|     505081 | 2134 | `				  pNode->pRight = apNode[iRight + 1];` |
|     505081 | 2135 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     252543 | 2136 | `			  }else{` |
|        ! 0 | 2137 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2138 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2139 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2140 | `				 }` |
|        ! 0 | 2141 | `				 return rc;` |
|          - | 2142 | `			  }` |
|          - | 2143 | `			  /* Point to the condition */` |
|     505081 | 2144 | `			  pNode->pCond  = apNode[iLeft];` |
|     505081 | 2145 | `			  apNode[iLeft] = 0;` |
|     505081 | 2146 | `			  break;` |
|          - | 2147 | `		  }` |
|   26915109 | 2148 | `		  iLeft = iCur;` |
|   13457557 | 2149 | `	  }` |
|          - | 2150 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2151 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2152 | `	  * so there is no need for a precedence loop here.` |
|          - | 2153 | `	  */` |
|   16457303 | 2154 | `	 iRight = -1;` |
|  122458181 | 2155 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  106000937 | 2156 | `		 if( apNode[iCur] == 0 ){` |
|   84314703 | 2157 | `			 continue;` |
|          - | 2158 | `		 }` |
|   21686239 | 2159 | `		 pNode = apNode[iCur];` |
|   21686239 | 2160 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2161 | `			 /* Get the left node */` |
|    5228871 | 2162 | `			 iLeft = iCur - 1;` |
|    7179995 | 2163 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1951129 | 2164 | `				 iLeft--;` |
|          5 | 2165 | `			 }` |
|    5228871 | 2166 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2167 | `				 /* Syntax error */` |
|         46 | 2168 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2169 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2170 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2171 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2172 | `				 }else{` |
|         41 | 2173 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2174 | `				 }` |
|         46 | 2175 | `				 if( rc != SXERR_ABORT ){` |
|         44 | 2176 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2177 | `				 }` |
|         46 | 2178 | `				 return rc;` |
|          - | 2179 | `			 }` |
|          - | 2180 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2181 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2182 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2183 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2184 | `			  * a write. */` |
|    5228829 | 2185 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2186 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2187 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2188 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2189 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2190 | `				 }` |
|         11 | 2191 | `				 return rc;` |
|          - | 2192 | `			 }` |
|          - | 2193 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2194 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2195 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2196 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2197 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    5228821 | 2198 | `			 pSuppress = 0;` |
|    5228816 | 2199 | `			 if( apNode[iLeft]->pOp` |
|    3364085 | 2200 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     749677 | 2201 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2202 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2203 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2204 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2205 | `			 }` |
|    5228821 | 2206 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2207 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2208 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2209 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2210 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2211 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2212 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        103 | 2213 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2214 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2215 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2216 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2217 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2218 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2219 | `					 }` |
|          8 | 2220 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2221 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2222 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2223 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2224 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2225 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2226 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2227 | `						 iRight = iCur;` |
|          9 | 2228 | `						 continue;` |
|          - | 2229 | `					 }` |
|        ! 0 | 2230 | `				 }` |
|        123 | 2231 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         88 | 2232 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2233 | `					 /* Left operand must be a modifiable l-value */` |
|          6 | 2234 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2235 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2236 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2237 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2238 | `					 }else{` |
|          4 | 2239 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          2 | 2240 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2241 | `					 }` |
|          6 | 2242 | `					 if( rc != SXERR_ABORT ){` |
|          6 | 2243 | `						 rc = SXERR_SYNTAX;` |
|          2 | 2244 | `					 }` |
|          6 | 2245 | `					 return rc;` |
|          - | 2246 | `				 }` |
|         43 | 2247 | `			 }` |
|          - | 2248 | `			 /* Link the node to the tree (Reverse) */` |
|    5228809 | 2249 | `			 pNode->pLeft = apNode[iRight];` |
|    5228809 | 2250 | `			 pNode->pRight = apNode[iLeft];` |
|    5228809 | 2251 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    5228809 | 2252 | `			 if( pSuppress ){` |
|          - | 2253 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2254 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2255 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2256 | `			 }` |
|    2614402 | 2257 | `		 }` |
|   21686177 | 2258 | `		 iRight = iCur;` |
|   10843091 | 2259 | `	 }` |
|          - | 2260 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   82286225 | 2261 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   65828981 | 2262 | `		 iLeft = -1;` |
|  489832437 | 2263 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  424003461 | 2264 | `			 if( apNode[iCur] == 0 ){` |
|  358174245 | 2265 | `				 continue;` |
|          - | 2266 | `			 }` |
|   65829221 | 2267 | `			 pNode = apNode[iCur];` |
|   65829221 | 2268 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2269 | `				 /* Get the right node */` |
|         48 | 2270 | `				 iRight = iCur + 1;` |
|         60 | 2271 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2272 | `					 iRight++;` |
|          1 | 2273 | `				 }` |
|         48 | 2274 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2275 | `					 /* Syntax error */` |
|        ! 0 | 2276 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2277 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2278 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2279 | `					 }` |
|        ! 0 | 2280 | `					 return rc;` |
|          - | 2281 | `				 }` |
|          - | 2282 | `				 /* Link the node to the tree */` |
|         48 | 2283 | `				 pNode->pLeft = apNode[iLeft];` |
|         48 | 2284 | `				 pNode->pRight = apNode[iRight];` |
|         48 | 2285 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         22 | 2286 | `			 }` |
|   65829221 | 2287 | `			 iLeft = iCur;` |
|   32914613 | 2288 | `		 }` |
|   32914493 | 2289 | `	 }` |
|          - | 2290 | `	 /* Point to the root of the expression tree */` |
|  106000841 | 2291 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   89543615 | 2292 | `		 if( apNode[iCur] ){` |
|   15654447 | 2293 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         23 | 2294 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|         23 | 2295 | `				  if( rc != SXERR_ABORT ){` |
|         23 | 2296 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2297 | `				  }` |
|         23 | 2298 | `				  return rc;` |
|          - | 2299 | `			 }` |
|   15654429 | 2300 | `			 apNode[0] = apNode[iCur];` |
|   15654429 | 2301 | `			 apNode[iCur] = 0;` |
|    7827212 | 2302 | `		 }` |
|   44771801 | 2303 | `	 }` |
|   16457231 | 2304 | `	 return SXRET_OK;` |
|   14740643 | 2305 | ` }` |
|          - | 2306 | ` /*` |
|          - | 2307 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2308 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2309 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2310 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2311 | `  */` |
|   17147142 | 2312 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2313 | `{` |
|          - | 2314 | `	ph7_expr_node **apNode;` |
|          - | 2315 | `	ph7_expr_node *pNode;` |
|          - | 2316 | `	sxi32 rc;` |
|          - | 2317 | `	/* Reset node container */` |
|   17147147 | 2318 | `	SySetReset(pExprNode);` |
|   17147147 | 2319 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2320 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2321 | `	{` |
|   17147147 | 2322 | `		int iLastWasTerm = 0;` |
|   17147147 | 2323 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  107484031 | 2324 | `		while( pGen->pIn < pGen->pEnd ){` |
|   90336921 | 2325 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   90336921 | 2326 | `			if( rc != SXRET_OK ){` |
|         36 | 2327 | `				return rc;` |
|          - | 2328 | `			}` |
|          - | 2329 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   90336889 | 2330 | `			if( pNode->xCode ){` |
|          - | 2331 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   46034697 | 2332 | `				iLastWasTerm = 1;` |
|   67319543 | 2333 | `			}else if( pNode->pOp ){` |
|          - | 2334 | `				/* Operator node */` |
|   25617809 | 2335 | `				iLastWasTerm = 0;` |
|   12808907 | 2336 | `			}else{` |
|          - | 2337 | `				/* Delimiter: ')' and ']' end terms */` |
|   18684393 | 2338 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2339 | `			}` |
|          - | 2340 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2341 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2342 | `			 * node kind, so this single test covers all branches. */` |
|   90336889 | 2343 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2344 | `			/* Save the extracted node */` |
|   90336889 | 2345 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2346 | `		}` |
|          - | 2347 | `	}` |
|   17147115 | 2348 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2349 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2350 | `		*ppRoot = 0;` |
|        ! 0 | 2351 | `		return SXRET_OK;` |
|          - | 2352 | `	}` |
|   17147115 | 2353 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2354 | `	/* Make sure we are dealing with valid nodes */` |
|   17147115 | 2355 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17147115 | 2356 | `	if( rc != SXRET_OK ){` |
|          - | 2357 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2358 | `		 * cleanup the mess left behind.` |
|          - | 2359 | `		 */` |
|         54 | 2360 | `		*ppRoot = 0;` |
|         54 | 2361 | `		return rc;` |
|          - | 2362 | `	}` |
|          - | 2363 | `	/* Build the tree */` |
|   17147065 | 2364 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17147065 | 2365 | `	if( rc != SXRET_OK ){` |
|          - | 2366 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        103 | 2367 | `		*ppRoot = 0;` |
|        103 | 2368 | `		return rc;` |
|          - | 2369 | `	}` |
|          - | 2370 | `	/* Point to the root of the tree */` |
|   17146967 | 2371 | `	*ppRoot = apNode[0];` |
|   17146967 | 2372 | `	return SXRET_OK;` |
|    8573576 | 2373 | `}` |
|          - | 2374 |  |
