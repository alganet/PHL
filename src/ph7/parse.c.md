# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1192/1375 lines (86.69%)

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
|   29172472 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|          5 |  274 | `{` |
|   29172477 |  275 | `	sxu32 n = 0;` |
|          - |  276 | `	sxi32 rc;` |
|          - |  277 | `	/* Do a linear lookup on the operators table */` |
|  460291353 |  278 | `	for(;;){` |
|  920582711 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|        ! 0 |  280 | `			break;` |
|          - |  281 | `		}` |
|  920582711 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|          - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   87985213 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   43992609 |  285 | `		}else{` |
|  832597503 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|          - |  287 | `		}` |
|  920582711 |  288 | `		if( rc == 0 ){` |
|   29507287 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|          - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   29051501 |  291 | `				return &aOpTable[n];` |
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
|  891410239 |  308 | `		++n; /* Next operator in the table */` |
|          5 |  309 | `	}` |
|          - |  310 | `	/* No such operator */` |
|        ! 0 |  311 | `	return 0;` |
|   14586241 |  312 | `}` |
|          - |  313 | `/*` |
|          - |  314 | ` * Delimit a set of token stream.` |
|          - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|          - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|          - |  317 | ` */` |
|    7931536 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|          5 |  319 | `{` |
|    7931541 |  320 | `	SyToken *pCur = pIn;` |
|    7931541 |  321 | `	sxi32 iNest = 1;` |
|   85063358 |  322 | `	for(;;){` |
|  170126721 |  323 | `		if( pCur >= pEnd ){` |
|      16043 |  324 | `			break;` |
|          - |  325 | `		}` |
|  170110683 |  326 | `		if( pCur->nType & nTokStart ){` |
|          - |  327 | `			/* Increment nesting level */` |
|    6557221 |  328 | `			iNest++;` |
|  166832075 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|          - |  330 | `			/* Decrement nesting level */` |
|   14472719 |  331 | `			iNest--;` |
|   14472719 |  332 | `			if( iNest <= 0 ){` |
|    7915503 |  333 | `				break;` |
|          - |  334 | `			}` |
|    3278608 |  335 | `		}` |
|          - |  336 | `		/* Advance cursor */` |
|  162195185 |  337 | `		pCur++;` |
|          5 |  338 | `	}` |
|          - |  339 | `	/* Point to the end of the chunk */` |
|    7931541 |  340 | `	*ppEnd = pCur;` |
|    7931541 |  341 | `}` |
|          - |  342 | `/*` |
|          - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|          - |  344 | ` * Note on reserved keywords.` |
|          - |  345 | ` *  According to the PHP language reference manual:` |
|          - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|          - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|          - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|          - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|          - |  350 | ` */` |
|     541012 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|          5 |  352 | `{` |
|     541012 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     540975 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|          - |  355 | `		){` |
|        131 |  356 | `			return TRUE;` |
|          - |  357 | `	}` |
|     540891 |  358 | `	if( bCheckFunc ){` |
|      58670 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      58658 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      58634 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|         61 |  362 | `				return TRUE;` |
|          - |  363 | `		}` |
|      29307 |  364 | `	}` |
|          - |  365 | `	/* Not a language construct */` |
|     540835 |  366 | `	return FALSE;` |
|     270511 |  367 | `}` |
|          - |  368 | `/*` |
|          - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|          - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|          - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|          - |  373 | ` */` |
|   17111994 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|          5 |  375 | `{` |
|          - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|          - |  377 | `	sxi32 i,rc;` |
|          - |  378 |  |
|   17111999 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|          - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       7803 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       7803 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       3899 |  383 | `	}` |
|   17111999 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  107263529 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   90151571 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|          - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     243309 |  388 | `			continue;` |
|          - |  389 | `		}` |
|   89908267 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|    7553151 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     408902 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|          - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|    7061855 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|          - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|          - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|          - |  397 | `						 */` |
|    7061855 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    7061855 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    7061855 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|    3530925 |  401 | `					}` |
|    3530925 |  402 | `			}` |
|    7553151 |  403 | `			iParen++;` |
|   86131694 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    7553155 |  405 | `			if( iParen <= 0 ){` |
|         16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|         16 |  407 | `				if( rc != SXERR_ABORT ){` |
|         16 |  408 | `					rc = SXERR_SYNTAX;` |
|          6 |  409 | `				}` |
|         16 |  410 | `				return rc;` |
|          - |  411 | `			}` |
|    7553143 |  412 | `			iParen--;` |
|   78578540 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    3027645 |  414 | `			iSquare++;` |
|   73288151 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    3027649 |  416 | `			if( iSquare <= 0 ){` |
|          9 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|          9 |  418 | `				if( rc != SXERR_ABORT ){` |
|          9 |  419 | `					rc = SXERR_SYNTAX;` |
|          3 |  420 | `				}` |
|          9 |  421 | `				return rc;` |
|          - |  422 | `			}` |
|    3027643 |  423 | `			iSquare--;` |
|   70260506 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|   68744740 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       3907 |  437 | `			if( iBraces <= 0 ){` |
|         16 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|         16 |  439 | `				if( rc != SXERR_ABORT ){` |
|         16 |  440 | `					rc = SXERR_SYNTAX;` |
|          6 |  441 | `				}` |
|         16 |  442 | `				return rc;` |
|          - |  443 | `			}` |
|       3895 |  444 | `			iBraces--;` |
|   68740838 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     504353 |  446 | `			if( iQuesty > 0 ){` |
|     504051 |  447 | `				iQuesty--;` |
|     252330 |  448 | `			}else if( iParen <= 0 ){` |
|          - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|          - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|          - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|          6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|          6 |  453 | `				if( rc != SXERR_ABORT ){` |
|          6 |  454 | `					rc = SXERR_SYNTAX;` |
|          2 |  455 | `				}` |
|          6 |  456 | `				return rc;` |
|          5 |  457 | `			}` |
|   68486717 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   22537579 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   22537579 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     504053 |  461 | `				iQuesty++;` |
|   22285555 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   11268787 |  482 | `		}` |
|   44954118 |  483 | `	}` |
|   17111963 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|         15 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|         15 |  486 | `		if( rc != SXERR_ABORT ){` |
|         15 |  487 | `			rc = SXERR_SYNTAX;` |
|          6 |  488 | `		}` |
|         15 |  489 | `		return rc;` |
|          - |  490 | `	}` |
|   17111951 |  491 | `	return SXRET_OK;` |
|    8556002 |  492 | `}` |
|          - |  493 | `/*` |
|          - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|          - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|          - |  496 | ` */` |
|   14110208 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|          5 |  498 | `{` |
|   14110213 |  499 | `	SyToken *pIn = *ppCur;` |
|          - |  500 | `	/* Jump the first literal seen */` |
|   14110213 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   14106261 |  502 | `		pIn++;` |
|    7053128 |  503 | `	}` |
|    7057121 |  504 | `	for(;;){` |
|   14114247 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       4039 |  506 | `			pIn++;` |
|       4039 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       4037 |  508 | `				pIn++;` |
|       2016 |  509 | `			}` |
|       2022 |  510 | `		}else{` |
|    7055109 |  511 | `			break;` |
|          - |  512 | `		}` |
|          5 |  513 | `	}` |
|          - |  514 | `	/* Synchronize pointers */` |
|   14110213 |  515 | `	*ppCur = pIn;` |
|   14110213 |  516 | `}` |
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
|   90155884 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|          5 |  868 | `{` |
|          - |  869 | `	ph7_expr_node *pNode;` |
|          - |  870 | `	SyToken *pCur;` |
|          - |  871 | `	sxi32 rc;` |
|          - |  872 | `	/* Allocate a new node */` |
|   90155889 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   90155889 |  874 | `	if( pNode == 0 ){` |
|          - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|          - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|          - |  877 | `		 */` |
|        ! 0 |  878 | `		return SXERR_MEM;` |
|          - |  879 | `	}` |
|          - |  880 | `	/* Zero the structure */` |
|   90155889 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   90155889 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|          - |  883 | `	/* Point to the head of the token stream */` |
|   90155889 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|          - |  885 | `	/* Start collecting tokens */` |
|   90155889 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       4237 |  887 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
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
|       4155 |  902 | `		pCur++;` |
|       4155 |  903 | `		pGen->pIn = pCur;` |
|       4155 |  904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       4155 |  905 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       4155 |  906 | `		if( rc == SXRET_OK && *ppNode ){` |
|       4155 |  907 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       2075 |  908 | `		}` |
|       4155 |  909 | `		return rc;` |
|          - |  910 | `	}` |
|   90151657 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|          - |  912 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|          - |  913 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|          - |  914 | `		 */` |
|     243311 |  915 | `		pCur++; /* Skip the opening '[' */` |
|     243311 |  916 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     243311 |  917 | `		if( pCur < pGen->pEnd ){` |
|     243311 |  918 | `			pCur++; /* Skip past the closing ']' */` |
|     121658 |  919 | `		}else{` |
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
|     243518 |  931 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|        419 |  932 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|        419 |  933 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|         57 |  934 | `				pNode->xCode = PH7_CompileShortList;` |
|         30 |  935 | `			}else{` |
|        365 |  936 | `				pNode->xCode = PH7_CompileShortArray;` |
|          - |  937 | `			}` |
|        212 |  938 | `		}else{` |
|     242897 |  939 | `			pNode->xCode = PH7_CompileShortArray;` |
|          5 |  940 | `		}` |
|   90030004 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|   89908338 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   25565274 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   12811791 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|   89908322 |  979 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|          - |  980 | `		/* Point to the instance that describe this operator */` |
|   25565257 |  981 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|          - |  982 | `		/* Advance the stream cursor */` |
|   25565257 |  983 | `		pCur++;` |
|   77125685 |  984 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|          - |  985 | `		/* Isolate variable */` |
|   43246341 |  986 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   21623179 |  987 | `			pCur++; /* Variable variable */` |
|          5 |  988 | `		}` |
|   21623167 |  989 | `		if( pCur < pGen->pEnd ){` |
|   21623167 |  990 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|          - |  991 | `				/* Variable name */` |
|   21623141 |  992 | `				pCur++;` |
|   10811599 |  993 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|         22 |  994 | `				pCur++;` |
|          - |  995 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|         22 |  996 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|         22 |  997 | `				if( pCur < pGen->pEnd ){` |
|         19 |  998 | `					pCur++;` |
|         11 |  999 | `				}else{` |
|          - | 1000 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|          - | 1001 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|          - | 1002 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|          - | 1003 | `					 * delimiter search stops at the slice end, so the token php names` |
|          - | 1004 | `					 * usually sits just past it, still inside the chunk stream. */` |
|          - | 1005 | `					{` |
|          3 | 1006 | `						SyToken *pBad = 0;` |
|          3 | 1007 | `						if( pGen->pTokenSet ){` |
|          3 | 1008 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|          3 | 1009 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|          3 | 1010 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|        ! 0 | 1011 | `								pBad = pCur;` |
|        ! 0 | 1012 | `							}` |
|          1 | 1013 | `						}` |
|          3 | 1014 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|          - | 1015 | `					}` |
|          3 | 1016 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1017 | `						rc = SXERR_SYNTAX;` |
|          1 | 1018 | `					}` |
|          3 | 1019 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1020 | `					return rc;` |
|          - | 1021 | `				}` |
|         11 | 1022 | `			}else{` |
|          - | 1023 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|          - | 1024 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|          - | 1025 | `				 * covered only the '$' and the offending token drifted into a later` |
|          - | 1026 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|          - | 1027 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|         10 | 1028 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|         10 | 1029 | `				if( rc != SXERR_ABORT ){` |
|         10 | 1030 | `					rc = SXERR_SYNTAX;` |
|          4 | 1031 | `				}` |
|         10 | 1032 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         10 | 1033 | `				return rc;` |
|          - | 1034 | `			}` |
|   10811576 | 1035 | `		}` |
|   21623157 | 1036 | `		pNode->xCode = PH7_CompileVariable;` |
|   53531473 | 1037 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1056787 | 1038 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    1056787 | 1039 | `		 if( bAfterMemberOp ){` |
|          - | 1040 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|          - | 1041 | `			  * method/property NAME, not a language construct — PHP allows any` |
|          - | 1042 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|          - | 1043 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|          - | 1044 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|          - | 1045 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|          - | 1046 | `			  * the word itself. */` |
|     128361 | 1047 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     128361 | 1048 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     128361 | 1049 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     992609 | 1050 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|          - | 1051 | `			 /* List/Array node */` |
|     428899 | 1052 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|          - | 1053 | `				 /* Assume a literal */` |
|        ! 0 | 1054 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1055 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1056 | `			 }else{` |
|     428899 | 1057 | `				 pCur += 2;` |
|          - | 1058 | `				 /* Collect array/list tokens */` |
|     428899 | 1059 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     428899 | 1060 | `				 if( pCur < pGen->pEnd ){` |
|     428897 | 1061 | `					 pCur++;` |
|     214451 | 1062 | `				 }else{` |
|          - | 1063 | `					 /* Syntax error */` |
|          - | 1064 | `					 /* php names the token it stopped on and says it expected ")". */` |
|          3 | 1065 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|          3 | 1066 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 1067 | `						 rc = SXERR_SYNTAX;` |
|          1 | 1068 | `					 }` |
|          3 | 1069 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1070 | `					 return rc;` |
|          - | 1071 | `				 }` |
|     428897 | 1072 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     428897 | 1073 | `				 if( pNode->xCode == PH7_CompileList ){` |
|         39 | 1074 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|         39 | 1075 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|          - | 1076 | `						 /* Syntax error */` |
|          3 | 1077 | `						 rc = PH7_GenSyntaxError(pGen,pNode->pStart,"\"=\"");` |
|          3 | 1078 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 1079 | `							 rc = SXERR_SYNTAX;` |
|          1 | 1080 | `						 }` |
|          3 | 1081 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1082 | `						 return rc;` |
|          - | 1083 | `					 }` |
|         16 | 1084 | `				 }` |
|          5 | 1085 | `			 }` |
|     713982 | 1086 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|          - | 1087 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      15921 | 1088 | `			 pCur++; /* Skip 'yield' keyword */` |
|      15921 | 1089 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1090 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1091 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      15921 | 1092 | `			 pNode->xCode = PH7_CompileYield;` |
|     491579 | 1093 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     483328 | 1094 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7856 | 1095 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3943 | 1096 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|          - | 1097 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|        647 | 1098 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|          - | 1099 | `				 /* Assume a literal */` |
|        ! 0 | 1100 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        ! 0 | 1101 | `				pNode->xCode = PH7_CompileLiteral;` |
|        ! 0 | 1102 | `			 }else{` |
|          - | 1103 | `				 /* Assemble annonymous functions body */` |
|        647 | 1104 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|        647 | 1105 | `				 if( rc != SXRET_OK ){` |
|         28 | 1106 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         28 | 1107 | `					 return rc;` |
|          - | 1108 | `				 }` |
|        623 | 1109 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|          - | 1110 | `			  }` |
|     483288 | 1111 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|         43 | 1112 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|         24 | 1113 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|         12 | 1114 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|          9 | 1115 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|          - | 1116 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|          - | 1117 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|          - | 1118 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|          - | 1119 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|         34 | 1120 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         34 | 1121 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1122 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1123 | `				 return rc;` |
|          - | 1124 | `			 }` |
|         34 | 1125 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     482963 | 1126 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|     482684 | 1127 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       7832 | 1128 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3919 | 1129 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|          - | 1130 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|        551 | 1131 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|        551 | 1132 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1133 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1134 | `				 return rc;` |
|          - | 1135 | `			 }` |
|        551 | 1136 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     482676 | 1137 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|          - | 1138 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|         78 | 1139 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|         78 | 1140 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1141 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        ! 0 | 1142 | `				 return rc;` |
|          - | 1143 | `			 }` |
|         78 | 1144 | `			 pNode->xCode = PH7_CompileMatch;` |
|     482366 | 1145 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|          - | 1146 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|          - | 1147 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|          - | 1148 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|         38 | 1149 | `			 pCur++; /* Skip 'throw' */` |
|         38 | 1150 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|          - | 1151 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|          - | 1152 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         38 | 1153 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     482311 | 1154 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|          - | 1155 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|         73 | 1156 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|         73 | 1157 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|         39 | 1158 | `		 }else{` |
|          - | 1159 | `			 /* Assume a literal */` |
|     482225 | 1160 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     482225 | 1161 | `			 pNode->xCode = PH7_CompileLiteral;` |
|          5 | 1162 | `		 }` |
|   42191492 | 1163 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|          - | 1164 | `		 /* Constants,function name,namespace path,class name... */` |
|   13499619 | 1165 | `		 if( bAfterMemberOp ){` |
|          - | 1166 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|          - | 1167 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|          - | 1168 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|          - | 1169 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|    5625225 | 1170 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|    2812610 | 1171 | `		 }` |
|   13499619 | 1172 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   13499619 | 1173 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    6749812 | 1174 | `	 }else{` |
|   28163501 | 1175 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|          - | 1176 | `			 /* Point to the code generator routine */` |
|    9517395 | 1177 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|    9517395 | 1178 | `			 if( pNode->xCode == 0 ){` |
|          3 | 1179 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 1180 | `				 if( rc != SXERR_ABORT ){` |
|          3 | 1181 | `					 rc = SXERR_SYNTAX;` |
|          1 | 1182 | `				 }` |
|          3 | 1183 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|          3 | 1184 | `				 return rc;` |
|          - | 1185 | `			 }` |
|    4758694 | 1186 | `		 }` |
|          - | 1187 | `		/* Advance the stream cursor */` |
|   28163499 | 1188 | `		pCur++;` |
|          - | 1189 | `	 }` |
|          - | 1190 | `	/* Point to the end of the token stream */` |
|   90151617 | 1191 | `	pNode->pEnd = pCur;` |
|          - | 1192 | `	/* Save the node for later processing */` |
|   90151617 | 1193 | `	*ppNode = pNode;` |
|          - | 1194 | `	/* Synchronize cursors */` |
|   90151617 | 1195 | `	pGen->pIn = pCur;` |
|   90151617 | 1196 | `	return SXRET_OK;` |
|   45077947 | 1197 | `}` |
|          - | 1198 | `/*` |
|          - | 1199 | ` * Point to the next expression that should be evaluated shortly.` |
|          - | 1200 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|          - | 1201 | ` * level is zero.` |
|          - | 1202 | ` */` |
|    1938982 | 1203 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|          5 | 1204 | `{` |
|    1938987 | 1205 | `	SyToken *pCur = pStart;` |
|    1938987 | 1206 | `	sxi32 iNest = 0;` |
|    1938987 | 1207 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|          - | 1208 | `		/* Last expression */` |
|     716041 | 1209 | `		return SXERR_EOF;` |
|          - | 1210 | `	}` |
|    4641085 | 1211 | `	while( pCur < pEnd ){` |
|    4339287 | 1212 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     921153 | 1213 | `			break;` |
|          - | 1214 | `		}` |
|    3418139 | 1215 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     241605 | 1216 | `			iNest++;` |
|    3297339 | 1217 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     241609 | 1218 | `			iNest--;` |
|     120802 | 1219 | `		}` |
|    3418139 | 1220 | `		pCur++;` |
|          5 | 1221 | `	}` |
|    1222951 | 1222 | `	*ppNext = pCur;` |
|    1222951 | 1223 | `	return SXRET_OK;` |
|     969496 | 1224 | `}` |
|          - | 1225 | `/*` |
|          - | 1226 | ` * Free an expression tree.` |
|          - | 1227 | ` */` |
|   77052258 | 1228 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|          5 | 1229 | `{` |
|   77052263 | 1230 | `	if( pNode->pLeft ){` |
|          - | 1231 | `		/* Release the left tree */` |
|   30117087 | 1232 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   15058541 | 1233 | `	}` |
|   77052263 | 1234 | `	if( pNode->pRight ){` |
|          - | 1235 | `		/* Release the right tree */` |
|   17518191 | 1236 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    8759093 | 1237 | `	}` |
|   77052263 | 1238 | `	if( pNode->pCond ){` |
|          - | 1239 | `		/* Release the conditional tree used by the ternary operator */` |
|     504049 | 1240 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     252022 | 1241 | `	}` |
|   77052263 | 1242 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|          - | 1243 | `		ph7_expr_node **apArg;` |
|          - | 1244 | `		sxu32 n;` |
|          - | 1245 | `		/* Release node arguments */` |
|    8303775 | 1246 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   19113419 | 1247 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   10809649 | 1248 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    5404827 | 1249 | `		}` |
|    8303775 | 1250 | `		SySetRelease(&pNode->aNodeArgs);` |
|    4151885 | 1251 | `	}` |
|          - | 1252 | `	/* Finally,release this node */` |
|   77052263 | 1253 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   77052263 | 1254 | `}` |
|          - | 1255 | `/*` |
|          - | 1256 | ` * Free an expression tree.` |
|          - | 1257 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|          - | 1258 | ` */` |
|   17112030 | 1259 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|          5 | 1260 | `{` |
|          - | 1261 | `	ph7_expr_node **apNode;` |
|          - | 1262 | `	sxu32 n;` |
|   17112035 | 1263 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  107263699 | 1264 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   90151669 | 1265 | `		if( apNode[n] ){` |
|   17112359 | 1266 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    8556177 | 1267 | `		}` |
|   45075837 | 1268 | `	}` |
|   17112035 | 1269 | `	return SXRET_OK;` |
|          5 | 1270 | `}` |
|          - | 1271 | `/*` |
|          - | 1272 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|          - | 1273 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|          - | 1274 | ` * references, and unset() that target any link of a nullsafe chain` |
|          - | 1275 | ` * (PHP 8.0 makes this a fatal parse error:` |
|          - | 1276 | ` * "Can't use nullsafe operator in write context").` |
|          - | 1277 | ` */` |
|   21312924 | 1278 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|          5 | 1279 | `{` |
|   21312929 | 1280 | `	if( pNode == 0 ){` |
|   13277245 | 1281 | `		return 0;` |
|          - | 1282 | `	}` |
|    8035689 | 1283 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         16 | 1284 | `		return 1;` |
|          - | 1285 | `	}` |
|    8035677 | 1286 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|          6 | 1287 | `		return 1;` |
|          - | 1288 | `	}` |
|    8035673 | 1289 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|        ! 0 | 1290 | `		return 1;` |
|          - | 1291 | `	}` |
|    8035673 | 1292 | `	return 0;` |
|   10656467 | 1293 | `}` |
|          - | 1294 | `/*` |
|          - | 1295 | ` * Check if the given node is a modifialbe l/r-value.` |
|          - | 1296 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|          - | 1297 | ` */` |
|    5218216 | 1298 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|          5 | 1299 | `{` |
|          - | 1300 | `	sxi32 iExprOp;` |
|    5218221 | 1301 | `	if( pNode->pOp == 0 ){` |
|    3721905 | 1302 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|          - | 1303 | `	}` |
|    1496321 | 1304 | `	iExprOp = pNode->pOp->iOp;` |
|    1496321 | 1305 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|     959701 | 1306 | `			return TRUE;` |
|          - | 1307 | `	}` |
|     536625 | 1308 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|     536611 | 1309 | `		if( pNode->pLeft->pOp ) {` |
|     128190 | 1310 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|      54384 | 1311 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|        ! 0 | 1312 | `				return FALSE;` |
|          5 | 1313 | `			}` |
|     472516 | 1314 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        ! 0 | 1315 | `			return FALSE;` |
|          - | 1316 | `		}` |
|     536611 | 1317 | `		return TRUE;` |
|          - | 1318 | `	}` |
|         16 | 1319 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|          8 | 1320 | `		return TRUE;` |
|          - | 1321 | `	}` |
|          - | 1322 | `	/* Not a modifiable l or r-value */` |
|          9 | 1323 | `	return FALSE;` |
|    2609113 | 1324 | `}` |
|          - | 1325 | `/* Forward declaration */` |
|          - | 1326 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|          - | 1327 | `/* Macro to check if the given node is a terminal.` |
|          - | 1328 | ` * A node is a term if it has no operator, or has already been linked into an` |
|          - | 1329 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|          - | 1330 | ` * linked ternary/elvis node). */` |
|          - | 1331 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|          - | 1332 | `/*` |
|          - | 1333 | ` * Buid an expression tree for each given function argument.` |
|          - | 1334 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1335 | ` */` |
|    5513386 | 1336 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1337 | `{` |
|          - | 1338 | `	sxi32 iNest,iCur,iNode;` |
|          - | 1339 | `	sxi32 rc;` |
|          - | 1340 | `	/* Process function arguments from left to right */` |
|    5513391 | 1341 | `	iCur = 0;` |
|    6766310 | 1342 | `	for(;;){` |
|   13532625 | 1343 | `		if( iCur >= nToken ){` |
|          - | 1344 | `			/* No more arguments to process */` |
|    5513363 | 1345 | `			break;` |
|          - | 1346 | `		}` |
|    8019267 | 1347 | `		iNode = iCur;` |
|    8019267 | 1348 | `		iNest = 0;` |
|   26271797 | 1349 | `		while( iCur < nToken ){` |
|   20758437 | 1350 | `			if( apNode[iCur] ){` |
|   20711547 | 1351 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    1252956 | 1352 | `					break;` |
|   18205640 | 1353 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    9747799 | 1354 | `					&& apNode[iCur]->pLeft == 0` |
|    1289951 | 1355 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    1287368 | 1356 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|          - | 1357 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|          - | 1358 | `					 * self-contained node that already consumed its matching ']', so its` |
|          - | 1359 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|          - | 1360 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|          - | 1361 | `					 * following comma is never seen as an argument separator (collapsing` |
|          - | 1362 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|          - | 1363 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|          - | 1364 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|          - | 1365 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|          - | 1366 | `					 * and swallow the following argument separator. */` |
|    1284787 | 1367 | `					iNest++;` |
|   17563254 | 1368 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    9102825 | 1369 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    1284787 | 1370 | `					iNest--;` |
|     642391 | 1371 | `				}` |
|    9102820 | 1372 | `			}` |
|   18252535 | 1373 | `			iCur++;` |
|          5 | 1374 | `		}` |
|    8019267 | 1375 | `		if( iCur > iNode ){` |
|    8019261 | 1376 | `			SyString sArgName = {0, 0};` |
|          - | 1377 | `			/* Check for named argument pattern: identifier ':' expr.` |
|          - | 1378 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|          - | 1379 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    8019256 | 1380 | `			if( (iCur - iNode) >= 2` |
|    5578715 | 1381 | `				&& apNode[iNode]` |
|    3138151 | 1382 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    1692888 | 1383 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     247373 | 1384 | `				&& apNode[iNode+1]` |
|     247103 | 1385 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|          - | 1386 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|        303 | 1387 | `				sArgName = apNode[iNode]->pStart->sData;` |
|        303 | 1388 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        303 | 1389 | `				apNode[iNode] = 0;` |
|        303 | 1390 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|        303 | 1391 | `				apNode[iNode+1] = 0;` |
|        303 | 1392 | `				iNode += 2;` |
|          - | 1393 | `				/* Guard: the value expression must not be empty.  Catches` |
|          - | 1394 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|        303 | 1395 | `				if( iNode >= iCur ){` |
|          4 | 1396 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|          2 | 1397 | `						pOp->pStart->nLine,` |
|          - | 1398 | `						"syntax error, expected expression after named argument '%z:'",` |
|          - | 1399 | `						&sArgName);` |
|          3 | 1400 | `					if( rc != SXERR_ABORT ){` |
|          3 | 1401 | `						rc = SXERR_SYNTAX;` |
|          1 | 1402 | `					}` |
|          3 | 1403 | `					return rc;` |
|          - | 1404 | `				}` |
|        148 | 1405 | `			}` |
|    8019254 | 1406 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|          5 | 1407 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|        ! 0 | 1408 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|          - | 1409 | `						"call-time pass-by-reference is depreceated");` |
|        ! 0 | 1410 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|        ! 0 | 1411 | `					apNode[iNode] = 0;` |
|        ! 0 | 1412 | `			}` |
|          - | 1413 | `			{` |
|          - | 1414 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|          - | 1415 | `				 * time; when the expression is more than a lone terminal` |
|          - | 1416 | `				 * (a call, member access, ...) tree-building roots the span` |
|          - | 1417 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|          - | 1418 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|          - | 1419 | `				 * used to pass the whole array as one argument). Scan for` |
|          - | 1420 | `				 * the first LIVE node: an outer paren pass may already have` |
|          - | 1421 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|          - | 1422 | `				 * NULL slots ahead of the flagged subtree. */` |
|    8019259 | 1423 | `				int bSpreadArg = 0;` |
|          - | 1424 | `				sxi32 iScan;` |
|    8019305 | 1425 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    8019305 | 1426 | `					if( apNode[iScan] ){` |
|    8019259 | 1427 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    8019259 | 1428 | `						break;` |
|          - | 1429 | `					}` |
|         26 | 1430 | `				}` |
|    8019259 | 1431 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    8019259 | 1432 | `				if( bSpreadArg && apNode[iNode] ){` |
|       4085 | 1433 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       2040 | 1434 | `				}` |
|          - | 1435 | `			}` |
|    8019259 | 1436 | `			if( apNode[iNode] ){` |
|    8019259 | 1437 | `				if( sArgName.nByte > 0 ){` |
|        300 | 1438 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|        300 | 1439 | `					apNode[iNode]->sArgName = sArgName;` |
|        148 | 1440 | `				}` |
|          - | 1441 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    8019259 | 1442 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    4009632 | 1443 | `			}else{` |
|          - | 1444 | `				/* No expression before comma */` |
|        ! 0 | 1445 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        ! 0 | 1446 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|          - | 1447 | `					"syntax error, unexpected token \",\"");` |
|        ! 0 | 1448 | `				if( rc != SXERR_ABORT ){` |
|        ! 0 | 1449 | `					rc = SXERR_SYNTAX;` |
|        ! 0 | 1450 | `				}` |
|        ! 0 | 1451 | `				return rc;` |
|          - | 1452 | `			}` |
|    4009632 | 1453 | `		}else{` |
|          - | 1454 | `			/* Comma with no preceding argument */` |
|          8 | 1455 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|          8 | 1456 | `			if( rc != SXERR_ABORT ){` |
|          8 | 1457 | `				rc = SXERR_SYNTAX;` |
|          3 | 1458 | `			}` |
|          8 | 1459 | `			return rc;` |
|          - | 1460 | `		}` |
|          - | 1461 | `		/* Jump trailing comma */` |
|    8019259 | 1462 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    2505901 | 1463 | `			iCur++;` |
|    2505901 | 1464 | `			if( iCur >= nToken ){` |
|          - | 1465 | `				/* Trailing comma after last argument */` |
|         21 | 1466 | `				break;` |
|          - | 1467 | `			}` |
|    1252938 | 1468 | `		}` |
|          5 | 1469 | `	}` |
|    5513383 | 1470 | `	return SXRET_OK;` |
|    2756698 | 1471 | `}` |
|          - | 1472 | ` /*` |
|          - | 1473 | `  * Create an expression tree from an array of tokens.` |
|          - | 1474 | `  * If successful, the root of the tree is stored in apNode[0].` |
|          - | 1475 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 1476 | `  */` |
|   29576514 | 1477 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|          5 | 1478 | ` {` |
|          - | 1479 | `	 sxi32 i,iLeft,iRight;` |
|          - | 1480 | `	 ph7_expr_node *pNode;` |
|          - | 1481 | `	 ph7_expr_node *pSuppress;` |
|          - | 1482 | `	 sxi32 iCur;` |
|          - | 1483 | `	 sxi32 rc;` |
|   29576519 | 1484 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|          - | 1485 | `		 /* TICKET 1433-17: self evaluating node */` |
|   12997333 | 1486 | `		 return SXRET_OK;` |
|          - | 1487 | `	 }` |
|          - | 1488 | `	 /* Process expressions enclosed in parenthesis first */` |
|  117983927 | 1489 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1490 | `		 sxi32 iNest;` |
|          - | 1491 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1492 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|          - | 1493 | `		  */` |
|  101404743 | 1494 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  100913455 | 1495 | `			 continue;` |
|          - | 1496 | `		 }` |
|     491293 | 1497 | `		 iNest = 1;` |
|     491293 | 1498 | `		 iLeft = iCur;` |
|          - | 1499 | `		 /* Find the closing parenthesis */` |
|     491293 | 1500 | `		 iCur++;` |
|    4379009 | 1501 | `		 while( iCur < nToken ){` |
|    4379009 | 1502 | `			 if( apNode[iCur] ){` |
|    4379009 | 1503 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|          - | 1504 | `					 /* Decrement nesting level */` |
|     717723 | 1505 | `					 iNest--;` |
|     717723 | 1506 | `					 if( iNest <= 0 ){` |
|     491293 | 1507 | `						 break;` |
|          5 | 1508 | `					 }` |
|    3774506 | 1509 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|          - | 1510 | `					 /* Increment nesting level */` |
|     226435 | 1511 | `					 iNest++;` |
|     113215 | 1512 | `				 }` |
|    1943858 | 1513 | `			 }` |
|    3887721 | 1514 | `			 iCur++;` |
|          5 | 1515 | `		 }` |
|     491293 | 1516 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1517 | `			 sxi32 j;` |
|          - | 1518 | `			 /* Recurse and process this expression */` |
|     491293 | 1519 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|     491293 | 1520 | `			 if( rc != SXRET_OK ){` |
|          3 | 1521 | `				 return rc;` |
|          - | 1522 | `			 }` |
|          - | 1523 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|          - | 1524 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|          - | 1525 | `			  * hoist a unary operator that the user explicitly isolated.` |
|          - | 1526 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|          - | 1527 | `			  * node at extraction — must survive onto the root too, or the` |
|          - | 1528 | `			  * group's free below silently drops the unpacking. */` |
|     491291 | 1529 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|     491291 | 1530 | `				 if( apNode[j] ){` |
|     491291 | 1531 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|     491286 | 1532 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|     491291 | 1533 | `					 break;` |
|          - | 1534 | `				 }` |
|        ! 0 | 1535 | `			 }` |
|     245643 | 1536 | `		 }` |
|          - | 1537 | `		 /* Free the left and right nodes */` |
|     491291 | 1538 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|     491291 | 1539 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|     491291 | 1540 | `		 apNode[iLeft] = 0;` |
|     491291 | 1541 | `		 apNode[iCur] = 0;` |
|     245648 | 1542 | `	 }` |
|          - | 1543 | `	  /* Process expressions enclosed in braces */` |
|  122043917 | 1544 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|          - | 1545 | `		 sxi32 iNest;` |
|          - | 1546 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|          - | 1547 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|          - | 1548 | `		  */` |
|  105775957 | 1549 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  105772067 | 1550 | `			 continue;` |
|          - | 1551 | `		 }` |
|       3895 | 1552 | `		 iNest = 1;` |
|       3895 | 1553 | `		 iLeft = iCur;` |
|          - | 1554 | `		 /* Find the closing parenthesis */` |
|       3895 | 1555 | `		 iCur++;` |
|       7783 | 1556 | `		 while( iCur < nToken ){` |
|       7783 | 1557 | `			 if( apNode[iCur] ){` |
|       7783 | 1558 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|          - | 1559 | `					 /* Decrement nesting level */` |
|       3895 | 1560 | `					 iNest--;` |
|       3895 | 1561 | `					 if( iNest <= 0 ){` |
|       3895 | 1562 | `						 break;` |
|        ! 0 | 1563 | `					 }` |
|       3893 | 1564 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|          - | 1565 | `					 /* Increment nesting level */` |
|        ! 0 | 1566 | `					 iNest++;` |
|        ! 0 | 1567 | `				 }` |
|       1944 | 1568 | `			 }` |
|       3893 | 1569 | `			 iCur++;` |
|          5 | 1570 | `		 }` |
|       3895 | 1571 | `		 if( iCur - iLeft > 1 ){` |
|          - | 1572 | `			 /* Recurse and process this expression */` |
|       3893 | 1573 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       3893 | 1574 | `			 if( rc != SXRET_OK ){` |
|        ! 0 | 1575 | `				 return rc;` |
|          - | 1576 | `			 }` |
|       1944 | 1577 | `		 }` |
|          - | 1578 | `		 /* Free the left and right nodes */` |
|       3895 | 1579 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       3895 | 1580 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       3895 | 1581 | `		 apNode[iLeft] = 0;` |
|       3895 | 1582 | `		 apNode[iCur] = 0;` |
|       1950 | 1583 | `	 }` |
|          - | 1584 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   16267965 | 1585 | `	 iLeft = -1;` |
|  122051659 | 1586 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105783711 | 1587 | `		 if( apNode[iCur] == 0 ){` |
|   46710991 | 1588 | `			 continue;` |
|          - | 1589 | `		 }` |
|   59072725 | 1590 | `		 pNode = apNode[iCur];` |
|   59072725 | 1591 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   16083881 | 1592 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|          - | 1593 | `				 /* Collect function arguments */` |
|    7061853 | 1594 | `				 sxi32 iPtr = 0;` |
|    7061853 | 1595 | `				 sxi32 nFuncTok = 0;` |
|   34882135 | 1596 | `				 while( nFuncTok + iCur < nToken ){` |
|   34882135 | 1597 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|          - | 1598 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|          - | 1599 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|          - | 1600 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|          - | 1601 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|          - | 1602 | `					  * nulled, so counting it here would over-count and never find` |
|          - | 1603 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   34882135 | 1604 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   34823549 | 1605 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    7523017 | 1606 | `							 iPtr++;` |
|   31062043 | 1607 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    7523017 | 1608 | `							 iPtr--;` |
|    7523017 | 1609 | `							 if( iPtr <= 0 ){` |
|    7061853 | 1610 | `								 break;` |
|          - | 1611 | `							 }` |
|     230582 | 1612 | `						 }` |
|   13880848 | 1613 | `					 }` |
|   27820287 | 1614 | `					 nFuncTok++;` |
|          5 | 1615 | `				 }` |
|    7061853 | 1616 | `				 if( nFuncTok + iCur >= nToken ){` |
|          - | 1617 | `					 /* Syntax error */` |
|        ! 0 | 1618 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|        ! 0 | 1619 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1620 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1621 | `					 }` |
|        ! 0 | 1622 | `					 return rc;` |
|          - | 1623 | `				 }` |
|    7061853 | 1624 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|          - | 1625 | `					 /* Syntax error */` |
|        ! 0 | 1626 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|        ! 0 | 1627 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1628 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1629 | `					 }` |
|        ! 0 | 1630 | `					 return rc;` |
|          - | 1631 | `				 }` |
|    7061853 | 1632 | `				 if( nFuncTok > 1 ){` |
|          - | 1633 | `					 /* Process function arguments */` |
|    5513391 | 1634 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    5513391 | 1635 | `					 if( rc != SXRET_OK ){` |
|         11 | 1636 | `						 return rc;` |
|          - | 1637 | `					 }` |
|    2756689 | 1638 | `				 }` |
|          - | 1639 | `				 /* Link the node to the tree */` |
|    7061845 | 1640 | `				 pNode->pLeft = apNode[iLeft];` |
|    7061845 | 1641 | `				 apNode[iLeft] = 0;` |
|   34882103 | 1642 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   27820263 | 1643 | `					 apNode[iCur+iPtr] = 0;` |
|   13910134 | 1644 | `				 }` |
|          - | 1645 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|          - | 1646 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|          - | 1647 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|          - | 1648 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|          - | 1649 | `				  * constructor call into that new-node NOW, before the postfix` |
|          - | 1650 | `				  * operators bind, and relocate the completed new-node onto this` |
|          - | 1651 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|          - | 1652 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|          - | 1653 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|          - | 1654 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|          - | 1655 | `				 {` |
|    7061845 | 1656 | `					 sxi32 iNew = iLeft - 1;` |
|    9127269 | 1657 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    2065429 | 1658 | `						 iNew--;` |
|          5 | 1659 | `					 }` |
|    7061840 | 1660 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    3981798 | 1661 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    2412523 | 1662 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     856847 | 1663 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     856847 | 1664 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     856847 | 1665 | `						 apNode[iNew] = 0;` |
|     856847 | 1666 | `						 pNode = apNode[iCur];` |
|     428426 | 1667 | `					 }` |
|          - | 1668 | `				 }` |
|   12552953 | 1669 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|          - | 1670 | `				 /* Subscripting */` |
|    3027643 | 1671 | `				 sxi32 iArrTok = iCur + 1;` |
|    3027643 | 1672 | `				 sxi32 iNest = 1;` |
|    3027638 | 1673 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         34 | 1674 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|         28 | 1675 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|          - | 1676 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|          - | 1677 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|          - | 1678 | `					  * subscripting a global constant raised "Invalid array name" while` |
|          - | 1679 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|          - | 1680 | `					  * worked. */` |
|         22 | 1681 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|         16 | 1682 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    3027638 | 1683 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|          - | 1684 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|          - | 1685 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     322490 | 1686 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|          - | 1687 | `						 /* Syntax error */` |
|        ! 0 | 1688 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        ! 0 | 1689 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1690 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1691 | `						 }` |
|        ! 0 | 1692 | `						 return rc;` |
|          - | 1693 | `				 }` |
|          - | 1694 | `				 /* Collect index tokens */` |
|    6361663 | 1695 | `				 while( iArrTok < nToken ){` |
|    6361663 | 1696 | `					 if( apNode[iArrTok] ){` |
|    6361631 | 1697 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|          - | 1698 | `							 /* Increment nesting level */` |
|      27179 | 1699 | `							 iNest++;` |
|    6348044 | 1700 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|          - | 1701 | `							 /* Decrement nesting level */` |
|    3054817 | 1702 | `							 iNest--;` |
|    3054817 | 1703 | `							 if( iNest <= 0 ){` |
|    3027643 | 1704 | `								 break;` |
|          - | 1705 | `							 }` |
|      13587 | 1706 | `						 }` |
|    1666994 | 1707 | `					 }` |
|    3334025 | 1708 | `					 ++iArrTok;` |
|          5 | 1709 | `				 }` |
|    3027643 | 1710 | `				 if( iArrTok > iCur + 1 ){` |
|          - | 1711 | `					 /* Recurse and process this expression */` |
|    2790395 | 1712 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    2790395 | 1713 | `					 if( rc != SXRET_OK ){` |
|        ! 0 | 1714 | `						 return rc;` |
|          - | 1715 | `					 }` |
|          - | 1716 | `					 /* Link the node to it's index */` |
|    2790395 | 1717 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    1395195 | 1718 | `				 }` |
|          - | 1719 | `				 /* Link the node to the tree */` |
|    3027643 | 1720 | `				 pNode->pLeft = apNode[iLeft];` |
|    3027643 | 1721 | `				 pNode->pRight = 0;` |
|    3027643 | 1722 | `				 apNode[iLeft] = 0;` |
|    9389301 | 1723 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    6361663 | 1724 | `					 apNode[iNest] = 0;` |
|    3180834 | 1725 | `				 }` |
|    1513824 | 1726 | `			 }else{` |
|          - | 1727 | `				 /* Member access operators [i.e: '->','::'] */` |
|    5994395 | 1728 | `				  iRight = iCur + 1;` |
|    5998283 | 1729 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       3893 | 1730 | `					 iRight++;` |
|          5 | 1731 | `				 }` |
|    5994395 | 1732 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1733 | `					 /* Syntax error */` |
|          5 | 1734 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|          5 | 1735 | `					 if( rc != SXERR_ABORT ){` |
|          5 | 1736 | `						 rc = SXERR_SYNTAX;` |
|          2 | 1737 | `					 }` |
|          5 | 1738 | `					 return rc;` |
|          - | 1739 | `				 }` |
|          - | 1740 | `				 /* Link the node to the tree */` |
|    5994391 | 1741 | `				 pNode->pLeft = apNode[iLeft];` |
|    5994386 | 1742 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|    5784221 | 1743 | `					 && pNode->pLeft->pOp == 0 &&` |
|    5483995 | 1744 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|          - | 1745 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|          - | 1746 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|          - | 1747 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|          - | 1748 | `					  * accepts. */` |
|          4 | 1749 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|          - | 1750 | `						 /* Syntax error */` |
|        ! 0 | 1751 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 1752 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|        ! 0 | 1753 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1754 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1755 | `						 }` |
|        ! 0 | 1756 | `						 return rc;` |
|          - | 1757 | `				 }` |
|    5994391 | 1758 | `				 pNode->pRight = apNode[iRight];` |
|    5994391 | 1759 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|          - | 1760 | `			 }` |
|    8041932 | 1761 | `		 }` |
|   59072713 | 1762 | `		 iLeft = iCur;` |
|   29536359 | 1763 | `	 }` |
|          - | 1764 | `	 /* Handle left associative (new, clone) operators */` |
|  122051627 | 1765 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105783679 | 1766 | `		 if( apNode[iCur] == 0 ){` |
|   63714279 | 1767 | `			 continue;` |
|          - | 1768 | `		 }` |
|   42069405 | 1769 | `		 pNode = apNode[iCur];` |
|   42069405 | 1770 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|          - | 1771 | `			 SyToken *pToken;` |
|          - | 1772 | `			 /* Get the left node */` |
|      62587 | 1773 | `			 iLeft = iCur + 1;` |
|      62595 | 1774 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|          9 | 1775 | `				 iLeft++;` |
|          1 | 1776 | `			 }` |
|      62587 | 1777 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 1778 | `				  /* Syntax error */` |
|        ! 0 | 1779 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|        ! 0 | 1780 | `					 &pNode->pOp->sOp);` |
|        ! 0 | 1781 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1782 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1783 | `				 }` |
|        ! 0 | 1784 | `				 return rc;` |
|          - | 1785 | `			 }` |
|          - | 1786 | `			 /* Make sure the operand are of a valid type */` |
|      62587 | 1787 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|          - | 1788 | `				 /* Clone:` |
|          - | 1789 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|          - | 1790 | `				  *  ++ function call (including annonymous)` |
|          - | 1791 | `				  *  ++ array member` |
|          - | 1792 | `				  *  ++ 'new' operator` |
|          - | 1793 | `				  * Example:` |
|          - | 1794 | `				  *   clone $pObj;` |
|          - | 1795 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|          - | 1796 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|          - | 1797 | `				  */` |
|      58281 | 1798 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      58275 | 1799 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|        ! 0 | 1800 | `						 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1801 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|        ! 0 | 1802 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1803 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1804 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1805 | `						 }` |
|        ! 0 | 1806 | `						 return rc;` |
|          - | 1807 | `					 }` |
|      29135 | 1808 | `				 }` |
|      29143 | 1809 | `			 }else{` |
|          - | 1810 | `				 /* New */` |
|       4306 | 1811 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|          5 | 1812 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          - | 1813 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|          - | 1814 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|          - | 1815 | `					  * expression (PHP parse error). The postfix pass folds` |
|          - | 1816 | ``					  * `new C()` into a completed term, so guard against the`` |
|          - | 1817 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|          - | 1818 | `					  * (the inner is a parenthesized group). */` |
|        ! 0 | 1819 | `					 pToken = apNode[iLeft]->pStart;` |
|        ! 0 | 1820 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1821 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1822 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1823 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1824 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1825 | `					 }` |
|        ! 0 | 1826 | `					 return rc;` |
|          - | 1827 | `				 }` |
|       4311 | 1828 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       4311 | 1829 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       4306 | 1830 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         35 | 1831 | `						 && xCons != PH7_CompileAnnonClass){` |
|        ! 0 | 1832 | `						 pToken = apNode[iLeft]->pStart;` |
|          - | 1833 | `						 /* Syntax error */` |
|        ! 0 | 1834 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 1835 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|        ! 0 | 1836 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|        ! 0 | 1837 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1838 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 1839 | `						 }` |
|        ! 0 | 1840 | `						 return rc;` |
|          - | 1841 | `					 }` |
|       2153 | 1842 | `				 }` |
|          - | 1843 | `			 }` |
|          - | 1844 | `			  /* Link the node to the tree */` |
|      62587 | 1845 | `			 pNode->pLeft = apNode[iLeft];` |
|      62587 | 1846 | `			 apNode[iLeft] = 0;` |
|      62587 | 1847 | `			 pNode->pRight = 0; /* Paranoid */` |
|      31291 | 1848 | `		 }` |
|   21034705 | 1849 | `	 }` |
|          - | 1850 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   16267953 | 1851 | `	 iLeft = -1;` |
|  122051627 | 1852 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105628067 | 1853 | `		 if( apNode[iCur] == 0 ){` |
|   63714279 | 1854 | `			 continue;` |
|          - | 1855 | `		 }` |
|   41913793 | 1856 | `		 pNode = apNode[iCur];` |
|   41913793 | 1857 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     373459 | 1858 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     186733 | 1859 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|          - | 1860 | `					 /* Link the node to the tree */` |
|     202291 | 1861 | `					 pNode->pLeft = apNode[iLeft];` |
|     202291 | 1862 | `					 apNode[iLeft] = 0;` |
|     101143 | 1863 | `			 }` |
|     435695 | 1864 | `		  }` |
|   42069405 | 1865 | `		 iLeft = iCur;` |
|   21034705 | 1866 | `	  }` |
|   16423565 | 1867 | `	 iLeft = -1;` |
|  122207239 | 1868 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105783679 | 1869 | `		 if( apNode[iCur] == 0 ){` |
|   63916565 | 1870 | `			 continue;` |
|          - | 1871 | `		 }` |
|   41867119 | 1872 | `		 pNode = apNode[iCur];` |
|   41867119 | 1873 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      15556 | 1874 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      15561 | 1875 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|          - | 1876 | `					 /* Syntax error */` |
|        ! 0 | 1877 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|        ! 0 | 1878 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1879 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 1880 | `					 }` |
|        ! 0 | 1881 | `					 return rc;` |
|          - | 1882 | `			 }` |
|          - | 1883 | `			 /* Link the node to the tree */` |
|      15561 | 1884 | `			 pNode->pLeft = apNode[iLeft];` |
|      15561 | 1885 | `			 apNode[iLeft] = 0;` |
|          - | 1886 | `			 /* Mark as pre-increment/decrement node */` |
|      15561 | 1887 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|       7778 | 1888 | `		  }` |
|   41867119 | 1889 | `		 iLeft = iCur;` |
|   20933562 | 1890 | `	 }` |
|          - | 1891 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|          - | 1892 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|          - | 1893 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|          - | 1894 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|          - | 1895 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|          - | 1896 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|          - | 1897 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|          - | 1898 | `	  * pass below skips it (pLeft != 0). */` |
|   16423565 | 1899 | `	 iLeft = -1;` |
|  122207239 | 1900 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  105783679 | 1901 | `		 if( apNode[iCur] == 0 ){` |
|   64013951 | 1902 | `			 continue;` |
|          - | 1903 | `		 }` |
|   41769733 | 1904 | `		 pNode = apNode[iCur];` |
|   41769733 | 1905 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      81835 | 1906 | `			 iRight = iCur + 1;` |
|      81835 | 1907 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        ! 0 | 1908 | `				 iRight++;` |
|        ! 0 | 1909 | `			 }` |
|      81835 | 1910 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        ! 0 | 1911 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1912 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1913 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1914 | `				 }` |
|        ! 0 | 1915 | `				 return rc;` |
|          - | 1916 | `			 }` |
|      81835 | 1917 | `			 pNode->pLeft = apNode[iLeft];` |
|      81835 | 1918 | `			 pNode->pRight = apNode[iRight];` |
|      81835 | 1919 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      40915 | 1920 | `		 }` |
|   41769733 | 1921 | `		 iLeft = iCur;` |
|   20884869 | 1922 | `	 }` |
|          - | 1923 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   16423565 | 1924 | `	  iLeft = 0;` |
|  122207233 | 1925 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  105783675 | 1926 | `		  if( apNode[iCur] ){` |
|   41687899 | 1927 | `			  pNode = apNode[iCur];` |
|   41687899 | 1928 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    1376113 | 1929 | `				  if( iLeft > 0 ){` |
|          - | 1930 | `					  /* Link the node to the tree */` |
|    1376111 | 1931 | `					  pNode->pLeft = apNode[iLeft];` |
|    1376111 | 1932 | `					  apNode[iLeft] = 0;` |
|    1376111 | 1933 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      54463 | 1934 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|          - | 1935 | `							   /* Syntax error */` |
|        ! 0 | 1936 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1937 | `							  if( rc != SXERR_ABORT ){` |
|        ! 0 | 1938 | `								  rc = SXERR_SYNTAX;` |
|        ! 0 | 1939 | `							  }` |
|        ! 0 | 1940 | `							  return rc;` |
|          - | 1941 | `						  }` |
|      27229 | 1942 | `					  }` |
|     688058 | 1943 | `				  }else{` |
|          - | 1944 | `					  /* Syntax error */` |
|          3 | 1945 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|          3 | 1946 | `					  if( rc != SXERR_ABORT ){` |
|          3 | 1947 | `						  rc = SXERR_SYNTAX;` |
|          1 | 1948 | `					  }` |
|          3 | 1949 | `					  return rc;` |
|          - | 1950 | `				  }` |
|     688053 | 1951 | `			  }` |
|          - | 1952 | `			  /* Save terminal position */` |
|   41687897 | 1953 | `			  iLeft = iCur;` |
|   20843946 | 1954 | `		  }` |
|   52891839 | 1955 | `	  }` |
|          - | 1956 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|          - | 1957 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|          - | 1958 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|          - | 1959 | `	  * yielding a right-leaning tree. */` |
|  122207231 | 1960 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  105783673 | 1961 | `		 if( apNode[iCur] == 0 ){` |
|   65472001 | 1962 | `			 continue;` |
|          - | 1963 | `		 }` |
|   40311677 | 1964 | `		 pNode = apNode[iCur];` |
|   40311677 | 1965 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|          - | 1966 | `			 sxi32 iL, iR;` |
|          - | 1967 | `			 /* Find the right operand */` |
|        115 | 1968 | `			 iR = -1;` |
|          - | 1969 | `			 {` |
|          - | 1970 | `				 sxi32 j;` |
|        127 | 1971 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|        127 | 1972 | `					 if( apNode[j] ){ iR = j; break; }` |
|          7 | 1973 | `				 }` |
|          - | 1974 | `			 }` |
|          - | 1975 | `			 /* Find the left operand */` |
|        115 | 1976 | `			 iL = -1;` |
|          - | 1977 | `			 {` |
|          - | 1978 | `				 sxi32 j;` |
|        183 | 1979 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|        183 | 1980 | `					 if( apNode[j] ){ iL = j; break; }` |
|         35 | 1981 | `				 }` |
|          - | 1982 | `			 }` |
|        115 | 1983 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|        ! 0 | 1984 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 1985 | `				 if( rc != SXERR_ABORT ){` |
|        ! 0 | 1986 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 1987 | `				 }` |
|        ! 0 | 1988 | `				 return rc;` |
|          - | 1989 | `			 }` |
|        115 | 1990 | `			 pNode->pLeft  = apNode[iL];` |
|        115 | 1991 | `			 pNode->pRight = apNode[iR];` |
|        115 | 1992 | `			 apNode[iL] = 0;` |
|        115 | 1993 | `			 apNode[iR] = 0;` |
|          - | 1994 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|          - | 1995 | `			  * The unary phase already attached its operand (pLeft) before` |
|          - | 1996 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|          - | 1997 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|          - | 1998 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|          - | 1999 | `			  * — the outermost unary stays outermost. The error-suppression` |
|          - | 2000 | `			  * operator '@' is treated identically to the other unaries:` |
|          - | 2001 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|          - | 2002 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|          - | 2003 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|          - | 2004 | `			  * operands are respected. */` |
|        114 | 2005 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|         75 | 2006 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|         35 | 2007 | `				 && pNode->pLeft->pLeft != 0` |
|         35 | 2008 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         27 | 2009 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|         27 | 2010 | `				 ph7_expr_node *pTail = pHead;` |
|          - | 2011 | `				 /* Walk down to the innermost hoistable unary — the one` |
|          - | 2012 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|         43 | 2013 | `				 while( pTail->pLeft` |
|         34 | 2014 | `					 && pTail->pLeft->pOp` |
|         23 | 2015 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|         12 | 2016 | `					 && pTail->pLeft->pLeft != 0` |
|         30 | 2017 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|          9 | 2018 | `					 pTail = pTail->pLeft;` |
|          1 | 2019 | `				 }` |
|          - | 2020 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|         27 | 2021 | `				 pNode->pLeft = pTail->pLeft;` |
|         27 | 2022 | `				 pTail->pLeft = pNode;` |
|         27 | 2023 | `				 apNode[iCur] = pHead;` |
|         13 | 2024 | `			 }` |
|         57 | 2025 | `		 }` |
|   20155841 | 2026 | `	 }` |
|          - | 2027 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  180659057 | 2028 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  164235509 | 2029 | `		 iLeft = -1;` |
| 1222071895 | 2030 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 1057836401 | 2031 | `			 if( apNode[iCur] == 0 ){` |
|  723519219 | 2032 | `				 continue;` |
|          - | 2033 | `			 }` |
|  334317187 | 2034 | `			 pNode = apNode[iCur];` |
|  334317187 | 2035 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2036 | `				 /* Get the right node */` |
|    5719709 | 2037 | `				 iRight = iCur + 1;` |
|    8666181 | 2038 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    2946477 | 2039 | `					 iRight++;` |
|          5 | 2040 | `				 }` |
|    5719709 | 2041 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2042 | `					 /* Syntax error */` |
|         10 | 2043 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         10 | 2044 | `					 if( rc != SXERR_ABORT ){` |
|         10 | 2045 | `						 rc = SXERR_SYNTAX;` |
|          4 | 2046 | `					 }` |
|         10 | 2047 | `					 return rc;` |
|          - | 2048 | `				 }` |
|    5719701 | 2049 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|          - | 2050 | `					 sxi32  iTmp;` |
|          - | 2051 | `					 /* Reference operator [i.e: '&=' ]*/` |
|          - | 2052 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|          - | 2053 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|          - | 2054 | `					  * right operand first since EXPR_OP_REF's operand order` |
|          - | 2055 | `					  * is swapped below. */` |
|         75 | 2056 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|          3 | 2057 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2058 | `							 "Can't use nullsafe operator in write context");` |
|          3 | 2059 | `						 if( rc != SXERR_ABORT ){` |
|          3 | 2060 | `							 rc = SXERR_SYNTAX;` |
|          1 | 2061 | `						 }` |
|          3 | 2062 | `						 return rc;` |
|          - | 2063 | `					 }` |
|          - | 2064 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|          - | 2065 | `					  * reference target — ExprIsModifiableValue already accepts` |
|          - | 2066 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|          - | 2067 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|          - | 2068 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|          - | 2069 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|         73 | 2070 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2071 | `						 /* Left operand must be a modifiable l-value */` |
|        ! 0 | 2072 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|        ! 0 | 2073 | `						 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2074 | `							 rc = SXERR_SYNTAX;` |
|        ! 0 | 2075 | `						 }` |
|        ! 0 | 2076 | `						 return rc;` |
|          - | 2077 | `					 }` |
|         73 | 2078 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|         55 | 2079 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|        ! 0 | 2080 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|        ! 0 | 2081 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|        ! 0 | 2082 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|          - | 2083 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|        ! 0 | 2084 | `									 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2085 | `										 rc = SXERR_SYNTAX;` |
|        ! 0 | 2086 | `									 }` |
|        ! 0 | 2087 | `									 return rc;` |
|          - | 2088 | `							 }` |
|        ! 0 | 2089 | `						 }` |
|         26 | 2090 | `					 }` |
|          - | 2091 | `					 /* Swap operands */` |
|         73 | 2092 | `					 iTmp = iRight;` |
|         73 | 2093 | `					 iRight = iLeft;` |
|         73 | 2094 | `					 iLeft = iTmp;` |
|         35 | 2095 | `				 }` |
|          - | 2096 | `				 /* Link the node to the tree */` |
|    5719699 | 2097 | `				 pNode->pLeft = apNode[iLeft];` |
|    5719699 | 2098 | `				 pNode->pRight = apNode[iRight];` |
|    5719699 | 2099 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    2859847 | 2100 | `			 }` |
|  334317177 | 2101 | `			 iLeft = iCur;` |
|  167158591 | 2102 | `		 }` |
|   82117752 | 2103 | `	 }` |
|          - | 2104 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|          - | 2105 | `	  * Note that we do not need a precedence loop here since` |
|          - | 2106 | `	  * we are dealing with a single operator.` |
|          - | 2107 | `	  */` |
|   16423553 | 2108 | `	  iLeft = -1;` |
|  118047027 | 2109 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  102127525 | 2110 | `		  if( apNode[iCur] == 0 ){` |
|   74763593 | 2111 | `			  continue;` |
|          - | 2112 | `		  }` |
|   27363937 | 2113 | `		  pNode = apNode[iCur];` |
|   27363937 | 2114 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     504051 | 2115 | `			  sxi32 iNest = 1;` |
|     504051 | 2116 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2117 | `				  /* Missing condition */` |
|          3 | 2118 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|          3 | 2119 | `				  if( rc != SXERR_ABORT ){` |
|          3 | 2120 | `					  rc = SXERR_SYNTAX;` |
|          1 | 2121 | `				  }` |
|          3 | 2122 | `				  return rc;` |
|          - | 2123 | `			  }` |
|          - | 2124 | `			  /* Get the right node */` |
|     504049 | 2125 | `			  iRight = iCur + 1;` |
|    2142053 | 2126 | `			  while( iRight < nToken  ){` |
|    2142053 | 2127 | `				  if( apNode[iRight] ){` |
|    1004141 | 2128 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|          - | 2129 | `						  /* Increment nesting level */` |
|        ! 0 | 2130 | `						  ++iNest;` |
|    1004141 | 2131 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|          - | 2132 | `						  /* Decrement nesting level */` |
|     504049 | 2133 | `						  --iNest;` |
|     504049 | 2134 | `						  if( iNest <= 0 ){` |
|     504049 | 2135 | `							  break;` |
|          - | 2136 | `						  }` |
|        ! 0 | 2137 | `					  }` |
|     250046 | 2138 | `				  }` |
|    1638009 | 2139 | `				  iRight++;` |
|          5 | 2140 | `			  }` |
|     504049 | 2141 | `			  if( iRight > iCur + 1 ){` |
|          - | 2142 | `				  /* Recurse and process the then expression */` |
|     500097 | 2143 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     500097 | 2144 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2145 | `					  return rc;` |
|          - | 2146 | `				  }` |
|          - | 2147 | `				  /* Link the node to the tree */` |
|     500097 | 2148 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     250046 | 2149 | `			  }else{` |
|          - | 2150 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|          - | 2151 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|          - | 2152 | `			  }` |
|     504049 | 2153 | `			  apNode[iCur + 1] = 0;` |
|     504049 | 2154 | `			  if( iRight + 1 < nToken ){` |
|          - | 2155 | `				  /* Recurse and process the else expression */` |
|     504049 | 2156 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     504049 | 2157 | `				  if( rc != SXRET_OK ){` |
|        ! 0 | 2158 | `					  return rc;` |
|          - | 2159 | `				  }` |
|          - | 2160 | `				  /* Link the node to the tree */` |
|     504049 | 2161 | `				  pNode->pRight = apNode[iRight + 1];` |
|     504049 | 2162 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     252027 | 2163 | `			  }else{` |
|        ! 0 | 2164 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|        ! 0 | 2165 | `				  if( rc != SXERR_ABORT ){` |
|        ! 0 | 2166 | `					 rc = SXERR_SYNTAX;` |
|        ! 0 | 2167 | `				 }` |
|        ! 0 | 2168 | `				 return rc;` |
|          - | 2169 | `			  }` |
|          - | 2170 | `			  /* Point to the condition */` |
|     504049 | 2171 | `			  pNode->pCond  = apNode[iLeft];` |
|     504049 | 2172 | `			  apNode[iLeft] = 0;` |
|     504049 | 2173 | `			  break;` |
|          - | 2174 | `		  }` |
|   26859891 | 2175 | `		  iLeft = iCur;` |
|   13429948 | 2176 | `	  }` |
|          - | 2177 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|          - | 2178 | `	  * Note: All right associative binary operators have precedence 18` |
|          - | 2179 | `	  * so there is no need for a precedence loop here.` |
|          - | 2180 | `	  */` |
|   16423551 | 2181 | `	 iRight = -1;` |
|  122207041 | 2182 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  105783547 | 2183 | `		 if( apNode[iCur] == 0 ){` |
|   84141797 | 2184 | `			 continue;` |
|          - | 2185 | `		 }` |
|   21641755 | 2186 | `		 pNode = apNode[iCur];` |
|   21641755 | 2187 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|          - | 2188 | `			 /* Get the left node */` |
|    5218139 | 2189 | `			 iLeft = iCur - 1;` |
|    7165255 | 2190 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    1947121 | 2191 | `				 iLeft--;` |
|          5 | 2192 | `			 }` |
|    5218139 | 2193 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2194 | `				 /* Syntax error */` |
|         46 | 2195 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2196 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|          8 | 2197 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          4 | 2198 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          4 | 2199 | `				 }else{` |
|         41 | 2200 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|          - | 2201 | `				 }` |
|         46 | 2202 | `				 if( rc != SXERR_ABORT ){` |
|         44 | 2203 | `					 rc = SXERR_SYNTAX;` |
|         20 | 2204 | `				 }` |
|         46 | 2205 | `				 return rc;` |
|          - | 2206 | `			 }` |
|          - | 2207 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|          - | 2208 | `			  * including deeper chains like $a?->b->c = 1 and` |
|          - | 2209 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|          - | 2210 | ``			  * chain still contains a `?->` that cannot participate in`` |
|          - | 2211 | `			  * a write. */` |
|    5218097 | 2212 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|         11 | 2213 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          - | 2214 | `					 "Can't use nullsafe operator in write context");` |
|         11 | 2215 | `				 if( rc != SXERR_ABORT ){` |
|         11 | 2216 | `					 rc = SXERR_SYNTAX;` |
|          4 | 2217 | `				 }` |
|         11 | 2218 | `				 return rc;` |
|          - | 2219 | `			 }` |
|          - | 2220 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|          - | 2221 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|          - | 2222 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|          - | 2223 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|          - | 2224 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    5218089 | 2225 | `			 pSuppress = 0;` |
|    5218084 | 2226 | `			 if( apNode[iLeft]->pOp` |
|    3357179 | 2227 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     748137 | 2228 | `				 && apNode[iLeft]->pLeft != 0` |
|          5 | 2229 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        ! 0 | 2230 | `				 pSuppress = apNode[iLeft];` |
|        ! 0 | 2231 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|        ! 0 | 2232 | `			 }` |
|    5218089 | 2233 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|          - | 2234 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|          - | 2235 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|          - | 2236 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|          - | 2237 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|          - | 2238 | `				  * assignment there, leaving the binary operator as the outer node.` |
|          - | 2239 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|        101 | 2240 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|          9 | 2241 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|          9 | 2242 | `					 ph7_expr_node *pParent = pHost;` |
|         13 | 2243 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|          7 | 2244 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|        ! 0 | 2245 | `						 pParent = pParent->pRight;` |
|        ! 0 | 2246 | `					 }` |
|          8 | 2247 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|          9 | 2248 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|          9 | 2249 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|          9 | 2250 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|          9 | 2251 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|          9 | 2252 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|          9 | 2253 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|          9 | 2254 | `						 iRight = iCur;` |
|          9 | 2255 | `						 continue;` |
|          - | 2256 | `					 }` |
|        ! 0 | 2257 | `				 }` |
|        120 | 2258 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|         86 | 2259 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|          - | 2260 | `					 /* Left operand must be a modifiable l-value */` |
|          3 | 2261 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|          - | 2262 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|          4 | 2263 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|          2 | 2264 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|          2 | 2265 | `					 }else{` |
|        ! 0 | 2266 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        ! 0 | 2267 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|          - | 2268 | `					 }` |
|          3 | 2269 | `					 if( rc != SXERR_ABORT ){` |
|          3 | 2270 | `						 rc = SXERR_SYNTAX;` |
|          1 | 2271 | `					 }` |
|          3 | 2272 | `					 return rc;` |
|          - | 2273 | `				 }` |
|         43 | 2274 | `			 }` |
|          - | 2275 | `			 /* Link the node to the tree (Reverse) */` |
|    5218079 | 2276 | `			 pNode->pLeft = apNode[iRight];` |
|    5218079 | 2277 | `			 pNode->pRight = apNode[iLeft];` |
|    5218079 | 2278 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    5218079 | 2279 | `			 if( pSuppress ){` |
|          - | 2280 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|        ! 0 | 2281 | `				 pSuppress->pLeft = pNode;` |
|        ! 0 | 2282 | `				 apNode[iCur] = pSuppress;` |
|        ! 0 | 2283 | `			 }` |
|    2609037 | 2284 | `		 }` |
|   21641695 | 2285 | `		 iRight = iCur;` |
|   10820850 | 2286 | `	 }` |
|          - | 2287 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   82117475 | 2288 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   65693981 | 2289 | `		 iLeft = -1;` |
|  488827893 | 2290 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  423133917 | 2291 | `			 if( apNode[iCur] == 0 ){` |
|  357439701 | 2292 | `				 continue;` |
|          - | 2293 | `			 }` |
|   65694221 | 2294 | `			 pNode = apNode[iCur];` |
|   65694221 | 2295 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|          - | 2296 | `				 /* Get the right node */` |
|         48 | 2297 | `				 iRight = iCur + 1;` |
|         60 | 2298 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         13 | 2299 | `					 iRight++;` |
|          1 | 2300 | `				 }` |
|         48 | 2301 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|          - | 2302 | `					 /* Syntax error */` |
|        ! 0 | 2303 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        ! 0 | 2304 | `					 if( rc != SXERR_ABORT ){` |
|        ! 0 | 2305 | `						 rc = SXERR_SYNTAX;` |
|        ! 0 | 2306 | `					 }` |
|        ! 0 | 2307 | `					 return rc;` |
|          - | 2308 | `				 }` |
|          - | 2309 | `				 /* Link the node to the tree */` |
|         48 | 2310 | `				 pNode->pLeft = apNode[iLeft];` |
|         48 | 2311 | `				 pNode->pRight = apNode[iRight];` |
|         48 | 2312 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         22 | 2313 | `			 }` |
|   65694221 | 2314 | `			 iLeft = iCur;` |
|   32847113 | 2315 | `		 }` |
|   32846993 | 2316 | `	 }` |
|          - | 2317 | `	 /* Point to the root of the expression tree */` |
|  105783455 | 2318 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   89359979 | 2319 | `		 if( apNode[iCur] ){` |
|   15622345 | 2320 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         22 | 2321 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|         22 | 2322 | `				  if( rc != SXERR_ABORT ){` |
|         22 | 2323 | `					  rc = SXERR_SYNTAX;` |
|          9 | 2324 | `				  }` |
|         22 | 2325 | `				  return rc;` |
|          - | 2326 | `			 }` |
|   15622327 | 2327 | `			 apNode[0] = apNode[iCur];` |
|   15622327 | 2328 | `			 apNode[iCur] = 0;` |
|    7811161 | 2329 | `		 }` |
|   44679983 | 2330 | `	 }` |
|   16423481 | 2331 | `	 return SXRET_OK;` |
|   14710456 | 2332 | ` }` |
|          - | 2333 | ` /*` |
|          - | 2334 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|          - | 2335 | `  * If successful, the root of the tree is stored in ppRoot.` |
|          - | 2336 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|          - | 2337 | `  * This is the public interface used by the most code generator routines.` |
|          - | 2338 | `  */` |
|   17112034 | 2339 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|          5 | 2340 | `{` |
|          - | 2341 | `	ph7_expr_node **apNode;` |
|          - | 2342 | `	ph7_expr_node *pNode;` |
|          - | 2343 | `	sxi32 rc;` |
|          - | 2344 | `	/* Reset node container */` |
|   17112039 | 2345 | `	SySetReset(pExprNode);` |
|   17112039 | 2346 | `	pNode = 0; /* Prevent compiler warning */` |
|          - | 2347 | `	/* Extract nodes one after one until we hit the end of the input */` |
|          - | 2348 | `	{` |
|   17112039 | 2349 | `		int iLastWasTerm = 0;` |
|   17112039 | 2350 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  107263733 | 2351 | `		while( pGen->pIn < pGen->pEnd ){` |
|   90151739 | 2352 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   90151739 | 2353 | `			if( rc != SXRET_OK ){` |
|         44 | 2354 | `				return rc;` |
|          - | 2355 | `			}` |
|          - | 2356 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   90151699 | 2357 | `			if( pNode->xCode ){` |
|          - | 2358 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   45940341 | 2359 | `				iLastWasTerm = 1;` |
|   67181531 | 2360 | `			}else if( pNode->pOp ){` |
|          - | 2361 | `				/* Operator node */` |
|   25565257 | 2362 | `				iLastWasTerm = 0;` |
|   12782631 | 2363 | `			}else{` |
|          - | 2364 | `				/* Delimiter: ')' and ']' end terms */` |
|   18646111 | 2365 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|          - | 2366 | `			}` |
|          - | 2367 | `			/* A keyword in the next node is a member name only right after a member` |
|          - | 2368 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|          - | 2369 | `			 * node kind, so this single test covers all branches. */` |
|   90151699 | 2370 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|          - | 2371 | `			/* Save the extracted node */` |
|   90151699 | 2372 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|          5 | 2373 | `		}` |
|          - | 2374 | `	}` |
|   17111999 | 2375 | `	if( SySetUsed(pExprNode) < 1 ){` |
|          - | 2376 | `		/* Empty expression [i.e: A semi-colon;] */` |
|        ! 0 | 2377 | `		*ppRoot = 0;` |
|        ! 0 | 2378 | `		return SXRET_OK;` |
|          - | 2379 | `	}` |
|   17111999 | 2380 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|          - | 2381 | `	/* Make sure we are dealing with valid nodes */` |
|   17111999 | 2382 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17111999 | 2383 | `	if( rc != SXRET_OK ){` |
|          - | 2384 | `		/* Don't worry about freeing memory,upper layer will` |
|          - | 2385 | `		 * cleanup the mess left behind.` |
|          - | 2386 | `		 */` |
|         52 | 2387 | `		*ppRoot = 0;` |
|         52 | 2388 | `		return rc;` |
|          - | 2389 | `	}` |
|          - | 2390 | `	/* Build the tree */` |
|   17111951 | 2391 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   17111951 | 2392 | `	if( rc != SXRET_OK ){` |
|          - | 2393 | `		/* Something goes wrong [i.e: Syntax error] */` |
|        101 | 2394 | `		*ppRoot = 0;` |
|        101 | 2395 | `		return rc;` |
|          - | 2396 | `	}` |
|          - | 2397 | `	/* Point to the root of the tree */` |
|   17111855 | 2398 | `	*ppRoot = apNode[0];` |
|   17111855 | 2399 | `	return SXRET_OK;` |
|    8556022 | 2400 | `}` |
|          - | 2401 |  |
