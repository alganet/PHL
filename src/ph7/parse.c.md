# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1061/1231 lines (86.19%)

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
|   914018 |  268 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  269 |  |
|   914020 |  270 | `	sxu32 n = 0;` |
|        - |  271 | `	sxi32 rc;` |
|        - |  272 | `	/* Do a linear lookup on the operators table */` |
| 15292603 |  273 | `	for(;;){` |
| 30585208 |  274 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  275 | `			break;` |
|        - |  276 | `		}` |
| 30585208 |  277 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  278 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3590608 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1795305 |  280 | `		}else{` |
| 26994602 |  281 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  282 | `		}` |
| 30585208 |  283 | `		if( rc == 0 ){` |
|   917584 |  284 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  285 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   913664 |  286 | `				return &aOpTable[n];` |
|        - |  287 | `			}` |
|        - |  288 | `			/* Handle ambiguity */` |
|     3922 |  289 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  290 | `				/* Unary opertors have prcedence here over binary operators */` |
|      236 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|     3688 |  293 | `			if( pLast->nType & PH7_TK_OP ){` |
|      132 |  294 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  295 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      132 |  296 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  297 | `					/* Unary opertors have prcedence here over binary operators */` |
|      124 |  298 | `					return &aOpTable[n];` |
|        - |  299 | `				}` |
|        - |  300 |  |
|        4 |  301 | `			}` |
|     1782 |  302 | `		}` |
| 29671190 |  303 | `		++n; /* Next operator in the table */` |
|        2 |  304 | `	}` |
|        - |  305 | `	/* No such operator */` |
|      ! 0 |  306 | `	return 0;` |
|   457011 |  307 |  |
|        - |  308 | `/*` |
|        - |  309 | ` * Delimit a set of token stream.` |
|        - |  310 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  311 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  312 | ` */` |
|   475792 |  313 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  314 |  |
|   475794 |  315 | `	SyToken *pCur = pIn;` |
|   475794 |  316 | `	sxi32 iNest = 1;` |
|  2860100 |  317 | `	for(;;){` |
|  5720202 |  318 | `		if( pCur >= pEnd ){` |
|      168 |  319 | `			break;` |
|        - |  320 | `		}` |
|  5720036 |  321 | `		if( pCur->nType & nTokStart ){` |
|        - |  322 | `			/* Increment nesting level */` |
|   300032 |  323 | `			iNest++;` |
|  5570021 |  324 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  325 | `			/* Decrement nesting level */` |
|   775658 |  326 | `			iNest--;` |
|   775658 |  327 | `			if( iNest <= 0 ){` |
|   475628 |  328 | `				break;` |
|        - |  329 | `			}` |
|   150015 |  330 | `		}` |
|        - |  331 | `		/* Advance cursor */` |
|  5244410 |  332 | `		pCur++;` |
|        2 |  333 | `	}` |
|        - |  334 | `	/* Point to the end of the chunk */` |
|   475794 |  335 | `	*ppEnd = pCur;` |
|   475794 |  336 |  |
|        - |  337 | `/*` |
|        - |  338 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  339 | ` * Note on reserved keywords.` |
|        - |  340 | ` *  According to the PHP language reference manual:` |
|        - |  341 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  342 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  343 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  344 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  345 | ` */` |
|    18370 |  346 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  347 |  |
|    27487 |  348 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    18275 |  349 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  350 | `		){` |
|      150 |  351 | `			return TRUE;` |
|        - |  352 | `	}` |
|    18224 |  353 | `	if( bCheckFunc ){` |
|       98 |  354 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       72 |  355 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       57 |  356 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  357 | `				return TRUE;` |
|        - |  358 | `		}` |
|       22 |  359 | `	}` |
|        - |  360 | `	/* Not a language construct */` |
|    18192 |  361 | `	return FALSE;` |
|     9187 |  362 |  |
|        - |  363 | `/*` |
|        - |  364 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  365 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  366 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  367 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  368 | ` */` |
|   775776 |  369 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  370 |  |
|        - |  371 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  372 | `	sxi32 i,rc;` |
|        - |  373 |  |
|   775778 |  374 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  375 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       26 |  376 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       26 |  377 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       12 |  378 | `	}` |
|   775778 |  379 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  4177402 |  380 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3401660 |  381 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  382 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      330 |  383 | `			continue;` |
|        - |  384 | `		}` |
|  3401332 |  385 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   373030 |  386 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    18566 |  387 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  388 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   348148 |  389 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  390 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  391 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  392 | `						 */` |
|   348148 |  393 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   348148 |  394 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   348148 |  395 | `						apNode[i]->pOp = &sFCallOp;` |
|   174073 |  396 | `					}` |
|   174073 |  397 | `			}` |
|   373030 |  398 | `			iParen++;` |
|  3214818 |  399 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   373030 |  400 | `			if( iParen <= 0 ){` |
|       13 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  402 | `				if( rc != SXERR_ABORT ){` |
|       13 |  403 | `					rc = SXERR_SYNTAX;` |
|        6 |  404 | `				}` |
|       13 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|   373018 |  407 | `			iParen--;` |
|  2841784 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    75296 |  409 | `			iSquare++;` |
|  2617629 |  410 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    75310 |  411 | `			if( iSquare <= 0 ){` |
|        7 |  412 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  413 | `				if( rc != SXERR_ABORT ){` |
|        7 |  414 | `					rc = SXERR_SYNTAX;` |
|        3 |  415 | `				}` |
|        7 |  416 | `				return rc;` |
|        - |  417 | `			}` |
|    75304 |  418 | `			iSquare--;` |
|  2542325 |  419 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       14 |  420 | `			iBraces++;` |
|       14 |  421 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|  2504668 |  466 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       15 |  467 | `			if( iBraces <= 0 ){` |
|       13 |  468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  469 | `				if( rc != SXERR_ABORT ){` |
|       13 |  470 | `					rc = SXERR_SYNTAX;` |
|        6 |  471 | `				}` |
|       13 |  472 | `				return rc;` |
|        - |  473 | `			}` |
|        3 |  474 | `			iBraces--;` |
|  2504649 |  475 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2274 |  476 | `			if( iQuesty > 0 ){` |
|     2094 |  477 | `				iQuesty--;` |
|     1228 |  478 | `			}else if( iParen <= 0 ){` |
|        - |  479 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  480 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  481 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  482 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  483 | `				if( rc != SXERR_ABORT ){` |
|        5 |  484 | `					rc = SXERR_SYNTAX;` |
|        2 |  485 | `				}` |
|        5 |  486 | `				return rc;` |
|        2 |  487 | `			}` |
|  2503510 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   714812 |  489 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   714812 |  490 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2096 |  491 | `				iQuesty++;` |
|   713765 |  492 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      344 |  493 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      171 |  511 | `			}` |
|   357405 |  512 | `		}` |
|  1700650 |  513 | `	}` |
|   775744 |  514 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  516 | `		if( rc != SXERR_ABORT ){` |
|       17 |  517 | `			rc = SXERR_SYNTAX;` |
|        8 |  518 | `		}` |
|       17 |  519 | `		return rc;` |
|        - |  520 | `	}` |
|   775728 |  521 | `	return SXRET_OK;` |
|   387890 |  522 |  |
|        - |  523 | `/*` |
|        - |  524 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  525 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  526 | ` */` |
|   646582 |  527 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  528 |  |
|   646584 |  529 | `	SyToken *pIn = *ppCur;` |
|        - |  530 | `	/* Jump the first literal seen */` |
|   646584 |  531 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   646550 |  532 | `		pIn++;` |
|   323274 |  533 | `	}` |
|   323332 |  534 | `	for(;;){` |
|   646666 |  535 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       84 |  536 | `			pIn++;` |
|       84 |  537 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       82 |  538 | `				pIn++;` |
|       40 |  539 | `			}` |
|       43 |  540 | `		}else{` |
|   323293 |  541 | `			break;` |
|        - |  542 | `		}` |
|        2 |  543 | `	}` |
|        - |  544 | `	/* Synchronize pointers */` |
|   646584 |  545 | `	*ppCur = pIn;` |
|   646584 |  546 |  |
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
|      202 |  580 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  581 |  |
|      204 |  582 | `	SyToken *pIn = *ppCur;` |
|        - |  583 | `	sxu32 nLine;` |
|        - |  584 | `	sxi32 rc;` |
|        - |  585 | `	/* Jump the 'function' keyword */` |
|      204 |  586 | `	nLine = pIn->nLine;` |
|      204 |  587 | `	pIn++;` |
|      204 |  588 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  589 | `		pIn++;` |
|        1 |  590 | `	}` |
|      204 |  591 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  592 | `		/* Syntax error */` |
|        5 |  593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  594 | `		if( rc != SXERR_ABORT ){` |
|        5 |  595 | `			rc = SXERR_SYNTAX;` |
|        2 |  596 | `		}` |
|        5 |  597 | `		goto Synchronize;` |
|        - |  598 | `	}` |
|      200 |  599 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      200 |  600 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      200 |  601 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  602 | `		/* Syntax error */` |
|        5 |  603 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  604 | `		if( rc != SXERR_ABORT ){` |
|        5 |  605 | `			rc = SXERR_SYNTAX;` |
|        2 |  606 | `		}` |
|        5 |  607 | `		goto Synchronize;` |
|        - |  608 | `	}` |
|      196 |  609 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  610 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      196 |  611 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      196 |  638 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       32 |  639 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  640 | `		/* Check if we are dealing with a closure */` |
|       32 |  641 | `		if( nKey == PH7_TKWRD_USE ){` |
|       24 |  642 | `			pIn++; /* Jump the 'use' keyword */` |
|       24 |  643 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  644 | `				/* Syntax error */` |
|        5 |  645 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  646 | `				if( rc != SXERR_ABORT ){` |
|        5 |  647 | `					rc = SXERR_SYNTAX;` |
|        2 |  648 | `				}` |
|        5 |  649 | `				goto Synchronize;` |
|        - |  650 | `			}` |
|       20 |  651 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       20 |  652 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       20 |  653 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  654 | `				/* Syntax error */` |
|        5 |  655 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  656 | `				if( rc != SXERR_ABORT ){` |
|        5 |  657 | `					rc = SXERR_SYNTAX;` |
|        2 |  658 | `				}` |
|        5 |  659 | `				goto Synchronize;` |
|        - |  660 | `			}` |
|       16 |  661 | `			pIn++;` |
|        9 |  662 | `		}else{` |
|        - |  663 | `			/* Syntax error */` |
|        9 |  664 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  665 | `			if( rc != SXERR_ABORT ){` |
|        9 |  666 | `				rc = SXERR_SYNTAX;` |
|        4 |  667 | `			}` |
|        9 |  668 | `			goto Synchronize;` |
|        - |  669 | `		}` |
|        7 |  670 | `	}` |
|      180 |  671 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      180 |  672 | `		pIn++; /* Jump the leading curly '{' */` |
|      180 |  673 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      180 |  674 | `		if( pIn < pEnd ){` |
|      180 |  675 | `			pIn++;` |
|       89 |  676 | `		}` |
|       91 |  677 | `	}else{` |
|        - |  678 | `		/* Syntax error */` |
|      ! 0 |  679 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|        - |  683 | `	}` |
|      180 |  684 | `	rc = SXRET_OK;` |
|      101 |  685 | `Synchronize:` |
|        - |  686 | `	/* Synchronize pointers */` |
|      204 |  687 | `	*ppCur = pIn;` |
|      204 |  688 | `	return rc;` |
|      103 |  689 |  |
|        - |  690 | `/*` |
|        - |  691 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  692 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  693 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  694 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  695 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  696 | ` */` |
|       86 |  697 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  698 |  |
|       88 |  699 | `	SyToken *pIn = *ppCur;` |
|        - |  700 | `	sxu32 nLine;` |
|        - |  701 | `	sxi32 rc;` |
|        - |  702 | `	int iNest;` |
|       88 |  703 | `	nLine = pIn->nLine;` |
|        - |  704 | `	/* Optional 'static' prefix */` |
|       86 |  705 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       88 |  706 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  707 | `		pIn++;` |
|        1 |  708 | `	}` |
|        - |  709 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       86 |  710 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       88 |  711 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  712 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  713 | `		goto Synchronize;` |
|        - |  714 | `	}` |
|       88 |  715 | `	pIn++; /* Jump 'fn' */` |
|       43 |  716 | `	SXUNUSED(nLine);` |
|       43 |  717 | `	SXUNUSED(pGen);` |
|        - |  718 | `	/* Optional '&' for return-by-reference */` |
|       88 |  719 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  720 | `		pIn++;` |
|      ! 0 |  721 | `	}` |
|        - |  722 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  723 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  724 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  725 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       88 |  726 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       86 |  727 | `		pIn++; /* '(' */` |
|       86 |  728 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       86 |  729 | `		if( pIn < pEnd ){` |
|       84 |  730 | `			pIn++; /* ')' */` |
|       41 |  731 | `		}` |
|       42 |  732 | `	}` |
|        - |  733 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|       88 |  734 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       88 |  760 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       81 |  761 | `		pIn++;` |
|       40 |  762 | `	}` |
|        - |  763 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       88 |  764 | `	iNest = 0;` |
|      586 |  765 | `	while( pIn < pEnd ){` |
|      506 |  766 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  767 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  768 | `			break;` |
|        - |  769 | `		}` |
|      500 |  770 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       23 |  771 | `			iNest++;` |
|      489 |  772 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       23 |  773 | `			iNest--;` |
|       11 |  774 | `		}` |
|      500 |  775 | `		pIn++;` |
|        2 |  776 | `	}` |
|       88 |  777 | `	rc = SXRET_OK;` |
|       43 |  778 | `Synchronize:` |
|       88 |  779 | `	*ppCur = pIn;` |
|       88 |  780 | `	return rc;` |
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
|  3401826 |  831 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  832 |  |
|        - |  833 | `	ph7_expr_node *pNode;` |
|        - |  834 | `	SyToken *pCur;` |
|        - |  835 | `	sxi32 rc;` |
|        - |  836 | `	/* Allocate a new node */` |
|  3401828 |  837 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3401828 |  838 | `	if( pNode == 0 ){` |
|        - |  839 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  840 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  841 | `		 */` |
|      ! 0 |  842 | `		return SXERR_MEM;` |
|        - |  843 | `	}` |
|        - |  844 | `	/* Zero the structure */` |
|  3401828 |  845 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3401828 |  846 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  847 | `	/* Point to the head of the token stream */` |
|  3401828 |  848 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  849 | `	/* Start collecting tokens */` |
|  3401828 |  850 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  851 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  852 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       20 |  853 | `		pCur++;` |
|       20 |  854 | `		pGen->pIn = pCur;` |
|       20 |  855 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       20 |  856 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       20 |  857 | `		if( rc == SXRET_OK && *ppNode ){` |
|       20 |  858 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        9 |  859 | `		}` |
|       20 |  860 | `		return rc;` |
|        - |  861 | `	}` |
|  3401810 |  862 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  863 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  864 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  865 | `		 */` |
|      332 |  866 | `		pCur++; /* Skip the opening '[' */` |
|      332 |  867 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      332 |  868 | `		if( pCur < pGen->pEnd ){` |
|      332 |  869 | `			pCur++; /* Skip past the closing ']' */` |
|      167 |  870 | `		}else{` |
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
|      355 |  882 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  883 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  884 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  885 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  886 | `			}else{` |
|       19 |  887 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  888 | `			}` |
|       25 |  889 | `		}else{` |
|      286 |  890 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  891 | `		}` |
|  3401645 |  892 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  893 | `		/* Point to the instance that describe this operator */` |
|   790140 |  894 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  895 | `		/* Advance the stream cursor */` |
|   790140 |  896 | `		pCur++;` |
|  3006411 |  897 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  898 | `		/* Isolate variable */` |
|  1839698 |  899 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   919856 |  900 | `			pCur++; /* Variable variable */` |
|        2 |  901 | `		}` |
|   919844 |  902 | `		if( pCur < pGen->pEnd ){` |
|   919844 |  903 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  904 | `				/* Variable name */` |
|   919816 |  905 | `				pCur++;` |
|   459937 |  906 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   459919 |  921 | `		}` |
|   919840 |  922 | `		pNode->xCode = PH7_CompileVariable;` |
|  2151419 |  923 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    43956 |  924 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    43956 |  925 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  926 | `			 /* List/Array node */` |
|    25298 |  927 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  928 | `				 /* Assume a literal */` |
|       17 |  929 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  930 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  931 | `			 }else{` |
|    25282 |  932 | `				 pCur += 2;` |
|        - |  933 | `				 /* Collect array/list tokens */` |
|    25282 |  934 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    25282 |  935 | `				 if( pCur < pGen->pEnd ){` |
|    25280 |  936 | `					 pCur++;` |
|    12641 |  937 | `				 }else{` |
|        - |  938 | `					 /* Syntax error */` |
|        4 |  939 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  940 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  941 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  942 | `						 rc = SXERR_SYNTAX;` |
|        1 |  943 | `					 }` |
|        3 |  944 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  945 | `					 return rc;` |
|        - |  946 | `				 }` |
|    25280 |  947 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    25280 |  948 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    31306 |  961 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  962 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  963 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  964 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  965 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  966 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  967 | `			 pNode->xCode = PH7_CompileYield;` |
|    18643 |  968 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  969 | `			 /* Annonymous function */` |
|      204 |  970 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  971 | `				 /* Assume a literal */` |
|      ! 0 |  972 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  973 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  974 | `			 }else{` |
|        - |  975 | `				 /* Assemble annonymous functions body */` |
|      204 |  976 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      204 |  977 | `				 if( rc != SXRET_OK ){` |
|       25 |  978 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  979 | `					 return rc;` |
|        - |  980 | `				 }` |
|      180 |  981 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  982 | `			  }` |
|    18514 |  983 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    18382 |  984 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  985 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  986 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  987 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       88 |  988 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       88 |  989 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  990 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  991 | `				 return rc;` |
|        - |  992 | `			 }` |
|       88 |  993 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    18381 |  994 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - |  995 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       72 |  996 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       72 |  997 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  998 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  999 | `				 return rc;` |
|        - | 1000 | `			 }` |
|       72 | 1001 | `			 pNode->xCode = PH7_CompileMatch;` |
|    18303 | 1002 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1003 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1004 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1005 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1006 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1007 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1008 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1009 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1010 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    18250 | 1011 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1012 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       82 | 1013 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       82 | 1014 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       42 | 1015 | `		 }else{` |
|        - | 1016 | `			 /* Assume a literal */` |
|    18152 | 1017 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    18152 | 1018 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 | 1019 | `		 }` |
|  1669509 | 1020 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1021 | `		 /* Constants,function name,namespace path,class name... */` |
|   628418 | 1022 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   628418 | 1023 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   314210 | 1024 | `	 }else{` |
|  1019130 | 1025 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1026 | `			 /* Point to the code generator routine */` |
|   195446 | 1027 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   195446 | 1028 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1029 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1030 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1031 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1032 | `				 }` |
|        3 | 1033 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1034 | `				 return rc;` |
|        - | 1035 | `			 }` |
|    97721 | 1036 | `		 }` |
|        - | 1037 | `		/* Advance the stream cursor */` |
|  1019128 | 1038 | `		pCur++;` |
|        - | 1039 | `	 }` |
|        - | 1040 | `	/* Point to the end of the token stream */` |
|  3401776 | 1041 | `	pNode->pEnd = pCur;` |
|        - | 1042 | `	/* Save the node for later processing */` |
|  3401776 | 1043 | `	*ppNode = pNode;` |
|        - | 1044 | `	/* Synchronize cursors */` |
|  3401776 | 1045 | `	pGen->pIn = pCur;` |
|  3401776 | 1046 | `	return SXRET_OK;` |
|  1700915 | 1047 |  |
|        - | 1048 | `/*` |
|        - | 1049 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1050 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1051 | ` * level is zero.` |
|        - | 1052 | ` */` |
|    76284 | 1053 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 | 1054 |  |
|    76286 | 1055 | `	SyToken *pCur = pStart;` |
|    76286 | 1056 | `	sxi32 iNest = 0;` |
|    76286 | 1057 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1058 | `		/* Last expression */` |
|    40484 | 1059 | `		return SXERR_EOF;` |
|        - | 1060 | `	}` |
|   144478 | 1061 | `	while( pCur < pEnd ){` |
|   131058 | 1062 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    22384 | 1063 | `			break;` |
|        - | 1064 | `		}` |
|   108676 | 1065 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     6304 | 1066 | `			iNest++;` |
|   105525 | 1067 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     6306 | 1068 | `			iNest--;` |
|     3152 | 1069 | `		}` |
|   108676 | 1070 | `		pCur++;` |
|        2 | 1071 | `	}` |
|    35804 | 1072 | `	*ppNext = pCur;` |
|    35804 | 1073 | `	return SXRET_OK;` |
|    38144 | 1074 |  |
|        - | 1075 | `/*` |
|        - | 1076 | ` * Free an expression tree.` |
|        - | 1077 | ` */` |
|  2936736 | 1078 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1079 |  |
|  2936738 | 1080 | `	if( pNode->pLeft ){` |
|        - | 1081 | `		/* Release the left tree */` |
|  1098546 | 1082 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   549272 | 1083 | `	}` |
|  2936738 | 1084 | `	if( pNode->pRight ){` |
|        - | 1085 | `		/* Release the right tree */` |
|   608180 | 1086 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   304089 | 1087 | `	}` |
|  2936738 | 1088 | `	if( pNode->pCond ){` |
|        - | 1089 | `		/* Release the conditional tree used by the ternary operator */` |
|     2092 | 1090 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1045 | 1091 | `	}` |
|  2936738 | 1092 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1093 | `		ph7_expr_node **apArg;` |
|        - | 1094 | `		sxu32 n;` |
|        - | 1095 | `		/* Release node arguments */` |
|   362190 | 1096 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   763874 | 1097 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   401686 | 1098 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   200844 | 1099 | `		}` |
|   362190 | 1100 | `		SySetRelease(&pNode->aNodeArgs);` |
|   181094 | 1101 | `	}` |
|        - | 1102 | `	/* Finally,release this node */` |
|  2936738 | 1103 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2936738 | 1104 |  |
|        - | 1105 | `/*` |
|        - | 1106 | ` * Free an expression tree.` |
|        - | 1107 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1108 | ` */` |
|   775810 | 1109 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1110 |  |
|        - | 1111 | `	ph7_expr_node **apNode;` |
|        - | 1112 | `	sxu32 n;` |
|   775812 | 1113 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  4177586 | 1114 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3401776 | 1115 | `		if( apNode[n] ){` |
|   776146 | 1116 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   388072 | 1117 | `		}` |
|  1700889 | 1118 | `	}` |
|   775812 | 1119 | `	return SXRET_OK;` |
|        2 | 1120 |  |
|        - | 1121 | `/*` |
|        - | 1122 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1123 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1124 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1125 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1126 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1127 | ` */` |
|  1089110 | 1128 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        2 | 1129 |  |
|  1089112 | 1130 | `	if( pNode == 0 ){` |
|   670896 | 1131 | `		return 0;` |
|        - | 1132 | `	}` |
|   418218 | 1133 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       13 | 1134 | `		return 1;` |
|        - | 1135 | `	}` |
|   418206 | 1136 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        5 | 1137 | `		return 1;` |
|        - | 1138 | `	}` |
|   418202 | 1139 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1140 | `		return 1;` |
|        - | 1141 | `	}` |
|   418202 | 1142 | `	return 0;` |
|   544557 | 1143 |  |
|        - | 1144 | `/*` |
|        - | 1145 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1146 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1147 | ` */` |
|   246260 | 1148 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1149 |  |
|        - | 1150 | `	sxi32 iExprOp;` |
|   246262 | 1151 | `	if( pNode->pOp == 0 ){` |
|   148684 | 1152 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1153 | `	}` |
|    97580 | 1154 | `	iExprOp = pNode->pOp->iOp;` |
|    97580 | 1155 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    67842 | 1156 | `			return TRUE;` |
|        - | 1157 | `	}` |
|    29740 | 1158 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    29736 | 1159 | `		if( pNode->pLeft->pOp ) {` |
|       18 | 1160 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        5 | 1161 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1162 | `				return FALSE;` |
|        1 | 1163 | `			}` |
|    29727 | 1164 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1165 | `			return FALSE;` |
|        - | 1166 | `		}` |
|    29736 | 1167 | `		return TRUE;` |
|        - | 1168 | `	}` |
|        5 | 1169 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1170 | `		return TRUE;` |
|        - | 1171 | `	}` |
|        - | 1172 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1173 | `	return FALSE;` |
|   123132 | 1174 |  |
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
|   301642 | 1186 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1187 |  |
|        - | 1188 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1189 | `	sxi32 rc;` |
|        - | 1190 | `	/* Process function arguments from left to right */` |
|   301644 | 1191 | `	iCur = 0;` |
|   321378 | 1192 | `	for(;;){` |
|   642758 | 1193 | `		if( iCur >= nToken ){` |
|        - | 1194 | `			/* No more arguments to process */` |
|   301618 | 1195 | `			break;` |
|        - | 1196 | `		}` |
|   341142 | 1197 | `		iNode = iCur;` |
|   341142 | 1198 | `		iNest = 0;` |
|   851064 | 1199 | `		while( iCur < nToken ){` |
|   549446 | 1200 | `			if( apNode[iCur] ){` |
|   537702 | 1201 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    19763 | 1202 | `					break;` |
|   498180 | 1203 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    27298 | 1204 | `					iNest++;` |
|   484532 | 1205 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    27278 | 1206 | `					iNest--;` |
|    13638 | 1207 | `				}` |
|   249089 | 1208 | `			}` |
|   509924 | 1209 | `			iCur++;` |
|        2 | 1210 | `		}` |
|   341142 | 1211 | `		if( iCur > iNode ){` |
|   341136 | 1212 | `			SyString sArgName = {0, 0};` |
|        - | 1213 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1214 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1215 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   342922 | 1216 | `			if( (iCur - iNode) >= 2` |
|   188978 | 1217 | `				&& apNode[iNode]` |
|    36822 | 1218 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    20241 | 1219 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3618 | 1220 | `				&& apNode[iNode+1]` |
|     3578 | 1221 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1222 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      178 | 1223 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      178 | 1224 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      178 | 1225 | `				apNode[iNode] = 0;` |
|      178 | 1226 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      178 | 1227 | `				apNode[iNode+1] = 0;` |
|      178 | 1228 | `				iNode += 2;` |
|        - | 1229 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1230 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      178 | 1231 | `				if( iNode >= iCur ){` |
|        4 | 1232 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1233 | `						pOp->pStart->nLine,` |
|        - | 1234 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1235 | `						&sArgName);` |
|        3 | 1236 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1237 | `						rc = SXERR_SYNTAX;` |
|        1 | 1238 | `					}` |
|        3 | 1239 | `					return rc;` |
|        - | 1240 | `				}` |
|       87 | 1241 | `			}` |
|   341132 | 1242 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1243 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1244 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1245 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1246 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1247 | `					apNode[iNode] = 0;` |
|      ! 0 | 1248 | `			}` |
|   341134 | 1249 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   341134 | 1250 | `			if( apNode[iNode] ){` |
|   341134 | 1251 | `				if( sArgName.nByte > 0 ){` |
|      176 | 1252 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      176 | 1253 | `					apNode[iNode]->sArgName = sArgName;` |
|       87 | 1254 | `				}` |
|        - | 1255 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   341134 | 1256 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   170568 | 1257 | `			}else{` |
|        - | 1258 | `				/* No expression before comma */` |
|      ! 0 | 1259 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1260 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1261 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1262 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1263 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1264 | `				}` |
|      ! 0 | 1265 | `				return rc;` |
|        - | 1266 | `			}` |
|   170568 | 1267 | `		}else{` |
|        - | 1268 | `			/* Comma with no preceding argument */` |
|        7 | 1269 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1270 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1271 | `				rc = SXERR_SYNTAX;` |
|        3 | 1272 | `			}` |
|        7 | 1273 | `			return rc;` |
|        - | 1274 | `		}` |
|        - | 1275 | `		/* Jump trailing comma */` |
|   341134 | 1276 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    39518 | 1277 | `			iCur++;` |
|    39518 | 1278 | `			if( iCur >= nToken ){` |
|        - | 1279 | `				/* Trailing comma after last argument */` |
|       19 | 1280 | `				break;` |
|        - | 1281 | `			}` |
|    19749 | 1282 | `		}` |
|        2 | 1283 | `	}` |
|   301636 | 1284 | `	return SXRET_OK;` |
|   150823 | 1285 |  |
|        - | 1286 | ` /*` |
|        - | 1287 | `  * Create an expression tree from an array of tokens.` |
|        - | 1288 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1289 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1290 | `  */` |
|  1209566 | 1291 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1292 | ` {` |
|        - | 1293 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1294 | `	 ph7_expr_node *pNode;` |
|        - | 1295 | `	 sxi32 iCur;` |
|        - | 1296 | `	 sxi32 rc;` |
|  1209568 | 1297 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1298 | `		 /* TICKET 1433-17: self evaluating node */` |
|   547226 | 1299 | `		 return SXRET_OK;` |
|        - | 1300 | `	 }` |
|        - | 1301 | `	 /* Process expressions enclosed in parenthesis first */` |
|  4066852 | 1302 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1303 | `		 sxi32 iNest;` |
|        - | 1304 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1305 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1306 | `		  */` |
|  3404512 | 1307 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3379640 | 1308 | `			 continue;` |
|        - | 1309 | `		 }` |
|    24874 | 1310 | `		 iNest = 1;` |
|    24874 | 1311 | `		 iLeft = iCur;` |
|        - | 1312 | `		 /* Find the closing parenthesis */` |
|    24874 | 1313 | `		 iCur++;` |
|   165324 | 1314 | `		 while( iCur < nToken ){` |
|   165324 | 1315 | `			 if( apNode[iCur] ){` |
|   165324 | 1316 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1317 | `					 /* Decrement nesting level */` |
|    43084 | 1318 | `					 iNest--;` |
|    43084 | 1319 | `					 if( iNest <= 0 ){` |
|    24874 | 1320 | `						 break;` |
|        2 | 1321 | `					 }` |
|   131347 | 1322 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1323 | `					 /* Increment nesting level */` |
|    18212 | 1324 | `					 iNest++;` |
|     9105 | 1325 | `				 }` |
|    70225 | 1326 | `			 }` |
|   140452 | 1327 | `			 iCur++;` |
|        2 | 1328 | `		 }` |
|    24874 | 1329 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1330 | `			 sxi32 j;` |
|        - | 1331 | `			 /* Recurse and process this expression */` |
|    24874 | 1332 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    24874 | 1333 | `			 if( rc != SXRET_OK ){` |
|        3 | 1334 | `				 return rc;` |
|        - | 1335 | `			 }` |
|        - | 1336 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1337 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1338 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    24872 | 1339 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    24872 | 1340 | `				 if( apNode[j] ){` |
|    24872 | 1341 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    24872 | 1342 | `					 break;` |
|        - | 1343 | `				 }` |
|      ! 0 | 1344 | `			 }` |
|    12435 | 1345 | `		 }` |
|        - | 1346 | `		 /* Free the left and right nodes */` |
|    24872 | 1347 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    24872 | 1348 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    24872 | 1349 | `		 apNode[iLeft] = 0;` |
|    24872 | 1350 | `		 apNode[iCur] = 0;` |
|    12437 | 1351 | `	 }` |
|        - | 1352 | `	  /* Process expressions enclosed in braces */` |
|  4225816 | 1353 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1354 | `		 sxi32 iNest;` |
|        - | 1355 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1356 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1357 | `		  */` |
|  3569820 | 1358 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3569818 | 1359 | `			 continue;` |
|        - | 1360 | `		 }` |
|        3 | 1361 | `		 iNest = 1;` |
|        3 | 1362 | `		 iLeft = iCur;` |
|        - | 1363 | `		 /* Find the closing parenthesis */` |
|        3 | 1364 | `		 iCur++;` |
|        3 | 1365 | `		 while( iCur < nToken ){` |
|        3 | 1366 | `			 if( apNode[iCur] ){` |
|        3 | 1367 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1368 | `					 /* Decrement nesting level */` |
|        3 | 1369 | `					 iNest--;` |
|        3 | 1370 | `					 if( iNest <= 0 ){` |
|        3 | 1371 | `						 break;` |
|      ! 0 | 1372 | `					 }` |
|      ! 0 | 1373 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1374 | `					 /* Increment nesting level */` |
|      ! 0 | 1375 | `					 iNest++;` |
|      ! 0 | 1376 | `				 }` |
|      ! 0 | 1377 | `			 }` |
|      ! 0 | 1378 | `			 iCur++;` |
|      ! 0 | 1379 | `		 }` |
|        3 | 1380 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1381 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1382 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1383 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1384 | `				 return rc;` |
|        - | 1385 | `			 }` |
|      ! 0 | 1386 | `		 }` |
|        - | 1387 | `		 /* Free the left and right nodes */` |
|        3 | 1388 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        3 | 1389 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        3 | 1390 | `		 apNode[iLeft] = 0;` |
|        3 | 1391 | `		 apNode[iCur] = 0;` |
|        2 | 1392 | `	 }` |
|        - | 1393 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   655998 | 1394 | `	 iLeft = -1;` |
|  4225782 | 1395 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3569798 | 1396 | `		 if( apNode[iCur] == 0 ){` |
|  1359952 | 1397 | `			 continue;` |
|        - | 1398 | `		 }` |
|  2209848 | 1399 | `		 pNode = apNode[iCur];` |
|  2209848 | 1400 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   583840 | 1401 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1402 | `				 /* Collect function arguments */` |
|   348144 | 1403 | `				 sxi32 iPtr = 0;` |
|   348144 | 1404 | `				 sxi32 nFuncTok = 0;` |
|  1245732 | 1405 | `				 while( nFuncTok + iCur < nToken ){` |
|  1245732 | 1406 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1233988 | 1407 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   360430 | 1408 | `							 iPtr++;` |
|  1053774 | 1409 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   360430 | 1410 | `							 iPtr--;` |
|   360430 | 1411 | `							 if( iPtr <= 0 ){` |
|   348144 | 1412 | `								 break;` |
|        - | 1413 | `							 }` |
|     6143 | 1414 | `						 }` |
|   442922 | 1415 | `					 }` |
|   897590 | 1416 | `					 nFuncTok++;` |
|        2 | 1417 | `				 }` |
|   348144 | 1418 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1419 | `					 /* Syntax error */` |
|      ! 0 | 1420 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1421 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1422 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1423 | `					 }` |
|      ! 0 | 1424 | `					 return rc;` |
|        - | 1425 | `				 }` |
|   348144 | 1426 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1427 | `					 /* Syntax error */` |
|      ! 0 | 1428 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1429 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1430 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1431 | `					 }` |
|      ! 0 | 1432 | `					 return rc;` |
|        - | 1433 | `				 }` |
|   348144 | 1434 | `				 if( nFuncTok > 1 ){` |
|        - | 1435 | `					 /* Process function arguments */` |
|   301644 | 1436 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   301644 | 1437 | `					 if( rc != SXRET_OK ){` |
|        9 | 1438 | `						 return rc;` |
|        - | 1439 | `					 }` |
|   150817 | 1440 | `				 }` |
|        - | 1441 | `				 /* Link the node to the tree */` |
|   348136 | 1442 | `				 pNode->pLeft = apNode[iLeft];` |
|   348136 | 1443 | `				 apNode[iLeft] = 0;` |
|  1245700 | 1444 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   897566 | 1445 | `					 apNode[iCur+iPtr] = 0;` |
|   448784 | 1446 | `				 }` |
|   409765 | 1447 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1448 | `				 /* Subscripting */` |
|    75304 | 1449 | `				 sxi32 iArrTok = iCur + 1;` |
|    75304 | 1450 | `				 sxi32 iNest = 1;` |
|    75386 | 1451 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1452 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1453 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1454 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    75302 | 1455 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1456 | `						 /* Syntax error */` |
|      ! 0 | 1457 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1458 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1459 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1460 | `						 }` |
|      ! 0 | 1461 | `						 return rc;` |
|        - | 1462 | `				 }` |
|        - | 1463 | `				 /* Collect index tokens */` |
|   135966 | 1464 | `				 while( iArrTok < nToken ){` |
|   135966 | 1465 | `					 if( apNode[iArrTok] ){` |
|   135934 | 1466 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1467 | `							 /* Increment nesting level */` |
|      ! 0 | 1468 | `							 iNest++;` |
|   135934 | 1469 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1470 | `							 /* Decrement nesting level */` |
|    75304 | 1471 | `							 iNest--;` |
|    75304 | 1472 | `							 if( iNest <= 0 ){` |
|    75304 | 1473 | `								 break;` |
|        - | 1474 | `							 }` |
|      ! 0 | 1475 | `						 }` |
|    30315 | 1476 | `					 }` |
|    60664 | 1477 | `					 ++iArrTok;` |
|        2 | 1478 | `				 }` |
|    75304 | 1479 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1480 | `					 /* Recurse and process this expression */` |
|    60554 | 1481 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    60554 | 1482 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1483 | `						 return rc;` |
|        - | 1484 | `					 }` |
|        - | 1485 | `					 /* Link the node to it's index */` |
|    60554 | 1486 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    30276 | 1487 | `				 }` |
|        - | 1488 | `				 /* Link the node to the tree */` |
|    75304 | 1489 | `				 pNode->pLeft = apNode[iLeft];` |
|    75304 | 1490 | `				 pNode->pRight = 0;` |
|    75304 | 1491 | `				 apNode[iLeft] = 0;` |
|   211268 | 1492 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   135966 | 1493 | `					 apNode[iNest] = 0;` |
|    67984 | 1494 | `				 }` |
|    37653 | 1495 | `			 }else{` |
|        - | 1496 | `				 /* Member access operators [i.e: '->','::'] */` |
|   160396 | 1497 | `				  iRight = iCur + 1;` |
|   160396 | 1498 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1499 | `					 iRight++;` |
|      ! 0 | 1500 | `				 }` |
|   160396 | 1501 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1502 | `					 /* Syntax error */` |
|        5 | 1503 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1504 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1505 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1506 | `					 }` |
|        5 | 1507 | `					 return rc;` |
|        - | 1508 | `				 }` |
|        - | 1509 | `				 /* Link the node to the tree */` |
|   160392 | 1510 | `				 pNode->pLeft = apNode[iLeft];` |
|   240431 | 1511 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   160276 | 1512 | `					 && pNode->pLeft->pOp == 0 &&` |
|   160082 | 1513 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1514 | `						 /* Syntax error */` |
|      ! 0 | 1515 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1516 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1517 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1518 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1519 | `						 }` |
|      ! 0 | 1520 | `						 return rc;` |
|        - | 1521 | `				 }` |
|   160392 | 1522 | `				 pNode->pRight = apNode[iRight];` |
|   160392 | 1523 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1524 | `			 }` |
|   291913 | 1525 | `		 }` |
|  2209836 | 1526 | `		 iLeft = iCur;` |
|  1104919 | 1527 | `	 }` |
|        - | 1528 | `	 /* Handle left associative (new, clone) operators */` |
|  4225750 | 1529 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3569766 | 1530 | `		 if( apNode[iCur] == 0 ){` |
|  1959278 | 1531 | `			 continue;` |
|        - | 1532 | `		 }` |
|  1610490 | 1533 | `		 pNode = apNode[iCur];` |
|  1610490 | 1534 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1535 | `			 SyToken *pToken;` |
|        - | 1536 | `			 /* Get the left node */` |
|    15502 | 1537 | `			 iLeft = iCur + 1;` |
|    30970 | 1538 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    15470 | 1539 | `				 iLeft++;` |
|        2 | 1540 | `			 }` |
|    15502 | 1541 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1542 | `				  /* Syntax error */` |
|      ! 0 | 1543 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1544 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1545 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1546 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1547 | `				 }` |
|      ! 0 | 1548 | `				 return rc;` |
|        - | 1549 | `			 }` |
|        - | 1550 | `			 /* Make sure the operand are of a valid type */` |
|    15502 | 1551 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1552 | `				 /* Clone:` |
|        - | 1553 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1554 | `				  *  ++ function call (including annonymous)` |
|        - | 1555 | `				  *  ++ array member` |
|        - | 1556 | `				  *  ++ 'new' operator` |
|        - | 1557 | `				  * Example:` |
|        - | 1558 | `				  *   clone $pObj;` |
|        - | 1559 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1560 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1561 | `				  */` |
|       20 | 1562 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1563 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1564 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1565 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1566 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1567 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1568 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1569 | `						 }` |
|      ! 0 | 1570 | `						 return rc;` |
|        - | 1571 | `					 }` |
|        8 | 1572 | `				 }` |
|       11 | 1573 | `			 }else{` |
|        - | 1574 | `				 /* New */` |
|    15484 | 1575 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1576 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1577 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1578 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1579 | `						 /* Syntax error */` |
|      ! 0 | 1580 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1581 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1582 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1583 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1584 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1585 | `						 }` |
|      ! 0 | 1586 | `						 return rc;` |
|        - | 1587 | `					 }` |
|        8 | 1588 | `				 }` |
|        - | 1589 | `			 }` |
|        - | 1590 | `			  /* Link the node to the tree */` |
|    15502 | 1591 | `			 pNode->pLeft = apNode[iLeft];` |
|    15502 | 1592 | `			 apNode[iLeft] = 0;` |
|    15502 | 1593 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7750 | 1594 | `		 }` |
|   805246 | 1595 | `	 }` |
|        - | 1596 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   655986 | 1597 | `	 iLeft = -1;` |
|  4228922 | 1598 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3569766 | 1599 | `		 if( apNode[iCur] == 0 ){` |
|  1959278 | 1600 | `			 continue;` |
|        - | 1601 | `		 }` |
|  1610490 | 1602 | `		 pNode = apNode[iCur];` |
|  1610490 | 1603 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8836 | 1604 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3190 | 1605 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1606 | `					 /* Link the node to the tree */` |
|     3192 | 1607 | `					 pNode->pLeft = apNode[iLeft];` |
|     3192 | 1608 | `					 apNode[iLeft] = 0;` |
|     1595 | 1609 | `			 }` |
|     6003 | 1610 | `		  }` |
|  1613662 | 1611 | `		 iLeft = iCur;` |
|   808418 | 1612 | `	  }` |
|   659158 | 1613 | `	 iLeft = -1;` |
|  4228922 | 1614 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3569766 | 1615 | `		 if( apNode[iCur] == 0 ){` |
|  1962468 | 1616 | `			 continue;` |
|        - | 1617 | `		 }` |
|  1607300 | 1618 | `		 pNode = apNode[iCur];` |
|  1607300 | 1619 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8817 | 1620 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8818 | 1621 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1622 | `					 /* Syntax error */` |
|      ! 0 | 1623 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1624 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1625 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1626 | `					 }` |
|      ! 0 | 1627 | `					 return rc;` |
|        - | 1628 | `			 }` |
|        - | 1629 | `			 /* Link the node to the tree */` |
|     8818 | 1630 | `			 pNode->pLeft = apNode[iLeft];` |
|     8818 | 1631 | `			 apNode[iLeft] = 0;` |
|        - | 1632 | `			 /* Mark as pre-increment/decrement node */` |
|     8818 | 1633 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4408 | 1634 | `		  }` |
|  1607300 | 1635 | `		 iLeft = iCur;` |
|   803651 | 1636 | `	 }` |
|        - | 1637 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   659158 | 1638 | `	  iLeft = 0;` |
|  4228916 | 1639 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3569762 | 1640 | `		  if( apNode[iCur] ){` |
|  1598480 | 1641 | `			  pNode = apNode[iCur];` |
|  1598480 | 1642 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    39496 | 1643 | `				  if( iLeft > 0 ){` |
|        - | 1644 | `					  /* Link the node to the tree */` |
|    39494 | 1645 | `					  pNode->pLeft = apNode[iLeft];` |
|    39494 | 1646 | `					  apNode[iLeft] = 0;` |
|    39494 | 1647 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1648 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1649 | `							   /* Syntax error */` |
|      ! 0 | 1650 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1651 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1652 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1653 | `							  }` |
|      ! 0 | 1654 | `							  return rc;` |
|        - | 1655 | `						  }` |
|       36 | 1656 | `					  }` |
|    19748 | 1657 | `				  }else{` |
|        - | 1658 | `					  /* Syntax error */` |
|        3 | 1659 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1660 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1661 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1662 | `					  }` |
|        3 | 1663 | `					  return rc;` |
|        - | 1664 | `				  }` |
|    19746 | 1665 | `			  }` |
|        - | 1666 | `			  /* Save terminal position */` |
|  1598478 | 1667 | `			  iLeft = iCur;` |
|   799238 | 1668 | `		  }` |
|  1784881 | 1669 | `	  }` |
|        - | 1670 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1671 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1672 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1673 | `	  * yielding a right-leaning tree. */` |
|  4228914 | 1674 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  3569760 | 1675 | `		 if( apNode[iCur] == 0 ){` |
|  2010880 | 1676 | `			 continue;` |
|        - | 1677 | `		 }` |
|  1558882 | 1678 | `		 pNode = apNode[iCur];` |
|  1558882 | 1679 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1680 | `			 sxi32 iL, iR;` |
|        - | 1681 | `			 /* Find the right operand */` |
|      105 | 1682 | `			 iR = -1;` |
|        - | 1683 | `			 {` |
|        - | 1684 | `				 sxi32 j;` |
|      117 | 1685 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      117 | 1686 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1687 | `				 }` |
|        - | 1688 | `			 }` |
|        - | 1689 | `			 /* Find the left operand */` |
|      105 | 1690 | `			 iL = -1;` |
|        - | 1691 | `			 {` |
|        - | 1692 | `				 sxi32 j;` |
|      157 | 1693 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      157 | 1694 | `					 if( apNode[j] ){ iL = j; break; }` |
|       27 | 1695 | `				 }` |
|        - | 1696 | `			 }` |
|      105 | 1697 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1698 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1699 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1700 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1701 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1702 | `				 }` |
|      ! 0 | 1703 | `				 return rc;` |
|        - | 1704 | `			 }` |
|      105 | 1705 | `			 pNode->pLeft  = apNode[iL];` |
|      105 | 1706 | `			 pNode->pRight = apNode[iR];` |
|      105 | 1707 | `			 apNode[iL] = 0;` |
|      105 | 1708 | `			 apNode[iR] = 0;` |
|        - | 1709 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast).` |
|        - | 1710 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1711 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1712 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1713 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1714 | `			  * — the outer unary must remain on top. Stop at parentheses or` |
|        - | 1715 | `			  * at the error-suppression operator '@', which PHP leaves` |
|        - | 1716 | `			  * wrapping the whole expression. */` |
|      118 | 1717 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       67 | 1718 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       29 | 1719 | `				 && pNode->pLeft->pOp->iOp != EXPR_OP_ALT` |
|       28 | 1720 | `				 && pNode->pLeft->pLeft != 0` |
|       29 | 1721 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       21 | 1722 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       21 | 1723 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1724 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1725 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       32 | 1726 | `				 while( pTail->pLeft` |
|       24 | 1727 | `					 && pTail->pLeft->pOp` |
|       15 | 1728 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        6 | 1729 | `					 && pTail->pLeft->pOp->iOp != EXPR_OP_ALT` |
|        6 | 1730 | `					 && pTail->pLeft->pLeft != 0` |
|       19 | 1731 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        5 | 1732 | `					 pTail = pTail->pLeft;` |
|        1 | 1733 | `				 }` |
|        - | 1734 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       21 | 1735 | `				 pNode->pLeft = pTail->pLeft;` |
|       21 | 1736 | `				 pTail->pLeft = pNode;` |
|       21 | 1737 | `				 apNode[iCur] = pHead;` |
|       10 | 1738 | `			 }` |
|       52 | 1739 | `		 }` |
|   779442 | 1740 | `	 }` |
|        - | 1741 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  7250610 | 1742 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  6591466 | 1743 | `		 iLeft = -1;` |
| 42288752 | 1744 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 35697298 | 1745 | `			 if( apNode[iCur] == 0 ){` |
| 22777360 | 1746 | `				 continue;` |
|        - | 1747 | `			 }` |
| 12919940 | 1748 | `			 pNode = apNode[iCur];` |
| 12919940 | 1749 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1750 | `				 /* Get the right node */` |
|   199358 | 1751 | `				 iRight = iCur + 1;` |
|   284532 | 1752 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    85176 | 1753 | `					 iRight++;` |
|        2 | 1754 | `				 }` |
|   199358 | 1755 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1756 | `					 /* Syntax error */` |
|        9 | 1757 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1758 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1759 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1760 | `					 }` |
|        9 | 1761 | `					 return rc;` |
|        - | 1762 | `				 }` |
|   199350 | 1763 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1764 | `					 sxi32  iTmp;` |
|        - | 1765 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1766 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1767 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1768 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1769 | `					  * is swapped below. */` |
|       50 | 1770 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1771 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1772 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1773 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1774 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1775 | `						 }` |
|        3 | 1776 | `						 return rc;` |
|        - | 1777 | `					 }` |
|       48 | 1778 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1779 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1780 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1781 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1782 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1783 | `						 }` |
|      ! 0 | 1784 | `						 return rc;` |
|        - | 1785 | `					 }` |
|       48 | 1786 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1787 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1788 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1789 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1790 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1791 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1792 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1793 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1794 | `									 }` |
|      ! 0 | 1795 | `									 return rc;` |
|        - | 1796 | `							 }` |
|      ! 0 | 1797 | `						 }` |
|       16 | 1798 | `					 }` |
|        - | 1799 | `					 /* Swap operands */` |
|       48 | 1800 | `					 iTmp = iRight;` |
|       48 | 1801 | `					 iRight = iLeft;` |
|       48 | 1802 | `					 iLeft = iTmp;` |
|       23 | 1803 | `				 }` |
|        - | 1804 | `				 /* Link the node to the tree */` |
|   199348 | 1805 | `				 pNode->pLeft = apNode[iLeft];` |
|   199348 | 1806 | `				 pNode->pRight = apNode[iRight];` |
|   199348 | 1807 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    99673 | 1808 | `			 }` |
| 12919930 | 1809 | `			 iLeft = iCur;` |
|  6459966 | 1810 | `		 }` |
|  3295729 | 1811 | `	 }` |
|        - | 1812 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1813 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1814 | `	  * we are dealing with a single operator.` |
|        - | 1815 | `	  */` |
|   659146 | 1816 | `	  iLeft = -1;` |
|  4219836 | 1817 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3562784 | 1818 | `		  if( apNode[iCur] == 0 ){` |
|  2408960 | 1819 | `			  continue;` |
|        - | 1820 | `		  }` |
|  1153826 | 1821 | `		  pNode = apNode[iCur];` |
|  1153826 | 1822 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2094 | 1823 | `			  sxi32 iNest = 1;` |
|     2094 | 1824 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1825 | `				  /* Missing condition */` |
|        3 | 1826 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1827 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1828 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1829 | `				  }` |
|        3 | 1830 | `				  return rc;` |
|        - | 1831 | `			  }` |
|        - | 1832 | `			  /* Get the right node */` |
|     2092 | 1833 | `			  iRight = iCur + 1;` |
|     4426 | 1834 | `			  while( iRight < nToken  ){` |
|     4426 | 1835 | `				  if( apNode[iRight] ){` |
|     4114 | 1836 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1837 | `						  /* Increment nesting level */` |
|      ! 0 | 1838 | `						  ++iNest;` |
|     4114 | 1839 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1840 | `						  /* Decrement nesting level */` |
|     2092 | 1841 | `						  --iNest;` |
|     2092 | 1842 | `						  if( iNest <= 0 ){` |
|     2092 | 1843 | `							  break;` |
|        - | 1844 | `						  }` |
|      ! 0 | 1845 | `					  }` |
|     1011 | 1846 | `				  }` |
|     2336 | 1847 | `				  iRight++;` |
|        2 | 1848 | `			  }` |
|     2092 | 1849 | `			  if( iRight > iCur + 1 ){` |
|        - | 1850 | `				  /* Recurse and process the then expression */` |
|     2024 | 1851 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2024 | 1852 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1853 | `					  return rc;` |
|        - | 1854 | `				  }` |
|        - | 1855 | `				  /* Link the node to the tree */` |
|     2024 | 1856 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1011 | 1857 | `			  }else{` |
|        - | 1858 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1859 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1860 | `			  }` |
|     2092 | 1861 | `			  apNode[iCur + 1] = 0;` |
|     2092 | 1862 | `			  if( iRight + 1 < nToken ){` |
|        - | 1863 | `				  /* Recurse and process the else expression */` |
|     2092 | 1864 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2092 | 1865 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1866 | `					  return rc;` |
|        - | 1867 | `				  }` |
|        - | 1868 | `				  /* Link the node to the tree */` |
|     2092 | 1869 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2092 | 1870 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1047 | 1871 | `			  }else{` |
|      ! 0 | 1872 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1873 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1874 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1875 | `				 }` |
|      ! 0 | 1876 | `				 return rc;` |
|        - | 1877 | `			  }` |
|        - | 1878 | `			  /* Point to the condition */` |
|     2092 | 1879 | `			  pNode->pCond  = apNode[iLeft];` |
|     2092 | 1880 | `			  apNode[iLeft] = 0;` |
|     2092 | 1881 | `			  break;` |
|        - | 1882 | `		  }` |
|  1151734 | 1883 | `		  iLeft = iCur;` |
|   575868 | 1884 | `	  }` |
|        - | 1885 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1886 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1887 | `	  * so there is no need for a precedence loop here.` |
|        - | 1888 | `	  */` |
|   659144 | 1889 | `	 iRight = -1;` |
|  4228718 | 1890 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3569630 | 1891 | `		 if( apNode[iCur] == 0 ){` |
|  2664134 | 1892 | `			 continue;` |
|        - | 1893 | `		 }` |
|   905498 | 1894 | `		 pNode = apNode[iCur];` |
|   905498 | 1895 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1896 | `			 /* Get the left node */` |
|   246234 | 1897 | `			 iLeft = iCur - 1;` |
|   358862 | 1898 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   112630 | 1899 | `				 iLeft--;` |
|        2 | 1900 | `			 }` |
|   246234 | 1901 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1902 | `				 /* Syntax error */` |
|       43 | 1903 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1904 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1905 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1906 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1907 | `				 }else{` |
|       39 | 1908 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1909 | `				 }` |
|       43 | 1910 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1911 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1912 | `				 }` |
|       43 | 1913 | `				 return rc;` |
|        - | 1914 | `			 }` |
|        - | 1915 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1916 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1917 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1918 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1919 | `			  * a write. */` |
|   246192 | 1920 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        9 | 1921 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1922 | `					 "Can't use nullsafe operator in write context");` |
|        9 | 1923 | `				 if( rc != SXERR_ABORT ){` |
|        9 | 1924 | `					 rc = SXERR_SYNTAX;` |
|        4 | 1925 | `				 }` |
|        9 | 1926 | `				 return rc;` |
|        - | 1927 | `			 }` |
|   246184 | 1928 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1929 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1930 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1931 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1932 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1933 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1934 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1935 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1936 | `					 }else{` |
|        4 | 1937 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1938 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1939 | `					 }` |
|        5 | 1940 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1941 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1942 | `					 }` |
|        5 | 1943 | `					 return rc;` |
|        - | 1944 | `				 }` |
|       26 | 1945 | `			 }` |
|        - | 1946 | `			 /* Link the node to the tree (Reverse) */` |
|   246180 | 1947 | `			 pNode->pLeft = apNode[iRight];` |
|   246180 | 1948 | `			 pNode->pRight = apNode[iLeft];` |
|   246180 | 1949 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   123089 | 1950 | `		 }` |
|   905444 | 1951 | `		 iRight = iCur;` |
|   452723 | 1952 | `	 }` |
|        - | 1953 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3295442 | 1954 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2636354 | 1955 | `		 iLeft = -1;` |
| 16914594 | 1956 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 14278242 | 1957 | `			 if( apNode[iCur] == 0 ){` |
| 11641484 | 1958 | `				 continue;` |
|        - | 1959 | `			 }` |
|  2636760 | 1960 | `			 pNode = apNode[iCur];` |
|  2636760 | 1961 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1962 | `				 /* Get the right node */` |
|       72 | 1963 | `				 iRight = iCur + 1;` |
|      110 | 1964 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1965 | `					 iRight++;` |
|        2 | 1966 | `				 }` |
|       72 | 1967 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1968 | `					 /* Syntax error */` |
|      ! 0 | 1969 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1970 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1971 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1972 | `					 }` |
|      ! 0 | 1973 | `					 return rc;` |
|        - | 1974 | `				 }` |
|        - | 1975 | `				 /* Link the node to the tree */` |
|       72 | 1976 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1977 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1978 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1979 | `			 }` |
|  2636760 | 1980 | `			 iLeft = iCur;` |
|  1318381 | 1981 | `		 }` |
|  1318178 | 1982 | `	 }` |
|        - | 1983 | `	 /* Point to the root of the expression tree */` |
|  3569534 | 1984 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2910464 | 1985 | `		 if( apNode[iCur] ){` |
|   599490 | 1986 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1987 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1988 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1989 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1990 | `				  }` |
|       20 | 1991 | `				  return rc;` |
|        - | 1992 | `			 }` |
|   599472 | 1993 | `			 apNode[0] = apNode[iCur];` |
|   599472 | 1994 | `			 apNode[iCur] = 0;` |
|   299735 | 1995 | `		 }` |
|  1455224 | 1996 | `	 }` |
|   659072 | 1997 | `	 return SXRET_OK;` |
|   603199 | 1998 | ` }` |
|        - | 1999 | ` /*` |
|        - | 2000 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2001 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2002 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2003 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2004 | `  */` |
|   775810 | 2005 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 2006 |  |
|        - | 2007 | `	ph7_expr_node **apNode;` |
|        - | 2008 | `	ph7_expr_node *pNode;` |
|        - | 2009 | `	sxi32 rc;` |
|        - | 2010 | `	/* Reset node container */` |
|   775812 | 2011 | `	SySetReset(pExprNode);` |
|   775812 | 2012 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2013 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2014 | `	{` |
|   775812 | 2015 | `		int iLastWasTerm = 0;` |
|  4177586 | 2016 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3401810 | 2017 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3401810 | 2018 | `			if( rc != SXRET_OK ){` |
|       35 | 2019 | `				return rc;` |
|        - | 2020 | `			}` |
|        - | 2021 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3401776 | 2022 | `			if( pNode->xCode ){` |
|        - | 2023 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1787954 | 2024 | `				iLastWasTerm = 1;` |
|  2507800 | 2025 | `			}else if( pNode->pOp ){` |
|        - | 2026 | `				/* Operator node */` |
|   790140 | 2027 | `				iLastWasTerm = 0;` |
|   395071 | 2028 | `			}else{` |
|        - | 2029 | `				/* Delimiter: ')' and ']' end terms */` |
|   823686 | 2030 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2031 | `			}` |
|        - | 2032 | `			/* Save the extracted node */` |
|  3401776 | 2033 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 2034 | `		}` |
|        - | 2035 | `	}` |
|   775778 | 2036 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2037 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2038 | `		*ppRoot = 0;` |
|      ! 0 | 2039 | `		return SXRET_OK;` |
|        - | 2040 | `	}` |
|   775778 | 2041 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2042 | `	/* Make sure we are dealing with valid nodes */` |
|   775778 | 2043 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   775778 | 2044 | `	if( rc != SXRET_OK ){` |
|        - | 2045 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2046 | `		 * cleanup the mess left behind.` |
|        - | 2047 | `		 */` |
|       51 | 2048 | `		*ppRoot = 0;` |
|       51 | 2049 | `		return rc;` |
|        - | 2050 | `	}` |
|        - | 2051 | `	/* Build the tree */` |
|   775728 | 2052 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   775728 | 2053 | `	if( rc != SXRET_OK ){` |
|        - | 2054 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      100 | 2055 | `		*ppRoot = 0;` |
|      100 | 2056 | `		return rc;` |
|        - | 2057 | `	}` |
|        - | 2058 | `	/* Point to the root of the tree */` |
|   775630 | 2059 | `	*ppRoot = apNode[0];` |
|   775630 | 2060 | `	return SXRET_OK;` |
|   387907 | 2061 |  |
|        - | 2062 |  |
