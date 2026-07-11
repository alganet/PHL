# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1133/1297 lines (87.36%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * This file implement a hand-coded, thread-safe, full-reentrant and highly-efficient` |
|        - |    9 | ` * expression parser for the PH7 engine.` |
|        - |   10 | ` * Besides from the one introudced by PHP (Over 60), the PH7 engine have introduced three new` |
|        - |   11 | ` * operators. These are 'eq', 'ne' and the comma operator ','.` |
|        - |   12 | ` * The eq and ne operators are borrowed from the Perl world. They are used for strict` |
|        - |   13 | ` * string comparison. The reason why they have been implemented in the PH7 engine` |
|        - |   14 | ` * and introduced as an extension to the PHP programming language is due to the confusion` |
|        - |   15 | ` * introduced by the standard PHP comparison operators ('==' or '===') especially if you` |
|        - |   16 | ` * are comparing strings with numbers.` |
|        - |   17 | ` * Take the following example:` |
|        - |   18 | ` * var_dump( 0xFF == '255' ); // bool(true) ???` |
|        - |   19 | ` * // use the type equal operator by adding a single space to one of the operand` |
|        - |   20 | ` * var_dump( '255  ' === '255' ); //bool(true) depending on the PHP version` |
|        - |   21 | ` * That is, if one of the operand looks like a number (either integer or float) then PHP` |
|        - |   22 | ` * will internally convert the two operands to numbers and then a numeric comparison is performed.` |
|        - |   23 | ` * This is what the PHP language reference manual says:` |
|        - |   24 | ` * If you compare a number with a string or the comparison involves numerical strings, then each` |
|        - |   25 | ` * string is converted to a number and the comparison performed numerically.` |
|        - |   26 | ` * Bummer, if you ask me,this is broken, badly broken. I mean,the programmer cannot dictate` |
|        - |   27 | ` * it's comparison rule, it's the underlying engine who decides in it's place and perform` |
|        - |   28 | ` * the internal conversion. In most cases,PHP developers wants simple string comparison and they` |
|        - |   29 | ` * are stuck to use the ugly and inefficient strcmp() function and it's variants instead.` |
|        - |   30 | ` * This is the big reason why we have introduced these two operators.` |
|        - |   31 | ` * The eq operator is used to compare two strings byte per byte. If you came from the C/C++ world` |
|        - |   32 | ` * think of this operator as a barebone implementation of the memcmp() C standard library function.` |
|        - |   33 | ` * Keep in mind that if you are comparing two ASCII strings then the capital letters and their lowercase` |
|        - |   34 | ` * letters are completely different and so this example will output false.` |
|        - |   35 | ` * var_dump('allo' eq 'Allo'); //bool(FALSE)` |
|        - |   36 | ` * The ne operator perform the opposite operation of the eq operator and is used to test for string` |
|        - |   37 | ` * inequality. This example will output true` |
|        - |   38 | ` * var_dump('allo' ne 'Allo'); //bool(TRUE) unequal strings` |
|        - |   39 | ` * The eq operator return a Boolean true if and only if the two strings are identical while the` |
|        - |   40 | ` * ne operator return a Boolean true if and only if the two strings are different. Otherwise` |
|        - |   41 | ` * a Boolean false is returned (equal strings).` |
|        - |   42 | ` * Note that the comparison is performed only if the two strings are of the same length.` |
|        - |   43 | ` * Otherwise the eq and ne operators return a Boolean false without performing any comparison` |
|        - |   44 | ` * and avoid us wasting CPU time for nothing.` |
|        - |   45 | ` * Again remember that we talk about a low level byte per byte comparison and nothing else.` |
|        - |   46 | ` * Also remember that zero length strings are always equal.` |
|        - |   47 | ` *` |
|        - |   48 | ` * Again, another powerful mechanism borrowed from the C/C++ world and introduced as an extension` |
|        - |   49 | ` * to the PHP programming language.` |
|        - |   50 | ` * A comma expression contains two operands of any type separated by a comma and has left-to-right` |
|        - |   51 | ` * associativity. The left operand is fully evaluated, possibly producing side effects, and its` |
|        - |   52 | ` * value, if there is one, is discarded. The right operand is then evaluated. The type and value` |
|        - |   53 | ` * of the result of a comma expression are those of its right operand, after the usual unary conversions.` |
|        - |   54 | ` * Any number of expressions separated by commas can form a single expression because the comma operator` |
|        - |   55 | ` * is associative. The use of the comma operator guarantees that the sub-expressions will be evaluated` |
|        - |   56 | ` * in left-to-right order, and the value of the last becomes the value of the entire expression.` |
|        - |   57 | ` * The following example assign the value 25 to the variable $a, multiply the value of $a with 2` |
|        - |   58 | ` * and assign the result to variable $b and finally we call a test function to output the value` |
|        - |   59 | ` * of $a and $b. Keep-in mind that all theses operations are done in a single expression using` |
|        - |   60 | ` * the comma operator to create side effect.` |
|        - |   61 | ` * $a = 25,$b = $a << 1 ,test();` |
|        - |   62 | ` * //Output the value of $a and $b` |
|        - |   63 | ` * function test(){` |
|        - |   64 | ` *	 global $a,$b;` |
|        - |   65 | ` *	 echo "\$a = $a \$b= $b\n"; // You should see: $a = 25 $b = 50` |
|        - |   66 | ` * }` |
|        - |   67 | ` *` |
|        - |   68 | ` * For a full discussions on these extensions, please refer to  offical` |
|        - |   69 | ` * documentation(http://ph7.symisc.net/features.html) or visit the offical forums` |
|        - |   70 | ` * (http://forums.symisc.net/) if you want to share your point of view.` |
|        - |   71 | ` *` |
|        - |   72 | ` * Exprressions: According to the PHP language reference manual` |
|        - |   73 | ` *` |
|        - |   74 | ` * Expressions are the most important building stones of PHP. In PHP, almost anything you write is an expression.` |
|        - |   75 | ` * The simplest yet most accurate way to define an expression is "anything that has a value".` |
|        - |   76 | ` * The most basic forms of expressions are constants and variables. When you type "$a = 5", you're assigning` |
|        - |   77 | ` * '5' into $a. '5', obviously, has the value 5, or in other words '5' is an expression with the value of 5` |
|        - |   78 | ` * (in this case, '5' is an integer constant).` |
|        - |   79 | ` * After this assignment, you'd expect $a's value to be 5 as well, so if you wrote $b = $a, you'd expect` |
|        - |   80 | ` * it to behave just as if you wrote $b = 5. In other words, $a is an expression with the value of 5 as well.` |
|        - |   81 | ` * If everything works right, this is exactly what will happen.` |
|        - |   82 | ` * Slightly more complex examples for expressions are functions. For instance, consider the following function:` |
|        - |   83 | ` * <?php` |
|        - |   84 | ` * function foo ()` |
|        - |   85 | ` * {` |
|        - |   86 | ` *   return 5;` |
|        - |   87 | ` * }` |
|        - |   88 | ` * ?>` |
|        - |   89 | ` * Assuming you're familiar with the concept of functions (if you're not, take a look at the chapter about functions)` |
|        - |   90 | ` * you'd assume that typing $c = foo() is essentially just like writing $c = 5, and you're right.` |
|        - |   91 | ` * Functions are expressions with the value of their return value. Since foo() returns 5, the value of the expression` |
|        - |   92 | ` * 'foo()' is 5. Usually functions don't just return a static value but compute something.` |
|        - |   93 | ` * Of course, values in PHP don't have to be integers, and very often they aren't.` |
|        - |   94 | ` * PHP supports four scalar value types: integer values, floating point values (float), string values and boolean values` |
|        - |   95 | ` * (scalar values are values that you can't 'break' into smaller pieces, unlike arrays, for instance).` |
|        - |   96 | ` * PHP also supports two composite (non-scalar) types: arrays and objects. Each of these value types can be assigned` |
|        - |   97 | ` * into variables or returned from functions.` |
|        - |   98 | ` * PHP takes expressions much further, in the same way many other languages do. PHP is an expression-oriented language` |
|        - |   99 | ` * in the sense that almost everything is an expression. Consider the example we've already dealt with, '$a = 5'.` |
|        - |  100 | ` * It's easy to see that there are two values involved here, the value of the integer constant '5', and the value` |
|        - |  101 | ` * of $a which is being updated to 5 as well. But the truth is that there's one additional value involved here` |
|        - |  102 | ` * and that's the value of the assignment itself. The assignment itself evaluates to the assigned value, in this case 5.` |
|        - |  103 | ` * In practice, it means that '$a = 5', regardless of what it does, is an expression with the value 5. Thus, writing` |
|        - |  104 | ` * something like '$b = ($a = 5)' is like writing '$a = 5; $b = 5;' (a semicolon marks the end of a statement).` |
|        - |  105 | ` * Since assignments are parsed in a right to left order, you can also write '$b = $a = 5'.` |
|        - |  106 | ` * Another good example of expression orientation is pre- and post-increment and decrement.` |
|        - |  107 | ` * Users of PHP and many other languages may be familiar with the notation of variable++ and variable--.` |
|        - |  108 | ` * These are increment and decrement operators. In PHP, like in C, there are two types of increment - pre-increment` |
|        - |  109 | ` * and post-increment. Both pre-increment and post-increment essentially increment the variable, and the effect` |
|        - |  110 | ` * on the variable is identical. The difference is with the value of the increment expression. Pre-increment, which is written` |
|        - |  111 | ` * '++$variable', evaluates to the incremented value (PHP increments the variable before reading its value, thus the name 'pre-increment').` |
|        - |  112 | ` * Post-increment, which is written '$variable++' evaluates to the original value of $variable, before it was incremented` |
|        - |  113 | ` * (PHP increments the variable after reading its value, thus the name 'post-increment').` |
|        - |  114 | ` * A very common type of expressions are comparison expressions. These expressions evaluate to either FALSE or TRUE.` |
|        - |  115 | ` * PHP supports > (bigger than), >= (bigger than or equal to), == (equal), != (not equal), < (smaller than) and <= (smaller than or equal to).` |
|        - |  116 | ` * The language also supports a set of strict equivalence operators: === (equal to and same type) and !== (not equal to or not same type).` |
|        - |  117 | ` * These expressions are most commonly used inside conditional execution, such as if statements.` |
|        - |  118 | ` * The last example of expressions we'll deal with here is combined operator-assignment expressions.` |
|        - |  119 | ` * You already know that if you want to increment $a by 1, you can simply write '$a++' or '++$a'.` |
|        - |  120 | ` * But what if you want to add more than one to it, for instance 3? You could write '$a++' multiple times, but this is obviously not a very` |
|        - |  121 | ` * efficient or comfortable way. A much more common practice is to write '$a = $a + 3'. '$a + 3' evaluates to the value of $a plus 3` |
|        - |  122 | ` * and is assigned back into $a, which results in incrementing $a by 3. In PHP, as in several other languages like C, you can write` |
|        - |  123 | ` * this in a shorter way, which with time would become clearer and quicker to understand as well. Adding 3 to the current value of $a` |
|        - |  124 | ` * can be written '$a += 3'. This means exactly "take the value of $a, add 3 to it, and assign it back into $a".` |
|        - |  125 | ` * In addition to being shorter and clearer, this also results in faster execution. The value of '$a += 3', like the value of a regular` |
|        - |  126 | ` * assignment, is the assigned value. Notice that it is NOT 3, but the combined value of $a plus 3 (this is the value that's assigned into $a).` |
|        - |  127 | ` * Any two-place operator can be used in this operator-assignment mode, for example '$a -= 5' (subtract 5 from the value of $a), '$b *= 7'` |
|        - |  128 | ` * (multiply the value of $b by 7), etc.` |
|        - |  129 | ` * There is one more expression that may seem odd if you haven't seen it in other languages, the ternary conditional operator:` |
|        - |  130 | ` * <?php` |
|        - |  131 | ` * $first ? $second : $third` |
|        - |  132 | ` * ?>` |
|        - |  133 | ` * If the value of the first subexpression is TRUE (non-zero), then the second subexpression is evaluated, and that is the result` |
|        - |  134 | ` * of the conditional expression. Otherwise, the third subexpression is evaluated, and that is the value.` |
|        - |  135 | ` */` |
|        - |  136 | `/* Operators associativity */` |
|        - |  137 | `#define EXPR_OP_ASSOC_LEFT   0x01 /* Left associative operator */` |
|        - |  138 | `#define EXPR_OP_ASSOC_RIGHT  0x02 /* Right associative operator */` |
|        - |  139 | `#define EXPR_OP_NON_ASSOC    0x04 /* Non-associative operator */` |
|        - |  140 | `/*` |
|        - |  141 | ` * Operators table` |
|        - |  142 | ` * This table is sorted by operators priority (highest to lowest) according` |
|        - |  143 | ` * the PHP language reference manual.` |
|        - |  144 | ` * PH7 implements all the 60 PHP operators and have introduced the eq and ne operators.` |
|        - |  145 | ` * The operators precedence table have been improved dramatically so that you can do same` |
|        - |  146 | ` * amazing things now such as array dereferencing,on the fly function call,anonymous function` |
|        - |  147 | ` * as array values,class member access on instantiation and so on.` |
|        - |  148 | ` * Refer to the following page for a full discussion on these improvements:` |
|        - |  149 | ` * http://ph7.symisc.net/features.html#improved_precedence` |
|        - |  150 | ` */` |
|        - |  151 | `static const ph7_expr_op aOpTable[] = {` |
|        - |  152 | `	/* Precedence 1: non-associative */` |
|        - |  153 | `	{ {"new",sizeof("new")-1},     EXPR_OP_NEW,   1, EXPR_OP_NON_ASSOC, PH7_OP_NEW  },` |
|        - |  154 | `	{ {"clone",sizeof("clone")-1}, EXPR_OP_CLONE, 1, EXPR_OP_NON_ASSOC, PH7_OP_CLONE},` |
|        - |  155 | `	                              /* Postfix operators */` |
|        - |  156 | `	/* Precedence 2(Highest),left-associative */` |
|        - |  157 | `	{ {"->",sizeof(char)*2}, EXPR_OP_ARROW,     2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|        - |  158 | `	{ {"?->",sizeof(char)*3},EXPR_OP_NULLSAFE_ARROW, 2, EXPR_OP_ASSOC_LEFT, PH7_OP_MEMBER},` |
|        - |  159 | `	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|        - |  160 | `	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},` |
|        - |  161 | `	/* Precedence 3,non-associative  */` |
|        - |  162 | `	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},` |
|        - |  163 | `	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},` |
|        - |  164 | `	                              /* Unary operators */` |
|        - |  165 | `	/* Precedence 4,right-associative  */` |
|        - |  166 | `	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },` |
|        - |  167 | `	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },` |
|        - |  168 | `	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },` |
|        - |  169 | `	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },` |
|        - |  170 | `	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},` |
|        - |  171 | `	                             /* Cast operators */` |
|        - |  172 | `	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },` |
|        - |  173 | `	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },` |
|        - |  174 | `	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },` |
|        - |  175 | `	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|        - |  176 | `	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},` |
|        - |  177 | `	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },` |
|        - |  178 | `	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },` |
|        - |  179 | `	                           /* Binary operators */` |
|        - |  180 | `	/* Precedence 5,right-associative: exponentiation (PHP 5.6) */` |
|        - |  181 | `	{ {"**",sizeof(char)*2}, EXPR_OP_POW, 5, EXPR_OP_ASSOC_RIGHT, PH7_OP_POW},` |
|        - |  182 | `	/* Precedence 7,left-associative */` |
|        - |  183 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|        - |  184 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|        - |  185 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|        - |  186 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|        - |  187 | `	/* Precedence 8,left-associative */` |
|        - |  188 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|        - |  189 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|        - |  190 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|        - |  191 | `	/* Precedence 9,left-associative */` |
|        - |  192 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|        - |  193 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|        - |  194 | ``	/* PHP 8.5 pipe operator: `$x \|> f(...)` desugars to `f($x)`. It binds`` |
|        - |  195 | `	 * looser than shift/arithmetic and tighter than comparison — PHP places it` |
|        - |  196 | `	 * between precedence 9 and 10. We share level 9 (left-associative) so the` |
|        - |  197 | `	 * generic binary tree-builder links it correctly; the actual codegen is` |
|        - |  198 | `	 * custom (a one-argument call of the RHS callable), handled in` |
|        - |  199 | `	 * GenStateEmitExprCode. iVmOp is 0 like the other codegen-only operators. */` |
|        - |  200 | `	{ {"\|>",sizeof(char)*2}, EXPR_OP_PIPE, 9, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  201 | `	/* Precedence 10,non-associative */` |
|        - |  202 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|        - |  203 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|        - |  204 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|        - |  205 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|        - |  206 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|        - |  207 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  208 | `	/* Precedence 11,non-associative */` |
|        - |  209 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|        - |  210 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  211 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|        - |  212 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|        - |  213 | `	/* Precedence 12,left-associative */` |
|        - |  214 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|        - |  215 | `	/* Precedence 12,left-associative */` |
|        - |  216 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|        - |  217 | `	                         /* Binary operators */` |
|        - |  218 | `	/* Precedence 13,left-associative */` |
|        - |  219 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|        - |  220 | `	/* Precedence 14,left-associative */` |
|        - |  221 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|        - |  222 | `	/* Precedence 15,left-associative */` |
|        - |  223 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  224 | `	/* Precedence 16,left-associative */` |
|        - |  225 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  226 | `	                      /* Null coalescing operator */` |
|        - |  227 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|        - |  228 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|        - |  229 | `	                      /* Ternary operator */` |
|        - |  230 | `	/* Precedence 17,left-associative */` |
|        - |  231 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  232 | `	                     /* Combined binary operators */` |
|        - |  233 | `	/* Precedence 18,right-associative */` |
|        - |  234 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  235 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  236 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  237 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  238 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  239 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  240 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  241 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|        - |  242 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  243 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  244 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  245 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  246 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  247 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|        - |  248 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|        - |  249 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|        - |  250 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|        - |  251 | `	 * in this file: keep one of the question marks escaped. */` |
|        - |  252 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|        - |  253 | `	/* Precedence 19,left-associative */` |
|        - |  254 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  255 | `	/* Precedence 20,left-associative */` |
|        - |  256 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  257 | `	/* Precedence 21,left-associative */` |
|        - |  258 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  259 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  260 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  261 | `};` |
|        - |  262 | `/* Function call operator need special handling */` |
|        - |  263 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  264 | `/*` |
|        - |  265 | ` * Check if the given token is a potential operator or not.` |
|        - |  266 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  267 | ` * look like an operator.` |
|        - |  268 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  269 | ` * Otherwise NULL.` |
|        - |  270 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  271 | ` * a binary minus or unary minus.]` |
|        - |  272 | ` */` |
|  1214978 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  274 | `{` |
|  1214983 |  275 | `	sxu32 n = 0;` |
|        - |  276 | `	sxi32 rc;` |
|        - |  277 | `	/* Do a linear lookup on the operators table */` |
| 21219912 |  278 | `	for(;;){` |
| 42439829 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  280 | `			break;` |
|        - |  281 | `		}` |
| 42439829 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3724633 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1862319 |  285 | `		}else{` |
| 38715201 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  287 | `		}` |
| 42439829 |  288 | `		if( rc == 0 ){` |
|  1219627 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1214527 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|        - |  293 | `			/* Handle ambiguity */` |
|     5105 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      341 |  296 | `				return &aOpTable[n];` |
|        - |  297 | `			}` |
|     4769 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      131 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      131 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      123 |  303 | `					return &aOpTable[n];` |
|        - |  304 | `				}` |
|        - |  305 |  |
|        4 |  306 | `			}` |
|     2322 |  307 | `		}` |
| 41224851 |  308 | `		++n; /* Next operator in the table */` |
|        5 |  309 | `	}` |
|        - |  310 | `	/* No such operator */` |
|      ! 0 |  311 | `	return 0;` |
|   607494 |  312 | `}` |
|        - |  313 | `/*` |
|        - |  314 | ` * Delimit a set of token stream.` |
|        - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  317 | ` */` |
|   743426 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  319 | `{` |
|   743431 |  320 | `	SyToken *pCur = pIn;` |
|   743431 |  321 | `	sxi32 iNest = 1;` |
|  4147843 |  322 | `	for(;;){` |
|  8295691 |  323 | `		if( pCur >= pEnd ){` |
|      471 |  324 | `			break;` |
|        - |  325 | `		}` |
|  8295225 |  326 | `		if( pCur->nType & nTokStart ){` |
|        - |  327 | `			/* Increment nesting level */` |
|   391097 |  328 | `			iNest++;` |
|  8099679 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  330 | `			/* Decrement nesting level */` |
|  1134057 |  331 | `			iNest--;` |
|  1134057 |  332 | `			if( iNest <= 0 ){` |
|   742965 |  333 | `				break;` |
|        - |  334 | `			}` |
|   195546 |  335 | `		}` |
|        - |  336 | `		/* Advance cursor */` |
|  7552265 |  337 | `		pCur++;` |
|        5 |  338 | `	}` |
|        - |  339 | `	/* Point to the end of the chunk */` |
|   743431 |  340 | `	*ppEnd = pCur;` |
|   743431 |  341 | `}` |
|        - |  342 | `/*` |
|        - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  344 | ` * Note on reserved keywords.` |
|        - |  345 | ` *  According to the PHP language reference manual:` |
|        - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  350 | ` */` |
|    24090 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  352 | `{` |
|    24090 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    23992 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  355 | `		){` |
|      167 |  356 | `			return TRUE;` |
|        - |  357 | `	}` |
|    23933 |  358 | `	if( bCheckFunc ){` |
|      356 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      349 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      331 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       49 |  362 | `				return TRUE;` |
|        - |  363 | `		}` |
|      156 |  364 | `	}` |
|        - |  365 | `	/* Not a language construct */` |
|    23889 |  366 | `	return FALSE;` |
|    12050 |  367 | `}` |
|        - |  368 | `/*` |
|        - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  373 | ` */` |
|  1024684 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  375 | `{` |
|        - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  377 | `	sxi32 i,rc;` |
|        - |  378 |  |
|  1024689 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  383 | `	}` |
|  1024689 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5554407 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4529757 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1277 |  388 | `			continue;` |
|        - |  389 | `		}` |
|  4528485 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   514881 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    24116 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   482443 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  397 | `						 */` |
|   482443 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   482443 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   482443 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   241219 |  401 | `					}` |
|   241219 |  402 | `			}` |
|   514881 |  403 | `			iParen++;` |
|  4271047 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   514881 |  405 | `			if( iParen <= 0 ){` |
|       16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       16 |  407 | `				if( rc != SXERR_ABORT ){` |
|       16 |  408 | `					rc = SXERR_SYNTAX;` |
|        6 |  409 | `				}` |
|       16 |  410 | `				return rc;` |
|        - |  411 | `			}` |
|   514869 |  412 | `			iParen--;` |
|  3756165 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    98419 |  414 | `			iSquare++;` |
|  3449526 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    98433 |  416 | `			if( iSquare <= 0 ){` |
|        8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        8 |  418 | `				if( rc != SXERR_ABORT ){` |
|        8 |  419 | `					rc = SXERR_SYNTAX;` |
|        3 |  420 | `				}` |
|        8 |  421 | `				return rc;` |
|        - |  422 | `			}` |
|    98427 |  423 | `			iSquare--;` |
|  3351102 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       20 |  425 | `			iBraces++;` |
|       20 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  427 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  428 | `				int iNest = 1;` |
|       11 |  429 | `				sxi32 j=i+1;` |
|        - |  430 | `				/*` |
|        - |  431 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  432 | `				 */` |
|       11 |  433 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  434 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  435 | `				pOp = aOpTable;` |
|       11 |  436 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       61 |  437 | `				while( pOp < pEnd ){` |
|       61 |  438 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  439 | `						break;` |
|        - |  440 | `					}` |
|       51 |  441 | `					pOp++;` |
|        1 |  442 | `				}` |
|       11 |  443 | `				if( pOp >= pEnd ){` |
|      ! 0 |  444 | `					pOp = 0;` |
|      ! 0 |  445 | `				}` |
|       11 |  446 | `				if( pOp ){` |
|       11 |  447 | `					apNode[i]->pOp = pOp;` |
|       11 |  448 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  449 | `				}` |
|       11 |  450 | `				iBraces--;` |
|       11 |  451 | `				iSquare++;` |
|       21 |  452 | `				while( j < nNode ){` |
|       21 |  453 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  454 | `						/* Increment nesting level */` |
|      ! 0 |  455 | `						iNest++;` |
|       21 |  456 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  457 | `						/* Decrement nesting level */` |
|       11 |  458 | `						iNest--;` |
|       11 |  459 | `						if( iNest < 1 ){` |
|       11 |  460 | `							break;` |
|        - |  461 | `						}` |
|      ! 0 |  462 | `					}` |
|       11 |  463 | `					j++;` |
|        1 |  464 | `				}` |
|       11 |  465 | `				if( j < nNode ){` |
|       11 |  466 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  467 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  468 | `				}` |
|        - |  469 |  |
|        7 |  470 | `			}` |
|  3301882 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       24 |  472 | `			if( iBraces <= 0 ){` |
|       15 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       15 |  474 | `				if( rc != SXERR_ABORT ){` |
|       15 |  475 | `					rc = SXERR_SYNTAX;` |
|        6 |  476 | `				}` |
|       15 |  477 | `				return rc;` |
|        - |  478 | `			}` |
|       10 |  479 | `			iBraces--;` |
|  3301857 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2907 |  481 | `			if( iQuesty > 0 ){` |
|     2653 |  482 | `				iQuesty--;` |
|     1583 |  483 | `			}else if( iParen <= 0 ){` |
|        - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        6 |  488 | `				if( rc != SXERR_ABORT ){` |
|        6 |  489 | `					rc = SXERR_SYNTAX;` |
|        2 |  490 | `				}` |
|        6 |  491 | `				return rc;` |
|        5 |  492 | `			}` |
|  3300400 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   924789 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   924789 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2655 |  496 | `				iQuesty++;` |
|   923464 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      401 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|        9 |  499 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|        9 |  500 | `					sxu32 n = 0;` |
|        9 |  501 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        5 |  502 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        2 |  503 | `					}` |
|        - |  504 | `					/*` |
|        - |  505 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  506 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  507 | `					 */` |
|      213 |  508 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      205 |  509 | `						++n;` |
|        1 |  510 | `					}` |
|        9 |  511 | `					pOp = &aOpTable[n];` |
|        - |  512 | `					/* Mark as binary '+' or '-',not an unary */` |
|        9 |  513 | `					apNode[i]->pOp = pOp;` |
|        9 |  514 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        4 |  515 | `				}` |
|      198 |  516 | `			}` |
|   462392 |  517 | `		}` |
|  2264228 |  518 | `	}` |
|  1024655 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       20 |  520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       20 |  521 | `		if( rc != SXERR_ABORT ){` |
|       20 |  522 | `			rc = SXERR_SYNTAX;` |
|        8 |  523 | `		}` |
|       20 |  524 | `		return rc;` |
|        - |  525 | `	}` |
|  1024639 |  526 | `	return SXRET_OK;` |
|   512347 |  527 | `}` |
|        - |  528 | `/*` |
|        - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  531 | ` */` |
|   849314 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  533 | `{` |
|   849319 |  534 | `	SyToken *pIn = *ppCur;` |
|        - |  535 | `	/* Jump the first literal seen */` |
|   849319 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   845441 |  537 | `		pIn++;` |
|   422718 |  538 | `	}` |
|   426623 |  539 | `	for(;;){` |
|   853251 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|     3937 |  541 | `			pIn++;` |
|     3937 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     3935 |  543 | `				pIn++;` |
|     1965 |  544 | `			}` |
|     1971 |  545 | `		}else{` |
|   424662 |  546 | `			break;` |
|        - |  547 | `		}` |
|        5 |  548 | `	}` |
|        - |  549 | `	/* Synchronize pointers */` |
|   849319 |  550 | `	*ppCur = pIn;` |
|   849319 |  551 | `}` |
|        - |  552 | `/*` |
|        - |  553 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  554 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  555 | ` * Note on annonymous functions.` |
|        - |  556 | ` *  According to the PHP language reference manual:` |
|        - |  557 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  558 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  559 | ` *  parameters, but they have many other uses.` |
|        - |  560 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  561 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  562 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  563 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  564 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  565 | ` *` |
|        - |  566 | ` * Some example:` |
|        - |  567 | ` *  $greet = function($name)` |
|        - |  568 | ` * {` |
|        - |  569 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  570 | ` * };` |
|        - |  571 | ` *  $greet('World');` |
|        - |  572 | ` *  $greet('PHP');` |
|        - |  573 | ` *` |
|        - |  574 | ` * $double = function($a) {` |
|        - |  575 | ` *   return $a * 2;` |
|        - |  576 | ` * };` |
|        - |  577 | ` * // This is our range of numbers` |
|        - |  578 | ` * $numbers = range(1, 5);` |
|        - |  579 | ` * // Use the Annonymous function as a callback here to` |
|        - |  580 | ` * // double the size of each element in our` |
|        - |  581 | ` * // range` |
|        - |  582 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  583 | ` * print implode(' ', $new_numbers);` |
|        - |  584 | ` */` |
|        - |  585 | `/*` |
|        - |  586 | ` * Skip an optional return-type declaration at *ppIn:` |
|        - |  587 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|        - |  588 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|        - |  589 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|        - |  590 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|        - |  591 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|        - |  592 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|        - |  593 | ` * authoritative type parse, so this must accept every shape it does` |
|        - |  594 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|        - |  595 | ` */` |
|      544 |  596 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|        5 |  597 | `{` |
|      549 |  598 | `	SyToken *pIn = *ppIn;` |
|      549 |  599 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|       25 |  600 | `		pIn++; /* Skip ':' */` |
|       11 |  601 | `		for(;;){` |
|        - |  602 | `			/* Optional '?' nullable prefix */` |
|       29 |  603 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        6 |  604 | `				pIn++;` |
|        2 |  605 | `			}` |
|       29 |  606 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - |  607 | `				/* Parenthesized DNF group '(A&B)' */` |
|      ! 0 |  608 | `				pIn++;` |
|      ! 0 |  609 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      ! 0 |  610 | `				if( pIn < pEnd ){` |
|      ! 0 |  611 | `					pIn++; /* ')' */` |
|      ! 0 |  612 | `				}` |
|       26 |  613 | `			}else if( pIn < pEnd` |
|       29 |  614 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|        - |  615 | `				/* ['\']Name('\'Name)* */` |
|       29 |  616 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|       29 |  617 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       29 |  618 | `					pIn++;` |
|       29 |  619 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  620 | `						pIn += 2;` |
|      ! 0 |  621 | `					}` |
|       13 |  622 | `				}` |
|       16 |  623 | `			}else{` |
|        - |  624 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|      ! 0 |  625 | `				break;` |
|        - |  626 | `			}` |
|        - |  627 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|       26 |  628 | `			if( pIn < pEnd` |
|       29 |  629 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|       26 |  630 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|        5 |  631 | `				pIn++;` |
|        5 |  632 | `				continue;` |
|        - |  633 | `			}` |
|       25 |  634 | `			break;` |
|      ! 0 |  635 | `		}` |
|       11 |  636 | `	}` |
|      549 |  637 | `	*ppIn = pIn;` |
|      549 |  638 | `}` |
|      346 |  639 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  640 | `{` |
|      351 |  641 | `	SyToken *pIn = *ppCur;` |
|        - |  642 | `	sxu32 nLine;` |
|        - |  643 | `	sxi32 rc;` |
|        - |  644 | `	/* Jump the 'function' keyword */` |
|      351 |  645 | `	nLine = pIn->nLine;` |
|      351 |  646 | `	pIn++;` |
|      351 |  647 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  648 | `		pIn++;` |
|        1 |  649 | `	}` |
|      351 |  650 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  651 | `		/* Syntax error */` |
|        6 |  652 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  653 | `		if( rc != SXERR_ABORT ){` |
|        6 |  654 | `			rc = SXERR_SYNTAX;` |
|        2 |  655 | `		}` |
|        6 |  656 | `		goto Synchronize;` |
|        - |  657 | `	}` |
|      347 |  658 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      347 |  659 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      347 |  660 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  661 | `		/* Syntax error */` |
|        6 |  662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  663 | `		if( rc != SXERR_ABORT ){` |
|        6 |  664 | `			rc = SXERR_SYNTAX;` |
|        2 |  665 | `		}` |
|        6 |  666 | `		goto Synchronize;` |
|        - |  667 | `	}` |
|      343 |  668 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  669 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      343 |  670 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      343 |  671 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       45 |  672 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  673 | `		/* Check if we are dealing with a closure */` |
|       45 |  674 | `		if( nKey == PH7_TKWRD_USE ){` |
|       37 |  675 | `			pIn++; /* Jump the 'use' keyword */` |
|       37 |  676 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  677 | `				/* Syntax error */` |
|        6 |  678 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  679 | `				if( rc != SXERR_ABORT ){` |
|        6 |  680 | `					rc = SXERR_SYNTAX;` |
|        2 |  681 | `				}` |
|        6 |  682 | `				goto Synchronize;` |
|        - |  683 | `			}` |
|       33 |  684 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       33 |  685 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       33 |  686 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  687 | `				/* Syntax error */` |
|        6 |  688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  689 | `				if( rc != SXERR_ABORT ){` |
|        6 |  690 | `					rc = SXERR_SYNTAX;` |
|        2 |  691 | `				}` |
|        6 |  692 | `				goto Synchronize;` |
|        - |  693 | `			}` |
|       29 |  694 | `			pIn++;` |
|        - |  695 | `			/* php 7.1+: the return type may also follow the use clause —` |
|        - |  696 | ``			 * `function (...) use (...) : int {` */`` |
|       29 |  697 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       17 |  698 | `		}else{` |
|        - |  699 | `			/* Syntax error */` |
|       11 |  700 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|       11 |  701 | `			if( rc != SXERR_ABORT ){` |
|       11 |  702 | `				rc = SXERR_SYNTAX;` |
|        4 |  703 | `			}` |
|       11 |  704 | `			goto Synchronize;` |
|        - |  705 | `		}` |
|       12 |  706 | `	}` |
|        - |  707 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|        - |  708 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|        - |  709 | `	 * the type), and pEnd is one past the last token. */` |
|      327 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      327 |  711 | `		pIn++; /* Jump the leading curly '{' */` |
|      327 |  712 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      327 |  713 | `		if( pIn < pEnd ){` |
|      327 |  714 | `			pIn++;` |
|      161 |  715 | `		}` |
|      166 |  716 | `	}else{` |
|        - |  717 | `		/* Syntax error */` |
|      ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  719 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  720 | `			return SXERR_ABORT;` |
|        - |  721 | `		}` |
|        - |  722 | `	}` |
|      327 |  723 | `	rc = SXRET_OK;` |
|      173 |  724 | `Synchronize:` |
|        - |  725 | `	/* Synchronize pointers */` |
|      351 |  726 | `	*ppCur = pIn;` |
|      351 |  727 | `	return rc;` |
|      178 |  728 | `}` |
|        - |  729 | `/*` |
|        - |  730 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|        - |  731 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|        - |  732 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|        - |  733 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|        - |  734 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|        - |  735 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|        - |  736 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|        - |  737 | ` */` |
|       26 |  738 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  739 | `{` |
|       30 |  740 | `	SyToken *pIn = *ppCur;` |
|       30 |  741 | `	sxu32 nLine = pIn->nLine;` |
|        - |  742 | `	sxi32 rc;` |
|       30 |  743 | `	pIn++; /* Jump the 'class' keyword */` |
|        - |  744 | `	/* Optional constructor argument list */` |
|       30 |  745 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  746 | `		pIn++; /* Jump '(' */` |
|        7 |  747 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        7 |  748 | `		if( pIn < pEnd ){` |
|        7 |  749 | `			pIn++; /* Jump ')' */` |
|        3 |  750 | `		}` |
|        3 |  751 | `	}` |
|        - |  752 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|        - |  753 | `	 * (no braces appear between ')' and the class body). */` |
|       58 |  754 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       32 |  755 | `		pIn++;` |
|        4 |  756 | `	}` |
|       30 |  757 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|        - |  758 | `		/* Syntax error: missing class body */` |
|      ! 0 |  759 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  760 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|      ! 0 |  761 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  762 | `			rc = SXERR_SYNTAX;` |
|      ! 0 |  763 | `		}` |
|      ! 0 |  764 | `		*ppCur = pIn;` |
|      ! 0 |  765 | `		return rc;` |
|        - |  766 | `	}` |
|       30 |  767 | `	pIn++; /* Jump the leading '{' */` |
|       30 |  768 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       30 |  769 | `	if( pIn < pEnd ){` |
|       30 |  770 | `		pIn++; /* Jump the trailing '}' */` |
|       13 |  771 | `	}` |
|       30 |  772 | `	*ppCur = pIn;` |
|       30 |  773 | `	return SXRET_OK;` |
|       17 |  774 | `}` |
|        - |  775 | `/*` |
|        - |  776 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  777 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  778 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  779 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  780 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  781 | ` */` |
|      182 |  782 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  783 | `{` |
|      186 |  784 | `	SyToken *pIn = *ppCur;` |
|        - |  785 | `	sxu32 nLine;` |
|        - |  786 | `	sxi32 rc;` |
|        - |  787 | `	int iNest;` |
|      186 |  788 | `	nLine = pIn->nLine;` |
|        - |  789 | `	/* Optional 'static' prefix */` |
|      182 |  790 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      186 |  791 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  792 | `		pIn++;` |
|        1 |  793 | `	}` |
|        - |  794 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      182 |  795 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      186 |  796 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  797 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  798 | `		goto Synchronize;` |
|        - |  799 | `	}` |
|      186 |  800 | `	pIn++; /* Jump 'fn' */` |
|       91 |  801 | `	SXUNUSED(nLine);` |
|       91 |  802 | `	SXUNUSED(pGen);` |
|        - |  803 | `	/* Optional '&' for return-by-reference */` |
|      186 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  805 | `		pIn++;` |
|      ! 0 |  806 | `	}` |
|        - |  807 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  808 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  809 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  810 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      186 |  811 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      183 |  812 | `		pIn++; /* '(' */` |
|      183 |  813 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      183 |  814 | `		if( pIn < pEnd ){` |
|      181 |  815 | `			pIn++; /* ')' */` |
|       89 |  816 | `		}` |
|       90 |  817 | `	}` |
|        - |  818 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      186 |  819 | `	ExprSkipReturnType(&pIn,pEnd);` |
|        - |  820 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      186 |  821 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      179 |  822 | `		pIn++;` |
|       88 |  823 | `	}` |
|        - |  824 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      186 |  825 | `	iNest = 0;` |
|     1100 |  826 | `	while( pIn < pEnd ){` |
|      996 |  827 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  828 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       79 |  829 | `			break;` |
|        - |  830 | `		}` |
|      918 |  831 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       65 |  832 | `			iNest++;` |
|      887 |  833 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       65 |  834 | `			iNest--;` |
|       31 |  835 | `		}` |
|      918 |  836 | `		pIn++;` |
|        4 |  837 | `	}` |
|      186 |  838 | `	rc = SXRET_OK;` |
|       91 |  839 | `Synchronize:` |
|      186 |  840 | `	*ppCur = pIn;` |
|      186 |  841 | `	return rc;` |
|        4 |  842 | `}` |
|        - |  843 | `/*` |
|        - |  844 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  845 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  846 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  847 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  848 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  849 | ` */` |
|       70 |  850 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  851 | `{` |
|       75 |  852 | `	SyToken *pIn = *ppCur;` |
|        - |  853 | `	sxi32 rc;` |
|       35 |  854 | `	SXUNUSED(pGen);` |
|        - |  855 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  856 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       75 |  857 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  858 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  859 | `		goto Synchronize;` |
|        - |  860 | `	}` |
|       75 |  861 | `	pIn++; /* Jump 'match' */` |
|        - |  862 | `	/* Optional '(' subject ')' */` |
|       75 |  863 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       75 |  864 | `		pIn++;` |
|       75 |  865 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       75 |  866 | `		if( pIn < pEnd ){` |
|       75 |  867 | `			pIn++; /* ')' */` |
|       35 |  868 | `		}` |
|       35 |  869 | `	}` |
|        - |  870 | `	/* Optional '{' arms '}' */` |
|       75 |  871 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       75 |  872 | `		pIn++;` |
|       75 |  873 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       75 |  874 | `		if( pIn < pEnd ){` |
|       75 |  875 | `			pIn++; /* '}' */` |
|       35 |  876 | `		}` |
|       35 |  877 | `	}` |
|       75 |  878 | `	rc = SXRET_OK;` |
|       35 |  879 | `Synchronize:` |
|       75 |  880 | `	*ppCur = pIn;` |
|       75 |  881 | `	return rc;` |
|        5 |  882 | `}` |
|        - |  883 | `/*` |
|        - |  884 | ` * Extract a single expression node from the input.` |
|        - |  885 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  886 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  887 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  888 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  889 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  890 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  891 | ` */` |
|  4533820 |  892 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|        5 |  893 | `{` |
|        - |  894 | `	ph7_expr_node *pNode;` |
|        - |  895 | `	SyToken *pCur;` |
|        - |  896 | `	sxi32 rc;` |
|        - |  897 | `	/* Allocate a new node */` |
|  4533825 |  898 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4533825 |  899 | `	if( pNode == 0 ){` |
|        - |  900 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  901 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  902 | `		 */` |
|      ! 0 |  903 | `		return SXERR_MEM;` |
|        - |  904 | `	}` |
|        - |  905 | `	/* Zero the structure */` |
|  4533825 |  906 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4533825 |  907 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  908 | `	/* Point to the head of the token stream */` |
|  4533825 |  909 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  910 | `	/* Start collecting tokens */` |
|  4533825 |  911 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|     3999 |  912 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|        - |  913 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|        - |  914 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|        - |  915 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|        - |  916 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       77 |  917 | `			pNode->pEnd = pCur;` |
|       77 |  918 | `			pCur++;` |
|       77 |  919 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       77 |  920 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       77 |  921 | `			pGen->pIn = pCur;` |
|       77 |  922 | `			*ppNode = pNode;` |
|       77 |  923 | `			return SXRET_OK;` |
|        - |  924 | `		}` |
|        - |  925 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  926 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|     3923 |  927 | `		pCur++;` |
|     3923 |  928 | `		pGen->pIn = pCur;` |
|     3923 |  929 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|     3923 |  930 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|     3923 |  931 | `		if( rc == SXRET_OK && *ppNode ){` |
|     3923 |  932 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|     1959 |  933 | `		}` |
|     3923 |  934 | `		return rc;` |
|        - |  935 | `	}` |
|  4529831 |  936 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  937 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  938 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  939 | `		 */` |
|     1279 |  940 | `		pCur++; /* Skip the opening '[' */` |
|     1279 |  941 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1279 |  942 | `		if( pCur < pGen->pEnd ){` |
|     1279 |  943 | `			pCur++; /* Skip past the closing ']' */` |
|      642 |  944 | `		}else{` |
|      ! 0 |  945 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  946 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  947 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  948 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  949 | `			}` |
|      ! 0 |  950 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  951 | `			return rc;` |
|        - |  952 | `		}` |
|        - |  953 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  954 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  955 | `		 */` |
|     1366 |  956 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      178 |  957 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      178 |  958 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       54 |  959 | `				pNode->xCode = PH7_CompileShortList;` |
|       29 |  960 | `			}else{` |
|      125 |  961 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  962 | `			}` |
|       91 |  963 | `		}else{` |
|     1105 |  964 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  965 | `		}` |
|  4529194 |  966 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  967 | `		/* Point to the instance that describe this operator */` |
|  1023237 |  968 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  969 | `		/* Advance the stream cursor */` |
|  1023237 |  970 | `		pCur++;` |
|  4016941 |  971 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  972 | `		/* Isolate variable */` |
|  2449205 |  973 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1224611 |  974 | `			pCur++; /* Variable variable */` |
|        5 |  975 | `		}` |
|  1224599 |  976 | `		if( pCur < pGen->pEnd ){` |
|  1224599 |  977 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  978 | `				/* Variable name */` |
|  1224571 |  979 | `				pCur++;` |
|   612315 |  980 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       24 |  981 | `				pCur++;` |
|        - |  982 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       24 |  983 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       24 |  984 | `				if( pCur < pGen->pEnd ){` |
|       19 |  985 | `					pCur++;` |
|       11 |  986 | `				}else{` |
|        6 |  987 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        6 |  988 | `					if( rc != SXERR_ABORT ){` |
|        6 |  989 | `						rc = SXERR_SYNTAX;` |
|        2 |  990 | `					}` |
|        6 |  991 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        6 |  992 | `					return rc;` |
|        - |  993 | `				}` |
|        8 |  994 | `			}` |
|   612295 |  995 | `		}` |
|  1224595 |  996 | `		pNode->xCode = PH7_CompileVariable;` |
|  2893026 |  997 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    57337 |  998 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    57337 |  999 | `		 if( bAfterMemberOp ){` |
|        - | 1000 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|        - | 1001 | `			  * method/property NAME, not a language construct — PHP allows any` |
|        - | 1002 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|        - | 1003 | `			  * as a plain literal like an ordinary identifier member name. */` |
|      185 | 1004 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      185 | 1005 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    57247 | 1006 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - | 1007 | `			 /* List/Array node */` |
|    32505 | 1008 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1009 | `				 /* Assume a literal */` |
|      ! 0 | 1010 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1011 | `				 pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1012 | `			 }else{` |
|    32505 | 1013 | `				 pCur += 2;` |
|        - | 1014 | `				 /* Collect array/list tokens */` |
|    32505 | 1015 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    32505 | 1016 | `				 if( pCur < pGen->pEnd ){` |
|    32503 | 1017 | `					 pCur++;` |
|    16254 | 1018 | `				 }else{` |
|        - | 1019 | `					 /* Syntax error */` |
|        4 | 1020 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 | 1021 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 | 1022 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1023 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1024 | `					 }` |
|        3 | 1025 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1026 | `					 return rc;` |
|        - | 1027 | `				 }` |
|    32503 | 1028 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    32503 | 1029 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       37 | 1030 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       37 | 1031 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - | 1032 | `						 /* Syntax error */` |
|        3 | 1033 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 | 1034 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1035 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1036 | `						 }` |
|        3 | 1037 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1038 | `						 return rc;` |
|        - | 1039 | `					 }` |
|       15 | 1040 | `				 }` |
|        5 | 1041 | `			 }` |
|    40905 | 1042 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1043 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      333 | 1044 | `			 pCur++; /* Skip 'yield' keyword */` |
|      333 | 1045 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1046 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1047 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      333 | 1048 | `			 pNode->xCode = PH7_CompileYield;` |
|    24493 | 1049 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1050 | `			 /* Annonymous function */` |
|      351 | 1051 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1052 | `				 /* Assume a literal */` |
|      ! 0 | 1053 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1054 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1055 | `			 }else{` |
|        - | 1056 | `				 /* Assemble annonymous functions body */` |
|      351 | 1057 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      351 | 1058 | `				 if( rc != SXRET_OK ){` |
|       28 | 1059 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1060 | `					 return rc;` |
|        - | 1061 | `				 }` |
|      327 | 1062 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1063 | `			  }` |
|    24144 | 1064 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       39 | 1065 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       22 | 1066 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       12 | 1067 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        9 | 1068 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1069 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1070 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1071 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1072 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       30 | 1073 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       30 | 1074 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1075 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1076 | `				 return rc;` |
|        - | 1077 | `			 }` |
|       30 | 1078 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    23969 | 1079 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    23868 | 1080 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       30 | 1081 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       15 | 1082 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1083 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      186 | 1084 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      186 | 1085 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1086 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1087 | `				 return rc;` |
|        - | 1088 | `			 }` |
|      186 | 1089 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    23866 | 1090 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1091 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1092 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1093 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1094 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1095 | `				 return rc;` |
|        - | 1096 | `			 }` |
|       75 | 1097 | `			 pNode->xCode = PH7_CompileMatch;` |
|    23740 | 1098 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1099 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1100 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1101 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1102 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1103 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1104 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1105 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1106 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    23687 | 1107 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1108 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       93 | 1109 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       93 | 1110 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       49 | 1111 | `		 }else{` |
|        - | 1112 | `			 /* Assume a literal */` |
|    23581 | 1113 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    23581 | 1114 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1115 | `		 }` |
|  2252051 | 1116 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1117 | `		 /* Constants,function name,namespace path,class name... */` |
|   825563 | 1118 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   825563 | 1119 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   412784 | 1120 | `	 }else{` |
|  1397841 | 1121 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1122 | `			 /* Point to the code generator routine */` |
|   266699 | 1123 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   266699 | 1124 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1125 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1126 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1127 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1128 | `				 }` |
|        3 | 1129 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1130 | `				 return rc;` |
|        - | 1131 | `			 }` |
|   133346 | 1132 | `		 }` |
|        - | 1133 | `		/* Advance the stream cursor */` |
|  1397839 | 1134 | `		pCur++;` |
|        - | 1135 | `	 }` |
|        - | 1136 | `	/* Point to the end of the token stream */` |
|  4529797 | 1137 | `	pNode->pEnd = pCur;` |
|        - | 1138 | `	/* Save the node for later processing */` |
|  4529797 | 1139 | `	*ppNode = pNode;` |
|        - | 1140 | `	/* Synchronize cursors */` |
|  4529797 | 1141 | `	pGen->pIn = pCur;` |
|  4529797 | 1142 | `	return SXRET_OK;` |
|  2266915 | 1143 | `}` |
|        - | 1144 | `/*` |
|        - | 1145 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1146 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1147 | ` * level is zero.` |
|        - | 1148 | ` */` |
|    99468 | 1149 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1150 | `{` |
|    99473 | 1151 | `	SyToken *pCur = pStart;` |
|    99473 | 1152 | `	sxi32 iNest = 0;` |
|    99473 | 1153 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1154 | `		/* Last expression */` |
|    51575 | 1155 | `		return SXERR_EOF;` |
|        - | 1156 | `	}` |
|   196613 | 1157 | `	while( pCur < pEnd ){` |
|   179395 | 1158 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    30685 | 1159 | `			break;` |
|        - | 1160 | `		}` |
|   148715 | 1161 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    10361 | 1162 | `			iNest++;` |
|   143537 | 1163 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    10363 | 1164 | `			iNest--;` |
|     5179 | 1165 | `		}` |
|   148715 | 1166 | `		pCur++;` |
|        5 | 1167 | `	}` |
|    47903 | 1168 | `	*ppNext = pCur;` |
|    47903 | 1169 | `	return SXRET_OK;` |
|    49739 | 1170 | `}` |
|        - | 1171 | `/*` |
|        - | 1172 | ` * Free an expression tree.` |
|        - | 1173 | ` */` |
|  3870942 | 1174 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1175 | `{` |
|  3870947 | 1176 | `	if( pNode->pLeft ){` |
|        - | 1177 | `		/* Release the left tree */` |
|  1430015 | 1178 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   715005 | 1179 | `	}` |
|  3870947 | 1180 | `	if( pNode->pRight ){` |
|        - | 1181 | `		/* Release the right tree */` |
|   772641 | 1182 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   386318 | 1183 | `	}` |
|  3870947 | 1184 | `	if( pNode->pCond ){` |
|        - | 1185 | `		/* Release the conditional tree used by the ternary operator */` |
|     2651 | 1186 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1323 | 1187 | `	}` |
|  3870947 | 1188 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1189 | `		ph7_expr_node **apArg;` |
|        - | 1190 | `		sxu32 n;` |
|        - | 1191 | `		/* Release node arguments */` |
|   499819 | 1192 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  1075049 | 1193 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   575235 | 1194 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   287620 | 1195 | `		}` |
|   499819 | 1196 | `		SySetRelease(&pNode->aNodeArgs);` |
|   249907 | 1197 | `	}` |
|        - | 1198 | `	/* Finally,release this node */` |
|  3870947 | 1199 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3870947 | 1200 | `}` |
|        - | 1201 | `/*` |
|        - | 1202 | ` * Free an expression tree.` |
|        - | 1203 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1204 | ` */` |
|  1024718 | 1205 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1206 | `{` |
|        - | 1207 | `	ph7_expr_node **apNode;` |
|        - | 1208 | `	sxu32 n;` |
|  1024723 | 1209 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5554591 | 1210 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4529873 | 1211 | `		if( apNode[n] ){` |
|  1025057 | 1212 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   512526 | 1213 | `		}` |
|  2264939 | 1214 | `	}` |
|  1024723 | 1215 | `	return SXRET_OK;` |
|        5 | 1216 | `}` |
|        - | 1217 | `/*` |
|        - | 1218 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1219 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1220 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1221 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1222 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1223 | ` */` |
|  1398638 | 1224 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1225 | `{` |
|  1398643 | 1226 | `	if( pNode == 0 ){` |
|   862863 | 1227 | `		return 0;` |
|        - | 1228 | `	}` |
|   535785 | 1229 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1230 | `		return 1;` |
|        - | 1231 | `	}` |
|   535773 | 1232 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1233 | `		return 1;` |
|        - | 1234 | `	}` |
|   535769 | 1235 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1236 | `		return 1;` |
|        - | 1237 | `	}` |
|   535769 | 1238 | `	return 0;` |
|   699324 | 1239 | `}` |
|        - | 1240 | `/*` |
|        - | 1241 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1242 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1243 | ` */` |
|   320428 | 1244 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1245 | `{` |
|        - | 1246 | `	sxi32 iExprOp;` |
|   320433 | 1247 | `	if( pNode->pOp == 0 ){` |
|   196843 | 1248 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1249 | `	}` |
|   123595 | 1250 | `	iExprOp = pNode->pOp->iOp;` |
|   123595 | 1251 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    84795 | 1252 | `			return TRUE;` |
|        - | 1253 | `	}` |
|    38805 | 1254 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    38799 | 1255 | `		if( pNode->pLeft->pOp ) {` |
|       68 | 1256 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       31 | 1257 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1258 | `				return FALSE;` |
|        5 | 1259 | `			}` |
|    38765 | 1260 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1261 | `			return FALSE;` |
|        - | 1262 | `		}` |
|    38799 | 1263 | `		return TRUE;` |
|        - | 1264 | `	}` |
|        8 | 1265 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        8 | 1266 | `		return TRUE;` |
|        - | 1267 | `	}` |
|        - | 1268 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1269 | `	return FALSE;` |
|   160219 | 1270 | `}` |
|        - | 1271 | `/* Forward declaration */` |
|        - | 1272 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1273 | `/* Macro to check if the given node is a terminal.` |
|        - | 1274 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1275 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1276 | ` * linked ternary/elvis node). */` |
|        - | 1277 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1278 | `/*` |
|        - | 1279 | ` * Buid an expression tree for each given function argument.` |
|        - | 1280 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1281 | ` */` |
|   420658 | 1282 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1283 | `{` |
|        - | 1284 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1285 | `	sxi32 rc;` |
|        - | 1286 | `	/* Process function arguments from left to right */` |
|   420663 | 1287 | `	iCur = 0;` |
|   458354 | 1288 | `	for(;;){` |
|   916713 | 1289 | `		if( iCur >= nToken ){` |
|        - | 1290 | `			/* No more arguments to process */` |
|   420637 | 1291 | `			break;` |
|        - | 1292 | `		}` |
|   496081 | 1293 | `		iNode = iCur;` |
|   496081 | 1294 | `		iNest = 0;` |
|  1227949 | 1295 | `		while( iCur < nToken ){` |
|   807315 | 1296 | `			if( apNode[iCur] ){` |
|   791995 | 1297 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    37726 | 1298 | `					break;` |
|   716548 | 1299 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   378582 | 1300 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    40395 | 1301 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1302 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1303 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1304 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1305 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1306 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1307 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    40169 | 1308 | `					iNest++;` |
|   696471 | 1309 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    40169 | 1310 | `					iNest--;` |
|    20082 | 1311 | `				}` |
|   358274 | 1312 | `			}` |
|   731873 | 1313 | `			iCur++;` |
|        5 | 1314 | `		}` |
|   496081 | 1315 | `		if( iCur > iNode ){` |
|   496075 | 1316 | `			SyString sArgName = {0, 0};` |
|        - | 1317 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1318 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1319 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   496070 | 1320 | `			if( (iCur - iNode) >= 2` |
|   274414 | 1321 | `				&& apNode[iNode]` |
|    52758 | 1322 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    31018 | 1323 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     9136 | 1324 | `				&& apNode[iNode+1]` |
|     8999 | 1325 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1326 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      255 | 1327 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      255 | 1328 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      255 | 1329 | `				apNode[iNode] = 0;` |
|      255 | 1330 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      255 | 1331 | `				apNode[iNode+1] = 0;` |
|      255 | 1332 | `				iNode += 2;` |
|        - | 1333 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1334 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      255 | 1335 | `				if( iNode >= iCur ){` |
|        4 | 1336 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1337 | `						pOp->pStart->nLine,` |
|        - | 1338 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1339 | `						&sArgName);` |
|        3 | 1340 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1341 | `						rc = SXERR_SYNTAX;` |
|        1 | 1342 | `					}` |
|        3 | 1343 | `					return rc;` |
|        - | 1344 | `				}` |
|      124 | 1345 | `			}` |
|   496068 | 1346 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1347 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1348 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1349 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1350 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1351 | `					apNode[iNode] = 0;` |
|      ! 0 | 1352 | `			}` |
|   496073 | 1353 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   496073 | 1354 | `			if( apNode[iNode] ){` |
|   496073 | 1355 | `				if( sArgName.nByte > 0 ){` |
|      253 | 1356 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      253 | 1357 | `					apNode[iNode]->sArgName = sArgName;` |
|      124 | 1358 | `				}` |
|        - | 1359 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   496073 | 1360 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   248039 | 1361 | `			}else{` |
|        - | 1362 | `				/* No expression before comma */` |
|      ! 0 | 1363 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1364 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1365 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1366 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1367 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1368 | `				}` |
|      ! 0 | 1369 | `				return rc;` |
|        - | 1370 | `			}` |
|   248039 | 1371 | `		}else{` |
|        - | 1372 | `			/* Comma with no preceding argument */` |
|        9 | 1373 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        9 | 1374 | `			if( rc != SXERR_ABORT ){` |
|        9 | 1375 | `				rc = SXERR_SYNTAX;` |
|        3 | 1376 | `			}` |
|        9 | 1377 | `			return rc;` |
|        - | 1378 | `		}` |
|        - | 1379 | `		/* Jump trailing comma */` |
|   496073 | 1380 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    75441 | 1381 | `			iCur++;` |
|    75441 | 1382 | `			if( iCur >= nToken ){` |
|        - | 1383 | `				/* Trailing comma after last argument */` |
|       19 | 1384 | `				break;` |
|        - | 1385 | `			}` |
|    37709 | 1386 | `		}` |
|        5 | 1387 | `	}` |
|   420655 | 1388 | `	return SXRET_OK;` |
|   210334 | 1389 | `}` |
|        - | 1390 | ` /*` |
|        - | 1391 | `  * Create an expression tree from an array of tokens.` |
|        - | 1392 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1393 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1394 | `  */` |
|  1641698 | 1395 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1396 | ` {` |
|        - | 1397 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1398 | `	 ph7_expr_node *pNode;` |
|        - | 1399 | `	 sxi32 iCur;` |
|        - | 1400 | `	 sxi32 rc;` |
|  1641703 | 1401 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1402 | `		 /* TICKET 1433-17: self evaluating node */` |
|   768937 | 1403 | `		 return SXRET_OK;` |
|        - | 1404 | `	 }` |
|        - | 1405 | `	 /* Process expressions enclosed in parenthesis first */` |
|  5417879 | 1406 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1407 | `		 sxi32 iNest;` |
|        - | 1408 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1409 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1410 | `		  */` |
|  4545115 | 1411 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4512687 | 1412 | `			 continue;` |
|        - | 1413 | `		 }` |
|    32433 | 1414 | `		 iNest = 1;` |
|    32433 | 1415 | `		 iLeft = iCur;` |
|        - | 1416 | `		 /* Find the closing parenthesis */` |
|    32433 | 1417 | `		 iCur++;` |
|   216189 | 1418 | `		 while( iCur < nToken ){` |
|   216189 | 1419 | `			 if( apNode[iCur] ){` |
|   216189 | 1420 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1421 | `					 /* Decrement nesting level */` |
|    56327 | 1422 | `					 iNest--;` |
|    56327 | 1423 | `					 if( iNest <= 0 ){` |
|    32433 | 1424 | `						 break;` |
|        5 | 1425 | `					 }` |
|   171814 | 1426 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1427 | `					 /* Increment nesting level */` |
|    23899 | 1428 | `					 iNest++;` |
|    11947 | 1429 | `				 }` |
|    91878 | 1430 | `			 }` |
|   183761 | 1431 | `			 iCur++;` |
|        5 | 1432 | `		 }` |
|    32433 | 1433 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1434 | `			 sxi32 j;` |
|        - | 1435 | `			 /* Recurse and process this expression */` |
|    32433 | 1436 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    32433 | 1437 | `			 if( rc != SXRET_OK ){` |
|        3 | 1438 | `				 return rc;` |
|        - | 1439 | `			 }` |
|        - | 1440 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1441 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1442 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    32431 | 1443 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    32431 | 1444 | `				 if( apNode[j] ){` |
|    32431 | 1445 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    32431 | 1446 | `					 break;` |
|        - | 1447 | `				 }` |
|      ! 0 | 1448 | `			 }` |
|    16213 | 1449 | `		 }` |
|        - | 1450 | `		 /* Free the left and right nodes */` |
|    32431 | 1451 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    32431 | 1452 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    32431 | 1453 | `		 apNode[iLeft] = 0;` |
|    32431 | 1454 | `		 apNode[iCur] = 0;` |
|    16218 | 1455 | `	 }` |
|        - | 1456 | `	  /* Process expressions enclosed in braces */` |
|  5625685 | 1457 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1458 | `		 sxi32 iNest;` |
|        - | 1459 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1460 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1461 | `		  */` |
|  4761273 | 1462 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4761265 | 1463 | `			 continue;` |
|        - | 1464 | `		 }` |
|       10 | 1465 | `		 iNest = 1;` |
|       10 | 1466 | `		 iLeft = iCur;` |
|        - | 1467 | `		 /* Find the closing parenthesis */` |
|       10 | 1468 | `		 iCur++;` |
|       16 | 1469 | `		 while( iCur < nToken ){` |
|       16 | 1470 | `			 if( apNode[iCur] ){` |
|       16 | 1471 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1472 | `					 /* Decrement nesting level */` |
|       10 | 1473 | `					 iNest--;` |
|       10 | 1474 | `					 if( iNest <= 0 ){` |
|       10 | 1475 | `						 break;` |
|      ! 0 | 1476 | `					 }` |
|        7 | 1477 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1478 | `					 /* Increment nesting level */` |
|      ! 0 | 1479 | `					 iNest++;` |
|      ! 0 | 1480 | `				 }` |
|        3 | 1481 | `			 }` |
|        7 | 1482 | `			 iCur++;` |
|        1 | 1483 | `		 }` |
|       10 | 1484 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1485 | `			 /* Recurse and process this expression */` |
|        7 | 1486 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        7 | 1487 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1488 | `				 return rc;` |
|        - | 1489 | `			 }` |
|        3 | 1490 | `		 }` |
|        - | 1491 | `		 /* Free the left and right nodes */` |
|       10 | 1492 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       10 | 1493 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       10 | 1494 | `		 apNode[iLeft] = 0;` |
|       10 | 1495 | `		 apNode[iCur] = 0;` |
|        6 | 1496 | `	 }` |
|        - | 1497 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   864417 | 1498 | `	 iLeft = -1;` |
|  5625663 | 1499 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4761263 | 1500 | `		 if( apNode[iCur] == 0 ){` |
|  1875993 | 1501 | `			 continue;` |
|        - | 1502 | `		 }` |
|  2885275 | 1503 | `		 pNode = apNode[iCur];` |
|  2885275 | 1504 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   772287 | 1505 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1506 | `				 /* Collect function arguments */` |
|   482439 | 1507 | `				 sxi32 iPtr = 0;` |
|   482439 | 1508 | `				 sxi32 nFuncTok = 0;` |
|  1772185 | 1509 | `				 while( nFuncTok + iCur < nToken ){` |
|  1772185 | 1510 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1756865 | 1511 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   503001 | 1512 | `							 iPtr++;` |
|  1505367 | 1513 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   503001 | 1514 | `							 iPtr--;` |
|   503001 | 1515 | `							 if( iPtr <= 0 ){` |
|   482439 | 1516 | `								 break;` |
|        - | 1517 | `							 }` |
|    10281 | 1518 | `						 }` |
|   637213 | 1519 | `					 }` |
|  1289751 | 1520 | `					 nFuncTok++;` |
|        5 | 1521 | `				 }` |
|   482439 | 1522 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1523 | `					 /* Syntax error */` |
|      ! 0 | 1524 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1525 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1526 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1527 | `					 }` |
|      ! 0 | 1528 | `					 return rc;` |
|        - | 1529 | `				 }` |
|   482439 | 1530 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1531 | `					 /* Syntax error */` |
|      ! 0 | 1532 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1533 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1534 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1535 | `					 }` |
|      ! 0 | 1536 | `					 return rc;` |
|        - | 1537 | `				 }` |
|   482439 | 1538 | `				 if( nFuncTok > 1 ){` |
|        - | 1539 | `					 /* Process function arguments */` |
|   420663 | 1540 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   420663 | 1541 | `					 if( rc != SXRET_OK ){` |
|       11 | 1542 | `						 return rc;` |
|        - | 1543 | `					 }` |
|   210325 | 1544 | `				 }` |
|        - | 1545 | `				 /* Link the node to the tree */` |
|   482431 | 1546 | `				 pNode->pLeft = apNode[iLeft];` |
|   482431 | 1547 | `				 apNode[iLeft] = 0;` |
|  1772153 | 1548 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1289727 | 1549 | `					 apNode[iCur+iPtr] = 0;` |
|   644866 | 1550 | `				 }` |
|        - | 1551 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|        - | 1552 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|        - | 1553 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|        - | 1554 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|        - | 1555 | `				  * constructor call into that new-node NOW, before the postfix` |
|        - | 1556 | `				  * operators bind, and relocate the completed new-node onto this` |
|        - | 1557 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|        - | 1558 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|        - | 1559 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|        - | 1560 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|        - | 1561 | `				 {` |
|   482431 | 1562 | `					 sxi32 iNew = iLeft - 1;` |
|   484485 | 1563 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     2059 | 1564 | `						 iNew--;` |
|        5 | 1565 | `					 }` |
|   482426 | 1566 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   228994 | 1567 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   126750 | 1568 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    24511 | 1569 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    24511 | 1570 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    24511 | 1571 | `						 apNode[iNew] = 0;` |
|    24511 | 1572 | `						 pNode = apNode[iCur];` |
|    12258 | 1573 | `					 }` |
|        - | 1574 | `				 }` |
|   531066 | 1575 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1576 | `				 /* Subscripting */` |
|    98427 | 1577 | `				 sxi32 iArrTok = iCur + 1;` |
|    98427 | 1578 | `				 sxi32 iNest = 1;` |
|    98422 | 1579 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1580 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1581 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       14 | 1582 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    98422 | 1583 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|        - | 1584 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|        - | 1585 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|      217 | 1586 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|        - | 1587 | `						 /* Syntax error */` |
|      ! 0 | 1588 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1589 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1590 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1591 | `						 }` |
|      ! 0 | 1592 | `						 return rc;` |
|        - | 1593 | `				 }` |
|        - | 1594 | `				 /* Collect index tokens */` |
|   177725 | 1595 | `				 while( iArrTok < nToken ){` |
|   177725 | 1596 | `					 if( apNode[iArrTok] ){` |
|   177693 | 1597 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1598 | `							 /* Increment nesting level */` |
|      ! 0 | 1599 | `							 iNest++;` |
|   177693 | 1600 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1601 | `							 /* Decrement nesting level */` |
|    98427 | 1602 | `							 iNest--;` |
|    98427 | 1603 | `							 if( iNest <= 0 ){` |
|    98427 | 1604 | `								 break;` |
|        - | 1605 | `							 }` |
|      ! 0 | 1606 | `						 }` |
|    39633 | 1607 | `					 }` |
|    79303 | 1608 | `					 ++iArrTok;` |
|        5 | 1609 | `				 }` |
|    98427 | 1610 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1611 | `					 /* Recurse and process this expression */` |
|    79167 | 1612 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    79167 | 1613 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1614 | `						 return rc;` |
|        - | 1615 | `					 }` |
|        - | 1616 | `					 /* Link the node to it's index */` |
|    79167 | 1617 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    39581 | 1618 | `				 }` |
|        - | 1619 | `				 /* Link the node to the tree */` |
|    98427 | 1620 | `				 pNode->pLeft = apNode[iLeft];` |
|    98427 | 1621 | `				 pNode->pRight = 0;` |
|    98427 | 1622 | `				 apNode[iLeft] = 0;` |
|   276147 | 1623 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   177725 | 1624 | `					 apNode[iNest] = 0;` |
|    88865 | 1625 | `				 }` |
|    49216 | 1626 | `			 }else{` |
|        - | 1627 | `				 /* Member access operators [i.e: '->','::'] */` |
|   191431 | 1628 | `				  iRight = iCur + 1;` |
|   191437 | 1629 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        7 | 1630 | `					 iRight++;` |
|        1 | 1631 | `				 }` |
|   191431 | 1632 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1633 | `					 /* Syntax error */` |
|        5 | 1634 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1635 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1636 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1637 | `					 }` |
|        5 | 1638 | `					 return rc;` |
|        - | 1639 | `				 }` |
|        - | 1640 | `				 /* Link the node to the tree */` |
|   191427 | 1641 | `				 pNode->pLeft = apNode[iLeft];` |
|   191422 | 1642 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   191239 | 1643 | `					 && pNode->pLeft->pOp == 0 &&` |
|   190920 | 1644 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1645 | `						 /* Syntax error */` |
|      ! 0 | 1646 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1647 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1648 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1649 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1650 | `						 }` |
|      ! 0 | 1651 | `						 return rc;` |
|        - | 1652 | `				 }` |
|   191427 | 1653 | `				 pNode->pRight = apNode[iRight];` |
|   191427 | 1654 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1655 | `			 }` |
|   386135 | 1656 | `		 }` |
|  2885263 | 1657 | `		 iLeft = iCur;` |
|  1442634 | 1658 | `	 }` |
|        - | 1659 | `	 /* Handle left associative (new, clone) operators */` |
|  5625631 | 1660 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4761231 | 1661 | `		 if( apNode[iCur] == 0 ){` |
|  2673021 | 1662 | `			 continue;` |
|        - | 1663 | `		 }` |
|  2088215 | 1664 | `		 pNode = apNode[iCur];` |
|  2088215 | 1665 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1666 | `			 SyToken *pToken;` |
|        - | 1667 | `			 /* Get the left node */` |
|      257 | 1668 | `			 iLeft = iCur + 1;` |
|      259 | 1669 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|        3 | 1670 | `				 iLeft++;` |
|        1 | 1671 | `			 }` |
|      257 | 1672 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1673 | `				  /* Syntax error */` |
|      ! 0 | 1674 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1675 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1676 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1677 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1678 | `				 }` |
|      ! 0 | 1679 | `				 return rc;` |
|        - | 1680 | `			 }` |
|        - | 1681 | `			 /* Make sure the operand are of a valid type */` |
|      257 | 1682 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1683 | `				 /* Clone:` |
|        - | 1684 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1685 | `				  *  ++ function call (including annonymous)` |
|        - | 1686 | `				  *  ++ array member` |
|        - | 1687 | `				  *  ++ 'new' operator` |
|        - | 1688 | `				  * Example:` |
|        - | 1689 | `				  *   clone $pObj;` |
|        - | 1690 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1691 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1692 | `				  */` |
|       38 | 1693 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       36 | 1694 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1695 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1696 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1697 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1698 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1699 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1700 | `						 }` |
|      ! 0 | 1701 | `						 return rc;` |
|        - | 1702 | `					 }` |
|       16 | 1703 | `				 }` |
|       21 | 1704 | `			 }else{` |
|        - | 1705 | `				 /* New */` |
|      218 | 1706 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|        5 | 1707 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        - | 1708 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|        - | 1709 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|        - | 1710 | `					  * expression (PHP parse error). The postfix pass folds` |
|        - | 1711 | ``					  * `new C()` into a completed term, so guard against the`` |
|        - | 1712 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|        - | 1713 | `					  * (the inner is a parenthesized group). */` |
|      ! 0 | 1714 | `					 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1715 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1716 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1717 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1718 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1719 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1720 | `					 }` |
|      ! 0 | 1721 | `					 return rc;` |
|        - | 1722 | `				 }` |
|      223 | 1723 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      223 | 1724 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      218 | 1725 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       31 | 1726 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1727 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1728 | `						 /* Syntax error */` |
|      ! 0 | 1729 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1730 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1731 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1732 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1733 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1734 | `						 }` |
|      ! 0 | 1735 | `						 return rc;` |
|        - | 1736 | `					 }` |
|      109 | 1737 | `				 }` |
|        - | 1738 | `			 }` |
|        - | 1739 | `			  /* Link the node to the tree */` |
|      257 | 1740 | `			 pNode->pLeft = apNode[iLeft];` |
|      257 | 1741 | `			 apNode[iLeft] = 0;` |
|      257 | 1742 | `			 pNode->pRight = 0; /* Paranoid */` |
|      126 | 1743 | `		 }` |
|  1044110 | 1744 | `	 }` |
|        - | 1745 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   864405 | 1746 | `	 iLeft = -1;` |
|  5625631 | 1747 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4757055 | 1748 | `		 if( apNode[iCur] == 0 ){` |
|  2673021 | 1749 | `			 continue;` |
|        - | 1750 | `		 }` |
|  2084039 | 1751 | `		 pNode = apNode[iCur];` |
|  2084039 | 1752 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11559 | 1753 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     4213 | 1754 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1755 | `					 /* Link the node to the tree */` |
|     4225 | 1756 | `					 pNode->pLeft = apNode[iLeft];` |
|     4225 | 1757 | `					 apNode[iLeft] = 0;` |
|     2110 | 1758 | `			 }` |
|     7865 | 1759 | `		  }` |
|  2088215 | 1760 | `		 iLeft = iCur;` |
|  1044110 | 1761 | `	  }` |
|   868581 | 1762 | `	 iLeft = -1;` |
|  5629807 | 1763 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4761231 | 1764 | `		 if( apNode[iCur] == 0 ){` |
|  2677241 | 1765 | `			 continue;` |
|        - | 1766 | `		 }` |
|  2083995 | 1767 | `		 pNode = apNode[iCur];` |
|  2083995 | 1768 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11510 | 1769 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    11515 | 1770 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1771 | `					 /* Syntax error */` |
|      ! 0 | 1772 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1773 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1774 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1775 | `					 }` |
|      ! 0 | 1776 | `					 return rc;` |
|        - | 1777 | `			 }` |
|        - | 1778 | `			 /* Link the node to the tree */` |
|    11515 | 1779 | `			 pNode->pLeft = apNode[iLeft];` |
|    11515 | 1780 | `			 apNode[iLeft] = 0;` |
|        - | 1781 | `			 /* Mark as pre-increment/decrement node */` |
|    11515 | 1782 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5755 | 1783 | `		  }` |
|  2083995 | 1784 | `		 iLeft = iCur;` |
|  1042000 | 1785 | `	 }` |
|        - | 1786 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   868581 | 1787 | `	  iLeft = 0;` |
|  5629801 | 1788 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4761227 | 1789 | `		  if( apNode[iCur] ){` |
|  2072481 | 1790 | `			  pNode = apNode[iCur];` |
|  2072481 | 1791 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    36113 | 1792 | `				  if( iLeft > 0 ){` |
|        - | 1793 | `					  /* Link the node to the tree */` |
|    36111 | 1794 | `					  pNode->pLeft = apNode[iLeft];` |
|    36111 | 1795 | `					  apNode[iLeft] = 0;` |
|    36111 | 1796 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1797 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1798 | `							   /* Syntax error */` |
|      ! 0 | 1799 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1800 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1801 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1802 | `							  }` |
|      ! 0 | 1803 | `							  return rc;` |
|        - | 1804 | `						  }` |
|       36 | 1805 | `					  }` |
|    18058 | 1806 | `				  }else{` |
|        - | 1807 | `					  /* Syntax error */` |
|        3 | 1808 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1809 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1810 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1811 | `					  }` |
|        3 | 1812 | `					  return rc;` |
|        - | 1813 | `				  }` |
|    18053 | 1814 | `			  }` |
|        - | 1815 | `			  /* Save terminal position */` |
|  2072479 | 1816 | `			  iLeft = iCur;` |
|  1036237 | 1817 | `		  }` |
|  2380615 | 1818 | `	  }` |
|        - | 1819 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1820 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1821 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1822 | `	  * yielding a right-leaning tree. */` |
|  5629799 | 1823 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4761225 | 1824 | `		 if( apNode[iCur] == 0 ){` |
|  2724969 | 1825 | `			 continue;` |
|        - | 1826 | `		 }` |
|  2036261 | 1827 | `		 pNode = apNode[iCur];` |
|  2036261 | 1828 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1829 | `			 sxi32 iL, iR;` |
|        - | 1830 | `			 /* Find the right operand */` |
|      113 | 1831 | `			 iR = -1;` |
|        - | 1832 | `			 {` |
|        - | 1833 | `				 sxi32 j;` |
|      125 | 1834 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1835 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1836 | `				 }` |
|        - | 1837 | `			 }` |
|        - | 1838 | `			 /* Find the left operand */` |
|      113 | 1839 | `			 iL = -1;` |
|        - | 1840 | `			 {` |
|        - | 1841 | `				 sxi32 j;` |
|      181 | 1842 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1843 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1844 | `				 }` |
|        - | 1845 | `			 }` |
|      113 | 1846 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1847 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1848 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1849 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1850 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1851 | `				 }` |
|      ! 0 | 1852 | `				 return rc;` |
|        - | 1853 | `			 }` |
|      113 | 1854 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1855 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1856 | `			 apNode[iL] = 0;` |
|      113 | 1857 | `			 apNode[iR] = 0;` |
|        - | 1858 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1859 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1860 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1861 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1862 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1863 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1864 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1865 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1866 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1867 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1868 | `			  * operands are respected. */` |
|      112 | 1869 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1870 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1871 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1872 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1873 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1874 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1875 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1876 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1877 | `				 while( pTail->pLeft` |
|       34 | 1878 | `					 && pTail->pLeft->pOp` |
|       23 | 1879 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1880 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1881 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1882 | `					 pTail = pTail->pLeft;` |
|        1 | 1883 | `				 }` |
|        - | 1884 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1885 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1886 | `				 pTail->pLeft = pNode;` |
|       27 | 1887 | `				 apNode[iCur] = pHead;` |
|       13 | 1888 | `			 }` |
|       56 | 1889 | `		 }` |
|  1018133 | 1890 | `	 }` |
|        - | 1891 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  9554233 | 1892 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  8685669 | 1893 | `		 iLeft = -1;` |
| 56297575 | 1894 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 47611921 | 1895 | `			 if( apNode[iCur] == 0 ){` |
| 30700831 | 1896 | `				 continue;` |
|        - | 1897 | `			 }` |
| 16911095 | 1898 | `			 pNode = apNode[iCur];` |
| 16911095 | 1899 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1900 | `				 /* Get the right node */` |
|   258069 | 1901 | `				 iRight = iCur + 1;` |
|   369031 | 1902 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   110967 | 1903 | `					 iRight++;` |
|        5 | 1904 | `				 }` |
|   258069 | 1905 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1906 | `					 /* Syntax error */` |
|       11 | 1907 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       11 | 1908 | `					 if( rc != SXERR_ABORT ){` |
|       11 | 1909 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1910 | `					 }` |
|       11 | 1911 | `					 return rc;` |
|        - | 1912 | `				 }` |
|   258061 | 1913 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1914 | `					 sxi32  iTmp;` |
|        - | 1915 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1916 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1917 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1918 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1919 | `					  * is swapped below. */` |
|       60 | 1920 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1921 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1922 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1923 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1924 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1925 | `						 }` |
|        3 | 1926 | `						 return rc;` |
|        - | 1927 | `					 }` |
|       57 | 1928 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1929 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1930 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1931 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1932 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1933 | `						 }` |
|      ! 0 | 1934 | `						 return rc;` |
|        - | 1935 | `					 }` |
|       57 | 1936 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       41 | 1937 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1938 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1939 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1940 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1941 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1942 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1943 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1944 | `									 }` |
|      ! 0 | 1945 | `									 return rc;` |
|        - | 1946 | `							 }` |
|      ! 0 | 1947 | `						 }` |
|       19 | 1948 | `					 }` |
|        - | 1949 | `					 /* Swap operands */` |
|       57 | 1950 | `					 iTmp = iRight;` |
|       57 | 1951 | `					 iRight = iLeft;` |
|       57 | 1952 | `					 iLeft = iTmp;` |
|       27 | 1953 | `				 }` |
|        - | 1954 | `				 /* Link the node to the tree */` |
|   258059 | 1955 | `				 pNode->pLeft = apNode[iLeft];` |
|   258059 | 1956 | `				 pNode->pRight = apNode[iRight];` |
|   258059 | 1957 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   129027 | 1958 | `			 }` |
| 16911085 | 1959 | `			 iLeft = iCur;` |
|  8455545 | 1960 | `		 }` |
|  4342832 | 1961 | `	 }` |
|        - | 1962 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1963 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1964 | `	  * we are dealing with a single operator.` |
|        - | 1965 | `	  */` |
|   868569 | 1966 | `	  iLeft = -1;` |
|  5618279 | 1967 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4752363 | 1968 | `		  if( apNode[iCur] == 0 ){` |
|  3240255 | 1969 | `			  continue;` |
|        - | 1970 | `		  }` |
|  1512113 | 1971 | `		  pNode = apNode[iCur];` |
|  1512113 | 1972 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2653 | 1973 | `			  sxi32 iNest = 1;` |
|     2653 | 1974 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1975 | `				  /* Missing condition */` |
|        3 | 1976 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1977 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1978 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1979 | `				  }` |
|        3 | 1980 | `				  return rc;` |
|        - | 1981 | `			  }` |
|        - | 1982 | `			  /* Get the right node */` |
|     2651 | 1983 | `			  iRight = iCur + 1;` |
|     5581 | 1984 | `			  while( iRight < nToken  ){` |
|     5581 | 1985 | `				  if( apNode[iRight] ){` |
|     5229 | 1986 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1987 | `						  /* Increment nesting level */` |
|      ! 0 | 1988 | `						  ++iNest;` |
|     5229 | 1989 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1990 | `						  /* Decrement nesting level */` |
|     2651 | 1991 | `						  --iNest;` |
|     2651 | 1992 | `						  if( iNest <= 0 ){` |
|     2651 | 1993 | `							  break;` |
|        - | 1994 | `						  }` |
|      ! 0 | 1995 | `					  }` |
|     1289 | 1996 | `				  }` |
|     2935 | 1997 | `				  iRight++;` |
|        5 | 1998 | `			  }` |
|     2651 | 1999 | `			  if( iRight > iCur + 1 ){` |
|        - | 2000 | `				  /* Recurse and process the then expression */` |
|     2583 | 2001 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2583 | 2002 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 2003 | `					  return rc;` |
|        - | 2004 | `				  }` |
|        - | 2005 | `				  /* Link the node to the tree */` |
|     2583 | 2006 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1289 | 2007 | `			  }else{` |
|        - | 2008 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 2009 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 2010 | `			  }` |
|     2651 | 2011 | `			  apNode[iCur + 1] = 0;` |
|     2651 | 2012 | `			  if( iRight + 1 < nToken ){` |
|        - | 2013 | `				  /* Recurse and process the else expression */` |
|     2651 | 2014 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2651 | 2015 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 2016 | `					  return rc;` |
|        - | 2017 | `				  }` |
|        - | 2018 | `				  /* Link the node to the tree */` |
|     2651 | 2019 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2651 | 2020 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1328 | 2021 | `			  }else{` |
|      ! 0 | 2022 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 2023 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 2024 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 2025 | `				 }` |
|      ! 0 | 2026 | `				 return rc;` |
|        - | 2027 | `			  }` |
|        - | 2028 | `			  /* Point to the condition */` |
|     2651 | 2029 | `			  pNode->pCond  = apNode[iLeft];` |
|     2651 | 2030 | `			  apNode[iLeft] = 0;` |
|     2651 | 2031 | `			  break;` |
|        - | 2032 | `		  }` |
|  1509465 | 2033 | `		  iLeft = iCur;` |
|   754735 | 2034 | `	  }` |
|        - | 2035 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 2036 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 2037 | `	  * so there is no need for a precedence loop here.` |
|        - | 2038 | `	  */` |
|   868567 | 2039 | `	 iRight = -1;` |
|  5629603 | 2040 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4761095 | 2041 | `		 if( apNode[iCur] == 0 ){` |
|  3572025 | 2042 | `			 continue;` |
|        - | 2043 | `		 }` |
|  1189075 | 2044 | `		 pNode = apNode[iCur];` |
|  1189075 | 2045 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 2046 | `			 /* Get the left node */` |
|   320391 | 2047 | `			 iLeft = iCur - 1;` |
|   463591 | 2048 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   143205 | 2049 | `				 iLeft--;` |
|        5 | 2050 | `			 }` |
|   320391 | 2051 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2052 | `				 /* Syntax error */` |
|       44 | 2053 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2054 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 2055 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 2056 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 2057 | `				 }else{` |
|       40 | 2058 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 2059 | `				 }` |
|       44 | 2060 | `				 if( rc != SXERR_ABORT ){` |
|       42 | 2061 | `					 rc = SXERR_SYNTAX;` |
|       20 | 2062 | `				 }` |
|       44 | 2063 | `				 return rc;` |
|        - | 2064 | `			 }` |
|        - | 2065 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 2066 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 2067 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 2068 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 2069 | `			  * a write. */` |
|   320349 | 2070 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 2071 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 2072 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 2073 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 2074 | `					 rc = SXERR_SYNTAX;` |
|        4 | 2075 | `				 }` |
|       11 | 2076 | `				 return rc;` |
|        - | 2077 | `			 }` |
|   320341 | 2078 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      115 | 2079 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       82 | 2080 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2081 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2082 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2083 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2084 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2085 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2086 | `					 }else{` |
|        4 | 2087 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2088 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2089 | `					 }` |
|        6 | 2090 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2091 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2092 | `					 }` |
|        6 | 2093 | `					 return rc;` |
|        - | 2094 | `				 }` |
|       40 | 2095 | `			 }` |
|        - | 2096 | `			 /* Link the node to the tree (Reverse) */` |
|   320337 | 2097 | `			 pNode->pLeft = apNode[iRight];` |
|   320337 | 2098 | `			 pNode->pRight = apNode[iLeft];` |
|   320337 | 2099 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   160166 | 2100 | `		 }` |
|  1189021 | 2101 | `		 iRight = iCur;` |
|   594513 | 2102 | `	 }` |
|        - | 2103 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  4342545 | 2104 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3474037 | 2105 | `		 iLeft = -1;` |
| 22518125 | 2106 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 19044093 | 2107 | `			 if( apNode[iCur] == 0 ){` |
| 15569655 | 2108 | `				 continue;` |
|        - | 2109 | `			 }` |
|  3474443 | 2110 | `			 pNode = apNode[iCur];` |
|  3474443 | 2111 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2112 | `				 /* Get the right node */` |
|       72 | 2113 | `				 iRight = iCur + 1;` |
|      110 | 2114 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2115 | `					 iRight++;` |
|        2 | 2116 | `				 }` |
|       72 | 2117 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2118 | `					 /* Syntax error */` |
|      ! 0 | 2119 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2120 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2121 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2122 | `					 }` |
|      ! 0 | 2123 | `					 return rc;` |
|        - | 2124 | `				 }` |
|        - | 2125 | `				 /* Link the node to the tree */` |
|       72 | 2126 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2127 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2128 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2129 | `			 }` |
|  3474443 | 2130 | `			 iLeft = iCur;` |
|  1737224 | 2131 | `		 }` |
|  1737021 | 2132 | `	 }` |
|        - | 2133 | `	 /* Point to the root of the expression tree */` |
|  4760999 | 2134 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3892509 | 2135 | `		 if( apNode[iCur] ){` |
|   825345 | 2136 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       23 | 2137 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       23 | 2138 | `				  if( rc != SXERR_ABORT ){` |
|       23 | 2139 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2140 | `				  }` |
|       23 | 2141 | `				  return rc;` |
|        - | 2142 | `			 }` |
|   825327 | 2143 | `			 apNode[0] = apNode[iCur];` |
|   825327 | 2144 | `			 apNode[iCur] = 0;` |
|   412661 | 2145 | `		 }` |
|  1946248 | 2146 | `	 }` |
|   868495 | 2147 | `	 return SXRET_OK;` |
|   818766 | 2148 | ` }` |
|        - | 2149 | ` /*` |
|        - | 2150 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2151 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2152 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2153 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2154 | `  */` |
|  1024718 | 2155 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2156 | `{` |
|        - | 2157 | `	ph7_expr_node **apNode;` |
|        - | 2158 | `	ph7_expr_node *pNode;` |
|        - | 2159 | `	sxi32 rc;` |
|        - | 2160 | `	/* Reset node container */` |
|  1024723 | 2161 | `	SySetReset(pExprNode);` |
|  1024723 | 2162 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2163 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2164 | `	{` |
|  1024723 | 2165 | `		int iLastWasTerm = 0;` |
|  1024723 | 2166 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  5554591 | 2167 | `		while( pGen->pIn < pGen->pEnd ){` |
|  4529907 | 2168 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  4529907 | 2169 | `			if( rc != SXRET_OK ){` |
|       38 | 2170 | `				return rc;` |
|        - | 2171 | `			}` |
|        - | 2172 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  4529873 | 2173 | `			if( pNode->xCode ){` |
|        - | 2174 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2375499 | 2175 | `				iLastWasTerm = 1;` |
|  3342126 | 2176 | `			}else if( pNode->pOp ){` |
|        - | 2177 | `				/* Operator node */` |
|  1023237 | 2178 | `				iLastWasTerm = 0;` |
|   511621 | 2179 | `			}else{` |
|        - | 2180 | `				/* Delimiter: ')' and ']' end terms */` |
|  1131147 | 2181 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2182 | `			}` |
|        - | 2183 | `			/* A keyword in the next node is a member name only right after a member` |
|        - | 2184 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|        - | 2185 | `			 * node kind, so this single test covers all branches. */` |
|  4529873 | 2186 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|        - | 2187 | `			/* Save the extracted node */` |
|  4529873 | 2188 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2189 | `		}` |
|        - | 2190 | `	}` |
|  1024689 | 2191 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2192 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2193 | `		*ppRoot = 0;` |
|      ! 0 | 2194 | `		return SXRET_OK;` |
|        - | 2195 | `	}` |
|  1024689 | 2196 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2197 | `	/* Make sure we are dealing with valid nodes */` |
|  1024689 | 2198 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1024689 | 2199 | `	if( rc != SXRET_OK ){` |
|        - | 2200 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2201 | `		 * cleanup the mess left behind.` |
|        - | 2202 | `		 */` |
|       54 | 2203 | `		*ppRoot = 0;` |
|       54 | 2204 | `		return rc;` |
|        - | 2205 | `	}` |
|        - | 2206 | `	/* Build the tree */` |
|  1024639 | 2207 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1024639 | 2208 | `	if( rc != SXRET_OK ){` |
|        - | 2209 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2210 | `		*ppRoot = 0;` |
|      103 | 2211 | `		return rc;` |
|        - | 2212 | `	}` |
|        - | 2213 | `	/* Point to the root of the tree */` |
|  1024541 | 2214 | `	*ppRoot = apNode[0];` |
|  1024541 | 2215 | `	return SXRET_OK;` |
|   512364 | 2216 | `}` |
|        - | 2217 |  |
