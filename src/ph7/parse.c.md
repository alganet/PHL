# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1113/1285 lines (86.61%)

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
|        - |  204 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|        - |  205 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|        - |  206 | `	/* Precedence 12,left-associative */` |
|        - |  207 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|        - |  208 | `	/* Precedence 12,left-associative */` |
|        - |  209 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|        - |  210 | `	                         /* Binary operators */` |
|        - |  211 | `	/* Precedence 13,left-associative */` |
|        - |  212 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|        - |  213 | `	/* Precedence 14,left-associative */` |
|        - |  214 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|        - |  215 | `	/* Precedence 15,left-associative */` |
|        - |  216 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  217 | `	/* Precedence 16,left-associative */` |
|        - |  218 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  219 | `	                      /* Null coalescing operator */` |
|        - |  220 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|        - |  221 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|        - |  222 | `	                      /* Ternary operator */` |
|        - |  223 | `	/* Precedence 17,left-associative */` |
|        - |  224 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  225 | `	                     /* Combined binary operators */` |
|        - |  226 | `	/* Precedence 18,right-associative */` |
|        - |  227 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  228 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  229 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  230 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  231 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  232 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  233 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  234 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|        - |  235 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  236 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  237 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  238 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  239 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  240 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|        - |  241 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|        - |  242 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|        - |  243 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|        - |  244 | `	 * in this file: keep one of the question marks escaped. */` |
|        - |  245 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|        - |  246 | `	/* Precedence 19,left-associative */` |
|        - |  247 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  248 | `	/* Precedence 20,left-associative */` |
|        - |  249 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  250 | `	/* Precedence 21,left-associative */` |
|        - |  251 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  252 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  253 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  254 | `};` |
|        - |  255 | `/* Function call operator need special handling */` |
|        - |  256 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  257 | `/*` |
|        - |  258 | ` * Check if the given token is a potential operator or not.` |
|        - |  259 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  260 | ` * look like an operator.` |
|        - |  261 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  262 | ` * Otherwise NULL.` |
|        - |  263 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  264 | ` * a binary minus or unary minus.]` |
|        - |  265 | ` */` |
|  1170886 |  266 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  267 | `{` |
|  1170891 |  268 | `	sxu32 n = 0;` |
|        - |  269 | `	sxi32 rc;` |
|        - |  270 | `	/* Do a linear lookup on the operators table */` |
| 20095103 |  271 | `	for(;;){` |
| 40190211 |  272 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  273 | `			break;` |
|        - |  274 | `		}` |
| 40190211 |  275 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  276 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3588813 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1794409 |  278 | `		}else{` |
| 36601403 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  280 | `		}` |
| 40190211 |  281 | `		if( rc == 0 ){` |
|  1175323 |  282 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  283 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1170433 |  284 | `				return &aOpTable[n];` |
|        - |  285 | `			}` |
|        - |  286 | `			/* Handle ambiguity */` |
|     4895 |  287 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  288 | `				/* Unary opertors have prcedence here over binary operators */` |
|      331 |  289 | `				return &aOpTable[n];` |
|        - |  290 | `			}` |
|     4569 |  291 | `			if( pLast->nType & PH7_TK_OP ){` |
|      143 |  292 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  293 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      143 |  294 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  295 | `					/* Unary opertors have prcedence here over binary operators */` |
|      135 |  296 | `					return &aOpTable[n];` |
|        - |  297 | `				}` |
|        - |  298 |  |
|        4 |  299 | `			}` |
|     2216 |  300 | `		}` |
| 39019325 |  301 | `		++n; /* Next operator in the table */` |
|        5 |  302 | `	}` |
|        - |  303 | `	/* No such operator */` |
|      ! 0 |  304 | `	return 0;` |
|   585448 |  305 | `}` |
|        - |  306 | `/*` |
|        - |  307 | ` * Delimit a set of token stream.` |
|        - |  308 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  309 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  310 | ` */` |
|   716866 |  311 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  312 | `{` |
|   716871 |  313 | `	SyToken *pCur = pIn;` |
|   716871 |  314 | `	sxi32 iNest = 1;` |
|  3998034 |  315 | `	for(;;){` |
|  7996073 |  316 | `		if( pCur >= pEnd ){` |
|      375 |  317 | `			break;` |
|        - |  318 | `		}` |
|  7995703 |  319 | `		if( pCur->nType & nTokStart ){` |
|        - |  320 | `			/* Increment nesting level */` |
|   377341 |  321 | `			iNest++;` |
|  7807035 |  322 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  323 | `			/* Decrement nesting level */` |
|  1093837 |  324 | `			iNest--;` |
|  1093837 |  325 | `			if( iNest <= 0 ){` |
|   716501 |  326 | `				break;` |
|        - |  327 | `			}` |
|   188668 |  328 | `		}` |
|        - |  329 | `		/* Advance cursor */` |
|  7279207 |  330 | `		pCur++;` |
|        5 |  331 | `	}` |
|        - |  332 | `	/* Point to the end of the chunk */` |
|   716871 |  333 | `	*ppEnd = pCur;` |
|   716871 |  334 | `}` |
|        - |  335 | `/*` |
|        - |  336 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  337 | ` * Note on reserved keywords.` |
|        - |  338 | ` *  According to the PHP language reference manual:` |
|        - |  339 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  340 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  341 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  342 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  343 | ` */` |
|    23132 |  344 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  345 | `{` |
|    23132 |  346 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    23034 |  347 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  348 | `		){` |
|      167 |  349 | `			return TRUE;` |
|        - |  350 | `	}` |
|    22975 |  351 | `	if( bCheckFunc ){` |
|      264 |  352 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      257 |  353 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      241 |  354 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       45 |  355 | `				return TRUE;` |
|        - |  356 | `		}` |
|      112 |  357 | `	}` |
|        - |  358 | `	/* Not a language construct */` |
|    22935 |  359 | `	return FALSE;` |
|    11571 |  360 | `}` |
|        - |  361 | `/*` |
|        - |  362 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  363 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  364 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  365 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  366 | ` */` |
|   988078 |  367 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  368 | `{` |
|        - |  369 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  370 | `	sxi32 i,rc;` |
|        - |  371 |  |
|   988083 |  372 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  373 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  374 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  375 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  376 | `	}` |
|   988083 |  377 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5355117 |  378 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4367073 |  379 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  380 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1213 |  381 | `			continue;` |
|        - |  382 | `		}` |
|  4365865 |  383 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   496079 |  384 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    23280 |  385 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  386 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   464777 |  387 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  388 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  389 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  390 | `						 */` |
|   464777 |  391 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   464777 |  392 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   464777 |  393 | `						apNode[i]->pOp = &sFCallOp;` |
|   232386 |  394 | `					}` |
|   232386 |  395 | `			}` |
|   496079 |  396 | `			iParen++;` |
|  4117828 |  397 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   496079 |  398 | `			if( iParen <= 0 ){` |
|       16 |  399 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       16 |  400 | `				if( rc != SXERR_ABORT ){` |
|       16 |  401 | `					rc = SXERR_SYNTAX;` |
|        6 |  402 | `				}` |
|       16 |  403 | `				return rc;` |
|        - |  404 | `			}` |
|   496067 |  405 | `			iParen--;` |
|  3621748 |  406 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    94881 |  407 | `			iSquare++;` |
|  3326279 |  408 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    94895 |  409 | `			if( iSquare <= 0 ){` |
|        8 |  410 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        8 |  411 | `				if( rc != SXERR_ABORT ){` |
|        8 |  412 | `					rc = SXERR_SYNTAX;` |
|        3 |  413 | `				}` |
|        8 |  414 | `				return rc;` |
|        - |  415 | `			}` |
|    94889 |  416 | `			iSquare--;` |
|  3231393 |  417 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       20 |  418 | `			iBraces++;` |
|       20 |  419 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  420 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  421 | `				int iNest = 1;` |
|       11 |  422 | `				sxi32 j=i+1;` |
|        - |  423 | `				/*` |
|        - |  424 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  425 | `				 */` |
|       11 |  426 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  427 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  428 | `				pOp = aOpTable;` |
|       11 |  429 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       61 |  430 | `				while( pOp < pEnd ){` |
|       61 |  431 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  432 | `						break;` |
|        - |  433 | `					}` |
|       51 |  434 | `					pOp++;` |
|        1 |  435 | `				}` |
|       11 |  436 | `				if( pOp >= pEnd ){` |
|      ! 0 |  437 | `					pOp = 0;` |
|      ! 0 |  438 | `				}` |
|       11 |  439 | `				if( pOp ){` |
|       11 |  440 | `					apNode[i]->pOp = pOp;` |
|       11 |  441 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  442 | `				}` |
|       11 |  443 | `				iBraces--;` |
|       11 |  444 | `				iSquare++;` |
|       21 |  445 | `				while( j < nNode ){` |
|       21 |  446 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  447 | `						/* Increment nesting level */` |
|      ! 0 |  448 | `						iNest++;` |
|       21 |  449 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  450 | `						/* Decrement nesting level */` |
|       11 |  451 | `						iNest--;` |
|       11 |  452 | `						if( iNest < 1 ){` |
|       11 |  453 | `							break;` |
|        - |  454 | `						}` |
|      ! 0 |  455 | `					}` |
|       11 |  456 | `					j++;` |
|        1 |  457 | `				}` |
|       11 |  458 | `				if( j < nNode ){` |
|       11 |  459 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  460 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  461 | `				}` |
|        - |  462 |  |
|        7 |  463 | `			}` |
|  3183942 |  464 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       24 |  465 | `			if( iBraces <= 0 ){` |
|       15 |  466 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       15 |  467 | `				if( rc != SXERR_ABORT ){` |
|       15 |  468 | `					rc = SXERR_SYNTAX;` |
|        6 |  469 | `				}` |
|       15 |  470 | `				return rc;` |
|        - |  471 | `			}` |
|       10 |  472 | `			iBraces--;` |
|  3183917 |  473 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2867 |  474 | `			if( iQuesty > 0 ){` |
|     2649 |  475 | `				iQuesty--;` |
|     1545 |  476 | `			}else if( iParen <= 0 ){` |
|        - |  477 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  478 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  479 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        6 |  480 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        6 |  481 | `				if( rc != SXERR_ABORT ){` |
|        6 |  482 | `					rc = SXERR_SYNTAX;` |
|        2 |  483 | `				}` |
|        6 |  484 | `				return rc;` |
|        5 |  485 | `			}` |
|  3182480 |  486 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   891637 |  487 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   891637 |  488 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2651 |  489 | `				iQuesty++;` |
|   890314 |  490 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      403 |  491 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|        9 |  492 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|        9 |  493 | `					sxu32 n = 0;` |
|        9 |  494 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        5 |  495 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        2 |  496 | `					}` |
|        - |  497 | `					/*` |
|        - |  498 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  499 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  500 | `					 */` |
|      213 |  501 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      205 |  502 | `						++n;` |
|        1 |  503 | `					}` |
|        9 |  504 | `					pOp = &aOpTable[n];` |
|        - |  505 | `					/* Mark as binary '+' or '-',not an unary */` |
|        9 |  506 | `					apNode[i]->pOp = pOp;` |
|        9 |  507 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        4 |  508 | `				}` |
|      199 |  509 | `			}` |
|   445816 |  510 | `		}` |
|  2182918 |  511 | `	}` |
|   988049 |  512 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       19 |  513 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       19 |  514 | `		if( rc != SXERR_ABORT ){` |
|       19 |  515 | `			rc = SXERR_SYNTAX;` |
|        8 |  516 | `		}` |
|       19 |  517 | `		return rc;` |
|        - |  518 | `	}` |
|   988033 |  519 | `	return SXRET_OK;` |
|   494044 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  523 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  524 | ` */` |
|   818520 |  525 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  526 | `{` |
|   818525 |  527 | `	SyToken *pIn = *ppCur;` |
|        - |  528 | `	/* Jump the first literal seen */` |
|   818525 |  529 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   814787 |  530 | `		pIn++;` |
|   407391 |  531 | `	}` |
|   411156 |  532 | `	for(;;){` |
|   822317 |  533 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|     3797 |  534 | `			pIn++;` |
|     3797 |  535 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     3795 |  536 | `				pIn++;` |
|     1895 |  537 | `			}` |
|     1901 |  538 | `		}else{` |
|   409265 |  539 | `			break;` |
|        - |  540 | `		}` |
|        5 |  541 | `	}` |
|        - |  542 | `	/* Synchronize pointers */` |
|   818525 |  543 | `	*ppCur = pIn;` |
|   818525 |  544 | `}` |
|        - |  545 | `/*` |
|        - |  546 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  547 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  548 | ` * Note on annonymous functions.` |
|        - |  549 | ` *  According to the PHP language reference manual:` |
|        - |  550 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  551 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  552 | ` *  parameters, but they have many other uses.` |
|        - |  553 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  554 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  555 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  556 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  557 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  558 | ` *` |
|        - |  559 | ` * Some example:` |
|        - |  560 | ` *  $greet = function($name)` |
|        - |  561 | ` * {` |
|        - |  562 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  563 | ` * };` |
|        - |  564 | ` *  $greet('World');` |
|        - |  565 | ` *  $greet('PHP');` |
|        - |  566 | ` *` |
|        - |  567 | ` * $double = function($a) {` |
|        - |  568 | ` *   return $a * 2;` |
|        - |  569 | ` * };` |
|        - |  570 | ` * // This is our range of numbers` |
|        - |  571 | ` * $numbers = range(1, 5);` |
|        - |  572 | ` * // Use the Annonymous function as a callback here to` |
|        - |  573 | ` * // double the size of each element in our` |
|        - |  574 | ` * // range` |
|        - |  575 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  576 | ` * print implode(' ', $new_numbers);` |
|        - |  577 | ` */` |
|      320 |  578 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  579 | `{` |
|      325 |  580 | `	SyToken *pIn = *ppCur;` |
|        - |  581 | `	sxu32 nLine;` |
|        - |  582 | `	sxi32 rc;` |
|        - |  583 | `	/* Jump the 'function' keyword */` |
|      325 |  584 | `	nLine = pIn->nLine;` |
|      325 |  585 | `	pIn++;` |
|      325 |  586 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  587 | `		pIn++;` |
|        1 |  588 | `	}` |
|      325 |  589 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  590 | `		/* Syntax error */` |
|        6 |  591 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  592 | `		if( rc != SXERR_ABORT ){` |
|        6 |  593 | `			rc = SXERR_SYNTAX;` |
|        2 |  594 | `		}` |
|        6 |  595 | `		goto Synchronize;` |
|        - |  596 | `	}` |
|      321 |  597 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      321 |  598 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      321 |  599 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  600 | `		/* Syntax error */` |
|        6 |  601 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  602 | `		if( rc != SXERR_ABORT ){` |
|        6 |  603 | `			rc = SXERR_SYNTAX;` |
|        2 |  604 | `		}` |
|        6 |  605 | `		goto Synchronize;` |
|        - |  606 | `	}` |
|      317 |  607 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  608 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      317 |  609 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        8 |  610 | `		pIn++; /* Skip ':' */` |
|        - |  611 | `		/* Skip optional '?' nullable prefix */` |
|        8 |  612 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  613 | `			pIn++;` |
|      ! 0 |  614 | `		}` |
|        - |  615 | `		/* Skip the first type (allow leading '\' and namespace path) */` |
|        8 |  616 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        8 |  617 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        8 |  618 | `			pIn++;` |
|        8 |  619 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  620 | `				pIn += 2;` |
|      ! 0 |  621 | `			}` |
|        3 |  622 | `		}` |
|        - |  623 | `		/* Skip union alternatives ( \| type )* */` |
|        9 |  624 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        5 |  625 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  626 | `			pIn++;` |
|      ! 0 |  627 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  628 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  629 | `				pIn++;` |
|      ! 0 |  630 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  631 | `					pIn += 2;` |
|      ! 0 |  632 | `				}` |
|      ! 0 |  633 | `			}` |
|      ! 0 |  634 | `		}` |
|        3 |  635 | `	}` |
|      317 |  636 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       39 |  637 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  638 | `		/* Check if we are dealing with a closure */` |
|       39 |  639 | `		if( nKey == PH7_TKWRD_USE ){` |
|       31 |  640 | `			pIn++; /* Jump the 'use' keyword */` |
|       31 |  641 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  642 | `				/* Syntax error */` |
|        6 |  643 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  644 | `				if( rc != SXERR_ABORT ){` |
|        6 |  645 | `					rc = SXERR_SYNTAX;` |
|        2 |  646 | `				}` |
|        6 |  647 | `				goto Synchronize;` |
|        - |  648 | `			}` |
|       27 |  649 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       27 |  650 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       27 |  651 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  652 | `				/* Syntax error */` |
|        6 |  653 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  654 | `				if( rc != SXERR_ABORT ){` |
|        6 |  655 | `					rc = SXERR_SYNTAX;` |
|        2 |  656 | `				}` |
|        6 |  657 | `				goto Synchronize;` |
|        - |  658 | `			}` |
|       23 |  659 | `			pIn++;` |
|       14 |  660 | `		}else{` |
|        - |  661 | `			/* Syntax error */` |
|       11 |  662 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|       11 |  663 | `			if( rc != SXERR_ABORT ){` |
|       11 |  664 | `				rc = SXERR_SYNTAX;` |
|        4 |  665 | `			}` |
|       11 |  666 | `			goto Synchronize;` |
|        - |  667 | `		}` |
|        9 |  668 | `	}` |
|      301 |  669 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      301 |  670 | `		pIn++; /* Jump the leading curly '{' */` |
|      301 |  671 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      301 |  672 | `		if( pIn < pEnd ){` |
|      301 |  673 | `			pIn++;` |
|      148 |  674 | `		}` |
|      153 |  675 | `	}else{` |
|        - |  676 | `		/* Syntax error */` |
|      ! 0 |  677 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  678 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  679 | `			return SXERR_ABORT;` |
|        - |  680 | `		}` |
|        - |  681 | `	}` |
|      301 |  682 | `	rc = SXRET_OK;` |
|      160 |  683 | `Synchronize:` |
|        - |  684 | `	/* Synchronize pointers */` |
|      325 |  685 | `	*ppCur = pIn;` |
|      325 |  686 | `	return rc;` |
|      165 |  687 | `}` |
|        - |  688 | `/*` |
|        - |  689 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|        - |  690 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|        - |  691 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|        - |  692 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|        - |  693 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|        - |  694 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|        - |  695 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|        - |  696 | ` */` |
|       26 |  697 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  698 | `{` |
|       30 |  699 | `	SyToken *pIn = *ppCur;` |
|       30 |  700 | `	sxu32 nLine = pIn->nLine;` |
|        - |  701 | `	sxi32 rc;` |
|       30 |  702 | `	pIn++; /* Jump the 'class' keyword */` |
|        - |  703 | `	/* Optional constructor argument list */` |
|       30 |  704 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  705 | `		pIn++; /* Jump '(' */` |
|        7 |  706 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        7 |  707 | `		if( pIn < pEnd ){` |
|        7 |  708 | `			pIn++; /* Jump ')' */` |
|        3 |  709 | `		}` |
|        3 |  710 | `	}` |
|        - |  711 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|        - |  712 | `	 * (no braces appear between ')' and the class body). */` |
|       58 |  713 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       32 |  714 | `		pIn++;` |
|        4 |  715 | `	}` |
|       30 |  716 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|        - |  717 | `		/* Syntax error: missing class body */` |
|      ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  719 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|      ! 0 |  720 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  721 | `			rc = SXERR_SYNTAX;` |
|      ! 0 |  722 | `		}` |
|      ! 0 |  723 | `		*ppCur = pIn;` |
|      ! 0 |  724 | `		return rc;` |
|        - |  725 | `	}` |
|       30 |  726 | `	pIn++; /* Jump the leading '{' */` |
|       30 |  727 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       30 |  728 | `	if( pIn < pEnd ){` |
|       30 |  729 | `		pIn++; /* Jump the trailing '}' */` |
|       13 |  730 | `	}` |
|       30 |  731 | `	*ppCur = pIn;` |
|       30 |  732 | `	return SXRET_OK;` |
|       17 |  733 | `}` |
|        - |  734 | `/*` |
|        - |  735 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  736 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  737 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  738 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  739 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  740 | ` */` |
|      174 |  741 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  742 | `{` |
|      179 |  743 | `	SyToken *pIn = *ppCur;` |
|        - |  744 | `	sxu32 nLine;` |
|        - |  745 | `	sxi32 rc;` |
|        - |  746 | `	int iNest;` |
|      179 |  747 | `	nLine = pIn->nLine;` |
|        - |  748 | `	/* Optional 'static' prefix */` |
|      174 |  749 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      179 |  750 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  751 | `		pIn++;` |
|        1 |  752 | `	}` |
|        - |  753 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      174 |  754 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      179 |  755 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  756 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  757 | `		goto Synchronize;` |
|        - |  758 | `	}` |
|      179 |  759 | `	pIn++; /* Jump 'fn' */` |
|       87 |  760 | `	SXUNUSED(nLine);` |
|       87 |  761 | `	SXUNUSED(pGen);` |
|        - |  762 | `	/* Optional '&' for return-by-reference */` |
|      179 |  763 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  764 | `		pIn++;` |
|      ! 0 |  765 | `	}` |
|        - |  766 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  767 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  768 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  769 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      179 |  770 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      177 |  771 | `		pIn++; /* '(' */` |
|      177 |  772 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      177 |  773 | `		if( pIn < pEnd ){` |
|      174 |  774 | `			pIn++; /* ')' */` |
|       85 |  775 | `		}` |
|       86 |  776 | `	}` |
|        - |  777 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|      179 |  778 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  779 | `		pIn++;` |
|        6 |  780 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  781 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  782 | `			pIn++;` |
|        1 |  783 | `		}` |
|        7 |  784 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        7 |  785 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  786 | `			pIn++;` |
|        7 |  787 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  788 | `				pIn += 2;` |
|      ! 0 |  789 | `			}` |
|        3 |  790 | `		}` |
|        9 |  791 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        4 |  792 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  793 | `			pIn++;` |
|      ! 0 |  794 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  795 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  796 | `				pIn++;` |
|      ! 0 |  797 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  798 | `					pIn += 2;` |
|      ! 0 |  799 | `				}` |
|      ! 0 |  800 | `			}` |
|      ! 0 |  801 | `		}` |
|        3 |  802 | `	}` |
|        - |  803 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      179 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      171 |  805 | `		pIn++;` |
|       84 |  806 | `	}` |
|        - |  807 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      179 |  808 | `	iNest = 0;` |
|     1057 |  809 | `	while( pIn < pEnd ){` |
|      956 |  810 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  811 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       75 |  812 | `			break;` |
|        - |  813 | `		}` |
|      882 |  814 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       59 |  815 | `			iNest++;` |
|      854 |  816 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       59 |  817 | `			iNest--;` |
|       28 |  818 | `		}` |
|      882 |  819 | `		pIn++;` |
|        4 |  820 | `	}` |
|      179 |  821 | `	rc = SXRET_OK;` |
|       87 |  822 | `Synchronize:` |
|      179 |  823 | `	*ppCur = pIn;` |
|      179 |  824 | `	return rc;` |
|        5 |  825 | `}` |
|        - |  826 | `/*` |
|        - |  827 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  828 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  829 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  830 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  831 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  832 | ` */` |
|       70 |  833 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  834 | `{` |
|       75 |  835 | `	SyToken *pIn = *ppCur;` |
|        - |  836 | `	sxi32 rc;` |
|       35 |  837 | `	SXUNUSED(pGen);` |
|        - |  838 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  839 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       75 |  840 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  841 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  842 | `		goto Synchronize;` |
|        - |  843 | `	}` |
|       75 |  844 | `	pIn++; /* Jump 'match' */` |
|        - |  845 | `	/* Optional '(' subject ')' */` |
|       75 |  846 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       75 |  847 | `		pIn++;` |
|       75 |  848 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       75 |  849 | `		if( pIn < pEnd ){` |
|       75 |  850 | `			pIn++; /* ')' */` |
|       35 |  851 | `		}` |
|       35 |  852 | `	}` |
|        - |  853 | `	/* Optional '{' arms '}' */` |
|       75 |  854 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       75 |  855 | `		pIn++;` |
|       75 |  856 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       75 |  857 | `		if( pIn < pEnd ){` |
|       75 |  858 | `			pIn++; /* '}' */` |
|       35 |  859 | `		}` |
|       35 |  860 | `	}` |
|       75 |  861 | `	rc = SXRET_OK;` |
|       35 |  862 | `Synchronize:` |
|       75 |  863 | `	*ppCur = pIn;` |
|       75 |  864 | `	return rc;` |
|        5 |  865 | `}` |
|        - |  866 | `/*` |
|        - |  867 | ` * Extract a single expression node from the input.` |
|        - |  868 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  869 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  870 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  871 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  872 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  873 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  874 | ` */` |
|  4370994 |  875 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|        5 |  876 | `{` |
|        - |  877 | `	ph7_expr_node *pNode;` |
|        - |  878 | `	SyToken *pCur;` |
|        - |  879 | `	sxi32 rc;` |
|        - |  880 | `	/* Allocate a new node */` |
|  4370999 |  881 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4370999 |  882 | `	if( pNode == 0 ){` |
|        - |  883 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  884 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  885 | `		 */` |
|      ! 0 |  886 | `		return SXERR_MEM;` |
|        - |  887 | `	}` |
|        - |  888 | `	/* Zero the structure */` |
|  4370999 |  889 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4370999 |  890 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  891 | `	/* Point to the head of the token stream */` |
|  4370999 |  892 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  893 | `	/* Start collecting tokens */` |
|  4370999 |  894 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|     3845 |  895 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|        - |  896 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|        - |  897 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|        - |  898 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|        - |  899 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       65 |  900 | `			pNode->pEnd = pCur;` |
|       65 |  901 | `			pCur++;` |
|       65 |  902 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       65 |  903 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       65 |  904 | `			pGen->pIn = pCur;` |
|       65 |  905 | `			*ppNode = pNode;` |
|       65 |  906 | `			return SXRET_OK;` |
|        - |  907 | `		}` |
|        - |  908 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  909 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|     3781 |  910 | `		pCur++;` |
|     3781 |  911 | `		pGen->pIn = pCur;` |
|     3781 |  912 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|     3781 |  913 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|     3781 |  914 | `		if( rc == SXRET_OK && *ppNode ){` |
|     3781 |  915 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|     1888 |  916 | `		}` |
|     3781 |  917 | `		return rc;` |
|        - |  918 | `	}` |
|  4367159 |  919 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  920 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  921 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  922 | `		 */` |
|     1215 |  923 | `		pCur++; /* Skip the opening '[' */` |
|     1215 |  924 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1215 |  925 | `		if( pCur < pGen->pEnd ){` |
|     1215 |  926 | `			pCur++; /* Skip past the closing ']' */` |
|      610 |  927 | `		}else{` |
|      ! 0 |  928 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  929 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  930 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  931 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  932 | `			}` |
|      ! 0 |  933 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  934 | `			return rc;` |
|        - |  935 | `		}` |
|        - |  936 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  937 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  938 | `		 */` |
|     1299 |  939 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      172 |  940 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      172 |  941 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       54 |  942 | `				pNode->xCode = PH7_CompileShortList;` |
|       29 |  943 | `			}else{` |
|      119 |  944 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  945 | `			}` |
|       88 |  946 | `		}else{` |
|     1047 |  947 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  948 | `		}` |
|  4366554 |  949 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  950 | `		/* Point to the instance that describe this operator */` |
|   986547 |  951 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  952 | `		/* Advance the stream cursor */` |
|   986547 |  953 | `		pCur++;` |
|  3872678 |  954 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  955 | `		/* Isolate variable */` |
|  2360725 |  956 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1180371 |  957 | `			pCur++; /* Variable variable */` |
|        5 |  958 | `		}` |
|  1180359 |  959 | `		if( pCur < pGen->pEnd ){` |
|  1180359 |  960 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  961 | `				/* Variable name */` |
|  1180331 |  962 | `				pCur++;` |
|   590195 |  963 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       24 |  964 | `				pCur++;` |
|        - |  965 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       24 |  966 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       24 |  967 | `				if( pCur < pGen->pEnd ){` |
|       19 |  968 | `					pCur++;` |
|       11 |  969 | `				}else{` |
|        6 |  970 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        6 |  971 | `					if( rc != SXERR_ABORT ){` |
|        6 |  972 | `						rc = SXERR_SYNTAX;` |
|        2 |  973 | `					}` |
|        6 |  974 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        6 |  975 | `					return rc;` |
|        - |  976 | `				}` |
|        8 |  977 | `			}` |
|   590175 |  978 | `		}` |
|  1180355 |  979 | `		pNode->xCode = PH7_CompileVariable;` |
|  2789228 |  980 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    55189 |  981 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    55189 |  982 | `		 if( bAfterMemberOp ){` |
|        - |  983 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|        - |  984 | `			  * method/property NAME, not a language construct — PHP allows any` |
|        - |  985 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|        - |  986 | `			  * as a plain literal like an ordinary identifier member name. */` |
|      177 |  987 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      177 |  988 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    55103 |  989 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  990 | `			 /* List/Array node */` |
|    31361 |  991 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  992 | `				 /* Assume a literal */` |
|      ! 0 |  993 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  994 | `				 pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  995 | `			 }else{` |
|    31361 |  996 | `				 pCur += 2;` |
|        - |  997 | `				 /* Collect array/list tokens */` |
|    31361 |  998 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    31361 |  999 | `				 if( pCur < pGen->pEnd ){` |
|    31359 | 1000 | `					 pCur++;` |
|    15682 | 1001 | `				 }else{` |
|        - | 1002 | `					 /* Syntax error */` |
|        4 | 1003 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 | 1004 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 | 1005 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1006 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1007 | `					 }` |
|        3 | 1008 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1009 | `					 return rc;` |
|        - | 1010 | `				 }` |
|    31359 | 1011 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    31359 | 1012 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       31 | 1013 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       31 | 1014 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - | 1015 | `						 /* Syntax error */` |
|        3 | 1016 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 | 1017 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1018 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1019 | `						 }` |
|        3 | 1020 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1021 | `						 return rc;` |
|        - | 1022 | `					 }` |
|       13 | 1023 | `				 }` |
|        5 | 1024 | `			 }` |
|    39337 | 1025 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1026 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      237 | 1027 | `			 pCur++; /* Skip 'yield' keyword */` |
|      237 | 1028 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1029 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1030 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      237 | 1031 | `			 pNode->xCode = PH7_CompileYield;` |
|    23545 | 1032 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1033 | `			 /* Annonymous function */` |
|      325 | 1034 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1035 | `				 /* Assume a literal */` |
|      ! 0 | 1036 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1037 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1038 | `			 }else{` |
|        - | 1039 | `				 /* Assemble annonymous functions body */` |
|      325 | 1040 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      325 | 1041 | `				 if( rc != SXRET_OK ){` |
|       28 | 1042 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1043 | `					 return rc;` |
|        - | 1044 | `				 }` |
|      301 | 1045 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1046 | `			  }` |
|    23257 | 1047 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       39 | 1048 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       22 | 1049 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       12 | 1050 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        9 | 1051 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1052 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1053 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1054 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1055 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       30 | 1056 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       30 | 1057 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1058 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1059 | `				 return rc;` |
|        - | 1060 | `			 }` |
|       30 | 1061 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    23095 | 1062 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    22998 | 1063 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       30 | 1064 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       15 | 1065 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1066 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      179 | 1067 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      179 | 1068 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1069 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1070 | `				 return rc;` |
|        - | 1071 | `			 }` |
|      179 | 1072 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    22996 | 1073 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1074 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1075 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1076 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1077 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1078 | `				 return rc;` |
|        - | 1079 | `			 }` |
|       75 | 1080 | `			 pNode->xCode = PH7_CompileMatch;` |
|    22874 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1082 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1083 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1084 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1085 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1086 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1087 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1088 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1089 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    22821 | 1090 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1091 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       93 | 1092 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       93 | 1093 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       49 | 1094 | `		 }else{` |
|        - | 1095 | `			 /* Assume a literal */` |
|    22715 | 1096 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    22715 | 1097 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1098 | `		 }` |
|  2171447 | 1099 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1100 | `		 /* Constants,function name,namespace path,class name... */` |
|   795643 | 1101 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   795643 | 1102 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   397824 | 1103 | `	 }else{` |
|  1348231 | 1104 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1105 | `			 /* Point to the code generator routine */` |
|   258271 | 1106 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   258271 | 1107 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1108 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1109 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1110 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1111 | `				 }` |
|        3 | 1112 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1113 | `				 return rc;` |
|        - | 1114 | `			 }` |
|   129132 | 1115 | `		 }` |
|        - | 1116 | `		/* Advance the stream cursor */` |
|  1348229 | 1117 | `		pCur++;` |
|        - | 1118 | `	 }` |
|        - | 1119 | `	/* Point to the end of the token stream */` |
|  4367125 | 1120 | `	pNode->pEnd = pCur;` |
|        - | 1121 | `	/* Save the node for later processing */` |
|  4367125 | 1122 | `	*ppNode = pNode;` |
|        - | 1123 | `	/* Synchronize cursors */` |
|  4367125 | 1124 | `	pGen->pIn = pCur;` |
|  4367125 | 1125 | `	return SXRET_OK;` |
|  2185502 | 1126 | `}` |
|        - | 1127 | `/*` |
|        - | 1128 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1129 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1130 | ` * level is zero.` |
|        - | 1131 | ` */` |
|    97256 | 1132 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1133 | `{` |
|    97261 | 1134 | `	SyToken *pCur = pStart;` |
|    97261 | 1135 | `	sxi32 iNest = 0;` |
|    97261 | 1136 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1137 | `		/* Last expression */` |
|    50579 | 1138 | `		return SXERR_EOF;` |
|        - | 1139 | `	}` |
|   190899 | 1140 | `	while( pCur < pEnd ){` |
|   174101 | 1141 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    29889 | 1142 | `			break;` |
|        - | 1143 | `		}` |
|   144217 | 1144 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     9795 | 1145 | `			iNest++;` |
|   139322 | 1146 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     9797 | 1147 | `			iNest--;` |
|     4896 | 1148 | `		}` |
|   144217 | 1149 | `		pCur++;` |
|        5 | 1150 | `	}` |
|    46687 | 1151 | `	*ppNext = pCur;` |
|    46687 | 1152 | `	return SXRET_OK;` |
|    48633 | 1153 | `}` |
|        - | 1154 | `/*` |
|        - | 1155 | ` * Free an expression tree.` |
|        - | 1156 | ` */` |
|  3732184 | 1157 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1158 | `{` |
|  3732189 | 1159 | `	if( pNode->pLeft ){` |
|        - | 1160 | `		/* Release the left tree */` |
|  1378377 | 1161 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   689186 | 1162 | `	}` |
|  3732189 | 1163 | `	if( pNode->pRight ){` |
|        - | 1164 | `		/* Release the right tree */` |
|   744967 | 1165 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   372481 | 1166 | `	}` |
|  3732189 | 1167 | `	if( pNode->pCond ){` |
|        - | 1168 | `		/* Release the conditional tree used by the ternary operator */` |
|     2647 | 1169 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1321 | 1170 | `	}` |
|  3732189 | 1171 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1172 | `		ph7_expr_node **apArg;` |
|        - | 1173 | `		sxu32 n;` |
|        - | 1174 | `		/* Release node arguments */` |
|   482045 | 1175 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  1036783 | 1176 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   554743 | 1177 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   277374 | 1178 | `		}` |
|   482045 | 1179 | `		SySetRelease(&pNode->aNodeArgs);` |
|   241020 | 1180 | `	}` |
|        - | 1181 | `	/* Finally,release this node */` |
|  3732189 | 1182 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3732189 | 1183 | `}` |
|        - | 1184 | `/*` |
|        - | 1185 | ` * Free an expression tree.` |
|        - | 1186 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1187 | ` */` |
|   988112 | 1188 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1189 | `{` |
|        - | 1190 | `	ph7_expr_node **apNode;` |
|        - | 1191 | `	sxu32 n;` |
|   988117 | 1192 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5355301 | 1193 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4367189 | 1194 | `		if( apNode[n] ){` |
|   988451 | 1195 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   494223 | 1196 | `		}` |
|  2183597 | 1197 | `	}` |
|   988117 | 1198 | `	return SXRET_OK;` |
|        5 | 1199 | `}` |
|        - | 1200 | `/*` |
|        - | 1201 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1202 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1203 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1204 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1205 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1206 | ` */` |
|  1349468 | 1207 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1208 | `{` |
|  1349473 | 1209 | `	if( pNode == 0 ){` |
|   832611 | 1210 | `		return 0;` |
|        - | 1211 | `	}` |
|   516867 | 1212 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1213 | `		return 1;` |
|        - | 1214 | `	}` |
|   516855 | 1215 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1216 | `		return 1;` |
|        - | 1217 | `	}` |
|   516851 | 1218 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1219 | `		return 1;` |
|        - | 1220 | `	}` |
|   516851 | 1221 | `	return 0;` |
|   674739 | 1222 | `}` |
|        - | 1223 | `/*` |
|        - | 1224 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1225 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1226 | ` */` |
|   308990 | 1227 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1228 | `{` |
|        - | 1229 | `	sxi32 iExprOp;` |
|   308995 | 1230 | `	if( pNode->pOp == 0 ){` |
|   189905 | 1231 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1232 | `	}` |
|   119095 | 1233 | `	iExprOp = pNode->pOp->iOp;` |
|   119095 | 1234 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    81713 | 1235 | `			return TRUE;` |
|        - | 1236 | `	}` |
|    37387 | 1237 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    37383 | 1238 | `		if( pNode->pLeft->pOp ) {` |
|       66 | 1239 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       31 | 1240 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1241 | `				return FALSE;` |
|        5 | 1242 | `			}` |
|    37350 | 1243 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1244 | `			return FALSE;` |
|        - | 1245 | `		}` |
|    37383 | 1246 | `		return TRUE;` |
|        - | 1247 | `	}` |
|        5 | 1248 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1249 | `		return TRUE;` |
|        - | 1250 | `	}` |
|        - | 1251 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1252 | `	return FALSE;` |
|   154500 | 1253 | `}` |
|        - | 1254 | `/* Forward declaration */` |
|        - | 1255 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1256 | `/* Macro to check if the given node is a terminal.` |
|        - | 1257 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1258 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1259 | ` * linked ternary/elvis node). */` |
|        - | 1260 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1261 | `/*` |
|        - | 1262 | ` * Buid an expression tree for each given function argument.` |
|        - | 1263 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1264 | ` */` |
|   405712 | 1265 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1266 | `{` |
|        - | 1267 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1268 | `	sxi32 rc;` |
|        - | 1269 | `	/* Process function arguments from left to right */` |
|   405717 | 1270 | `	iCur = 0;` |
|   442049 | 1271 | `	for(;;){` |
|   884103 | 1272 | `		if( iCur >= nToken ){` |
|        - | 1273 | `			/* No more arguments to process */` |
|   405691 | 1274 | `			break;` |
|        - | 1275 | `		}` |
|   478417 | 1276 | `		iNode = iCur;` |
|   478417 | 1277 | `		iNest = 0;` |
|  1183931 | 1278 | `		while( iCur < nToken ){` |
|   778243 | 1279 | `			if( apNode[iCur] ){` |
|   763483 | 1280 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    36367 | 1281 | `					break;` |
|   690754 | 1282 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   364936 | 1283 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    38914 | 1284 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1285 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1286 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1287 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1288 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1289 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1290 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    38705 | 1291 | `					iNest++;` |
|   671409 | 1292 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    38705 | 1293 | `					iNest--;` |
|    19350 | 1294 | `				}` |
|   345377 | 1295 | `			}` |
|   705519 | 1296 | `			iCur++;` |
|        5 | 1297 | `		}` |
|   478417 | 1298 | `		if( iCur > iNode ){` |
|   478411 | 1299 | `			SyString sArgName = {0, 0};` |
|        - | 1300 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1301 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1302 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   478406 | 1303 | `			if( (iCur - iNode) >= 2` |
|   264599 | 1304 | `				&& apNode[iNode]` |
|    50792 | 1305 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    29857 | 1306 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     8787 | 1307 | `				&& apNode[iNode+1]` |
|     8657 | 1308 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1309 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      219 | 1310 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      219 | 1311 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      219 | 1312 | `				apNode[iNode] = 0;` |
|      219 | 1313 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      219 | 1314 | `				apNode[iNode+1] = 0;` |
|      219 | 1315 | `				iNode += 2;` |
|        - | 1316 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1317 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      219 | 1318 | `				if( iNode >= iCur ){` |
|        4 | 1319 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1320 | `						pOp->pStart->nLine,` |
|        - | 1321 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1322 | `						&sArgName);` |
|        3 | 1323 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1324 | `						rc = SXERR_SYNTAX;` |
|        1 | 1325 | `					}` |
|        3 | 1326 | `					return rc;` |
|        - | 1327 | `				}` |
|      106 | 1328 | `			}` |
|   478404 | 1329 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1330 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1331 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1332 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1333 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1334 | `					apNode[iNode] = 0;` |
|      ! 0 | 1335 | `			}` |
|   478409 | 1336 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   478409 | 1337 | `			if( apNode[iNode] ){` |
|   478409 | 1338 | `				if( sArgName.nByte > 0 ){` |
|      216 | 1339 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      216 | 1340 | `					apNode[iNode]->sArgName = sArgName;` |
|      106 | 1341 | `				}` |
|        - | 1342 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   478409 | 1343 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   239207 | 1344 | `			}else{` |
|        - | 1345 | `				/* No expression before comma */` |
|      ! 0 | 1346 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1347 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1348 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1349 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1350 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1351 | `				}` |
|      ! 0 | 1352 | `				return rc;` |
|        - | 1353 | `			}` |
|   239207 | 1354 | `		}else{` |
|        - | 1355 | `			/* Comma with no preceding argument */` |
|        9 | 1356 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        9 | 1357 | `			if( rc != SXERR_ABORT ){` |
|        9 | 1358 | `				rc = SXERR_SYNTAX;` |
|        3 | 1359 | `			}` |
|        9 | 1360 | `			return rc;` |
|        - | 1361 | `		}` |
|        - | 1362 | `		/* Jump trailing comma */` |
|   478409 | 1363 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    72723 | 1364 | `			iCur++;` |
|    72723 | 1365 | `			if( iCur >= nToken ){` |
|        - | 1366 | `				/* Trailing comma after last argument */` |
|       19 | 1367 | `				break;` |
|        - | 1368 | `			}` |
|    36350 | 1369 | `		}` |
|        5 | 1370 | `	}` |
|   405709 | 1371 | `	return SXRET_OK;` |
|   202861 | 1372 | `}` |
|        - | 1373 | ` /*` |
|        - | 1374 | `  * Create an expression tree from an array of tokens.` |
|        - | 1375 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1376 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1377 | `  */` |
|  1583306 | 1378 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1379 | ` {` |
|        - | 1380 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1381 | `	 ph7_expr_node *pNode;` |
|        - | 1382 | `	 sxi32 iCur;` |
|        - | 1383 | `	 sxi32 rc;` |
|  1583311 | 1384 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1385 | `		 /* TICKET 1433-17: self evaluating node */` |
|   742125 | 1386 | `		 return SXRET_OK;` |
|        - | 1387 | `	 }` |
|        - | 1388 | `	 /* Process expressions enclosed in parenthesis first */` |
|  5222297 | 1389 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1390 | `		 sxi32 iNest;` |
|        - | 1391 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1392 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1393 | `		  */` |
|  4381113 | 1394 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4349821 | 1395 | `			 continue;` |
|        - | 1396 | `		 }` |
|    31297 | 1397 | `		 iNest = 1;` |
|    31297 | 1398 | `		 iLeft = iCur;` |
|        - | 1399 | `		 /* Find the closing parenthesis */` |
|    31297 | 1400 | `		 iCur++;` |
|   208681 | 1401 | `		 while( iCur < nToken ){` |
|   208681 | 1402 | `			 if( apNode[iCur] ){` |
|   208681 | 1403 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1404 | `					 /* Decrement nesting level */` |
|    54343 | 1405 | `					 iNest--;` |
|    54343 | 1406 | `					 if( iNest <= 0 ){` |
|    31297 | 1407 | `						 break;` |
|        5 | 1408 | `					 }` |
|   165866 | 1409 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1410 | `					 /* Increment nesting level */` |
|    23051 | 1411 | `					 iNest++;` |
|    11523 | 1412 | `				 }` |
|    88692 | 1413 | `			 }` |
|   177389 | 1414 | `			 iCur++;` |
|        5 | 1415 | `		 }` |
|    31297 | 1416 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1417 | `			 sxi32 j;` |
|        - | 1418 | `			 /* Recurse and process this expression */` |
|    31297 | 1419 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    31297 | 1420 | `			 if( rc != SXRET_OK ){` |
|        3 | 1421 | `				 return rc;` |
|        - | 1422 | `			 }` |
|        - | 1423 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1424 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1425 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    31295 | 1426 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    31295 | 1427 | `				 if( apNode[j] ){` |
|    31295 | 1428 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    31295 | 1429 | `					 break;` |
|        - | 1430 | `				 }` |
|      ! 0 | 1431 | `			 }` |
|    15645 | 1432 | `		 }` |
|        - | 1433 | `		 /* Free the left and right nodes */` |
|    31295 | 1434 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    31295 | 1435 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    31295 | 1436 | `		 apNode[iLeft] = 0;` |
|    31295 | 1437 | `		 apNode[iCur] = 0;` |
|    15650 | 1438 | `	 }` |
|        - | 1439 | `	  /* Process expressions enclosed in braces */` |
|  5422895 | 1440 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1441 | `		 sxi32 iNest;` |
|        - | 1442 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1443 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1444 | `		  */` |
|  4589763 | 1445 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4589755 | 1446 | `			 continue;` |
|        - | 1447 | `		 }` |
|       10 | 1448 | `		 iNest = 1;` |
|       10 | 1449 | `		 iLeft = iCur;` |
|        - | 1450 | `		 /* Find the closing parenthesis */` |
|       10 | 1451 | `		 iCur++;` |
|       16 | 1452 | `		 while( iCur < nToken ){` |
|       16 | 1453 | `			 if( apNode[iCur] ){` |
|       16 | 1454 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1455 | `					 /* Decrement nesting level */` |
|       10 | 1456 | `					 iNest--;` |
|       10 | 1457 | `					 if( iNest <= 0 ){` |
|       10 | 1458 | `						 break;` |
|      ! 0 | 1459 | `					 }` |
|        7 | 1460 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1461 | `					 /* Increment nesting level */` |
|      ! 0 | 1462 | `					 iNest++;` |
|      ! 0 | 1463 | `				 }` |
|        3 | 1464 | `			 }` |
|        7 | 1465 | `			 iCur++;` |
|        1 | 1466 | `		 }` |
|       10 | 1467 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1468 | `			 /* Recurse and process this expression */` |
|        7 | 1469 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        7 | 1470 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1471 | `				 return rc;` |
|        - | 1472 | `			 }` |
|        3 | 1473 | `		 }` |
|        - | 1474 | `		 /* Free the left and right nodes */` |
|       10 | 1475 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       10 | 1476 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       10 | 1477 | `		 apNode[iLeft] = 0;` |
|       10 | 1478 | `		 apNode[iCur] = 0;` |
|        6 | 1479 | `	 }` |
|        - | 1480 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   833137 | 1481 | `	 iLeft = -1;` |
|  5422873 | 1482 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4589753 | 1483 | `		 if( apNode[iCur] == 0 ){` |
|  1807835 | 1484 | `			 continue;` |
|        - | 1485 | `		 }` |
|  2781923 | 1486 | `		 pNode = apNode[iCur];` |
|  2781923 | 1487 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   743685 | 1488 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1489 | `				 /* Collect function arguments */` |
|   464773 | 1490 | `				 sxi32 iPtr = 0;` |
|   464773 | 1491 | `				 sxi32 nFuncTok = 0;` |
|  1707781 | 1492 | `				 while( nFuncTok + iCur < nToken ){` |
|  1707781 | 1493 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1693021 | 1494 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   484581 | 1495 | `							 iPtr++;` |
|  1450733 | 1496 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   484581 | 1497 | `							 iPtr--;` |
|   484581 | 1498 | `							 if( iPtr <= 0 ){` |
|   464773 | 1499 | `								 break;` |
|        - | 1500 | `							 }` |
|     9904 | 1501 | `						 }` |
|   614124 | 1502 | `					 }` |
|  1243013 | 1503 | `					 nFuncTok++;` |
|        5 | 1504 | `				 }` |
|   464773 | 1505 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1506 | `					 /* Syntax error */` |
|      ! 0 | 1507 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1508 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1509 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1510 | `					 }` |
|      ! 0 | 1511 | `					 return rc;` |
|        - | 1512 | `				 }` |
|   464773 | 1513 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1514 | `					 /* Syntax error */` |
|      ! 0 | 1515 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1516 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1517 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1518 | `					 }` |
|      ! 0 | 1519 | `					 return rc;` |
|        - | 1520 | `				 }` |
|   464773 | 1521 | `				 if( nFuncTok > 1 ){` |
|        - | 1522 | `					 /* Process function arguments */` |
|   405717 | 1523 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   405717 | 1524 | `					 if( rc != SXRET_OK ){` |
|       12 | 1525 | `						 return rc;` |
|        - | 1526 | `					 }` |
|   202852 | 1527 | `				 }` |
|        - | 1528 | `				 /* Link the node to the tree */` |
|   464765 | 1529 | `				 pNode->pLeft = apNode[iLeft];` |
|   464765 | 1530 | `				 apNode[iLeft] = 0;` |
|  1707749 | 1531 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1242989 | 1532 | `					 apNode[iCur+iPtr] = 0;` |
|   621497 | 1533 | `				 }` |
|   511297 | 1534 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1535 | `				 /* Subscripting */` |
|    94889 | 1536 | `				 sxi32 iArrTok = iCur + 1;` |
|    94889 | 1537 | `				 sxi32 iNest = 1;` |
|    94884 | 1538 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1539 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1540 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|      225 | 1541 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    94884 | 1542 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1543 | `						 /* Syntax error */` |
|      ! 0 | 1544 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1545 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1546 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1547 | `						 }` |
|      ! 0 | 1548 | `						 return rc;` |
|        - | 1549 | `				 }` |
|        - | 1550 | `				 /* Collect index tokens */` |
|   171345 | 1551 | `				 while( iArrTok < nToken ){` |
|   171345 | 1552 | `					 if( apNode[iArrTok] ){` |
|   171313 | 1553 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1554 | `							 /* Increment nesting level */` |
|      ! 0 | 1555 | `							 iNest++;` |
|   171313 | 1556 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1557 | `							 /* Decrement nesting level */` |
|    94889 | 1558 | `							 iNest--;` |
|    94889 | 1559 | `							 if( iNest <= 0 ){` |
|    94889 | 1560 | `								 break;` |
|        - | 1561 | `							 }` |
|      ! 0 | 1562 | `						 }` |
|    38212 | 1563 | `					 }` |
|    76461 | 1564 | `					 ++iArrTok;` |
|        5 | 1565 | `				 }` |
|    94889 | 1566 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1567 | `					 /* Recurse and process this expression */` |
|    76339 | 1568 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    76339 | 1569 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1570 | `						 return rc;` |
|        - | 1571 | `					 }` |
|        - | 1572 | `					 /* Link the node to it's index */` |
|    76339 | 1573 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    38167 | 1574 | `				 }` |
|        - | 1575 | `				 /* Link the node to the tree */` |
|    94889 | 1576 | `				 pNode->pLeft = apNode[iLeft];` |
|    94889 | 1577 | `				 pNode->pRight = 0;` |
|    94889 | 1578 | `				 apNode[iLeft] = 0;` |
|   266229 | 1579 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   171345 | 1580 | `					 apNode[iNest] = 0;` |
|    85675 | 1581 | `				 }` |
|    47447 | 1582 | `			 }else{` |
|        - | 1583 | `				 /* Member access operators [i.e: '->','::'] */` |
|   184033 | 1584 | `				  iRight = iCur + 1;` |
|   184039 | 1585 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        7 | 1586 | `					 iRight++;` |
|        1 | 1587 | `				 }` |
|   184033 | 1588 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1589 | `					 /* Syntax error */` |
|        5 | 1590 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1591 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1592 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1593 | `					 }` |
|        5 | 1594 | `					 return rc;` |
|        - | 1595 | `				 }` |
|        - | 1596 | `				 /* Link the node to the tree */` |
|   184029 | 1597 | `				 pNode->pLeft = apNode[iLeft];` |
|   184024 | 1598 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   183860 | 1599 | `					 && pNode->pLeft->pOp == 0 &&` |
|   183572 | 1600 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1601 | `						 /* Syntax error */` |
|      ! 0 | 1602 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1603 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1604 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1605 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1606 | `						 }` |
|      ! 0 | 1607 | `						 return rc;` |
|        - | 1608 | `				 }` |
|   184029 | 1609 | `				 pNode->pRight = apNode[iRight];` |
|   184029 | 1610 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1611 | `			 }` |
|   371834 | 1612 | `		 }` |
|  2781911 | 1613 | `		 iLeft = iCur;` |
|  1390958 | 1614 | `	 }` |
|        - | 1615 | `	 /* Handle left associative (new, clone) operators */` |
|  5422841 | 1616 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4589721 | 1617 | `		 if( apNode[iCur] == 0 ){` |
|  2575333 | 1618 | `			 continue;` |
|        - | 1619 | `		 }` |
|  2014393 | 1620 | `		 pNode = apNode[iCur];` |
|  2014393 | 1621 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1622 | `			 SyToken *pToken;` |
|        - | 1623 | `			 /* Get the left node */` |
|    23835 | 1624 | `			 iLeft = iCur + 1;` |
|    47415 | 1625 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    23585 | 1626 | `				 iLeft++;` |
|        5 | 1627 | `			 }` |
|    23835 | 1628 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1629 | `				  /* Syntax error */` |
|      ! 0 | 1630 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1631 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1632 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1633 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1634 | `				 }` |
|      ! 0 | 1635 | `				 return rc;` |
|        - | 1636 | `			 }` |
|        - | 1637 | `			 /* Make sure the operand are of a valid type */` |
|    23835 | 1638 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1639 | `				 /* Clone:` |
|        - | 1640 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1641 | `				  *  ++ function call (including annonymous)` |
|        - | 1642 | `				  *  ++ array member` |
|        - | 1643 | `				  *  ++ 'new' operator` |
|        - | 1644 | `				  * Example:` |
|        - | 1645 | `				  *   clone $pObj;` |
|        - | 1646 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1647 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1648 | `				  */` |
|       38 | 1649 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       36 | 1650 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1651 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1652 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1653 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1654 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1655 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1656 | `						 }` |
|      ! 0 | 1657 | `						 return rc;` |
|        - | 1658 | `					 }` |
|       16 | 1659 | `				 }` |
|       21 | 1660 | `			 }else{` |
|        - | 1661 | `				 /* New */` |
|    23801 | 1662 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      223 | 1663 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      218 | 1664 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       31 | 1665 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1666 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1667 | `						 /* Syntax error */` |
|      ! 0 | 1668 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1669 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1670 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1671 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1672 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1673 | `						 }` |
|      ! 0 | 1674 | `						 return rc;` |
|        - | 1675 | `					 }` |
|      109 | 1676 | `				 }` |
|        - | 1677 | `			 }` |
|        - | 1678 | `			  /* Link the node to the tree */` |
|    23835 | 1679 | `			 pNode->pLeft = apNode[iLeft];` |
|    23835 | 1680 | `			 apNode[iLeft] = 0;` |
|    23835 | 1681 | `			 pNode->pRight = 0; /* Paranoid */` |
|    11915 | 1682 | `		 }` |
|  1007199 | 1683 | `	 }` |
|        - | 1684 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   833125 | 1685 | `	 iLeft = -1;` |
|  5426867 | 1686 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4589721 | 1687 | `		 if( apNode[iCur] == 0 ){` |
|  2575333 | 1688 | `			 continue;` |
|        - | 1689 | `		 }` |
|  2014393 | 1690 | `		 pNode = apNode[iCur];` |
|  2014393 | 1691 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11137 | 1692 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     4062 | 1693 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1694 | `					 /* Link the node to the tree */` |
|     4073 | 1695 | `					 pNode->pLeft = apNode[iLeft];` |
|     4073 | 1696 | `					 apNode[iLeft] = 0;` |
|     2034 | 1697 | `			 }` |
|     7579 | 1698 | `		  }` |
|  2018419 | 1699 | `		 iLeft = iCur;` |
|  1011225 | 1700 | `	  }` |
|   837151 | 1701 | `	 iLeft = -1;` |
|  5426867 | 1702 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4589721 | 1703 | `		 if( apNode[iCur] == 0 ){` |
|  2579401 | 1704 | `			 continue;` |
|        - | 1705 | `		 }` |
|  2010325 | 1706 | `		 pNode = apNode[iCur];` |
|  2010325 | 1707 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11090 | 1708 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    11095 | 1709 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1710 | `					 /* Syntax error */` |
|      ! 0 | 1711 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1712 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1713 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1714 | `					 }` |
|      ! 0 | 1715 | `					 return rc;` |
|        - | 1716 | `			 }` |
|        - | 1717 | `			 /* Link the node to the tree */` |
|    11095 | 1718 | `			 pNode->pLeft = apNode[iLeft];` |
|    11095 | 1719 | `			 apNode[iLeft] = 0;` |
|        - | 1720 | `			 /* Mark as pre-increment/decrement node */` |
|    11095 | 1721 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5545 | 1722 | `		  }` |
|  2010325 | 1723 | `		 iLeft = iCur;` |
|  1005165 | 1724 | `	 }` |
|        - | 1725 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   837151 | 1726 | `	  iLeft = 0;` |
|  5426861 | 1727 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4589717 | 1728 | `		  if( apNode[iCur] ){` |
|  1999231 | 1729 | `			  pNode = apNode[iCur];` |
|  1999231 | 1730 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    34853 | 1731 | `				  if( iLeft > 0 ){` |
|        - | 1732 | `					  /* Link the node to the tree */` |
|    34851 | 1733 | `					  pNode->pLeft = apNode[iLeft];` |
|    34851 | 1734 | `					  apNode[iLeft] = 0;` |
|    34851 | 1735 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1736 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1737 | `							   /* Syntax error */` |
|      ! 0 | 1738 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1739 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1740 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1741 | `							  }` |
|      ! 0 | 1742 | `							  return rc;` |
|        - | 1743 | `						  }` |
|       36 | 1744 | `					  }` |
|    17428 | 1745 | `				  }else{` |
|        - | 1746 | `					  /* Syntax error */` |
|        3 | 1747 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1748 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1749 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1750 | `					  }` |
|        3 | 1751 | `					  return rc;` |
|        - | 1752 | `				  }` |
|    17423 | 1753 | `			  }` |
|        - | 1754 | `			  /* Save terminal position */` |
|  1999229 | 1755 | `			  iLeft = iCur;` |
|   999612 | 1756 | `		  }` |
|  2294860 | 1757 | `	  }` |
|        - | 1758 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1759 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1760 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1761 | `	  * yielding a right-leaning tree. */` |
|  5426859 | 1762 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4589715 | 1763 | `		 if( apNode[iCur] == 0 ){` |
|  2625449 | 1764 | `			 continue;` |
|        - | 1765 | `		 }` |
|  1964271 | 1766 | `		 pNode = apNode[iCur];` |
|  1964271 | 1767 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1768 | `			 sxi32 iL, iR;` |
|        - | 1769 | `			 /* Find the right operand */` |
|      113 | 1770 | `			 iR = -1;` |
|        - | 1771 | `			 {` |
|        - | 1772 | `				 sxi32 j;` |
|      125 | 1773 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1774 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1775 | `				 }` |
|        - | 1776 | `			 }` |
|        - | 1777 | `			 /* Find the left operand */` |
|      113 | 1778 | `			 iL = -1;` |
|        - | 1779 | `			 {` |
|        - | 1780 | `				 sxi32 j;` |
|      181 | 1781 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1782 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1783 | `				 }` |
|        - | 1784 | `			 }` |
|      113 | 1785 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1786 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1787 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1788 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1789 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1790 | `				 }` |
|      ! 0 | 1791 | `				 return rc;` |
|        - | 1792 | `			 }` |
|      113 | 1793 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1794 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1795 | `			 apNode[iL] = 0;` |
|      113 | 1796 | `			 apNode[iR] = 0;` |
|        - | 1797 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1798 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1799 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1800 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1801 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1802 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1803 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1804 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1805 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1806 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1807 | `			  * operands are respected. */` |
|      112 | 1808 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1809 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1810 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1811 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1812 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1813 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1814 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1815 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1816 | `				 while( pTail->pLeft` |
|       34 | 1817 | `					 && pTail->pLeft->pOp` |
|       23 | 1818 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1819 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1820 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1821 | `					 pTail = pTail->pLeft;` |
|        1 | 1822 | `				 }` |
|        - | 1823 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1824 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1825 | `				 pTail->pLeft = pNode;` |
|       27 | 1826 | `				 apNode[iCur] = pHead;` |
|       13 | 1827 | `			 }` |
|       56 | 1828 | `		 }` |
|   982138 | 1829 | `	 }` |
|        - | 1830 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  9208503 | 1831 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  8371369 | 1832 | `		 iLeft = -1;` |
| 54268175 | 1833 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 45896821 | 1834 | `			 if( apNode[iCur] == 0 ){` |
| 29587541 | 1835 | `				 continue;` |
|        - | 1836 | `			 }` |
| 16309285 | 1837 | `			 pNode = apNode[iCur];` |
| 16309285 | 1838 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1839 | `				 /* Get the right node */` |
|   249231 | 1840 | `				 iRight = iCur + 1;` |
|   356237 | 1841 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   107011 | 1842 | `					 iRight++;` |
|        5 | 1843 | `				 }` |
|   249231 | 1844 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1845 | `					 /* Syntax error */` |
|       10 | 1846 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       10 | 1847 | `					 if( rc != SXERR_ABORT ){` |
|       10 | 1848 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1849 | `					 }` |
|       10 | 1850 | `					 return rc;` |
|        - | 1851 | `				 }` |
|   249223 | 1852 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1853 | `					 sxi32  iTmp;` |
|        - | 1854 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1855 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1856 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1857 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1858 | `					  * is swapped below. */` |
|       57 | 1859 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1860 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1861 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1862 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1863 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1864 | `						 }` |
|        3 | 1865 | `						 return rc;` |
|        - | 1866 | `					 }` |
|       54 | 1867 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1868 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1869 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1870 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1871 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1872 | `						 }` |
|      ! 0 | 1873 | `						 return rc;` |
|        - | 1874 | `					 }` |
|       54 | 1875 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       38 | 1876 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1877 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1878 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1879 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1880 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1881 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1882 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1883 | `									 }` |
|      ! 0 | 1884 | `									 return rc;` |
|        - | 1885 | `							 }` |
|      ! 0 | 1886 | `						 }` |
|       18 | 1887 | `					 }` |
|        - | 1888 | `					 /* Swap operands */` |
|       54 | 1889 | `					 iTmp = iRight;` |
|       54 | 1890 | `					 iRight = iLeft;` |
|       54 | 1891 | `					 iLeft = iTmp;` |
|       26 | 1892 | `				 }` |
|        - | 1893 | `				 /* Link the node to the tree */` |
|   249221 | 1894 | `				 pNode->pLeft = apNode[iLeft];` |
|   249221 | 1895 | `				 pNode->pRight = apNode[iRight];` |
|   249221 | 1896 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   124608 | 1897 | `			 }` |
| 16309275 | 1898 | `			 iLeft = iCur;` |
|  8154640 | 1899 | `		 }` |
|  4185682 | 1900 | `	 }` |
|        - | 1901 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1902 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1903 | `	  * we are dealing with a single operator.` |
|        - | 1904 | `	  */` |
|   837139 | 1905 | `	  iLeft = -1;` |
|  5415489 | 1906 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4580999 | 1907 | `		  if( apNode[iCur] == 0 ){` |
|  3123193 | 1908 | `			  continue;` |
|        - | 1909 | `		  }` |
|  1457811 | 1910 | `		  pNode = apNode[iCur];` |
|  1457811 | 1911 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2649 | 1912 | `			  sxi32 iNest = 1;` |
|     2649 | 1913 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1914 | `				  /* Missing condition */` |
|        3 | 1915 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1916 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1917 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1918 | `				  }` |
|        3 | 1919 | `				  return rc;` |
|        - | 1920 | `			  }` |
|        - | 1921 | `			  /* Get the right node */` |
|     2647 | 1922 | `			  iRight = iCur + 1;` |
|     5549 | 1923 | `			  while( iRight < nToken  ){` |
|     5549 | 1924 | `				  if( apNode[iRight] ){` |
|     5221 | 1925 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1926 | `						  /* Increment nesting level */` |
|      ! 0 | 1927 | `						  ++iNest;` |
|     5221 | 1928 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1929 | `						  /* Decrement nesting level */` |
|     2647 | 1930 | `						  --iNest;` |
|     2647 | 1931 | `						  if( iNest <= 0 ){` |
|     2647 | 1932 | `							  break;` |
|        - | 1933 | `						  }` |
|      ! 0 | 1934 | `					  }` |
|     1287 | 1935 | `				  }` |
|     2907 | 1936 | `				  iRight++;` |
|        5 | 1937 | `			  }` |
|     2647 | 1938 | `			  if( iRight > iCur + 1 ){` |
|        - | 1939 | `				  /* Recurse and process the then expression */` |
|     2579 | 1940 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2579 | 1941 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1942 | `					  return rc;` |
|        - | 1943 | `				  }` |
|        - | 1944 | `				  /* Link the node to the tree */` |
|     2579 | 1945 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1287 | 1946 | `			  }else{` |
|        - | 1947 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1948 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1949 | `			  }` |
|     2647 | 1950 | `			  apNode[iCur + 1] = 0;` |
|     2647 | 1951 | `			  if( iRight + 1 < nToken ){` |
|        - | 1952 | `				  /* Recurse and process the else expression */` |
|     2647 | 1953 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2647 | 1954 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1955 | `					  return rc;` |
|        - | 1956 | `				  }` |
|        - | 1957 | `				  /* Link the node to the tree */` |
|     2647 | 1958 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2647 | 1959 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1326 | 1960 | `			  }else{` |
|      ! 0 | 1961 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1962 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1963 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1964 | `				 }` |
|      ! 0 | 1965 | `				 return rc;` |
|        - | 1966 | `			  }` |
|        - | 1967 | `			  /* Point to the condition */` |
|     2647 | 1968 | `			  pNode->pCond  = apNode[iLeft];` |
|     2647 | 1969 | `			  apNode[iLeft] = 0;` |
|     2647 | 1970 | `			  break;` |
|        - | 1971 | `		  }` |
|  1455167 | 1972 | `		  iLeft = iCur;` |
|   727586 | 1973 | `	  }` |
|        - | 1974 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1975 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1976 | `	  * so there is no need for a precedence loop here.` |
|        - | 1977 | `	  */` |
|   837137 | 1978 | `	 iRight = -1;` |
|  5426663 | 1979 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4589585 | 1980 | `		 if( apNode[iCur] == 0 ){` |
|  3443379 | 1981 | `			 continue;` |
|        - | 1982 | `		 }` |
|  1146211 | 1983 | `		 pNode = apNode[iCur];` |
|  1146211 | 1984 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1985 | `			 /* Get the left node */` |
|   308957 | 1986 | `			 iLeft = iCur - 1;` |
|   446937 | 1987 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   137985 | 1988 | `				 iLeft--;` |
|        5 | 1989 | `			 }` |
|   308957 | 1990 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1991 | `				 /* Syntax error */` |
|       45 | 1992 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1993 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 1994 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1995 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 1996 | `				 }else{` |
|       41 | 1997 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1998 | `				 }` |
|       45 | 1999 | `				 if( rc != SXERR_ABORT ){` |
|       43 | 2000 | `					 rc = SXERR_SYNTAX;` |
|       20 | 2001 | `				 }` |
|       45 | 2002 | `				 return rc;` |
|        - | 2003 | `			 }` |
|        - | 2004 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 2005 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 2006 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 2007 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 2008 | `			  * a write. */` |
|   308915 | 2009 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 2010 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 2011 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 2012 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 2013 | `					 rc = SXERR_SYNTAX;` |
|        4 | 2014 | `				 }` |
|       11 | 2015 | `				 return rc;` |
|        - | 2016 | `			 }` |
|   308907 | 2017 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      111 | 2018 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       78 | 2019 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2020 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2021 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2022 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2023 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2024 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2025 | `					 }else{` |
|        4 | 2026 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2027 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2028 | `					 }` |
|        6 | 2029 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2030 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2031 | `					 }` |
|        6 | 2032 | `					 return rc;` |
|        - | 2033 | `				 }` |
|       38 | 2034 | `			 }` |
|        - | 2035 | `			 /* Link the node to the tree (Reverse) */` |
|   308903 | 2036 | `			 pNode->pLeft = apNode[iRight];` |
|   308903 | 2037 | `			 pNode->pRight = apNode[iLeft];` |
|   308903 | 2038 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   154449 | 2039 | `		 }` |
|  1146157 | 2040 | `		 iRight = iCur;` |
|   573081 | 2041 | `	 }` |
|        - | 2042 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  4185395 | 2043 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3348317 | 2044 | `		 iLeft = -1;` |
| 21706365 | 2045 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 18358053 | 2046 | `			 if( apNode[iCur] == 0 ){` |
| 15009335 | 2047 | `				 continue;` |
|        - | 2048 | `			 }` |
|  3348723 | 2049 | `			 pNode = apNode[iCur];` |
|  3348723 | 2050 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2051 | `				 /* Get the right node */` |
|       72 | 2052 | `				 iRight = iCur + 1;` |
|      110 | 2053 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2054 | `					 iRight++;` |
|        2 | 2055 | `				 }` |
|       72 | 2056 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2057 | `					 /* Syntax error */` |
|      ! 0 | 2058 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2059 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2060 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2061 | `					 }` |
|      ! 0 | 2062 | `					 return rc;` |
|        - | 2063 | `				 }` |
|        - | 2064 | `				 /* Link the node to the tree */` |
|       72 | 2065 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2066 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2067 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2068 | `			 }` |
|  3348723 | 2069 | `			 iLeft = iCur;` |
|  1674364 | 2070 | `		 }` |
|  1674161 | 2071 | `	 }` |
|        - | 2072 | `	 /* Point to the root of the expression tree */` |
|  4589489 | 2073 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3752429 | 2074 | `		 if( apNode[iCur] ){` |
|   772691 | 2075 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       23 | 2076 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       23 | 2077 | `				  if( rc != SXERR_ABORT ){` |
|       23 | 2078 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2079 | `				  }` |
|       23 | 2080 | `				  return rc;` |
|        - | 2081 | `			 }` |
|   772673 | 2082 | `			 apNode[0] = apNode[iCur];` |
|   772673 | 2083 | `			 apNode[iCur] = 0;` |
|   386334 | 2084 | `		 }` |
|  1876208 | 2085 | `	 }` |
|   837065 | 2086 | `	 return SXRET_OK;` |
|   789645 | 2087 | ` }` |
|        - | 2088 | ` /*` |
|        - | 2089 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2090 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2091 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2092 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2093 | `  */` |
|   988112 | 2094 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2095 | `{` |
|        - | 2096 | `	ph7_expr_node **apNode;` |
|        - | 2097 | `	ph7_expr_node *pNode;` |
|        - | 2098 | `	sxi32 rc;` |
|        - | 2099 | `	/* Reset node container */` |
|   988117 | 2100 | `	SySetReset(pExprNode);` |
|   988117 | 2101 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2102 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2103 | `	{` |
|   988117 | 2104 | `		int iLastWasTerm = 0;` |
|   988117 | 2105 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  5355301 | 2106 | `		while( pGen->pIn < pGen->pEnd ){` |
|  4367223 | 2107 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  4367223 | 2108 | `			if( rc != SXRET_OK ){` |
|       38 | 2109 | `				return rc;` |
|        - | 2110 | `			}` |
|        - | 2111 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  4367189 | 2112 | `			if( pNode->xCode ){` |
|        - | 2113 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2290687 | 2114 | `				iLastWasTerm = 1;` |
|  3221848 | 2115 | `			}else if( pNode->pOp ){` |
|        - | 2116 | `				/* Operator node */` |
|   986547 | 2117 | `				iLastWasTerm = 0;` |
|   493276 | 2118 | `			}else{` |
|        - | 2119 | `				/* Delimiter: ')' and ']' end terms */` |
|  1089965 | 2120 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2121 | `			}` |
|        - | 2122 | `			/* A keyword in the next node is a member name only right after a member` |
|        - | 2123 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|        - | 2124 | `			 * node kind, so this single test covers all branches. */` |
|  4367189 | 2125 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|        - | 2126 | `			/* Save the extracted node */` |
|  4367189 | 2127 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2128 | `		}` |
|        - | 2129 | `	}` |
|   988083 | 2130 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2131 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2132 | `		*ppRoot = 0;` |
|      ! 0 | 2133 | `		return SXRET_OK;` |
|        - | 2134 | `	}` |
|   988083 | 2135 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2136 | `	/* Make sure we are dealing with valid nodes */` |
|   988083 | 2137 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   988083 | 2138 | `	if( rc != SXRET_OK ){` |
|        - | 2139 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2140 | `		 * cleanup the mess left behind.` |
|        - | 2141 | `		 */` |
|       54 | 2142 | `		*ppRoot = 0;` |
|       54 | 2143 | `		return rc;` |
|        - | 2144 | `	}` |
|        - | 2145 | `	/* Build the tree */` |
|   988033 | 2146 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   988033 | 2147 | `	if( rc != SXRET_OK ){` |
|        - | 2148 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2149 | `		*ppRoot = 0;` |
|      103 | 2150 | `		return rc;` |
|        - | 2151 | `	}` |
|        - | 2152 | `	/* Point to the root of the tree */` |
|   987935 | 2153 | `	*ppRoot = apNode[0];` |
|   987935 | 2154 | `	return SXRET_OK;` |
|   494061 | 2155 | `}` |
|        - | 2156 |  |
