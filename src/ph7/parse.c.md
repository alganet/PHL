# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1103/1272 lines (86.71%)

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
|        - |  194 | `	/* Precedence 10,non-associative */` |
|        - |  195 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|        - |  196 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|        - |  197 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|        - |  198 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|        - |  199 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|        - |  200 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  201 | `	/* Precedence 11,non-associative */` |
|        - |  202 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|        - |  203 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  204 | `	{ {"eq",sizeof(char)*2},  EXPR_OP_SEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_SEQ}, /* IMP-0137-EQ: Symisc eXtension */` |
|        - |  205 | `	{ {"ne",sizeof(char)*2},  EXPR_OP_SNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_SNE}, /* IMP-0138-NE: Symisc eXtension */` |
|        - |  206 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|        - |  207 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|        - |  208 | `	/* Precedence 12,left-associative */` |
|        - |  209 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|        - |  210 | `	/* Precedence 12,left-associative */` |
|        - |  211 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|        - |  212 | `	                         /* Binary operators */` |
|        - |  213 | `	/* Precedence 13,left-associative */` |
|        - |  214 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|        - |  215 | `	/* Precedence 14,left-associative */` |
|        - |  216 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|        - |  217 | `	/* Precedence 15,left-associative */` |
|        - |  218 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  219 | `	/* Precedence 16,left-associative */` |
|        - |  220 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  221 | `	                      /* Null coalescing operator */` |
|        - |  222 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|        - |  223 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|        - |  224 | `	                      /* Ternary operator */` |
|        - |  225 | `	/* Precedence 17,left-associative */` |
|        - |  226 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  227 | `	                     /* Combined binary operators */` |
|        - |  228 | `	/* Precedence 18,right-associative */` |
|        - |  229 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  230 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  231 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  232 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  233 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  234 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  235 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  236 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|        - |  237 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  238 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  239 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  240 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  241 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  242 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|        - |  243 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|        - |  244 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|        - |  245 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|        - |  246 | `	 * in this file: keep one of the question marks escaped. */` |
|        - |  247 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|        - |  248 | `	/* Precedence 19,left-associative */` |
|        - |  249 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  250 | `	/* Precedence 20,left-associative */` |
|        - |  251 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  252 | `	/* Precedence 21,left-associative */` |
|        - |  253 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  254 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  255 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  256 | `};` |
|        - |  257 | `/* Function call operator need special handling */` |
|        - |  258 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  259 | `/*` |
|        - |  260 | ` * Check if the given token is a potential operator or not.` |
|        - |  261 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  262 | ` * look like an operator.` |
|        - |  263 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  264 | ` * Otherwise NULL.` |
|        - |  265 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  266 | ` * a binary minus or unary minus.]` |
|        - |  267 | ` */` |
|  1075506 |  268 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  269 |  |
|  1075511 |  270 | `	sxu32 n = 0;` |
|        - |  271 | `	sxi32 rc;` |
|        - |  272 | `	/* Do a linear lookup on the operators table */` |
| 18056707 |  273 | `	for(;;){` |
| 36113419 |  274 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  275 | `			break;` |
|        - |  276 | `		}` |
| 36113419 |  277 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  278 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  4242087 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  2121046 |  280 | `		}else{` |
| 31871337 |  281 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  282 | `		}` |
| 36113419 |  283 | `		if( rc == 0 ){` |
|  1079663 |  284 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  285 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1075115 |  286 | `				return &aOpTable[n];` |
|        - |  287 | `			}` |
|        - |  288 | `			/* Handle ambiguity */` |
|     4553 |  289 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  290 | `				/* Unary opertors have prcedence here over binary operators */` |
|      269 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|     4289 |  293 | `			if( pLast->nType & PH7_TK_OP ){` |
|      142 |  294 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  295 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      142 |  296 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  297 | `					/* Unary opertors have prcedence here over binary operators */` |
|      134 |  298 | `					return &aOpTable[n];` |
|        - |  299 | `				}` |
|        - |  300 |  |
|        4 |  301 | `			}` |
|     2076 |  302 | `		}` |
| 35037913 |  303 | `		++n; /* Next operator in the table */` |
|        5 |  304 | `	}` |
|        - |  305 | `	/* No such operator */` |
|      ! 0 |  306 | `	return 0;` |
|   537758 |  307 |  |
|        - |  308 | `/*` |
|        - |  309 | ` * Delimit a set of token stream.` |
|        - |  310 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  311 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  312 | ` */` |
|   660274 |  313 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  314 |  |
|   660279 |  315 | `	SyToken *pCur = pIn;` |
|   660279 |  316 | `	sxi32 iNest = 1;` |
|  3561767 |  317 | `	for(;;){` |
|  7123539 |  318 | `		if( pCur >= pEnd ){` |
|      273 |  319 | `			break;` |
|        - |  320 | `		}` |
|  7123271 |  321 | `		if( pCur->nType & nTokStart ){` |
|        - |  322 | `			/* Increment nesting level */` |
|   351311 |  323 | `			iNest++;` |
|  6947618 |  324 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  325 | `			/* Decrement nesting level */` |
|  1011317 |  326 | `			iNest--;` |
|  1011317 |  327 | `			if( iNest <= 0 ){` |
|   660011 |  328 | `				break;` |
|        - |  329 | `			}` |
|   175653 |  330 | `		}` |
|        - |  331 | `		/* Advance cursor */` |
|  6463265 |  332 | `		pCur++;` |
|        5 |  333 | `	}` |
|        - |  334 | `	/* Point to the end of the chunk */` |
|   660279 |  335 | `	*ppEnd = pCur;` |
|   660279 |  336 |  |
|        - |  337 | `/*` |
|        - |  338 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  339 | ` * Note on reserved keywords.` |
|        - |  340 | ` *  According to the PHP language reference manual:` |
|        - |  341 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  342 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  343 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  344 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  345 | ` */` |
|    21550 |  346 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  347 |  |
|    32251 |  348 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    21454 |  349 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  350 | `		){` |
|      165 |  351 | `			return TRUE;` |
|        - |  352 | `	}` |
|    21395 |  353 | `	if( bCheckFunc ){` |
|      244 |  354 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      169 |  355 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      153 |  356 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       45 |  357 | `				return TRUE;` |
|        - |  358 | `		}` |
|       68 |  359 | `	}` |
|        - |  360 | `	/* Not a language construct */` |
|    21355 |  361 | `	return FALSE;` |
|    10780 |  362 |  |
|        - |  363 | `/*` |
|        - |  364 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  365 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  366 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  367 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  368 | ` */` |
|   909154 |  369 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  370 |  |
|        - |  371 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  372 | `	sxi32 i,rc;` |
|        - |  373 |  |
|   909159 |  374 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  375 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  376 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  377 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  378 | `	}` |
|   909159 |  379 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  4894189 |  380 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3985069 |  381 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  382 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1007 |  383 | `			continue;` |
|        - |  384 | `		}` |
|  3984067 |  385 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   437087 |  386 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    21798 |  387 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  388 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   407779 |  389 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  390 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  391 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  392 | `						 */` |
|   407779 |  393 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   407779 |  394 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   407779 |  395 | `						apNode[i]->pOp = &sFCallOp;` |
|   203887 |  396 | `					}` |
|   203887 |  397 | `			}` |
|   437087 |  398 | `			iParen++;` |
|  3765526 |  399 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   437087 |  400 | `			if( iParen <= 0 ){` |
|       15 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       15 |  402 | `				if( rc != SXERR_ABORT ){` |
|       15 |  403 | `					rc = SXERR_SYNTAX;` |
|        6 |  404 | `				}` |
|       15 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|   437075 |  407 | `			iParen--;` |
|  3328438 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    88431 |  409 | `			iSquare++;` |
|  3065690 |  410 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    88445 |  411 | `			if( iSquare <= 0 ){` |
|        8 |  412 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        8 |  413 | `				if( rc != SXERR_ABORT ){` |
|        8 |  414 | `					rc = SXERR_SYNTAX;` |
|        3 |  415 | `				}` |
|        8 |  416 | `				return rc;` |
|        - |  417 | `			}` |
|    88439 |  418 | `			iSquare--;` |
|  2977254 |  419 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       16 |  420 | `			iBraces++;` |
|       16 |  421 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  422 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  423 | `				int iNest = 1;` |
|       11 |  424 | `				sxi32 j=i+1;` |
|        - |  425 | `				/*` |
|        - |  426 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  427 | `				 */` |
|       11 |  428 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  429 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  430 | `				pOp = aOpTable;` |
|       11 |  431 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       61 |  432 | `				while( pOp < pEnd ){` |
|       61 |  433 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  434 | `						break;` |
|        - |  435 | `					}` |
|       51 |  436 | `					pOp++;` |
|        1 |  437 | `				}` |
|       11 |  438 | `				if( pOp >= pEnd ){` |
|      ! 0 |  439 | `					pOp = 0;` |
|      ! 0 |  440 | `				}` |
|       11 |  441 | `				if( pOp ){` |
|       11 |  442 | `					apNode[i]->pOp = pOp;` |
|       11 |  443 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  444 | `				}` |
|       11 |  445 | `				iBraces--;` |
|       11 |  446 | `				iSquare++;` |
|       21 |  447 | `				while( j < nNode ){` |
|       21 |  448 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  449 | `						/* Increment nesting level */` |
|      ! 0 |  450 | `						iNest++;` |
|       21 |  451 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  452 | `						/* Decrement nesting level */` |
|       11 |  453 | `						iNest--;` |
|       11 |  454 | `						if( iNest < 1 ){` |
|       11 |  455 | `							break;` |
|        - |  456 | `						}` |
|      ! 0 |  457 | `					}` |
|       11 |  458 | `					j++;` |
|        1 |  459 | `				}` |
|       11 |  460 | `				if( j < nNode ){` |
|       11 |  461 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  462 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  463 | `				}` |
|        - |  464 |  |
|        7 |  465 | `			}` |
|  2933030 |  466 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       21 |  467 | `			if( iBraces <= 0 ){` |
|       16 |  468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       16 |  469 | `				if( rc != SXERR_ABORT ){` |
|       16 |  470 | `					rc = SXERR_SYNTAX;` |
|        6 |  471 | `				}` |
|       16 |  472 | `				return rc;` |
|        - |  473 | `			}` |
|        6 |  474 | `			iBraces--;` |
|  2933009 |  475 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2837 |  476 | `			if( iQuesty > 0 ){` |
|     2647 |  477 | `				iQuesty--;` |
|     1516 |  478 | `			}else if( iParen <= 0 ){` |
|        - |  479 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  480 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  481 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        6 |  482 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        6 |  483 | `				if( rc != SXERR_ABORT ){` |
|        6 |  484 | `					rc = SXERR_SYNTAX;` |
|        2 |  485 | `				}` |
|        6 |  486 | `				return rc;` |
|        5 |  487 | `			}` |
|  2931589 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   836941 |  489 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   836941 |  490 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2649 |  491 | `				iQuesty++;` |
|   835619 |  492 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      375 |  493 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  494 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  495 | `					sxu32 n = 0;` |
|       11 |  496 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  497 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  498 | `					}` |
|        - |  499 | `					/*` |
|        - |  500 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  501 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  502 | `					 */` |
|      265 |  503 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      255 |  504 | `						++n;` |
|        1 |  505 | `					}` |
|       11 |  506 | `					pOp = &aOpTable[n];` |
|        - |  507 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  508 | `					apNode[i]->pOp = pOp;` |
|       11 |  509 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  510 | `				}` |
|      185 |  511 | `			}` |
|   418468 |  512 | `		}` |
|  1992019 |  513 | `	}` |
|   909125 |  514 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       20 |  515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       20 |  516 | `		if( rc != SXERR_ABORT ){` |
|       20 |  517 | `			rc = SXERR_SYNTAX;` |
|        8 |  518 | `		}` |
|       20 |  519 | `		return rc;` |
|        - |  520 | `	}` |
|   909109 |  521 | `	return SXRET_OK;` |
|   454582 |  522 |  |
|        - |  523 | `/*` |
|        - |  524 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  525 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  526 | ` */` |
|   757284 |  527 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  528 |  |
|   757289 |  529 | `	SyToken *pIn = *ppCur;` |
|        - |  530 | `	/* Jump the first literal seen */` |
|   757289 |  531 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   757249 |  532 | `		pIn++;` |
|   378622 |  533 | `	}` |
|   378686 |  534 | `	for(;;){` |
|   757377 |  535 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       92 |  536 | `			pIn++;` |
|       92 |  537 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       90 |  538 | `				pIn++;` |
|       43 |  539 | `			}` |
|       48 |  540 | `		}else{` |
|   378647 |  541 | `			break;` |
|        - |  542 | `		}` |
|        4 |  543 | `	}` |
|        - |  544 | `	/* Synchronize pointers */` |
|   757289 |  545 | `	*ppCur = pIn;` |
|   757289 |  546 |  |
|        - |  547 | `/*` |
|        - |  548 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  549 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  550 | ` * Note on annonymous functions.` |
|        - |  551 | ` *  According to the PHP language reference manual:` |
|        - |  552 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  553 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  554 | ` *  parameters, but they have many other uses.` |
|        - |  555 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  556 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  557 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  558 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  559 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  560 | ` *` |
|        - |  561 | ` * Some example:` |
|        - |  562 | ` *  $greet = function($name)` |
|        - |  563 | ` * {` |
|        - |  564 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  565 | ` * };` |
|        - |  566 | ` *  $greet('World');` |
|        - |  567 | ` *  $greet('PHP');` |
|        - |  568 | ` *` |
|        - |  569 | ` * $double = function($a) {` |
|        - |  570 | ` *   return $a * 2;` |
|        - |  571 | ` * };` |
|        - |  572 | ` * // This is our range of numbers` |
|        - |  573 | ` * $numbers = range(1, 5);` |
|        - |  574 | ` * // Use the Annonymous function as a callback here to` |
|        - |  575 | ` * // double the size of each element in our` |
|        - |  576 | ` * // range` |
|        - |  577 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  578 | ` * print implode(' ', $new_numbers);` |
|        - |  579 | ` */` |
|      278 |  580 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  581 |  |
|      283 |  582 | `	SyToken *pIn = *ppCur;` |
|        - |  583 | `	sxu32 nLine;` |
|        - |  584 | `	sxi32 rc;` |
|        - |  585 | `	/* Jump the 'function' keyword */` |
|      283 |  586 | `	nLine = pIn->nLine;` |
|      283 |  587 | `	pIn++;` |
|      283 |  588 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  589 | `		pIn++;` |
|        1 |  590 | `	}` |
|      283 |  591 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  592 | `		/* Syntax error */` |
|        6 |  593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  594 | `		if( rc != SXERR_ABORT ){` |
|        6 |  595 | `			rc = SXERR_SYNTAX;` |
|        2 |  596 | `		}` |
|        6 |  597 | `		goto Synchronize;` |
|        - |  598 | `	}` |
|      279 |  599 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      279 |  600 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      279 |  601 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  602 | `		/* Syntax error */` |
|        6 |  603 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  604 | `		if( rc != SXERR_ABORT ){` |
|        6 |  605 | `			rc = SXERR_SYNTAX;` |
|        2 |  606 | `		}` |
|        6 |  607 | `		goto Synchronize;` |
|        - |  608 | `	}` |
|      275 |  609 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  610 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      275 |  611 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  612 | `		pIn++; /* Skip ':' */` |
|        - |  613 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  614 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  615 | `			pIn++;` |
|      ! 0 |  616 | `		}` |
|        - |  617 | `		/* Skip the first type (allow leading '\' and namespace path) */` |
|        5 |  618 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        5 |  619 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  620 | `			pIn++;` |
|        5 |  621 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  622 | `				pIn += 2;` |
|      ! 0 |  623 | `			}` |
|        2 |  624 | `		}` |
|        - |  625 | `		/* Skip union alternatives ( \| type )* */` |
|        6 |  626 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        3 |  627 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  628 | `			pIn++;` |
|      ! 0 |  629 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  630 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  631 | `				pIn++;` |
|      ! 0 |  632 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  633 | `					pIn += 2;` |
|      ! 0 |  634 | `				}` |
|      ! 0 |  635 | `			}` |
|      ! 0 |  636 | `		}` |
|        2 |  637 | `	}` |
|      275 |  638 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       37 |  639 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  640 | `		/* Check if we are dealing with a closure */` |
|       37 |  641 | `		if( nKey == PH7_TKWRD_USE ){` |
|       29 |  642 | `			pIn++; /* Jump the 'use' keyword */` |
|       29 |  643 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  644 | `				/* Syntax error */` |
|        6 |  645 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  646 | `				if( rc != SXERR_ABORT ){` |
|        6 |  647 | `					rc = SXERR_SYNTAX;` |
|        2 |  648 | `				}` |
|        6 |  649 | `				goto Synchronize;` |
|        - |  650 | `			}` |
|       25 |  651 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       25 |  652 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       25 |  653 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  654 | `				/* Syntax error */` |
|        6 |  655 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  656 | `				if( rc != SXERR_ABORT ){` |
|        6 |  657 | `					rc = SXERR_SYNTAX;` |
|        2 |  658 | `				}` |
|        6 |  659 | `				goto Synchronize;` |
|        - |  660 | `			}` |
|       21 |  661 | `			pIn++;` |
|       13 |  662 | `		}else{` |
|        - |  663 | `			/* Syntax error */` |
|       11 |  664 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|       11 |  665 | `			if( rc != SXERR_ABORT ){` |
|       11 |  666 | `				rc = SXERR_SYNTAX;` |
|        4 |  667 | `			}` |
|       11 |  668 | `			goto Synchronize;` |
|        - |  669 | `		}` |
|        8 |  670 | `	}` |
|      259 |  671 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      259 |  672 | `		pIn++; /* Jump the leading curly '{' */` |
|      259 |  673 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      259 |  674 | `		if( pIn < pEnd ){` |
|      259 |  675 | `			pIn++;` |
|      127 |  676 | `		}` |
|      132 |  677 | `	}else{` |
|        - |  678 | `		/* Syntax error */` |
|      ! 0 |  679 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|        - |  683 | `	}` |
|      259 |  684 | `	rc = SXRET_OK;` |
|      139 |  685 | `Synchronize:` |
|        - |  686 | `	/* Synchronize pointers */` |
|      283 |  687 | `	*ppCur = pIn;` |
|      283 |  688 | `	return rc;` |
|      144 |  689 |  |
|        - |  690 | `/*` |
|        - |  691 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|        - |  692 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|        - |  693 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|        - |  694 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|        - |  695 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|        - |  696 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|        - |  697 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|        - |  698 | ` */` |
|       20 |  699 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        1 |  700 |  |
|       21 |  701 | `	SyToken *pIn = *ppCur;` |
|       21 |  702 | `	sxu32 nLine = pIn->nLine;` |
|        - |  703 | `	sxi32 rc;` |
|       21 |  704 | `	pIn++; /* Jump the 'class' keyword */` |
|        - |  705 | `	/* Optional constructor argument list */` |
|       21 |  706 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  707 | `		pIn++; /* Jump '(' */` |
|        7 |  708 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        7 |  709 | `		if( pIn < pEnd ){` |
|        7 |  710 | `			pIn++; /* Jump ')' */` |
|        3 |  711 | `		}` |
|        3 |  712 | `	}` |
|        - |  713 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|        - |  714 | `	 * (no braces appear between ')' and the class body). */` |
|       37 |  715 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       17 |  716 | `		pIn++;` |
|        1 |  717 | `	}` |
|       21 |  718 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|        - |  719 | `		/* Syntax error: missing class body */` |
|      ! 0 |  720 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  721 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|      ! 0 |  722 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  723 | `			rc = SXERR_SYNTAX;` |
|      ! 0 |  724 | `		}` |
|      ! 0 |  725 | `		*ppCur = pIn;` |
|      ! 0 |  726 | `		return rc;` |
|        - |  727 | `	}` |
|       21 |  728 | `	pIn++; /* Jump the leading '{' */` |
|       21 |  729 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       21 |  730 | `	if( pIn < pEnd ){` |
|       21 |  731 | `		pIn++; /* Jump the trailing '}' */` |
|       10 |  732 | `	}` |
|       21 |  733 | `	*ppCur = pIn;` |
|       21 |  734 | `	return SXRET_OK;` |
|       11 |  735 |  |
|        - |  736 | `/*` |
|        - |  737 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  738 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  739 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  740 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  741 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  742 | ` */` |
|      120 |  743 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  744 |  |
|      124 |  745 | `	SyToken *pIn = *ppCur;` |
|        - |  746 | `	sxu32 nLine;` |
|        - |  747 | `	sxi32 rc;` |
|        - |  748 | `	int iNest;` |
|      124 |  749 | `	nLine = pIn->nLine;` |
|        - |  750 | `	/* Optional 'static' prefix */` |
|      120 |  751 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      124 |  752 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  753 | `		pIn++;` |
|        1 |  754 | `	}` |
|        - |  755 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      120 |  756 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      124 |  757 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  758 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  759 | `		goto Synchronize;` |
|        - |  760 | `	}` |
|      124 |  761 | `	pIn++; /* Jump 'fn' */` |
|       60 |  762 | `	SXUNUSED(nLine);` |
|       60 |  763 | `	SXUNUSED(pGen);` |
|        - |  764 | `	/* Optional '&' for return-by-reference */` |
|      124 |  765 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  766 | `		pIn++;` |
|      ! 0 |  767 | `	}` |
|        - |  768 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  769 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  770 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  771 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      124 |  772 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      121 |  773 | `		pIn++; /* '(' */` |
|      121 |  774 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      121 |  775 | `		if( pIn < pEnd ){` |
|      118 |  776 | `			pIn++; /* ')' */` |
|       58 |  777 | `		}` |
|       59 |  778 | `	}` |
|        - |  779 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|      124 |  780 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  781 | `		pIn++;` |
|        7 |  782 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  783 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  784 | `			pIn++;` |
|        1 |  785 | `		}` |
|        7 |  786 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        7 |  787 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  788 | `			pIn++;` |
|        7 |  789 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  790 | `				pIn += 2;` |
|      ! 0 |  791 | `			}` |
|        3 |  792 | `		}` |
|        9 |  793 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        4 |  794 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  795 | `			pIn++;` |
|      ! 0 |  796 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  797 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  798 | `				pIn++;` |
|      ! 0 |  799 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  800 | `					pIn += 2;` |
|      ! 0 |  801 | `				}` |
|      ! 0 |  802 | `			}` |
|      ! 0 |  803 | `		}` |
|        3 |  804 | `	}` |
|        - |  805 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      124 |  806 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      116 |  807 | `		pIn++;` |
|       57 |  808 | `	}` |
|        - |  809 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      124 |  810 | `	iNest = 0;` |
|      742 |  811 | `	while( pIn < pEnd ){` |
|      657 |  812 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  813 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       37 |  814 | `			break;` |
|        - |  815 | `		}` |
|      621 |  816 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       25 |  817 | `			iNest++;` |
|      609 |  818 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       25 |  819 | `			iNest--;` |
|       12 |  820 | `		}` |
|      621 |  821 | `		pIn++;` |
|        3 |  822 | `	}` |
|      124 |  823 | `	rc = SXRET_OK;` |
|       60 |  824 | `Synchronize:` |
|      124 |  825 | `	*ppCur = pIn;` |
|      124 |  826 | `	return rc;` |
|        4 |  827 |  |
|        - |  828 | `/*` |
|        - |  829 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  830 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  831 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  832 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  833 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  834 | ` */` |
|       70 |  835 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  836 |  |
|       75 |  837 | `	SyToken *pIn = *ppCur;` |
|        - |  838 | `	sxi32 rc;` |
|       35 |  839 | `	SXUNUSED(pGen);` |
|        - |  840 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  841 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       75 |  842 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  843 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  844 | `		goto Synchronize;` |
|        - |  845 | `	}` |
|       75 |  846 | `	pIn++; /* Jump 'match' */` |
|        - |  847 | `	/* Optional '(' subject ')' */` |
|       75 |  848 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       75 |  849 | `		pIn++;` |
|       75 |  850 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       75 |  851 | `		if( pIn < pEnd ){` |
|       75 |  852 | `			pIn++; /* ')' */` |
|       35 |  853 | `		}` |
|       35 |  854 | `	}` |
|        - |  855 | `	/* Optional '{' arms '}' */` |
|       75 |  856 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       75 |  857 | `		pIn++;` |
|       75 |  858 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       75 |  859 | `		if( pIn < pEnd ){` |
|       75 |  860 | `			pIn++; /* '}' */` |
|       35 |  861 | `		}` |
|       35 |  862 | `	}` |
|       75 |  863 | `	rc = SXRET_OK;` |
|       35 |  864 | `Synchronize:` |
|       75 |  865 | `	*ppCur = pIn;` |
|       75 |  866 | `	return rc;` |
|        5 |  867 |  |
|        - |  868 | `/*` |
|        - |  869 | ` * Extract a single expression node from the input.` |
|        - |  870 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  871 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  872 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  873 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  874 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  875 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  876 | ` */` |
|  3985296 |  877 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        5 |  878 |  |
|        - |  879 | `	ph7_expr_node *pNode;` |
|        - |  880 | `	SyToken *pCur;` |
|        - |  881 | `	sxi32 rc;` |
|        - |  882 | `	/* Allocate a new node */` |
|  3985301 |  883 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3985301 |  884 | `	if( pNode == 0 ){` |
|        - |  885 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  886 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  887 | `		 */` |
|      ! 0 |  888 | `		return SXERR_MEM;` |
|        - |  889 | `	}` |
|        - |  890 | `	/* Zero the structure */` |
|  3985301 |  891 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3985301 |  892 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  893 | `	/* Point to the head of the token stream */` |
|  3985301 |  894 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  895 | `	/* Start collecting tokens */` |
|  3985301 |  896 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  897 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  898 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       85 |  899 | `		pCur++;` |
|       85 |  900 | `		pGen->pIn = pCur;` |
|       85 |  901 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       85 |  902 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       85 |  903 | `		if( rc == SXRET_OK && *ppNode ){` |
|       85 |  904 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       41 |  905 | `		}` |
|       85 |  906 | `		return rc;` |
|        - |  907 | `	}` |
|  3985219 |  908 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  909 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  910 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  911 | `		 */` |
|     1009 |  912 | `		pCur++; /* Skip the opening '[' */` |
|     1009 |  913 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1009 |  914 | `		if( pCur < pGen->pEnd ){` |
|     1009 |  915 | `			pCur++; /* Skip past the closing ']' */` |
|      507 |  916 | `		}else{` |
|      ! 0 |  917 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  918 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  919 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  920 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  921 | `			}` |
|      ! 0 |  922 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  923 | `			return rc;` |
|        - |  924 | `		}` |
|        - |  925 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  926 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  927 | `		 */` |
|     1077 |  928 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      139 |  929 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      139 |  930 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       47 |  931 | `				pNode->xCode = PH7_CompileShortList;` |
|       25 |  932 | `			}else{` |
|       93 |  933 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  934 | `			}` |
|       71 |  935 | `		}else{` |
|      873 |  936 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  937 | `		}` |
|  3984717 |  938 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  939 | `		/* Point to the instance that describe this operator */` |
|   925401 |  940 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  941 | `		/* Advance the stream cursor */` |
|   925401 |  942 | `		pCur++;` |
|  3521517 |  943 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  944 | `		/* Isolate variable */` |
|  2150661 |  945 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1075339 |  946 | `			pCur++; /* Variable variable */` |
|        5 |  947 | `		}` |
|  1075327 |  948 | `		if( pCur < pGen->pEnd ){` |
|  1075327 |  949 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  950 | `				/* Variable name */` |
|  1075299 |  951 | `				pCur++;` |
|   537680 |  952 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       25 |  953 | `				pCur++;` |
|        - |  954 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       25 |  955 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       25 |  956 | `				if( pCur < pGen->pEnd ){` |
|       19 |  957 | `					pCur++;` |
|       11 |  958 | `				}else{` |
|        6 |  959 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        6 |  960 | `					if( rc != SXERR_ABORT ){` |
|        6 |  961 | `						rc = SXERR_SYNTAX;` |
|        2 |  962 | `					}` |
|        6 |  963 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        6 |  964 | `					return rc;` |
|        - |  965 | `				}` |
|        8 |  966 | `			}` |
|   537659 |  967 | `		}` |
|  1075323 |  968 | `		pNode->xCode = PH7_CompileVariable;` |
|  2521156 |  969 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    51289 |  970 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    51289 |  971 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  972 | `			 /* List/Array node */` |
|    29331 |  973 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  974 | `				 /* Assume a literal */` |
|       17 |  975 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  976 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  977 | `			 }else{` |
|    29315 |  978 | `				 pCur += 2;` |
|        - |  979 | `				 /* Collect array/list tokens */` |
|    29315 |  980 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    29315 |  981 | `				 if( pCur < pGen->pEnd ){` |
|    29313 |  982 | `					 pCur++;` |
|    14659 |  983 | `				 }else{` |
|        - |  984 | `					 /* Syntax error */` |
|        4 |  985 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  986 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  987 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  988 | `						 rc = SXERR_SYNTAX;` |
|        1 |  989 | `					 }` |
|        3 |  990 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  991 | `					 return rc;` |
|        - |  992 | `				 }` |
|    29313 |  993 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    29313 |  994 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       31 |  995 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       31 |  996 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  997 | `						 /* Syntax error */` |
|        3 |  998 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  999 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1000 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1001 | `						 }` |
|        3 | 1002 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1003 | `						 return rc;` |
|        - | 1004 | `					 }` |
|       13 | 1005 | `				 }` |
|        5 | 1006 | `			 }` |
|    36624 | 1007 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1008 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      135 | 1009 | `			 pCur++; /* Skip 'yield' keyword */` |
|      135 | 1010 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1011 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1012 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      135 | 1013 | `			 pNode->xCode = PH7_CompileYield;` |
|    21898 | 1014 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1015 | `			 /* Annonymous function */` |
|      283 | 1016 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1017 | `				 /* Assume a literal */` |
|      ! 0 | 1018 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1019 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1020 | `			 }else{` |
|        - | 1021 | `				 /* Assemble annonymous functions body */` |
|      283 | 1022 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      283 | 1023 | `				 if( rc != SXRET_OK ){` |
|       28 | 1024 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1025 | `					 return rc;` |
|        - | 1026 | `				 }` |
|      259 | 1027 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1028 | `			  }` |
|    21683 | 1029 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       93 | 1030 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       58 | 1031 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       27 | 1032 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        3 | 1033 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1034 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1035 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1036 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1037 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       21 | 1038 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       21 | 1039 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1040 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1041 | `				 return rc;` |
|        - | 1042 | `			 }` |
|       21 | 1043 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    21542 | 1044 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    21476 | 1045 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 | 1046 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 | 1047 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1048 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      124 | 1049 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      124 | 1050 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1051 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1052 | `				 return rc;` |
|        - | 1053 | `			 }` |
|      124 | 1054 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    21475 | 1055 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1056 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1057 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1058 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1059 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1060 | `				 return rc;` |
|        - | 1061 | `			 }` |
|       75 | 1062 | `			 pNode->xCode = PH7_CompileMatch;` |
|    21380 | 1063 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1064 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1065 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1066 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1067 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1068 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1069 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1070 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1071 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    21327 | 1072 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1073 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       91 | 1074 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       91 | 1075 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       48 | 1076 | `		 }else{` |
|        - | 1077 | `			 /* Assume a literal */` |
|    21223 | 1078 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    21223 | 1079 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1080 | `		 }` |
|  1957841 | 1081 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1082 | `		 /* Constants,function name,namespace path,class name... */` |
|   736055 | 1083 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   736055 | 1084 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   368030 | 1085 | `	 }else{` |
|  1196163 | 1086 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1087 | `			 /* Point to the code generator routine */` |
|   230675 | 1088 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   230675 | 1089 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1090 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1091 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1092 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1093 | `				 }` |
|        3 | 1094 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1095 | `				 return rc;` |
|        - | 1096 | `			 }` |
|   115334 | 1097 | `		 }` |
|        - | 1098 | `		/* Advance the stream cursor */` |
|  1196161 | 1099 | `		pCur++;` |
|        - | 1100 | `	 }` |
|        - | 1101 | `	/* Point to the end of the token stream */` |
|  3985185 | 1102 | `	pNode->pEnd = pCur;` |
|        - | 1103 | `	/* Save the node for later processing */` |
|  3985185 | 1104 | `	*ppNode = pNode;` |
|        - | 1105 | `	/* Synchronize cursors */` |
|  3985185 | 1106 | `	pGen->pIn = pCur;` |
|  3985185 | 1107 | `	return SXRET_OK;` |
|  1992653 | 1108 |  |
|        - | 1109 | `/*` |
|        - | 1110 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1111 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1112 | ` * level is zero.` |
|        - | 1113 | ` */` |
|    90682 | 1114 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1115 |  |
|    90687 | 1116 | `	SyToken *pCur = pStart;` |
|    90687 | 1117 | `	sxi32 iNest = 0;` |
|    90687 | 1118 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1119 | `		/* Last expression */` |
|    47507 | 1120 | `		return SXERR_EOF;` |
|        - | 1121 | `	}` |
|   177101 | 1122 | `	while( pCur < pEnd ){` |
|   161295 | 1123 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    27379 | 1124 | `			break;` |
|        - | 1125 | `		}` |
|   133921 | 1126 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     8685 | 1127 | `			iNest++;` |
|   129581 | 1128 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     8687 | 1129 | `			iNest--;` |
|     4341 | 1130 | `		}` |
|   133921 | 1131 | `		pCur++;` |
|        5 | 1132 | `	}` |
|    43185 | 1133 | `	*ppNext = pCur;` |
|    43185 | 1134 | `	return SXRET_OK;` |
|    45346 | 1135 |  |
|        - | 1136 | `/*` |
|        - | 1137 | ` * Free an expression tree.` |
|        - | 1138 | ` */` |
|  3439614 | 1139 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1140 |  |
|  3439619 | 1141 | `	if( pNode->pLeft ){` |
|        - | 1142 | `		/* Release the left tree */` |
|  1286217 | 1143 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   643106 | 1144 | `	}` |
|  3439619 | 1145 | `	if( pNode->pRight ){` |
|        - | 1146 | `		/* Release the right tree */` |
|   711289 | 1147 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   355642 | 1148 | `	}` |
|  3439619 | 1149 | `	if( pNode->pCond ){` |
|        - | 1150 | `		/* Release the conditional tree used by the ternary operator */` |
|     2645 | 1151 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1320 | 1152 | `	}` |
|  3439619 | 1153 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1154 | `		ph7_expr_node **apArg;` |
|        - | 1155 | `		sxu32 n;` |
|        - | 1156 | `		/* Release node arguments */` |
|   424275 | 1157 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   895259 | 1158 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   470989 | 1159 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   235497 | 1160 | `		}` |
|   424275 | 1161 | `		SySetRelease(&pNode->aNodeArgs);` |
|   212135 | 1162 | `	}` |
|        - | 1163 | `	/* Finally,release this node */` |
|  3439619 | 1164 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3439619 | 1165 |  |
|        - | 1166 | `/*` |
|        - | 1167 | ` * Free an expression tree.` |
|        - | 1168 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1169 | ` */` |
|   909188 | 1170 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1171 |  |
|        - | 1172 | `	ph7_expr_node **apNode;` |
|        - | 1173 | `	sxu32 n;` |
|   909193 | 1174 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  4894373 | 1175 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3985185 | 1176 | `		if( apNode[n] ){` |
|   909527 | 1177 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   454761 | 1178 | `		}` |
|  1992595 | 1179 | `	}` |
|   909193 | 1180 | `	return SXRET_OK;` |
|        5 | 1181 |  |
|        - | 1182 | `/*` |
|        - | 1183 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1184 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1185 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1186 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1187 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1188 | ` */` |
|  1271516 | 1189 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1190 |  |
|  1271521 | 1191 | `	if( pNode == 0 ){` |
|   783055 | 1192 | `		return 0;` |
|        - | 1193 | `	}` |
|   488471 | 1194 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1195 | `		return 1;` |
|        - | 1196 | `	}` |
|   488459 | 1197 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1198 | `		return 1;` |
|        - | 1199 | `	}` |
|   488455 | 1200 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1201 | `		return 1;` |
|        - | 1202 | `	}` |
|   488455 | 1203 | `	return 0;` |
|   635763 | 1204 |  |
|        - | 1205 | `/*` |
|        - | 1206 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1207 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1208 | ` */` |
|   287814 | 1209 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1210 |  |
|        - | 1211 | `	sxi32 iExprOp;` |
|   287819 | 1212 | `	if( pNode->pOp == 0 ){` |
|   173587 | 1213 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1214 | `	}` |
|   114237 | 1215 | `	iExprOp = pNode->pOp->iOp;` |
|   114237 | 1216 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    79431 | 1217 | `			return TRUE;` |
|        - | 1218 | `	}` |
|    34811 | 1219 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    34807 | 1220 | `		if( pNode->pLeft->pOp ) {` |
|       50 | 1221 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       24 | 1222 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1223 | `				return FALSE;` |
|        5 | 1224 | `			}` |
|    34782 | 1225 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1226 | `			return FALSE;` |
|        - | 1227 | `		}` |
|    34807 | 1228 | `		return TRUE;` |
|        - | 1229 | `	}` |
|        5 | 1230 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1231 | `		return TRUE;` |
|        - | 1232 | `	}` |
|        - | 1233 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1234 | `	return FALSE;` |
|   143912 | 1235 |  |
|        - | 1236 | `/* Forward declaration */` |
|        - | 1237 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1238 | `/* Macro to check if the given node is a terminal.` |
|        - | 1239 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1240 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1241 | ` * linked ternary/elvis node). */` |
|        - | 1242 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1243 | `/*` |
|        - | 1244 | ` * Buid an expression tree for each given function argument.` |
|        - | 1245 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1246 | ` */` |
|   353106 | 1247 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1248 |  |
|        - | 1249 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1250 | `	sxi32 rc;` |
|        - | 1251 | `	/* Process function arguments from left to right */` |
|   353111 | 1252 | `	iCur = 0;` |
|   376451 | 1253 | `	for(;;){` |
|   752907 | 1254 | `		if( iCur >= nToken ){` |
|        - | 1255 | `			/* No more arguments to process */` |
|   353085 | 1256 | `			break;` |
|        - | 1257 | `		}` |
|   399827 | 1258 | `		iNode = iCur;` |
|   399827 | 1259 | `		iNest = 0;` |
|   998589 | 1260 | `		while( iCur < nToken ){` |
|   645507 | 1261 | `			if( apNode[iCur] ){` |
|   631771 | 1262 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    23375 | 1263 | `					break;` |
|   601139 | 1264 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   308760 | 1265 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    32365 | 1266 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1267 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1268 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1269 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1270 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1271 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1272 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    32231 | 1273 | `					iNest++;` |
|   568918 | 1274 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    32231 | 1275 | `					iNest--;` |
|    16113 | 1276 | `				}` |
|   292513 | 1277 | `			}` |
|   598767 | 1278 | `			iCur++;` |
|        5 | 1279 | `		}` |
|   399827 | 1280 | `		if( iCur > iNode ){` |
|   399821 | 1281 | `			SyString sArgName = {0, 0};` |
|        - | 1282 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1283 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1284 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   401977 | 1285 | `			if( (iCur - iNode) >= 2` |
|   221595 | 1286 | `				&& apNode[iNode]` |
|    43374 | 1287 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    23932 | 1288 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     4406 | 1289 | `				&& apNode[iNode+1]` |
|     4327 | 1290 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1291 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      191 | 1292 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      191 | 1293 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      191 | 1294 | `				apNode[iNode] = 0;` |
|      191 | 1295 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      191 | 1296 | `				apNode[iNode+1] = 0;` |
|      191 | 1297 | `				iNode += 2;` |
|        - | 1298 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1299 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      191 | 1300 | `				if( iNode >= iCur ){` |
|        4 | 1301 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1302 | `						pOp->pStart->nLine,` |
|        - | 1303 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1304 | `						&sArgName);` |
|        3 | 1305 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1306 | `						rc = SXERR_SYNTAX;` |
|        1 | 1307 | `					}` |
|        3 | 1308 | `					return rc;` |
|        - | 1309 | `				}` |
|       92 | 1310 | `			}` |
|   399814 | 1311 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1312 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1313 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1314 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1315 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1316 | `					apNode[iNode] = 0;` |
|      ! 0 | 1317 | `			}` |
|   399819 | 1318 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   399819 | 1319 | `			if( apNode[iNode] ){` |
|   399819 | 1320 | `				if( sArgName.nByte > 0 ){` |
|      188 | 1321 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      188 | 1322 | `					apNode[iNode]->sArgName = sArgName;` |
|       92 | 1323 | `				}` |
|        - | 1324 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   399819 | 1325 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   199912 | 1326 | `			}else{` |
|        - | 1327 | `				/* No expression before comma */` |
|      ! 0 | 1328 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1329 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1330 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1331 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1332 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1333 | `				}` |
|      ! 0 | 1334 | `				return rc;` |
|        - | 1335 | `			}` |
|   199912 | 1336 | `		}else{` |
|        - | 1337 | `			/* Comma with no preceding argument */` |
|        8 | 1338 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        8 | 1339 | `			if( rc != SXERR_ABORT ){` |
|        8 | 1340 | `				rc = SXERR_SYNTAX;` |
|        3 | 1341 | `			}` |
|        8 | 1342 | `			return rc;` |
|        - | 1343 | `		}` |
|        - | 1344 | `		/* Jump trailing comma */` |
|   399819 | 1345 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    46739 | 1346 | `			iCur++;` |
|    46739 | 1347 | `			if( iCur >= nToken ){` |
|        - | 1348 | `				/* Trailing comma after last argument */` |
|       19 | 1349 | `				break;` |
|        - | 1350 | `			}` |
|    23358 | 1351 | `		}` |
|        5 | 1352 | `	}` |
|   353103 | 1353 | `	return SXRET_OK;` |
|   176558 | 1354 |  |
|        - | 1355 | ` /*` |
|        - | 1356 | `  * Create an expression tree from an array of tokens.` |
|        - | 1357 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1358 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1359 | `  */` |
|  1418366 | 1360 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1361 | ` {` |
|        - | 1362 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1363 | `	 ph7_expr_node *pNode;` |
|        - | 1364 | `	 sxi32 iCur;` |
|        - | 1365 | `	 sxi32 rc;` |
|  1418371 | 1366 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1367 | `		 /* TICKET 1433-17: self evaluating node */` |
|   642603 | 1368 | `		 return SXRET_OK;` |
|        - | 1369 | `	 }` |
|        - | 1370 | `	 /* Process expressions enclosed in parenthesis first */` |
|  4764493 | 1371 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1372 | `		 sxi32 iNest;` |
|        - | 1373 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1374 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1375 | `		  */` |
|  3988727 | 1376 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3959429 | 1377 | `			 continue;` |
|        - | 1378 | `		 }` |
|    29303 | 1379 | `		 iNest = 1;` |
|    29303 | 1380 | `		 iLeft = iCur;` |
|        - | 1381 | `		 /* Find the closing parenthesis */` |
|    29303 | 1382 | `		 iCur++;` |
|   195525 | 1383 | `		 while( iCur < nToken ){` |
|   195525 | 1384 | `			 if( apNode[iCur] ){` |
|   195525 | 1385 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1386 | `					 /* Decrement nesting level */` |
|    50839 | 1387 | `					 iNest--;` |
|    50839 | 1388 | `					 if( iNest <= 0 ){` |
|    29303 | 1389 | `						 break;` |
|        5 | 1390 | `					 }` |
|   155459 | 1391 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1392 | `					 /* Increment nesting level */` |
|    21541 | 1393 | `					 iNest++;` |
|    10768 | 1394 | `				 }` |
|    83111 | 1395 | `			 }` |
|   166227 | 1396 | `			 iCur++;` |
|        5 | 1397 | `		 }` |
|    29303 | 1398 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1399 | `			 sxi32 j;` |
|        - | 1400 | `			 /* Recurse and process this expression */` |
|    29303 | 1401 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    29303 | 1402 | `			 if( rc != SXRET_OK ){` |
|        3 | 1403 | `				 return rc;` |
|        - | 1404 | `			 }` |
|        - | 1405 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1406 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1407 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    29301 | 1408 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    29301 | 1409 | `				 if( apNode[j] ){` |
|    29301 | 1410 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    29301 | 1411 | `					 break;` |
|        - | 1412 | `				 }` |
|      ! 0 | 1413 | `			 }` |
|    14648 | 1414 | `		 }` |
|        - | 1415 | `		 /* Free the left and right nodes */` |
|    29301 | 1416 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    29301 | 1417 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    29301 | 1418 | `		 apNode[iLeft] = 0;` |
|    29301 | 1419 | `		 apNode[iCur] = 0;` |
|    14653 | 1420 | `	 }` |
|        - | 1421 | `	  /* Process expressions enclosed in braces */` |
|  4952463 | 1422 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1423 | `		 sxi32 iNest;` |
|        - | 1424 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1425 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1426 | `		  */` |
|  4184229 | 1427 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4184225 | 1428 | `			 continue;` |
|        - | 1429 | `		 }` |
|        6 | 1430 | `		 iNest = 1;` |
|        6 | 1431 | `		 iLeft = iCur;` |
|        - | 1432 | `		 /* Find the closing parenthesis */` |
|        6 | 1433 | `		 iCur++;` |
|        8 | 1434 | `		 while( iCur < nToken ){` |
|        8 | 1435 | `			 if( apNode[iCur] ){` |
|        8 | 1436 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1437 | `					 /* Decrement nesting level */` |
|        6 | 1438 | `					 iNest--;` |
|        6 | 1439 | `					 if( iNest <= 0 ){` |
|        6 | 1440 | `						 break;` |
|      ! 0 | 1441 | `					 }` |
|        3 | 1442 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1443 | `					 /* Increment nesting level */` |
|      ! 0 | 1444 | `					 iNest++;` |
|      ! 0 | 1445 | `				 }` |
|        1 | 1446 | `			 }` |
|        3 | 1447 | `			 iCur++;` |
|        1 | 1448 | `		 }` |
|        6 | 1449 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1450 | `			 /* Recurse and process this expression */` |
|        3 | 1451 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        3 | 1452 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1453 | `				 return rc;` |
|        - | 1454 | `			 }` |
|        1 | 1455 | `		 }` |
|        - | 1456 | `		 /* Free the left and right nodes */` |
|        6 | 1457 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        6 | 1458 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        6 | 1459 | `		 apNode[iLeft] = 0;` |
|        6 | 1460 | `		 apNode[iCur] = 0;` |
|        4 | 1461 | `	 }` |
|        - | 1462 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   768239 | 1463 | `	 iLeft = -1;` |
|  4952433 | 1464 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4184211 | 1465 | `		 if( apNode[iCur] == 0 ){` |
|  1597225 | 1466 | `			 continue;` |
|        - | 1467 | `		 }` |
|  2586991 | 1468 | `		 pNode = apNode[iCur];` |
|  2586991 | 1469 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   684169 | 1470 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1471 | `				 /* Collect function arguments */` |
|   407775 | 1472 | `				 sxi32 iPtr = 0;` |
|   407775 | 1473 | `				 sxi32 nFuncTok = 0;` |
|  1461049 | 1474 | `				 while( nFuncTok + iCur < nToken ){` |
|  1461049 | 1475 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1447313 | 1476 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   422389 | 1477 | `							 iPtr++;` |
|  1236121 | 1478 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   422389 | 1479 | `							 iPtr--;` |
|   422389 | 1480 | `							 if( iPtr <= 0 ){` |
|   407775 | 1481 | `								 break;` |
|        - | 1482 | `							 }` |
|     7307 | 1483 | `						 }` |
|   519769 | 1484 | `					 }` |
|  1053279 | 1485 | `					 nFuncTok++;` |
|        5 | 1486 | `				 }` |
|   407775 | 1487 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1488 | `					 /* Syntax error */` |
|      ! 0 | 1489 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1490 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1491 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1492 | `					 }` |
|      ! 0 | 1493 | `					 return rc;` |
|        - | 1494 | `				 }` |
|   407775 | 1495 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1496 | `					 /* Syntax error */` |
|      ! 0 | 1497 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1498 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1499 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1500 | `					 }` |
|      ! 0 | 1501 | `					 return rc;` |
|        - | 1502 | `				 }` |
|   407775 | 1503 | `				 if( nFuncTok > 1 ){` |
|        - | 1504 | `					 /* Process function arguments */` |
|   353111 | 1505 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   353111 | 1506 | `					 if( rc != SXRET_OK ){` |
|       11 | 1507 | `						 return rc;` |
|        - | 1508 | `					 }` |
|   176549 | 1509 | `				 }` |
|        - | 1510 | `				 /* Link the node to the tree */` |
|   407767 | 1511 | `				 pNode->pLeft = apNode[iLeft];` |
|   407767 | 1512 | `				 apNode[iLeft] = 0;` |
|  1461017 | 1513 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1053255 | 1514 | `					 apNode[iCur+iPtr] = 0;` |
|   526630 | 1515 | `				 }` |
|   480280 | 1516 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1517 | `				 /* Subscripting */` |
|    88439 | 1518 | `				 sxi32 iArrTok = iCur + 1;` |
|    88439 | 1519 | `				 sxi32 iNest = 1;` |
|    88635 | 1520 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1521 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1522 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       13 | 1523 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    88434 | 1524 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1525 | `						 /* Syntax error */` |
|      ! 0 | 1526 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1527 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1528 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1529 | `						 }` |
|      ! 0 | 1530 | `						 return rc;` |
|        - | 1531 | `				 }` |
|        - | 1532 | `				 /* Collect index tokens */` |
|   159731 | 1533 | `				 while( iArrTok < nToken ){` |
|   159731 | 1534 | `					 if( apNode[iArrTok] ){` |
|   159699 | 1535 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1536 | `							 /* Increment nesting level */` |
|      ! 0 | 1537 | `							 iNest++;` |
|   159699 | 1538 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1539 | `							 /* Decrement nesting level */` |
|    88439 | 1540 | `							 iNest--;` |
|    88439 | 1541 | `							 if( iNest <= 0 ){` |
|    88439 | 1542 | `								 break;` |
|        - | 1543 | `							 }` |
|      ! 0 | 1544 | `						 }` |
|    35630 | 1545 | `					 }` |
|    71297 | 1546 | `					 ++iArrTok;` |
|        5 | 1547 | `				 }` |
|    88439 | 1548 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1549 | `					 /* Recurse and process this expression */` |
|    71175 | 1550 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    71175 | 1551 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1552 | `						 return rc;` |
|        - | 1553 | `					 }` |
|        - | 1554 | `					 /* Link the node to it's index */` |
|    71175 | 1555 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    35585 | 1556 | `				 }` |
|        - | 1557 | `				 /* Link the node to the tree */` |
|    88439 | 1558 | `				 pNode->pLeft = apNode[iLeft];` |
|    88439 | 1559 | `				 pNode->pRight = 0;` |
|    88439 | 1560 | `				 apNode[iLeft] = 0;` |
|   248165 | 1561 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   159731 | 1562 | `					 apNode[iNest] = 0;` |
|    79868 | 1563 | `				 }` |
|    44222 | 1564 | `			 }else{` |
|        - | 1565 | `				 /* Member access operators [i.e: '->','::'] */` |
|   187965 | 1566 | `				  iRight = iCur + 1;` |
|   187967 | 1567 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        3 | 1568 | `					 iRight++;` |
|        1 | 1569 | `				 }` |
|   187965 | 1570 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1571 | `					 /* Syntax error */` |
|        5 | 1572 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1573 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1574 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1575 | `					 }` |
|        5 | 1576 | `					 return rc;` |
|        - | 1577 | `				 }` |
|        - | 1578 | `				 /* Link the node to the tree */` |
|   187961 | 1579 | `				 pNode->pLeft = apNode[iLeft];` |
|   281746 | 1580 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   187818 | 1581 | `					 && pNode->pLeft->pOp == 0 &&` |
|   187580 | 1582 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1583 | `						 /* Syntax error */` |
|      ! 0 | 1584 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1585 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1586 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1587 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1588 | `						 }` |
|      ! 0 | 1589 | `						 return rc;` |
|        - | 1590 | `				 }` |
|   187961 | 1591 | `				 pNode->pRight = apNode[iRight];` |
|   187961 | 1592 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1593 | `			 }` |
|   342076 | 1594 | `		 }` |
|  2586979 | 1595 | `		 iLeft = iCur;` |
|  1293492 | 1596 | `	 }` |
|        - | 1597 | `	 /* Handle left associative (new, clone) operators */` |
|  4952401 | 1598 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4184179 | 1599 | `		 if( apNode[iCur] == 0 ){` |
|  2299857 | 1600 | `			 continue;` |
|        - | 1601 | `		 }` |
|  1884327 | 1602 | `		 pNode = apNode[iCur];` |
|  1884327 | 1603 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1604 | `			 SyToken *pToken;` |
|        - | 1605 | `			 /* Get the left node */` |
|    18485 | 1606 | `			 iLeft = iCur + 1;` |
|    36823 | 1607 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    18343 | 1608 | `				 iLeft++;` |
|        5 | 1609 | `			 }` |
|    18485 | 1610 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1611 | `				  /* Syntax error */` |
|      ! 0 | 1612 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1613 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1614 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1615 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1616 | `				 }` |
|      ! 0 | 1617 | `				 return rc;` |
|        - | 1618 | `			 }` |
|        - | 1619 | `			 /* Make sure the operand are of a valid type */` |
|    18485 | 1620 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1621 | `				 /* Clone:` |
|        - | 1622 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1623 | `				  *  ++ function call (including annonymous)` |
|        - | 1624 | `				  *  ++ array member` |
|        - | 1625 | `				  *  ++ 'new' operator` |
|        - | 1626 | `				  * Example:` |
|        - | 1627 | `				  *   clone $pObj;` |
|        - | 1628 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1629 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1630 | `				  */` |
|       30 | 1631 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       28 | 1632 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1633 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1634 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1635 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1636 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1637 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1638 | `						 }` |
|      ! 0 | 1639 | `						 return rc;` |
|        - | 1640 | `					 }` |
|       12 | 1641 | `				 }` |
|       17 | 1642 | `			 }else{` |
|        - | 1643 | `				 /* New */` |
|    18459 | 1644 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      123 | 1645 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      118 | 1646 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       25 | 1647 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1648 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1649 | `						 /* Syntax error */` |
|      ! 0 | 1650 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1651 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1652 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1653 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1654 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1655 | `						 }` |
|      ! 0 | 1656 | `						 return rc;` |
|        - | 1657 | `					 }` |
|       59 | 1658 | `				 }` |
|        - | 1659 | `			 }` |
|        - | 1660 | `			  /* Link the node to the tree */` |
|    18485 | 1661 | `			 pNode->pLeft = apNode[iLeft];` |
|    18485 | 1662 | `			 apNode[iLeft] = 0;` |
|    18485 | 1663 | `			 pNode->pRight = 0; /* Paranoid */` |
|     9240 | 1664 | `		 }` |
|   942166 | 1665 | `	 }` |
|        - | 1666 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   768227 | 1667 | `	 iLeft = -1;` |
|  4956167 | 1668 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4184179 | 1669 | `		 if( apNode[iCur] == 0 ){` |
|  2299857 | 1670 | `			 continue;` |
|        - | 1671 | `		 }` |
|  1884327 | 1672 | `		 pNode = apNode[iCur];` |
|  1884327 | 1673 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    10369 | 1674 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3802 | 1675 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1676 | `					 /* Link the node to the tree */` |
|     3813 | 1677 | `					 pNode->pLeft = apNode[iLeft];` |
|     3813 | 1678 | `					 apNode[iLeft] = 0;` |
|     1904 | 1679 | `			 }` |
|     7065 | 1680 | `		  }` |
|  1888093 | 1681 | `		 iLeft = iCur;` |
|   945932 | 1682 | `	  }` |
|   771993 | 1683 | `	 iLeft = -1;` |
|  4956167 | 1684 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4184179 | 1685 | `		 if( apNode[iCur] == 0 ){` |
|  2303665 | 1686 | `			 continue;` |
|        - | 1687 | `		 }` |
|  1880519 | 1688 | `		 pNode = apNode[iCur];` |
|  1880519 | 1689 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    10325 | 1690 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    10327 | 1691 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1692 | `					 /* Syntax error */` |
|      ! 0 | 1693 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1694 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1695 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1696 | `					 }` |
|      ! 0 | 1697 | `					 return rc;` |
|        - | 1698 | `			 }` |
|        - | 1699 | `			 /* Link the node to the tree */` |
|    10327 | 1700 | `			 pNode->pLeft = apNode[iLeft];` |
|    10327 | 1701 | `			 apNode[iLeft] = 0;` |
|        - | 1702 | `			 /* Mark as pre-increment/decrement node */` |
|    10327 | 1703 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5161 | 1704 | `		  }` |
|  1880519 | 1705 | `		 iLeft = iCur;` |
|   940262 | 1706 | `	 }` |
|        - | 1707 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   771993 | 1708 | `	  iLeft = 0;` |
|  4956161 | 1709 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4184175 | 1710 | `		  if( apNode[iCur] ){` |
|  1870193 | 1711 | `			  pNode = apNode[iCur];` |
|  1870193 | 1712 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    46197 | 1713 | `				  if( iLeft > 0 ){` |
|        - | 1714 | `					  /* Link the node to the tree */` |
|    46195 | 1715 | `					  pNode->pLeft = apNode[iLeft];` |
|    46195 | 1716 | `					  apNode[iLeft] = 0;` |
|    46195 | 1717 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1718 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1719 | `							   /* Syntax error */` |
|      ! 0 | 1720 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1721 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1722 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1723 | `							  }` |
|      ! 0 | 1724 | `							  return rc;` |
|        - | 1725 | `						  }` |
|       36 | 1726 | `					  }` |
|    23100 | 1727 | `				  }else{` |
|        - | 1728 | `					  /* Syntax error */` |
|        3 | 1729 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1730 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1731 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1732 | `					  }` |
|        3 | 1733 | `					  return rc;` |
|        - | 1734 | `				  }` |
|    23095 | 1735 | `			  }` |
|        - | 1736 | `			  /* Save terminal position */` |
|  1870191 | 1737 | `			  iLeft = iCur;` |
|   935093 | 1738 | `		  }` |
|  2092089 | 1739 | `	  }` |
|        - | 1740 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1741 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1742 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1743 | `	  * yielding a right-leaning tree. */` |
|  4956159 | 1744 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4184173 | 1745 | `		 if( apNode[iCur] == 0 ){` |
|  2360289 | 1746 | `			 continue;` |
|        - | 1747 | `		 }` |
|  1823889 | 1748 | `		 pNode = apNode[iCur];` |
|  1823889 | 1749 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1750 | `			 sxi32 iL, iR;` |
|        - | 1751 | `			 /* Find the right operand */` |
|      113 | 1752 | `			 iR = -1;` |
|        - | 1753 | `			 {` |
|        - | 1754 | `				 sxi32 j;` |
|      125 | 1755 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1756 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1757 | `				 }` |
|        - | 1758 | `			 }` |
|        - | 1759 | `			 /* Find the left operand */` |
|      113 | 1760 | `			 iL = -1;` |
|        - | 1761 | `			 {` |
|        - | 1762 | `				 sxi32 j;` |
|      181 | 1763 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1764 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1765 | `				 }` |
|        - | 1766 | `			 }` |
|      113 | 1767 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1768 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1769 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1770 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1771 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1772 | `				 }` |
|      ! 0 | 1773 | `				 return rc;` |
|        - | 1774 | `			 }` |
|      113 | 1775 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1776 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1777 | `			 apNode[iL] = 0;` |
|      113 | 1778 | `			 apNode[iR] = 0;` |
|        - | 1779 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1780 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1781 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1782 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1783 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1784 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1785 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1786 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1787 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1788 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1789 | `			  * operands are respected. */` |
|      129 | 1790 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1791 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1792 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1793 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1794 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1795 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1796 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1797 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1798 | `				 while( pTail->pLeft` |
|       34 | 1799 | `					 && pTail->pLeft->pOp` |
|       23 | 1800 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1801 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1802 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1803 | `					 pTail = pTail->pLeft;` |
|        1 | 1804 | `				 }` |
|        - | 1805 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1806 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1807 | `				 pTail->pLeft = pNode;` |
|       27 | 1808 | `				 apNode[iCur] = pHead;` |
|       13 | 1809 | `			 }` |
|       56 | 1810 | `		 }` |
|   911947 | 1811 | `	 }` |
|        - | 1812 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  8491765 | 1813 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  7719789 | 1814 | `		 iLeft = -1;` |
| 49561175 | 1815 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 41841401 | 1816 | `			 if( apNode[iCur] == 0 ){` |
| 26717689 | 1817 | `				 continue;` |
|        - | 1818 | `			 }` |
| 15123717 | 1819 | `			 pNode = apNode[iCur];` |
| 15123717 | 1820 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1821 | `				 /* Get the right node */` |
|   232799 | 1822 | `				 iRight = iCur + 1;` |
|   332533 | 1823 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    99739 | 1824 | `					 iRight++;` |
|        5 | 1825 | `				 }` |
|   232799 | 1826 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1827 | `					 /* Syntax error */` |
|       10 | 1828 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       10 | 1829 | `					 if( rc != SXERR_ABORT ){` |
|       10 | 1830 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1831 | `					 }` |
|       10 | 1832 | `					 return rc;` |
|        - | 1833 | `				 }` |
|   232791 | 1834 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1835 | `					 sxi32  iTmp;` |
|        - | 1836 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1837 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1838 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1839 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1840 | `					  * is swapped below. */` |
|       57 | 1841 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1842 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1843 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1844 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1845 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1846 | `						 }` |
|        3 | 1847 | `						 return rc;` |
|        - | 1848 | `					 }` |
|       54 | 1849 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1850 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1851 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1852 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1853 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1854 | `						 }` |
|      ! 0 | 1855 | `						 return rc;` |
|        - | 1856 | `					 }` |
|       54 | 1857 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       38 | 1858 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1859 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1860 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1861 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1862 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1863 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1864 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1865 | `									 }` |
|      ! 0 | 1866 | `									 return rc;` |
|        - | 1867 | `							 }` |
|      ! 0 | 1868 | `						 }` |
|       18 | 1869 | `					 }` |
|        - | 1870 | `					 /* Swap operands */` |
|       54 | 1871 | `					 iTmp = iRight;` |
|       54 | 1872 | `					 iRight = iLeft;` |
|       54 | 1873 | `					 iLeft = iTmp;` |
|       26 | 1874 | `				 }` |
|        - | 1875 | `				 /* Link the node to the tree */` |
|   232789 | 1876 | `				 pNode->pLeft = apNode[iLeft];` |
|   232789 | 1877 | `				 pNode->pRight = apNode[iRight];` |
|   232789 | 1878 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   116392 | 1879 | `			 }` |
| 15123707 | 1880 | `			 iLeft = iCur;` |
|  7561856 | 1881 | `		 }` |
|  3859892 | 1882 | `	 }` |
|        - | 1883 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1884 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1885 | `	  * we are dealing with a single operator.` |
|        - | 1886 | `	  */` |
|   771981 | 1887 | `	  iLeft = -1;` |
|  4944827 | 1888 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4175493 | 1889 | `		  if( apNode[iCur] == 0 ){` |
|  2825199 | 1890 | `			  continue;` |
|        - | 1891 | `		  }` |
|  1350299 | 1892 | `		  pNode = apNode[iCur];` |
|  1350299 | 1893 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2647 | 1894 | `			  sxi32 iNest = 1;` |
|     2647 | 1895 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1896 | `				  /* Missing condition */` |
|        3 | 1897 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1898 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1899 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1900 | `				  }` |
|        3 | 1901 | `				  return rc;` |
|        - | 1902 | `			  }` |
|        - | 1903 | `			  /* Get the right node */` |
|     2645 | 1904 | `			  iRight = iCur + 1;` |
|     5539 | 1905 | `			  while( iRight < nToken  ){` |
|     5539 | 1906 | `				  if( apNode[iRight] ){` |
|     5217 | 1907 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1908 | `						  /* Increment nesting level */` |
|      ! 0 | 1909 | `						  ++iNest;` |
|     5217 | 1910 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1911 | `						  /* Decrement nesting level */` |
|     2645 | 1912 | `						  --iNest;` |
|     2645 | 1913 | `						  if( iNest <= 0 ){` |
|     2645 | 1914 | `							  break;` |
|        - | 1915 | `						  }` |
|      ! 0 | 1916 | `					  }` |
|     1286 | 1917 | `				  }` |
|     2899 | 1918 | `				  iRight++;` |
|        5 | 1919 | `			  }` |
|     2645 | 1920 | `			  if( iRight > iCur + 1 ){` |
|        - | 1921 | `				  /* Recurse and process the then expression */` |
|     2577 | 1922 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2577 | 1923 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1924 | `					  return rc;` |
|        - | 1925 | `				  }` |
|        - | 1926 | `				  /* Link the node to the tree */` |
|     2577 | 1927 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1286 | 1928 | `			  }else{` |
|        - | 1929 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1930 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1931 | `			  }` |
|     2645 | 1932 | `			  apNode[iCur + 1] = 0;` |
|     2645 | 1933 | `			  if( iRight + 1 < nToken ){` |
|        - | 1934 | `				  /* Recurse and process the else expression */` |
|     2645 | 1935 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2645 | 1936 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1937 | `					  return rc;` |
|        - | 1938 | `				  }` |
|        - | 1939 | `				  /* Link the node to the tree */` |
|     2645 | 1940 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2645 | 1941 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1325 | 1942 | `			  }else{` |
|      ! 0 | 1943 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1944 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1945 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1946 | `				 }` |
|      ! 0 | 1947 | `				 return rc;` |
|        - | 1948 | `			  }` |
|        - | 1949 | `			  /* Point to the condition */` |
|     2645 | 1950 | `			  pNode->pCond  = apNode[iLeft];` |
|     2645 | 1951 | `			  apNode[iLeft] = 0;` |
|     2645 | 1952 | `			  break;` |
|        - | 1953 | `		  }` |
|  1347657 | 1954 | `		  iLeft = iCur;` |
|   673831 | 1955 | `	  }` |
|        - | 1956 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1957 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1958 | `	  * so there is no need for a precedence loop here.` |
|        - | 1959 | `	  */` |
|   771979 | 1960 | `	 iRight = -1;` |
|  4955963 | 1961 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4184043 | 1962 | `		 if( apNode[iCur] == 0 ){` |
|  3124171 | 1963 | `			 continue;` |
|        - | 1964 | `		 }` |
|  1059877 | 1965 | `		 pNode = apNode[iCur];` |
|  1059877 | 1966 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1967 | `			 /* Get the left node */` |
|   287781 | 1968 | `			 iLeft = iCur - 1;` |
|   419613 | 1969 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   131837 | 1970 | `				 iLeft--;` |
|        5 | 1971 | `			 }` |
|   287781 | 1972 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1973 | `				 /* Syntax error */` |
|       45 | 1974 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1975 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 1976 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1977 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 1978 | `				 }else{` |
|       41 | 1979 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1980 | `				 }` |
|       45 | 1981 | `				 if( rc != SXERR_ABORT ){` |
|       43 | 1982 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1983 | `				 }` |
|       45 | 1984 | `				 return rc;` |
|        - | 1985 | `			 }` |
|        - | 1986 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1987 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1988 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1989 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1990 | `			  * a write. */` |
|   287739 | 1991 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 1992 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1993 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 1994 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 1995 | `					 rc = SXERR_SYNTAX;` |
|        4 | 1996 | `				 }` |
|       11 | 1997 | `				 return rc;` |
|        - | 1998 | `			 }` |
|   287731 | 1999 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      101 | 2000 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       72 | 2001 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2002 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2003 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2004 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2005 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2006 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2007 | `					 }else{` |
|        4 | 2008 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2009 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2010 | `					 }` |
|        6 | 2011 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2012 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2013 | `					 }` |
|        6 | 2014 | `					 return rc;` |
|        - | 2015 | `				 }` |
|       35 | 2016 | `			 }` |
|        - | 2017 | `			 /* Link the node to the tree (Reverse) */` |
|   287727 | 2018 | `			 pNode->pLeft = apNode[iRight];` |
|   287727 | 2019 | `			 pNode->pRight = apNode[iLeft];` |
|   287727 | 2020 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   143861 | 2021 | `		 }` |
|  1059823 | 2022 | `		 iRight = iCur;` |
|   529914 | 2023 | `	 }` |
|        - | 2024 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3859605 | 2025 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3087685 | 2026 | `		 iLeft = -1;` |
| 19823565 | 2027 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 16735885 | 2028 | `			 if( apNode[iCur] == 0 ){` |
| 13647799 | 2029 | `				 continue;` |
|        - | 2030 | `			 }` |
|  3088091 | 2031 | `			 pNode = apNode[iCur];` |
|  3088091 | 2032 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2033 | `				 /* Get the right node */` |
|       72 | 2034 | `				 iRight = iCur + 1;` |
|      110 | 2035 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2036 | `					 iRight++;` |
|        2 | 2037 | `				 }` |
|       72 | 2038 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2039 | `					 /* Syntax error */` |
|      ! 0 | 2040 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2041 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2042 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2043 | `					 }` |
|      ! 0 | 2044 | `					 return rc;` |
|        - | 2045 | `				 }` |
|        - | 2046 | `				 /* Link the node to the tree */` |
|       72 | 2047 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2048 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2049 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2050 | `			 }` |
|  3088091 | 2051 | `			 iLeft = iCur;` |
|  1544048 | 2052 | `		 }` |
|  1543845 | 2053 | `	 }` |
|        - | 2054 | `	 /* Point to the root of the expression tree */` |
|  4183947 | 2055 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3412045 | 2056 | `		 if( apNode[iCur] ){` |
|   701913 | 2057 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       23 | 2058 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       23 | 2059 | `				  if( rc != SXERR_ABORT ){` |
|       23 | 2060 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2061 | `				  }` |
|       23 | 2062 | `				  return rc;` |
|        - | 2063 | `			 }` |
|   701895 | 2064 | `			 apNode[0] = apNode[iCur];` |
|   701895 | 2065 | `			 apNode[iCur] = 0;` |
|   350945 | 2066 | `		 }` |
|  1706016 | 2067 | `	 }` |
|   771907 | 2068 | `	 return SXRET_OK;` |
|   707305 | 2069 | ` }` |
|        - | 2070 | ` /*` |
|        - | 2071 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2072 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2073 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2074 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2075 | `  */` |
|   909188 | 2076 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2077 |  |
|        - | 2078 | `	ph7_expr_node **apNode;` |
|        - | 2079 | `	ph7_expr_node *pNode;` |
|        - | 2080 | `	sxi32 rc;` |
|        - | 2081 | `	/* Reset node container */` |
|   909193 | 2082 | `	SySetReset(pExprNode);` |
|   909193 | 2083 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2084 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2085 | `	{` |
|   909193 | 2086 | `		int iLastWasTerm = 0;` |
|  4894373 | 2087 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3985219 | 2088 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3985219 | 2089 | `			if( rc != SXRET_OK ){` |
|       38 | 2090 | `				return rc;` |
|        - | 2091 | `			}` |
|        - | 2092 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3985185 | 2093 | `			if( pNode->xCode ){` |
|        - | 2094 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2094301 | 2095 | `				iLastWasTerm = 1;` |
|  2938037 | 2096 | `			}else if( pNode->pOp ){` |
|        - | 2097 | `				/* Operator node */` |
|   925401 | 2098 | `				iLastWasTerm = 0;` |
|   462703 | 2099 | `			}else{` |
|        - | 2100 | `				/* Delimiter: ')' and ']' end terms */` |
|   965493 | 2101 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2102 | `			}` |
|        - | 2103 | `			/* Save the extracted node */` |
|  3985185 | 2104 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2105 | `		}` |
|        - | 2106 | `	}` |
|   909159 | 2107 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2108 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2109 | `		*ppRoot = 0;` |
|      ! 0 | 2110 | `		return SXRET_OK;` |
|        - | 2111 | `	}` |
|   909159 | 2112 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2113 | `	/* Make sure we are dealing with valid nodes */` |
|   909159 | 2114 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   909159 | 2115 | `	if( rc != SXRET_OK ){` |
|        - | 2116 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2117 | `		 * cleanup the mess left behind.` |
|        - | 2118 | `		 */` |
|       54 | 2119 | `		*ppRoot = 0;` |
|       54 | 2120 | `		return rc;` |
|        - | 2121 | `	}` |
|        - | 2122 | `	/* Build the tree */` |
|   909109 | 2123 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   909109 | 2124 | `	if( rc != SXRET_OK ){` |
|        - | 2125 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2126 | `		*ppRoot = 0;` |
|      103 | 2127 | `		return rc;` |
|        - | 2128 | `	}` |
|        - | 2129 | `	/* Point to the root of the tree */` |
|   909011 | 2130 | `	*ppRoot = apNode[0];` |
|   909011 | 2131 | `	return SXRET_OK;` |
|   454599 | 2132 |  |
|        - | 2133 |  |
