# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1070/1231 lines (86.92%)

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
|   981370 |  268 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  269 |  |
|   981372 |  270 | `	sxu32 n = 0;` |
|        - |  271 | `	sxi32 rc;` |
|        - |  272 | `	/* Do a linear lookup on the operators table */` |
| 16480719 |  273 | `	for(;;){` |
| 32961440 |  274 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  275 | `			break;` |
|        - |  276 | `		}` |
| 32961440 |  277 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  278 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3870740 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1935371 |  280 | `		}else{` |
| 29090702 |  281 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  282 | `		}` |
| 32961440 |  283 | `		if( rc == 0 ){` |
|   985170 |  284 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  285 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   980994 |  286 | `				return &aOpTable[n];` |
|        - |  287 | `			}` |
|        - |  288 | `			/* Handle ambiguity */` |
|     4178 |  289 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  290 | `				/* Unary opertors have prcedence here over binary operators */` |
|      248 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|     3932 |  293 | `			if( pLast->nType & PH7_TK_OP ){` |
|      142 |  294 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  295 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      142 |  296 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  297 | `					/* Unary opertors have prcedence here over binary operators */` |
|      134 |  298 | `					return &aOpTable[n];` |
|        - |  299 | `				}` |
|        - |  300 |  |
|        4 |  301 | `			}` |
|     1899 |  302 | `		}` |
| 31980070 |  303 | `		++n; /* Next operator in the table */` |
|        2 |  304 | `	}` |
|        - |  305 | `	/* No such operator */` |
|      ! 0 |  306 | `	return 0;` |
|   490687 |  307 |  |
|        - |  308 | `/*` |
|        - |  309 | ` * Delimit a set of token stream.` |
|        - |  310 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  311 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  312 | ` */` |
|   555424 |  313 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  314 |  |
|   555426 |  315 | `	SyToken *pCur = pIn;` |
|   555426 |  316 | `	sxi32 iNest = 1;` |
|  3216287 |  317 | `	for(;;){` |
|  6432576 |  318 | `		if( pCur >= pEnd ){` |
|      172 |  319 | `			break;` |
|        - |  320 | `		}` |
|  6432406 |  321 | `		if( pCur->nType & nTokStart ){` |
|        - |  322 | `			/* Increment nesting level */` |
|   320506 |  323 | `			iNest++;` |
|  6272154 |  324 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  325 | `			/* Decrement nesting level */` |
|   875760 |  326 | `			iNest--;` |
|   875760 |  327 | `			if( iNest <= 0 ){` |
|   555256 |  328 | `				break;` |
|        - |  329 | `			}` |
|   160252 |  330 | `		}` |
|        - |  331 | `		/* Advance cursor */` |
|  5877152 |  332 | `		pCur++;` |
|        2 |  333 | `	}` |
|        - |  334 | `	/* Point to the end of the chunk */` |
|   555426 |  335 | `	*ppEnd = pCur;` |
|   555426 |  336 |  |
|        - |  337 | `/*` |
|        - |  338 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  339 | ` * Note on reserved keywords.` |
|        - |  340 | ` *  According to the PHP language reference manual:` |
|        - |  341 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  342 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  343 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  344 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  345 | ` */` |
|    19636 |  346 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  347 |  |
|    29382 |  348 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    19537 |  349 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  350 | `		){` |
|      158 |  351 | `			return TRUE;` |
|        - |  352 | `	}` |
|    19482 |  353 | `	if( bCheckFunc ){` |
|      100 |  354 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       73 |  355 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       57 |  356 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       36 |  357 | `				return TRUE;` |
|        - |  358 | `		}` |
|       22 |  359 | `	}` |
|        - |  360 | `	/* Not a language construct */` |
|    19448 |  361 | `	return FALSE;` |
|     9820 |  362 |  |
|        - |  363 | `/*` |
|        - |  364 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  365 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  366 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  367 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  368 | ` */` |
|   829726 |  369 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  370 |  |
|        - |  371 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  372 | `	sxi32 i,rc;` |
|        - |  373 |  |
|   829728 |  374 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  375 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       28 |  376 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       28 |  377 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       13 |  378 | `	}` |
|   829728 |  379 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  4467050 |  380 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3637358 |  381 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  382 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      726 |  383 | `			continue;` |
|        - |  384 | `		}` |
|  3636634 |  385 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   398756 |  386 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    19944 |  387 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  388 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   372006 |  389 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  390 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  391 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  392 | `						 */` |
|   372006 |  393 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   372006 |  394 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   372006 |  395 | `						apNode[i]->pOp = &sFCallOp;` |
|   186002 |  396 | `					}` |
|   186002 |  397 | `			}` |
|   398756 |  398 | `			iParen++;` |
|  3437257 |  399 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   398756 |  400 | `			if( iParen <= 0 ){` |
|       13 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  402 | `				if( rc != SXERR_ABORT ){` |
|       13 |  403 | `					rc = SXERR_SYNTAX;` |
|        6 |  404 | `				}` |
|       13 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|   398744 |  407 | `			iParen--;` |
|  3038497 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    80708 |  409 | `			iSquare++;` |
|  2798773 |  410 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    80722 |  411 | `			if( iSquare <= 0 ){` |
|        7 |  412 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  413 | `				if( rc != SXERR_ABORT ){` |
|        7 |  414 | `					rc = SXERR_SYNTAX;` |
|        3 |  415 | `				}` |
|        7 |  416 | `				return rc;` |
|        - |  417 | `			}` |
|    80716 |  418 | `			iSquare--;` |
|  2718057 |  419 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2677693 |  466 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       18 |  467 | `			if( iBraces <= 0 ){` |
|       13 |  468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  469 | `				if( rc != SXERR_ABORT ){` |
|       13 |  470 | `					rc = SXERR_SYNTAX;` |
|        6 |  471 | `				}` |
|       13 |  472 | `				return rc;` |
|        - |  473 | `			}` |
|        6 |  474 | `			iBraces--;` |
|  2677672 |  475 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2650 |  476 | `			if( iQuesty > 0 ){` |
|     2460 |  477 | `				iQuesty--;` |
|     1421 |  478 | `			}else if( iParen <= 0 ){` |
|        - |  479 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  480 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  481 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  482 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  483 | `				if( rc != SXERR_ABORT ){` |
|        5 |  484 | `					rc = SXERR_SYNTAX;` |
|        2 |  485 | `				}` |
|        5 |  486 | `				return rc;` |
|        2 |  487 | `			}` |
|  2676344 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   764072 |  489 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   764072 |  490 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2462 |  491 | `				iQuesty++;` |
|   762842 |  492 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      366 |  493 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      182 |  511 | `			}` |
|   382035 |  512 | `		}` |
|  1818301 |  513 | `	}` |
|   829694 |  514 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  516 | `		if( rc != SXERR_ABORT ){` |
|       17 |  517 | `			rc = SXERR_SYNTAX;` |
|        8 |  518 | `		}` |
|       17 |  519 | `		return rc;` |
|        - |  520 | `	}` |
|   829678 |  521 | `	return SXRET_OK;` |
|   414865 |  522 |  |
|        - |  523 | `/*` |
|        - |  524 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  525 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  526 | ` */` |
|   690616 |  527 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  528 |  |
|   690618 |  529 | `	SyToken *pIn = *ppCur;` |
|        - |  530 | `	/* Jump the first literal seen */` |
|   690618 |  531 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   690584 |  532 | `		pIn++;` |
|   345291 |  533 | `	}` |
|   345349 |  534 | `	for(;;){` |
|   690700 |  535 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       84 |  536 | `			pIn++;` |
|       84 |  537 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       82 |  538 | `				pIn++;` |
|       40 |  539 | `			}` |
|       43 |  540 | `		}else{` |
|   345310 |  541 | `			break;` |
|        - |  542 | `		}` |
|        2 |  543 | `	}` |
|        - |  544 | `	/* Synchronize pointers */` |
|   690618 |  545 | `	*ppCur = pIn;` |
|   690618 |  546 |  |
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
|      252 |  580 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  581 |  |
|      254 |  582 | `	SyToken *pIn = *ppCur;` |
|        - |  583 | `	sxu32 nLine;` |
|        - |  584 | `	sxi32 rc;` |
|        - |  585 | `	/* Jump the 'function' keyword */` |
|      254 |  586 | `	nLine = pIn->nLine;` |
|      254 |  587 | `	pIn++;` |
|      254 |  588 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  589 | `		pIn++;` |
|        1 |  590 | `	}` |
|      254 |  591 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  592 | `		/* Syntax error */` |
|        5 |  593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  594 | `		if( rc != SXERR_ABORT ){` |
|        5 |  595 | `			rc = SXERR_SYNTAX;` |
|        2 |  596 | `		}` |
|        5 |  597 | `		goto Synchronize;` |
|        - |  598 | `	}` |
|      250 |  599 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      250 |  600 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      250 |  601 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  602 | `		/* Syntax error */` |
|        5 |  603 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  604 | `		if( rc != SXERR_ABORT ){` |
|        5 |  605 | `			rc = SXERR_SYNTAX;` |
|        2 |  606 | `		}` |
|        5 |  607 | `		goto Synchronize;` |
|        - |  608 | `	}` |
|      246 |  609 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  610 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      246 |  611 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      246 |  638 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       34 |  639 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  640 | `		/* Check if we are dealing with a closure */` |
|       34 |  641 | `		if( nKey == PH7_TKWRD_USE ){` |
|       26 |  642 | `			pIn++; /* Jump the 'use' keyword */` |
|       26 |  643 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  644 | `				/* Syntax error */` |
|        5 |  645 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  646 | `				if( rc != SXERR_ABORT ){` |
|        5 |  647 | `					rc = SXERR_SYNTAX;` |
|        2 |  648 | `				}` |
|        5 |  649 | `				goto Synchronize;` |
|        - |  650 | `			}` |
|       22 |  651 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       22 |  652 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       22 |  653 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  654 | `				/* Syntax error */` |
|        5 |  655 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  656 | `				if( rc != SXERR_ABORT ){` |
|        5 |  657 | `					rc = SXERR_SYNTAX;` |
|        2 |  658 | `				}` |
|        5 |  659 | `				goto Synchronize;` |
|        - |  660 | `			}` |
|       18 |  661 | `			pIn++;` |
|       10 |  662 | `		}else{` |
|        - |  663 | `			/* Syntax error */` |
|        9 |  664 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  665 | `			if( rc != SXERR_ABORT ){` |
|        9 |  666 | `				rc = SXERR_SYNTAX;` |
|        4 |  667 | `			}` |
|        9 |  668 | `			goto Synchronize;` |
|        - |  669 | `		}` |
|        8 |  670 | `	}` |
|      230 |  671 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      230 |  672 | `		pIn++; /* Jump the leading curly '{' */` |
|      230 |  673 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      230 |  674 | `		if( pIn < pEnd ){` |
|      230 |  675 | `			pIn++;` |
|      114 |  676 | `		}` |
|      116 |  677 | `	}else{` |
|        - |  678 | `		/* Syntax error */` |
|      ! 0 |  679 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|        - |  683 | `	}` |
|      230 |  684 | `	rc = SXRET_OK;` |
|      126 |  685 | `Synchronize:` |
|        - |  686 | `	/* Synchronize pointers */` |
|      254 |  687 | `	*ppCur = pIn;` |
|      254 |  688 | `	return rc;` |
|      128 |  689 |  |
|        - |  690 | `/*` |
|        - |  691 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  692 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  693 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  694 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  695 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  696 | ` */` |
|      116 |  697 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  698 |  |
|      118 |  699 | `	SyToken *pIn = *ppCur;` |
|        - |  700 | `	sxu32 nLine;` |
|        - |  701 | `	sxi32 rc;` |
|        - |  702 | `	int iNest;` |
|      118 |  703 | `	nLine = pIn->nLine;` |
|        - |  704 | `	/* Optional 'static' prefix */` |
|      116 |  705 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      118 |  706 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  707 | `		pIn++;` |
|        1 |  708 | `	}` |
|        - |  709 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      116 |  710 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      118 |  711 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  712 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  713 | `		goto Synchronize;` |
|        - |  714 | `	}` |
|      118 |  715 | `	pIn++; /* Jump 'fn' */` |
|       58 |  716 | `	SXUNUSED(nLine);` |
|       58 |  717 | `	SXUNUSED(pGen);` |
|        - |  718 | `	/* Optional '&' for return-by-reference */` |
|      118 |  719 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  720 | `		pIn++;` |
|      ! 0 |  721 | `	}` |
|        - |  722 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  723 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  724 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  725 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      118 |  726 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      116 |  727 | `		pIn++; /* '(' */` |
|      116 |  728 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      116 |  729 | `		if( pIn < pEnd ){` |
|      114 |  730 | `			pIn++; /* ')' */` |
|       56 |  731 | `		}` |
|       57 |  732 | `	}` |
|        - |  733 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|      118 |  734 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  735 | `		pIn++;` |
|        7 |  736 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  737 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  738 | `			pIn++;` |
|        1 |  739 | `		}` |
|        7 |  740 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        7 |  741 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  742 | `			pIn++;` |
|        7 |  743 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  744 | `				pIn += 2;` |
|      ! 0 |  745 | `			}` |
|        3 |  746 | `		}` |
|        9 |  747 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        4 |  748 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  749 | `			pIn++;` |
|      ! 0 |  750 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  751 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  752 | `				pIn++;` |
|      ! 0 |  753 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  754 | `					pIn += 2;` |
|      ! 0 |  755 | `				}` |
|      ! 0 |  756 | `			}` |
|      ! 0 |  757 | `		}` |
|        3 |  758 | `	}` |
|        - |  759 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      118 |  760 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      112 |  761 | `		pIn++;` |
|       55 |  762 | `	}` |
|        - |  763 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      118 |  764 | `	iNest = 0;` |
|      726 |  765 | `	while( pIn < pEnd ){` |
|      644 |  766 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  767 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       35 |  768 | `			break;` |
|        - |  769 | `		}` |
|      610 |  770 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       25 |  771 | `			iNest++;` |
|      598 |  772 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       25 |  773 | `			iNest--;` |
|       12 |  774 | `		}` |
|      610 |  775 | `		pIn++;` |
|        2 |  776 | `	}` |
|      118 |  777 | `	rc = SXRET_OK;` |
|       58 |  778 | `Synchronize:` |
|      118 |  779 | `	*ppCur = pIn;` |
|      118 |  780 | `	return rc;` |
|        2 |  781 |  |
|        - |  782 | `/*` |
|        - |  783 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  784 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  785 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  786 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  787 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  788 | ` */` |
|       70 |  789 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  790 |  |
|       72 |  791 | `	SyToken *pIn = *ppCur;` |
|        - |  792 | `	sxi32 rc;` |
|       35 |  793 | `	SXUNUSED(pGen);` |
|        - |  794 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  795 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       72 |  796 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  797 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  798 | `		goto Synchronize;` |
|        - |  799 | `	}` |
|       72 |  800 | `	pIn++; /* Jump 'match' */` |
|        - |  801 | `	/* Optional '(' subject ')' */` |
|       72 |  802 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       72 |  803 | `		pIn++;` |
|       72 |  804 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       72 |  805 | `		if( pIn < pEnd ){` |
|       72 |  806 | `			pIn++; /* ')' */` |
|       35 |  807 | `		}` |
|       35 |  808 | `	}` |
|        - |  809 | `	/* Optional '{' arms '}' */` |
|       72 |  810 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       72 |  811 | `		pIn++;` |
|       72 |  812 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       72 |  813 | `		if( pIn < pEnd ){` |
|       72 |  814 | `			pIn++; /* '}' */` |
|       35 |  815 | `		}` |
|       35 |  816 | `	}` |
|       72 |  817 | `	rc = SXRET_OK;` |
|       35 |  818 | `Synchronize:` |
|       72 |  819 | `	*ppCur = pIn;` |
|       72 |  820 | `	return rc;` |
|        2 |  821 |  |
|        - |  822 | `/*` |
|        - |  823 | ` * Extract a single expression node from the input.` |
|        - |  824 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  825 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  826 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  827 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  828 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  829 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  830 | ` */` |
|  3637580 |  831 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  832 |  |
|        - |  833 | `	ph7_expr_node *pNode;` |
|        - |  834 | `	SyToken *pCur;` |
|        - |  835 | `	sxi32 rc;` |
|        - |  836 | `	/* Allocate a new node */` |
|  3637582 |  837 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3637582 |  838 | `	if( pNode == 0 ){` |
|        - |  839 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  840 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  841 | `		 */` |
|      ! 0 |  842 | `		return SXERR_MEM;` |
|        - |  843 | `	}` |
|        - |  844 | `	/* Zero the structure */` |
|  3637582 |  845 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3637582 |  846 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  847 | `	/* Point to the head of the token stream */` |
|  3637582 |  848 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  849 | `	/* Start collecting tokens */` |
|  3637582 |  850 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  851 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  852 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       76 |  853 | `		pCur++;` |
|       76 |  854 | `		pGen->pIn = pCur;` |
|       76 |  855 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       76 |  856 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       76 |  857 | `		if( rc == SXRET_OK && *ppNode ){` |
|       76 |  858 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       37 |  859 | `		}` |
|       76 |  860 | `		return rc;` |
|        - |  861 | `	}` |
|  3637508 |  862 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  863 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  864 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  865 | `		 */` |
|      728 |  866 | `		pCur++; /* Skip the opening '[' */` |
|      728 |  867 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      728 |  868 | `		if( pCur < pGen->pEnd ){` |
|      728 |  869 | `			pCur++; /* Skip past the closing ']' */` |
|      365 |  870 | `		}else{` |
|      ! 0 |  871 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  872 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  873 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  874 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  875 | `			}` |
|      ! 0 |  876 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  877 | `			return rc;` |
|        - |  878 | `		}` |
|        - |  879 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  880 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  881 | `		 */` |
|      787 |  882 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      120 |  883 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      120 |  884 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  885 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  886 | `			}else{` |
|       91 |  887 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  888 | `			}` |
|       61 |  889 | `		}else{` |
|      610 |  890 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  891 | `		}` |
|  3637145 |  892 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  893 | `		/* Point to the instance that describe this operator */` |
|   844812 |  894 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  895 | `		/* Advance the stream cursor */` |
|   844812 |  896 | `		pCur++;` |
|  3214377 |  897 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  898 | `		/* Isolate variable */` |
|  1964538 |  899 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   982276 |  900 | `			pCur++; /* Variable variable */` |
|        2 |  901 | `		}` |
|   982264 |  902 | `		if( pCur < pGen->pEnd ){` |
|   982264 |  903 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  904 | `				/* Variable name */` |
|   982236 |  905 | `				pCur++;` |
|   491147 |  906 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  907 | `				pCur++;` |
|        - |  908 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  909 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  910 | `				if( pCur < pGen->pEnd ){` |
|       18 |  911 | `					pCur++;` |
|       10 |  912 | `				}else{` |
|        5 |  913 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  914 | `					if( rc != SXERR_ABORT ){` |
|        5 |  915 | `						rc = SXERR_SYNTAX;` |
|        2 |  916 | `					}` |
|        5 |  917 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  918 | `					return rc;` |
|        - |  919 | `				}` |
|        8 |  920 | `			}` |
|   491129 |  921 | `		}` |
|   982260 |  922 | `		pNode->xCode = PH7_CompileVariable;` |
|  2300839 |  923 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    46914 |  924 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    46914 |  925 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  926 | `			 /* List/Array node */` |
|    26916 |  927 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  928 | `				 /* Assume a literal */` |
|       17 |  929 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  930 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  931 | `			 }else{` |
|    26900 |  932 | `				 pCur += 2;` |
|        - |  933 | `				 /* Collect array/list tokens */` |
|    26900 |  934 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    26900 |  935 | `				 if( pCur < pGen->pEnd ){` |
|    26898 |  936 | `					 pCur++;` |
|    13450 |  937 | `				 }else{` |
|        - |  938 | `					 /* Syntax error */` |
|        4 |  939 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  940 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  941 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  942 | `						 rc = SXERR_SYNTAX;` |
|        1 |  943 | `					 }` |
|        3 |  944 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  945 | `					 return rc;` |
|        - |  946 | `				 }` |
|    26898 |  947 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    26898 |  948 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  949 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  950 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  951 | `						 /* Syntax error */` |
|        3 |  952 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  953 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  954 | `							 rc = SXERR_SYNTAX;` |
|        1 |  955 | `						 }` |
|        3 |  956 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  957 | `						 return rc;` |
|        - |  958 | `					 }` |
|       12 |  959 | `				 }` |
|        2 |  960 | `			 }` |
|    33455 |  961 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  962 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  963 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  964 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  965 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  966 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  967 | `			 pNode->xCode = PH7_CompileYield;` |
|    19983 |  968 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  969 | `			 /* Annonymous function */` |
|      254 |  970 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  971 | `				 /* Assume a literal */` |
|      ! 0 |  972 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  973 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  974 | `			 }else{` |
|        - |  975 | `				 /* Assemble annonymous functions body */` |
|      254 |  976 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      254 |  977 | `				 if( rc != SXRET_OK ){` |
|       25 |  978 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  979 | `					 return rc;` |
|        - |  980 | `				 }` |
|      230 |  981 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  982 | `			  }` |
|    19829 |  983 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    19657 |  984 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  985 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  986 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  987 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      118 |  988 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      118 |  989 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  990 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  991 | `				 return rc;` |
|        - |  992 | `			 }` |
|      118 |  993 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    19656 |  994 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - |  995 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       72 |  996 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       72 |  997 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  998 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  999 | `				 return rc;` |
|        - | 1000 | `			 }` |
|       72 | 1001 | `			 pNode->xCode = PH7_CompileMatch;` |
|    19563 | 1002 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1003 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1004 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1005 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1006 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1007 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1008 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1009 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1010 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    19510 | 1011 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1012 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       86 | 1013 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       86 | 1014 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       44 | 1015 | `		 }else{` |
|        - | 1016 | `			 /* Assume a literal */` |
|    19408 | 1017 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    19408 | 1018 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 | 1019 | `		 }` |
|  1786240 | 1020 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1021 | `		 /* Constants,function name,namespace path,class name... */` |
|   671196 | 1022 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   671196 | 1023 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   335599 | 1024 | `	 }else{` |
|  1091604 | 1025 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1026 | `			 /* Point to the code generator routine */` |
|   210676 | 1027 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   210676 | 1028 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1029 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1030 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1031 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1032 | `				 }` |
|        3 | 1033 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1034 | `				 return rc;` |
|        - | 1035 | `			 }` |
|   105336 | 1036 | `		 }` |
|        - | 1037 | `		/* Advance the stream cursor */` |
|  1091602 | 1038 | `		pCur++;` |
|        - | 1039 | `	 }` |
|        - | 1040 | `	/* Point to the end of the token stream */` |
|  3637474 | 1041 | `	pNode->pEnd = pCur;` |
|        - | 1042 | `	/* Save the node for later processing */` |
|  3637474 | 1043 | `	*ppNode = pNode;` |
|        - | 1044 | `	/* Synchronize cursors */` |
|  3637474 | 1045 | `	pGen->pIn = pCur;` |
|  3637474 | 1046 | `	return SXRET_OK;` |
|  1818792 | 1047 |  |
|        - | 1048 | `/*` |
|        - | 1049 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1050 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1051 | ` * level is zero.` |
|        - | 1052 | ` */` |
|    83006 | 1053 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 | 1054 |  |
|    83008 | 1055 | `	SyToken *pCur = pStart;` |
|    83008 | 1056 | `	sxi32 iNest = 0;` |
|    83008 | 1057 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1058 | `		/* Last expression */` |
|    43638 | 1059 | `		return SXERR_EOF;` |
|        - | 1060 | `	}` |
|   161806 | 1061 | `	while( pCur < pEnd ){` |
|   147334 | 1062 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    24900 | 1063 | `			break;` |
|        - | 1064 | `		}` |
|   122436 | 1065 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     7728 | 1066 | `			iNest++;` |
|   118573 | 1067 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     7730 | 1068 | `			iNest--;` |
|     3864 | 1069 | `		}` |
|   122436 | 1070 | `		pCur++;` |
|        2 | 1071 | `	}` |
|    39372 | 1072 | `	*ppNext = pCur;` |
|    39372 | 1073 | `	return SXRET_OK;` |
|    41505 | 1074 |  |
|        - | 1075 | `/*` |
|        - | 1076 | ` * Free an expression tree.` |
|        - | 1077 | ` */` |
|  3139768 | 1078 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1079 |  |
|  3139770 | 1080 | `	if( pNode->pLeft ){` |
|        - | 1081 | `		/* Release the left tree */` |
|  1174046 | 1082 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   587022 | 1083 | `	}` |
|  3139770 | 1084 | `	if( pNode->pRight ){` |
|        - | 1085 | `		/* Release the right tree */` |
|   649622 | 1086 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   324810 | 1087 | `	}` |
|  3139770 | 1088 | `	if( pNode->pCond ){` |
|        - | 1089 | `		/* Release the conditional tree used by the ternary operator */` |
|     2458 | 1090 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1228 | 1091 | `	}` |
|  3139770 | 1092 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1093 | `		ph7_expr_node **apArg;` |
|        - | 1094 | `		sxu32 n;` |
|        - | 1095 | `		/* Release node arguments */` |
|   387174 | 1096 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   816872 | 1097 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   429700 | 1098 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   214851 | 1099 | `		}` |
|   387174 | 1100 | `		SySetRelease(&pNode->aNodeArgs);` |
|   193586 | 1101 | `	}` |
|        - | 1102 | `	/* Finally,release this node */` |
|  3139770 | 1103 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3139770 | 1104 |  |
|        - | 1105 | `/*` |
|        - | 1106 | ` * Free an expression tree.` |
|        - | 1107 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1108 | ` */` |
|   829760 | 1109 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1110 |  |
|        - | 1111 | `	ph7_expr_node **apNode;` |
|        - | 1112 | `	sxu32 n;` |
|   829762 | 1113 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  4467234 | 1114 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3637474 | 1115 | `		if( apNode[n] ){` |
|   830096 | 1116 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   415047 | 1117 | `		}` |
|  1818738 | 1118 | `	}` |
|   829762 | 1119 | `	return SXRET_OK;` |
|        2 | 1120 |  |
|        - | 1121 | `/*` |
|        - | 1122 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1123 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1124 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1125 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1126 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1127 | ` */` |
|  1162294 | 1128 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        2 | 1129 |  |
|  1162296 | 1130 | `	if( pNode == 0 ){` |
|   715918 | 1131 | `		return 0;` |
|        - | 1132 | `	}` |
|   446380 | 1133 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       13 | 1134 | `		return 1;` |
|        - | 1135 | `	}` |
|   446368 | 1136 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        5 | 1137 | `		return 1;` |
|        - | 1138 | `	}` |
|   446364 | 1139 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1140 | `		return 1;` |
|        - | 1141 | `	}` |
|   446364 | 1142 | `	return 0;` |
|   581149 | 1143 |  |
|        - | 1144 | `/*` |
|        - | 1145 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1146 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1147 | ` */` |
|   262920 | 1148 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1149 |  |
|        - | 1150 | `	sxi32 iExprOp;` |
|   262922 | 1151 | `	if( pNode->pOp == 0 ){` |
|   158716 | 1152 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1153 | `	}` |
|   104208 | 1154 | `	iExprOp = pNode->pOp->iOp;` |
|   104208 | 1155 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    72410 | 1156 | `			return TRUE;` |
|        - | 1157 | `	}` |
|    31800 | 1158 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    31796 | 1159 | `		if( pNode->pLeft->pOp ) {` |
|       50 | 1160 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       21 | 1161 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1162 | `				return FALSE;` |
|        2 | 1163 | `			}` |
|    31771 | 1164 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1165 | `			return FALSE;` |
|        - | 1166 | `		}` |
|    31796 | 1167 | `		return TRUE;` |
|        - | 1168 | `	}` |
|        5 | 1169 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1170 | `		return TRUE;` |
|        - | 1171 | `	}` |
|        - | 1172 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1173 | `	return FALSE;` |
|   131462 | 1174 |  |
|        - | 1175 | `/* Forward declaration */` |
|        - | 1176 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1177 | `/* Macro to check if the given node is a terminal.` |
|        - | 1178 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1179 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1180 | ` * linked ternary/elvis node). */` |
|        - | 1181 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1182 | `/*` |
|        - | 1183 | ` * Buid an expression tree for each given function argument.` |
|        - | 1184 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1185 | ` */` |
|   322224 | 1186 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1187 |  |
|        - | 1188 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1189 | `	sxi32 rc;` |
|        - | 1190 | `	/* Process function arguments from left to right */` |
|   322226 | 1191 | `	iCur = 0;` |
|   343475 | 1192 | `	for(;;){` |
|   686952 | 1193 | `		if( iCur >= nToken ){` |
|        - | 1194 | `			/* No more arguments to process */` |
|   322200 | 1195 | `			break;` |
|        - | 1196 | `		}` |
|   364754 | 1197 | `		iNode = iCur;` |
|   364754 | 1198 | `		iNest = 0;` |
|   910084 | 1199 | `		while( iCur < nToken ){` |
|   587884 | 1200 | `			if( apNode[iCur] ){` |
|   575348 | 1201 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    21278 | 1202 | `					break;` |
|   547406 | 1203 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   281096 | 1204 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    29313 | 1205 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1206 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1207 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1208 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1209 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1210 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1211 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    29226 | 1212 | `					iNest++;` |
|   518184 | 1213 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    29226 | 1214 | `					iNest--;` |
|    14612 | 1215 | `				}` |
|   266397 | 1216 | `			}` |
|   545332 | 1217 | `			iCur++;` |
|        2 | 1218 | `		}` |
|   364754 | 1219 | `		if( iCur > iNode ){` |
|   364748 | 1220 | `			SyString sArgName = {0, 0};` |
|        - | 1221 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1222 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1223 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   366658 | 1224 | `			if( (iCur - iNode) >= 2` |
|   202079 | 1225 | `				&& apNode[iNode]` |
|    39412 | 1226 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    21681 | 1227 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3887 | 1228 | `				&& apNode[iNode+1]` |
|     3826 | 1229 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1230 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      188 | 1231 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      188 | 1232 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      188 | 1233 | `				apNode[iNode] = 0;` |
|      188 | 1234 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      188 | 1235 | `				apNode[iNode+1] = 0;` |
|      188 | 1236 | `				iNode += 2;` |
|        - | 1237 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1238 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      188 | 1239 | `				if( iNode >= iCur ){` |
|        4 | 1240 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1241 | `						pOp->pStart->nLine,` |
|        - | 1242 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1243 | `						&sArgName);` |
|        3 | 1244 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1245 | `						rc = SXERR_SYNTAX;` |
|        1 | 1246 | `					}` |
|        3 | 1247 | `					return rc;` |
|        - | 1248 | `				}` |
|       92 | 1249 | `			}` |
|   364744 | 1250 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1251 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1252 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1253 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1254 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1255 | `					apNode[iNode] = 0;` |
|      ! 0 | 1256 | `			}` |
|   364746 | 1257 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   364746 | 1258 | `			if( apNode[iNode] ){` |
|   364746 | 1259 | `				if( sArgName.nByte > 0 ){` |
|      186 | 1260 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      186 | 1261 | `					apNode[iNode]->sArgName = sArgName;` |
|       92 | 1262 | `				}` |
|        - | 1263 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   364746 | 1264 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   182374 | 1265 | `			}else{` |
|        - | 1266 | `				/* No expression before comma */` |
|      ! 0 | 1267 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1268 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1269 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1270 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1271 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1272 | `				}` |
|      ! 0 | 1273 | `				return rc;` |
|        - | 1274 | `			}` |
|   182374 | 1275 | `		}else{` |
|        - | 1276 | `			/* Comma with no preceding argument */` |
|        7 | 1277 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1278 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1279 | `				rc = SXERR_SYNTAX;` |
|        3 | 1280 | `			}` |
|        7 | 1281 | `			return rc;` |
|        - | 1282 | `		}` |
|        - | 1283 | `		/* Jump trailing comma */` |
|   364746 | 1284 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    42548 | 1285 | `			iCur++;` |
|    42548 | 1286 | `			if( iCur >= nToken ){` |
|        - | 1287 | `				/* Trailing comma after last argument */` |
|       19 | 1288 | `				break;` |
|        - | 1289 | `			}` |
|    21264 | 1290 | `		}` |
|        2 | 1291 | `	}` |
|   322218 | 1292 | `	return SXRET_OK;` |
|   161114 | 1293 |  |
|        - | 1294 | ` /*` |
|        - | 1295 | `  * Create an expression tree from an array of tokens.` |
|        - | 1296 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1297 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1298 | `  */` |
|  1294418 | 1299 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1300 | ` {` |
|        - | 1301 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1302 | `	 ph7_expr_node *pNode;` |
|        - | 1303 | `	 sxi32 iCur;` |
|        - | 1304 | `	 sxi32 rc;` |
|  1294420 | 1305 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1306 | `		 /* TICKET 1433-17: self evaluating node */` |
|   586412 | 1307 | `		 return SXRET_OK;` |
|        - | 1308 | `	 }` |
|        - | 1309 | `	 /* Process expressions enclosed in parenthesis first */` |
|  4347710 | 1310 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1311 | `		 sxi32 iNest;` |
|        - | 1312 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1313 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1314 | `		  */` |
|  3639704 | 1315 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3612964 | 1316 | `			 continue;` |
|        - | 1317 | `		 }` |
|    26742 | 1318 | `		 iNest = 1;` |
|    26742 | 1319 | `		 iLeft = iCur;` |
|        - | 1320 | `		 /* Find the closing parenthesis */` |
|    26742 | 1321 | `		 iCur++;` |
|   178542 | 1322 | `		 while( iCur < nToken ){` |
|   178542 | 1323 | `			 if( apNode[iCur] ){` |
|   178542 | 1324 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1325 | `					 /* Decrement nesting level */` |
|    46392 | 1326 | `					 iNest--;` |
|    46392 | 1327 | `					 if( iNest <= 0 ){` |
|    26742 | 1328 | `						 break;` |
|        2 | 1329 | `					 }` |
|   141977 | 1330 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1331 | `					 /* Increment nesting level */` |
|    19652 | 1332 | `					 iNest++;` |
|     9825 | 1333 | `				 }` |
|    75900 | 1334 | `			 }` |
|   151802 | 1335 | `			 iCur++;` |
|        2 | 1336 | `		 }` |
|    26742 | 1337 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1338 | `			 sxi32 j;` |
|        - | 1339 | `			 /* Recurse and process this expression */` |
|    26742 | 1340 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    26742 | 1341 | `			 if( rc != SXRET_OK ){` |
|        3 | 1342 | `				 return rc;` |
|        - | 1343 | `			 }` |
|        - | 1344 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1345 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1346 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    26740 | 1347 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    26740 | 1348 | `				 if( apNode[j] ){` |
|    26740 | 1349 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    26740 | 1350 | `					 break;` |
|        - | 1351 | `				 }` |
|      ! 0 | 1352 | `			 }` |
|    13369 | 1353 | `		 }` |
|        - | 1354 | `		 /* Free the left and right nodes */` |
|    26740 | 1355 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    26740 | 1356 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    26740 | 1357 | `		 apNode[iLeft] = 0;` |
|    26740 | 1358 | `		 apNode[iCur] = 0;` |
|    13371 | 1359 | `	 }` |
|        - | 1360 | `	  /* Process expressions enclosed in braces */` |
|  4519316 | 1361 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1362 | `		 sxi32 iNest;` |
|        - | 1363 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1364 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1365 | `		  */` |
|  3818226 | 1366 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3818222 | 1367 | `			 continue;` |
|        - | 1368 | `		 }` |
|        6 | 1369 | `		 iNest = 1;` |
|        6 | 1370 | `		 iLeft = iCur;` |
|        - | 1371 | `		 /* Find the closing parenthesis */` |
|        6 | 1372 | `		 iCur++;` |
|        8 | 1373 | `		 while( iCur < nToken ){` |
|        8 | 1374 | `			 if( apNode[iCur] ){` |
|        8 | 1375 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1376 | `					 /* Decrement nesting level */` |
|        6 | 1377 | `					 iNest--;` |
|        6 | 1378 | `					 if( iNest <= 0 ){` |
|        6 | 1379 | `						 break;` |
|      ! 0 | 1380 | `					 }` |
|        3 | 1381 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1382 | `					 /* Increment nesting level */` |
|      ! 0 | 1383 | `					 iNest++;` |
|      ! 0 | 1384 | `				 }` |
|        1 | 1385 | `			 }` |
|        3 | 1386 | `			 iCur++;` |
|        1 | 1387 | `		 }` |
|        6 | 1388 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1389 | `			 /* Recurse and process this expression */` |
|        3 | 1390 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        3 | 1391 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1392 | `				 return rc;` |
|        - | 1393 | `			 }` |
|        1 | 1394 | `		 }` |
|        - | 1395 | `		 /* Free the left and right nodes */` |
|        6 | 1396 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        6 | 1397 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        6 | 1398 | `		 apNode[iLeft] = 0;` |
|        6 | 1399 | `		 apNode[iCur] = 0;` |
|        4 | 1400 | `	 }` |
|        - | 1401 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   701092 | 1402 | `	 iLeft = -1;` |
|  4519286 | 1403 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3818208 | 1404 | `		 if( apNode[iCur] == 0 ){` |
|  1456240 | 1405 | `			 continue;` |
|        - | 1406 | `		 }` |
|  2361970 | 1407 | `		 pNode = apNode[iCur];` |
|  2361970 | 1408 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   624042 | 1409 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1410 | `				 /* Collect function arguments */` |
|   372002 | 1411 | `				 sxi32 iPtr = 0;` |
|   372002 | 1412 | `				 sxi32 nFuncTok = 0;` |
|  1331886 | 1413 | `				 while( nFuncTok + iCur < nToken ){` |
|  1331886 | 1414 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1319350 | 1415 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   385162 | 1416 | `							 iPtr++;` |
|  1126770 | 1417 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   385162 | 1418 | `							 iPtr--;` |
|   385162 | 1419 | `							 if( iPtr <= 0 ){` |
|   372002 | 1420 | `								 break;` |
|        - | 1421 | `							 }` |
|     6580 | 1422 | `						 }` |
|   473674 | 1423 | `					 }` |
|   959886 | 1424 | `					 nFuncTok++;` |
|        2 | 1425 | `				 }` |
|   372002 | 1426 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1427 | `					 /* Syntax error */` |
|      ! 0 | 1428 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1429 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1430 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1431 | `					 }` |
|      ! 0 | 1432 | `					 return rc;` |
|        - | 1433 | `				 }` |
|   372002 | 1434 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1435 | `					 /* Syntax error */` |
|      ! 0 | 1436 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1437 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1438 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1439 | `					 }` |
|      ! 0 | 1440 | `					 return rc;` |
|        - | 1441 | `				 }` |
|   372002 | 1442 | `				 if( nFuncTok > 1 ){` |
|        - | 1443 | `					 /* Process function arguments */` |
|   322226 | 1444 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   322226 | 1445 | `					 if( rc != SXRET_OK ){` |
|        9 | 1446 | `						 return rc;` |
|        - | 1447 | `					 }` |
|   161108 | 1448 | `				 }` |
|        - | 1449 | `				 /* Link the node to the tree */` |
|   371994 | 1450 | `				 pNode->pLeft = apNode[iLeft];` |
|   371994 | 1451 | `				 apNode[iLeft] = 0;` |
|  1331854 | 1452 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   959862 | 1453 | `					 apNode[iCur+iPtr] = 0;` |
|   479932 | 1454 | `				 }` |
|   438038 | 1455 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1456 | `				 /* Subscripting */` |
|    80716 | 1457 | `				 sxi32 iArrTok = iCur + 1;` |
|    80716 | 1458 | `				 sxi32 iNest = 1;` |
|    80880 | 1459 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1460 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1461 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1462 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    80714 | 1463 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1464 | `						 /* Syntax error */` |
|      ! 0 | 1465 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1466 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1467 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1468 | `						 }` |
|      ! 0 | 1469 | `						 return rc;` |
|        - | 1470 | `				 }` |
|        - | 1471 | `				 /* Collect index tokens */` |
|   145780 | 1472 | `				 while( iArrTok < nToken ){` |
|   145780 | 1473 | `					 if( apNode[iArrTok] ){` |
|   145748 | 1474 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1475 | `							 /* Increment nesting level */` |
|      ! 0 | 1476 | `							 iNest++;` |
|   145748 | 1477 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1478 | `							 /* Decrement nesting level */` |
|    80716 | 1479 | `							 iNest--;` |
|    80716 | 1480 | `							 if( iNest <= 0 ){` |
|    80716 | 1481 | `								 break;` |
|        - | 1482 | `							 }` |
|      ! 0 | 1483 | `						 }` |
|    32516 | 1484 | `					 }` |
|    65066 | 1485 | `					 ++iArrTok;` |
|        2 | 1486 | `				 }` |
|    80716 | 1487 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1488 | `					 /* Recurse and process this expression */` |
|    64956 | 1489 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    64956 | 1490 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1491 | `						 return rc;` |
|        - | 1492 | `					 }` |
|        - | 1493 | `					 /* Link the node to it's index */` |
|    64956 | 1494 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    32477 | 1495 | `				 }` |
|        - | 1496 | `				 /* Link the node to the tree */` |
|    80716 | 1497 | `				 pNode->pLeft = apNode[iLeft];` |
|    80716 | 1498 | `				 pNode->pRight = 0;` |
|    80716 | 1499 | `				 apNode[iLeft] = 0;` |
|   226494 | 1500 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   145780 | 1501 | `					 apNode[iNest] = 0;` |
|    72891 | 1502 | `				 }` |
|    40359 | 1503 | `			 }else{` |
|        - | 1504 | `				 /* Member access operators [i.e: '->','::'] */` |
|   171328 | 1505 | `				  iRight = iCur + 1;` |
|   171330 | 1506 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        3 | 1507 | `					 iRight++;` |
|        1 | 1508 | `				 }` |
|   171328 | 1509 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1510 | `					 /* Syntax error */` |
|        5 | 1511 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1512 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1513 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1514 | `					 }` |
|        5 | 1515 | `					 return rc;` |
|        - | 1516 | `				 }` |
|        - | 1517 | `				 /* Link the node to the tree */` |
|   171324 | 1518 | `				 pNode->pLeft = apNode[iLeft];` |
|   256828 | 1519 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   171207 | 1520 | `					 && pNode->pLeft->pOp == 0 &&` |
|   171012 | 1521 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1522 | `						 /* Syntax error */` |
|      ! 0 | 1523 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1524 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1525 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1526 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1527 | `						 }` |
|      ! 0 | 1528 | `						 return rc;` |
|        - | 1529 | `				 }` |
|   171324 | 1530 | `				 pNode->pRight = apNode[iRight];` |
|   171324 | 1531 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1532 | `			 }` |
|   312014 | 1533 | `		 }` |
|  2361958 | 1534 | `		 iLeft = iCur;` |
|  1180980 | 1535 | `	 }` |
|        - | 1536 | `	 /* Handle left associative (new, clone) operators */` |
|  4519254 | 1537 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3818176 | 1538 | `		 if( apNode[iCur] == 0 ){` |
|  2096964 | 1539 | `			 continue;` |
|        - | 1540 | `		 }` |
|  1721214 | 1541 | `		 pNode = apNode[iCur];` |
|  1721214 | 1542 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1543 | `			 SyToken *pToken;` |
|        - | 1544 | `			 /* Get the left node */` |
|    16698 | 1545 | `			 iLeft = iCur + 1;` |
|    33360 | 1546 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    16664 | 1547 | `				 iLeft++;` |
|        2 | 1548 | `			 }` |
|    16698 | 1549 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1550 | `				  /* Syntax error */` |
|      ! 0 | 1551 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1552 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1553 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1554 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1555 | `				 }` |
|      ! 0 | 1556 | `				 return rc;` |
|        - | 1557 | `			 }` |
|        - | 1558 | `			 /* Make sure the operand are of a valid type */` |
|    16698 | 1559 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1560 | `				 /* Clone:` |
|        - | 1561 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1562 | `				  *  ++ function call (including annonymous)` |
|        - | 1563 | `				  *  ++ array member` |
|        - | 1564 | `				  *  ++ 'new' operator` |
|        - | 1565 | `				  * Example:` |
|        - | 1566 | `				  *   clone $pObj;` |
|        - | 1567 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1568 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1569 | `				  */` |
|       20 | 1570 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1571 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1572 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1573 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1574 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1575 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1576 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1577 | `						 }` |
|      ! 0 | 1578 | `						 return rc;` |
|        - | 1579 | `					 }` |
|        8 | 1580 | `				 }` |
|       11 | 1581 | `			 }else{` |
|        - | 1582 | `				 /* New */` |
|    16680 | 1583 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       20 | 1584 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       20 | 1585 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1586 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1587 | `						 /* Syntax error */` |
|      ! 0 | 1588 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1589 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1590 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1591 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1592 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1593 | `						 }` |
|      ! 0 | 1594 | `						 return rc;` |
|        - | 1595 | `					 }` |
|        9 | 1596 | `				 }` |
|        - | 1597 | `			 }` |
|        - | 1598 | `			  /* Link the node to the tree */` |
|    16698 | 1599 | `			 pNode->pLeft = apNode[iLeft];` |
|    16698 | 1600 | `			 apNode[iLeft] = 0;` |
|    16698 | 1601 | `			 pNode->pRight = 0; /* Paranoid */` |
|     8348 | 1602 | `		 }` |
|   860608 | 1603 | `	 }` |
|        - | 1604 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   701080 | 1605 | `	 iLeft = -1;` |
|  4522712 | 1606 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3818176 | 1607 | `		 if( apNode[iCur] == 0 ){` |
|  2096964 | 1608 | `			 continue;` |
|        - | 1609 | `		 }` |
|  1721214 | 1610 | `		 pNode = apNode[iCur];` |
|  1721214 | 1611 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     9442 | 1612 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3485 | 1613 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1614 | `					 /* Link the node to the tree */` |
|     3484 | 1615 | `					 pNode->pLeft = apNode[iLeft];` |
|     3484 | 1616 | `					 apNode[iLeft] = 0;` |
|     1741 | 1617 | `			 }` |
|     6449 | 1618 | `		  }` |
|  1724672 | 1619 | `		 iLeft = iCur;` |
|   864066 | 1620 | `	  }` |
|   704538 | 1621 | `	 iLeft = -1;` |
|  4522712 | 1622 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3818176 | 1623 | `		 if( apNode[iCur] == 0 ){` |
|  2100446 | 1624 | `			 continue;` |
|        - | 1625 | `		 }` |
|  1717732 | 1626 | `		 pNode = apNode[iCur];` |
|  1717732 | 1627 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     9417 | 1628 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     9418 | 1629 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1630 | `					 /* Syntax error */` |
|      ! 0 | 1631 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1632 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1633 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1634 | `					 }` |
|      ! 0 | 1635 | `					 return rc;` |
|        - | 1636 | `			 }` |
|        - | 1637 | `			 /* Link the node to the tree */` |
|     9418 | 1638 | `			 pNode->pLeft = apNode[iLeft];` |
|     9418 | 1639 | `			 apNode[iLeft] = 0;` |
|        - | 1640 | `			 /* Mark as pre-increment/decrement node */` |
|     9418 | 1641 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4708 | 1642 | `		  }` |
|  1717732 | 1643 | `		 iLeft = iCur;` |
|   858867 | 1644 | `	 }` |
|        - | 1645 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   704538 | 1646 | `	  iLeft = 0;` |
|  4522706 | 1647 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3818172 | 1648 | `		  if( apNode[iCur] ){` |
|  1708312 | 1649 | `			  pNode = apNode[iCur];` |
|  1708312 | 1650 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    42196 | 1651 | `				  if( iLeft > 0 ){` |
|        - | 1652 | `					  /* Link the node to the tree */` |
|    42194 | 1653 | `					  pNode->pLeft = apNode[iLeft];` |
|    42194 | 1654 | `					  apNode[iLeft] = 0;` |
|    42194 | 1655 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1656 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1657 | `							   /* Syntax error */` |
|      ! 0 | 1658 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1659 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1660 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1661 | `							  }` |
|      ! 0 | 1662 | `							  return rc;` |
|        - | 1663 | `						  }` |
|       36 | 1664 | `					  }` |
|    21098 | 1665 | `				  }else{` |
|        - | 1666 | `					  /* Syntax error */` |
|        3 | 1667 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1668 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1669 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1670 | `					  }` |
|        3 | 1671 | `					  return rc;` |
|        - | 1672 | `				  }` |
|    21096 | 1673 | `			  }` |
|        - | 1674 | `			  /* Save terminal position */` |
|  1708310 | 1675 | `			  iLeft = iCur;` |
|   854154 | 1676 | `		  }` |
|  1909086 | 1677 | `	  }` |
|        - | 1678 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1679 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1680 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1681 | `	  * yielding a right-leaning tree. */` |
|  4522704 | 1682 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  3818170 | 1683 | `		 if( apNode[iCur] == 0 ){` |
|  2152166 | 1684 | `			 continue;` |
|        - | 1685 | `		 }` |
|  1666006 | 1686 | `		 pNode = apNode[iCur];` |
|  1666006 | 1687 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1688 | `			 sxi32 iL, iR;` |
|        - | 1689 | `			 /* Find the right operand */` |
|      113 | 1690 | `			 iR = -1;` |
|        - | 1691 | `			 {` |
|        - | 1692 | `				 sxi32 j;` |
|      125 | 1693 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1694 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1695 | `				 }` |
|        - | 1696 | `			 }` |
|        - | 1697 | `			 /* Find the left operand */` |
|      113 | 1698 | `			 iL = -1;` |
|        - | 1699 | `			 {` |
|        - | 1700 | `				 sxi32 j;` |
|      181 | 1701 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1702 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1703 | `				 }` |
|        - | 1704 | `			 }` |
|      113 | 1705 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1706 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1707 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1708 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1709 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1710 | `				 }` |
|      ! 0 | 1711 | `				 return rc;` |
|        - | 1712 | `			 }` |
|      113 | 1713 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1714 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1715 | `			 apNode[iL] = 0;` |
|      113 | 1716 | `			 apNode[iR] = 0;` |
|        - | 1717 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1718 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1719 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1720 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1721 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1722 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1723 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1724 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1725 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1726 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1727 | `			  * operands are respected. */` |
|      129 | 1728 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1729 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1730 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1731 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1732 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1733 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1734 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1735 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1736 | `				 while( pTail->pLeft` |
|       34 | 1737 | `					 && pTail->pLeft->pOp` |
|       23 | 1738 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1739 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1740 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1741 | `					 pTail = pTail->pLeft;` |
|        1 | 1742 | `				 }` |
|        - | 1743 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1744 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1745 | `				 pTail->pLeft = pNode;` |
|       27 | 1746 | `				 apNode[iCur] = pHead;` |
|       13 | 1747 | `			 }` |
|       56 | 1748 | `		 }` |
|   833004 | 1749 | `	 }` |
|        - | 1750 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  7749790 | 1751 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  7045266 | 1752 | `		 iLeft = -1;` |
| 45226652 | 1753 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 38181398 | 1754 | `			 if( apNode[iCur] == 0 ){` |
| 24370652 | 1755 | `				 continue;` |
|        - | 1756 | `			 }` |
| 13810748 | 1757 | `			 pNode = apNode[iCur];` |
| 13810748 | 1758 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1759 | `				 /* Get the right node */` |
|   212834 | 1760 | `				 iRight = iCur + 1;` |
|   303948 | 1761 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    91116 | 1762 | `					 iRight++;` |
|        2 | 1763 | `				 }` |
|   212834 | 1764 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1765 | `					 /* Syntax error */` |
|        9 | 1766 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1767 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1768 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1769 | `					 }` |
|        9 | 1770 | `					 return rc;` |
|        - | 1771 | `				 }` |
|   212826 | 1772 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1773 | `					 sxi32  iTmp;` |
|        - | 1774 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1775 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1776 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1777 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1778 | `					  * is swapped below. */` |
|       50 | 1779 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1780 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1781 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1782 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1783 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1784 | `						 }` |
|        3 | 1785 | `						 return rc;` |
|        - | 1786 | `					 }` |
|       48 | 1787 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1788 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1789 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1790 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1791 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1792 | `						 }` |
|      ! 0 | 1793 | `						 return rc;` |
|        - | 1794 | `					 }` |
|       48 | 1795 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1796 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1797 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1798 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1799 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1800 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1801 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1802 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1803 | `									 }` |
|      ! 0 | 1804 | `									 return rc;` |
|        - | 1805 | `							 }` |
|      ! 0 | 1806 | `						 }` |
|       16 | 1807 | `					 }` |
|        - | 1808 | `					 /* Swap operands */` |
|       48 | 1809 | `					 iTmp = iRight;` |
|       48 | 1810 | `					 iRight = iLeft;` |
|       48 | 1811 | `					 iLeft = iTmp;` |
|       23 | 1812 | `				 }` |
|        - | 1813 | `				 /* Link the node to the tree */` |
|   212824 | 1814 | `				 pNode->pLeft = apNode[iLeft];` |
|   212824 | 1815 | `				 pNode->pRight = apNode[iRight];` |
|   212824 | 1816 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   106411 | 1817 | `			 }` |
| 13810738 | 1818 | `			 iLeft = iCur;` |
|  6905370 | 1819 | `		 }` |
|  3522629 | 1820 | `	 }` |
|        - | 1821 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1822 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1823 | `	  * we are dealing with a single operator.` |
|        - | 1824 | `	  */` |
|   704526 | 1825 | `	  iLeft = -1;` |
|  4512138 | 1826 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3810072 | 1827 | `		  if( apNode[iCur] == 0 ){` |
|  2577182 | 1828 | `			  continue;` |
|        - | 1829 | `		  }` |
|  1232892 | 1830 | `		  pNode = apNode[iCur];` |
|  1232892 | 1831 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2460 | 1832 | `			  sxi32 iNest = 1;` |
|     2460 | 1833 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1834 | `				  /* Missing condition */` |
|        3 | 1835 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1836 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1837 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1838 | `				  }` |
|        3 | 1839 | `				  return rc;` |
|        - | 1840 | `			  }` |
|        - | 1841 | `			  /* Get the right node */` |
|     2458 | 1842 | `			  iRight = iCur + 1;` |
|     5158 | 1843 | `			  while( iRight < nToken  ){` |
|     5158 | 1844 | `				  if( apNode[iRight] ){` |
|     4846 | 1845 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1846 | `						  /* Increment nesting level */` |
|      ! 0 | 1847 | `						  ++iNest;` |
|     4846 | 1848 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1849 | `						  /* Decrement nesting level */` |
|     2458 | 1850 | `						  --iNest;` |
|     2458 | 1851 | `						  if( iNest <= 0 ){` |
|     2458 | 1852 | `							  break;` |
|        - | 1853 | `						  }` |
|      ! 0 | 1854 | `					  }` |
|     1194 | 1855 | `				  }` |
|     2702 | 1856 | `				  iRight++;` |
|        2 | 1857 | `			  }` |
|     2458 | 1858 | `			  if( iRight > iCur + 1 ){` |
|        - | 1859 | `				  /* Recurse and process the then expression */` |
|     2390 | 1860 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2390 | 1861 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1862 | `					  return rc;` |
|        - | 1863 | `				  }` |
|        - | 1864 | `				  /* Link the node to the tree */` |
|     2390 | 1865 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1194 | 1866 | `			  }else{` |
|        - | 1867 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1868 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1869 | `			  }` |
|     2458 | 1870 | `			  apNode[iCur + 1] = 0;` |
|     2458 | 1871 | `			  if( iRight + 1 < nToken ){` |
|        - | 1872 | `				  /* Recurse and process the else expression */` |
|     2458 | 1873 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2458 | 1874 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1875 | `					  return rc;` |
|        - | 1876 | `				  }` |
|        - | 1877 | `				  /* Link the node to the tree */` |
|     2458 | 1878 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2458 | 1879 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1230 | 1880 | `			  }else{` |
|      ! 0 | 1881 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1882 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1883 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1884 | `				 }` |
|      ! 0 | 1885 | `				 return rc;` |
|        - | 1886 | `			  }` |
|        - | 1887 | `			  /* Point to the condition */` |
|     2458 | 1888 | `			  pNode->pCond  = apNode[iLeft];` |
|     2458 | 1889 | `			  apNode[iLeft] = 0;` |
|     2458 | 1890 | `			  break;` |
|        - | 1891 | `		  }` |
|  1230434 | 1892 | `		  iLeft = iCur;` |
|   615218 | 1893 | `	  }` |
|        - | 1894 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1895 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1896 | `	  * so there is no need for a precedence loop here.` |
|        - | 1897 | `	  */` |
|   704524 | 1898 | `	 iRight = -1;` |
|  4522508 | 1899 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3818040 | 1900 | `		 if( apNode[iCur] == 0 ){` |
|  2850504 | 1901 | `			 continue;` |
|        - | 1902 | `		 }` |
|   967538 | 1903 | `		 pNode = apNode[iCur];` |
|   967538 | 1904 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1905 | `			 /* Get the left node */` |
|   262894 | 1906 | `			 iLeft = iCur - 1;` |
|   383200 | 1907 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   120308 | 1908 | `				 iLeft--;` |
|        2 | 1909 | `			 }` |
|   262894 | 1910 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1911 | `				 /* Syntax error */` |
|       43 | 1912 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1913 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1914 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1915 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1916 | `				 }else{` |
|       39 | 1917 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1918 | `				 }` |
|       43 | 1919 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1920 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1921 | `				 }` |
|       43 | 1922 | `				 return rc;` |
|        - | 1923 | `			 }` |
|        - | 1924 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1925 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1926 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1927 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1928 | `			  * a write. */` |
|   262852 | 1929 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        9 | 1930 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1931 | `					 "Can't use nullsafe operator in write context");` |
|        9 | 1932 | `				 if( rc != SXERR_ABORT ){` |
|        9 | 1933 | `					 rc = SXERR_SYNTAX;` |
|        4 | 1934 | `				 }` |
|        9 | 1935 | `				 return rc;` |
|        - | 1936 | `			 }` |
|   262844 | 1937 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1938 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1939 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1940 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1941 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1942 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1943 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1944 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1945 | `					 }else{` |
|        4 | 1946 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1947 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1948 | `					 }` |
|        5 | 1949 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1950 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1951 | `					 }` |
|        5 | 1952 | `					 return rc;` |
|        - | 1953 | `				 }` |
|       26 | 1954 | `			 }` |
|        - | 1955 | `			 /* Link the node to the tree (Reverse) */` |
|   262840 | 1956 | `			 pNode->pLeft = apNode[iRight];` |
|   262840 | 1957 | `			 pNode->pRight = apNode[iLeft];` |
|   262840 | 1958 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   131419 | 1959 | `		 }` |
|   967484 | 1960 | `		 iRight = iCur;` |
|   483743 | 1961 | `	 }` |
|        - | 1962 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3522342 | 1963 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2817874 | 1964 | `		 iLeft = -1;` |
| 18089754 | 1965 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 15271882 | 1966 | `			 if( apNode[iCur] == 0 ){` |
| 12453604 | 1967 | `				 continue;` |
|        - | 1968 | `			 }` |
|  2818280 | 1969 | `			 pNode = apNode[iCur];` |
|  2818280 | 1970 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1971 | `				 /* Get the right node */` |
|       72 | 1972 | `				 iRight = iCur + 1;` |
|      110 | 1973 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1974 | `					 iRight++;` |
|        2 | 1975 | `				 }` |
|       72 | 1976 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1977 | `					 /* Syntax error */` |
|      ! 0 | 1978 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1979 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1980 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1981 | `					 }` |
|      ! 0 | 1982 | `					 return rc;` |
|        - | 1983 | `				 }` |
|        - | 1984 | `				 /* Link the node to the tree */` |
|       72 | 1985 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1986 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1987 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1988 | `			 }` |
|  2818280 | 1989 | `			 iLeft = iCur;` |
|  1409141 | 1990 | `		 }` |
|  1408938 | 1991 | `	 }` |
|        - | 1992 | `	 /* Point to the root of the expression tree */` |
|  3817944 | 1993 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3113494 | 1994 | `		 if( apNode[iCur] ){` |
|   640694 | 1995 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1996 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1997 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1998 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1999 | `				  }` |
|       20 | 2000 | `				  return rc;` |
|        - | 2001 | `			 }` |
|   640676 | 2002 | `			 apNode[0] = apNode[iCur];` |
|   640676 | 2003 | `			 apNode[iCur] = 0;` |
|   320337 | 2004 | `		 }` |
|  1556739 | 2005 | `	 }` |
|   704452 | 2006 | `	 return SXRET_OK;` |
|   645482 | 2007 | ` }` |
|        - | 2008 | ` /*` |
|        - | 2009 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2010 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2011 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2012 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2013 | `  */` |
|   829760 | 2014 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 2015 |  |
|        - | 2016 | `	ph7_expr_node **apNode;` |
|        - | 2017 | `	ph7_expr_node *pNode;` |
|        - | 2018 | `	sxi32 rc;` |
|        - | 2019 | `	/* Reset node container */` |
|   829762 | 2020 | `	SySetReset(pExprNode);` |
|   829762 | 2021 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2022 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2023 | `	{` |
|   829762 | 2024 | `		int iLastWasTerm = 0;` |
|  4467234 | 2025 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3637508 | 2026 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3637508 | 2027 | `			if( rc != SXRET_OK ){` |
|       35 | 2028 | `				return rc;` |
|        - | 2029 | `			}` |
|        - | 2030 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3637474 | 2031 | `			if( pNode->xCode ){` |
|        - | 2032 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1911736 | 2033 | `				iLastWasTerm = 1;` |
|  2681607 | 2034 | `			}else if( pNode->pOp ){` |
|        - | 2035 | `				/* Operator node */` |
|   844812 | 2036 | `				iLastWasTerm = 0;` |
|   422407 | 2037 | `			}else{` |
|        - | 2038 | `				/* Delimiter: ')' and ']' end terms */` |
|   880930 | 2039 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2040 | `			}` |
|        - | 2041 | `			/* Save the extracted node */` |
|  3637474 | 2042 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 2043 | `		}` |
|        - | 2044 | `	}` |
|   829728 | 2045 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2046 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2047 | `		*ppRoot = 0;` |
|      ! 0 | 2048 | `		return SXRET_OK;` |
|        - | 2049 | `	}` |
|   829728 | 2050 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2051 | `	/* Make sure we are dealing with valid nodes */` |
|   829728 | 2052 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   829728 | 2053 | `	if( rc != SXRET_OK ){` |
|        - | 2054 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2055 | `		 * cleanup the mess left behind.` |
|        - | 2056 | `		 */` |
|       51 | 2057 | `		*ppRoot = 0;` |
|       51 | 2058 | `		return rc;` |
|        - | 2059 | `	}` |
|        - | 2060 | `	/* Build the tree */` |
|   829678 | 2061 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   829678 | 2062 | `	if( rc != SXRET_OK ){` |
|        - | 2063 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      100 | 2064 | `		*ppRoot = 0;` |
|      100 | 2065 | `		return rc;` |
|        - | 2066 | `	}` |
|        - | 2067 | `	/* Point to the root of the tree */` |
|   829580 | 2068 | `	*ppRoot = apNode[0];` |
|   829580 | 2069 | `	return SXRET_OK;` |
|   414882 | 2070 |  |
|        - | 2071 |  |
