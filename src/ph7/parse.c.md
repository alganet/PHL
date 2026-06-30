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
|  1124912 |  268 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  269 |  |
|  1124917 |  270 | `	sxu32 n = 0;` |
|        - |  271 | `	sxi32 rc;` |
|        - |  272 | `	/* Do a linear lookup on the operators table */` |
| 19896915 |  273 | `	for(;;){` |
| 39793835 |  274 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  275 | `			break;` |
|        - |  276 | `		}` |
| 39793835 |  277 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  278 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  4640081 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  2320043 |  280 | `		}else{` |
| 35153759 |  281 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  282 | `		}` |
| 39793835 |  283 | `		if( rc == 0 ){` |
|  1129197 |  284 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  285 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1124521 |  286 | `				return &aOpTable[n];` |
|        - |  287 | `			}` |
|        - |  288 | `			/* Handle ambiguity */` |
|     4681 |  289 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  290 | `				/* Unary opertors have prcedence here over binary operators */` |
|      269 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|     4417 |  293 | `			if( pLast->nType & PH7_TK_OP ){` |
|      143 |  294 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  295 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      143 |  296 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  297 | `					/* Unary opertors have prcedence here over binary operators */` |
|      135 |  298 | `					return &aOpTable[n];` |
|        - |  299 | `				}` |
|        - |  300 |  |
|        4 |  301 | `			}` |
|     2140 |  302 | `		}` |
| 38668923 |  303 | `		++n; /* Next operator in the table */` |
|        5 |  304 | `	}` |
|        - |  305 | `	/* No such operator */` |
|      ! 0 |  306 | `	return 0;` |
|   562461 |  307 |  |
|        - |  308 | `/*` |
|        - |  309 | ` * Delimit a set of token stream.` |
|        - |  310 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  311 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  312 | ` */` |
|   685310 |  313 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  314 |  |
|   685315 |  315 | `	SyToken *pCur = pIn;` |
|   685315 |  316 | `	sxi32 iNest = 1;` |
|  3839462 |  317 | `	for(;;){` |
|  7678929 |  318 | `		if( pCur >= pEnd ){` |
|      313 |  319 | `			break;` |
|        - |  320 | `		}` |
|  7678621 |  321 | `		if( pCur->nType & nTokStart ){` |
|        - |  322 | `			/* Increment nesting level */` |
|   362575 |  323 | `			iNest++;` |
|  7497336 |  324 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  325 | `			/* Decrement nesting level */` |
|  1047577 |  326 | `			iNest--;` |
|  1047577 |  327 | `			if( iNest <= 0 ){` |
|   685007 |  328 | `				break;` |
|        - |  329 | `			}` |
|   181285 |  330 | `		}` |
|        - |  331 | `		/* Advance cursor */` |
|  6993619 |  332 | `		pCur++;` |
|        5 |  333 | `	}` |
|        - |  334 | `	/* Point to the end of the chunk */` |
|   685315 |  335 | `	*ppEnd = pCur;` |
|   685315 |  336 |  |
|        - |  337 | `/*` |
|        - |  338 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  339 | ` * Note on reserved keywords.` |
|        - |  340 | ` *  According to the PHP language reference manual:` |
|        - |  341 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  342 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  343 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  344 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  345 | ` */` |
|    22206 |  346 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  347 |  |
|    33235 |  348 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    22110 |  349 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  350 | `		){` |
|      165 |  351 | `			return TRUE;` |
|        - |  352 | `	}` |
|    22051 |  353 | `	if( bCheckFunc ){` |
|      301 |  354 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      207 |  355 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      191 |  356 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       45 |  357 | `				return TRUE;` |
|        - |  358 | `		}` |
|       87 |  359 | `	}` |
|        - |  360 | `	/* Not a language construct */` |
|    22011 |  361 | `	return FALSE;` |
|    11108 |  362 |  |
|        - |  363 | `/*` |
|        - |  364 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  365 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  366 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  367 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  368 | ` */` |
|   949680 |  369 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  370 |  |
|        - |  371 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  372 | `	sxi32 i,rc;` |
|        - |  373 |  |
|   949685 |  374 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  375 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  376 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  377 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  378 | `	}` |
|   949685 |  379 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5146307 |  380 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4196661 |  381 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  382 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1063 |  383 | `			continue;` |
|        - |  384 | `		}` |
|  4195603 |  385 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   476413 |  386 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    22470 |  387 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  388 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   446215 |  389 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  390 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  391 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  392 | `						 */` |
|   446215 |  393 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   446215 |  394 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   446215 |  395 | `						apNode[i]->pOp = &sFCallOp;` |
|   223105 |  396 | `					}` |
|   223105 |  397 | `			}` |
|   476413 |  398 | `			iParen++;` |
|  3957399 |  399 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   476413 |  400 | `			if( iParen <= 0 ){` |
|       16 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       16 |  402 | `				if( rc != SXERR_ABORT ){` |
|       16 |  403 | `					rc = SXERR_SYNTAX;` |
|        6 |  404 | `				}` |
|       16 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|   476401 |  407 | `			iParen--;` |
|  3480985 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    91207 |  409 | `			iSquare++;` |
|  3197186 |  410 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    91221 |  411 | `			if( iSquare <= 0 ){` |
|        9 |  412 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        9 |  413 | `				if( rc != SXERR_ABORT ){` |
|        9 |  414 | `					rc = SXERR_SYNTAX;` |
|        3 |  415 | `				}` |
|        9 |  416 | `				return rc;` |
|        - |  417 | `			}` |
|    91215 |  418 | `			iSquare--;` |
|  3105974 |  419 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       20 |  420 | `			iBraces++;` |
|       20 |  421 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|  3060360 |  466 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       24 |  467 | `			if( iBraces <= 0 ){` |
|       15 |  468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       15 |  469 | `				if( rc != SXERR_ABORT ){` |
|       15 |  470 | `					rc = SXERR_SYNTAX;` |
|        6 |  471 | `				}` |
|       15 |  472 | `				return rc;` |
|        - |  473 | `			}` |
|       10 |  474 | `			iBraces--;` |
|  3060335 |  475 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2887 |  476 | `			if( iQuesty > 0 ){` |
|     2669 |  477 | `				iQuesty--;` |
|     1555 |  478 | `			}else if( iParen <= 0 ){` |
|        - |  479 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  480 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  481 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  482 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  483 | `				if( rc != SXERR_ABORT ){` |
|        5 |  484 | `					rc = SXERR_SYNTAX;` |
|        2 |  485 | `				}` |
|        5 |  486 | `				return rc;` |
|        5 |  487 | `			}` |
|  3058888 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   857041 |  489 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   857041 |  490 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2671 |  491 | `				iQuesty++;` |
|   855708 |  492 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   428518 |  512 | `		}` |
|  2097787 |  513 | `	}` |
|   949651 |  514 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       19 |  515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       19 |  516 | `		if( rc != SXERR_ABORT ){` |
|       19 |  517 | `			rc = SXERR_SYNTAX;` |
|        8 |  518 | `		}` |
|       19 |  519 | `		return rc;` |
|        - |  520 | `	}` |
|   949635 |  521 | `	return SXRET_OK;` |
|   474845 |  522 |  |
|        - |  523 | `/*` |
|        - |  524 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  525 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  526 | ` */` |
|   785896 |  527 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  528 |  |
|   785901 |  529 | `	SyToken *pIn = *ppCur;` |
|        - |  530 | `	/* Jump the first literal seen */` |
|   785901 |  531 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   782317 |  532 | `		pIn++;` |
|   391156 |  533 | `	}` |
|   394764 |  534 | `	for(;;){` |
|   789533 |  535 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|     3637 |  536 | `			pIn++;` |
|     3637 |  537 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     3635 |  538 | `				pIn++;` |
|     1815 |  539 | `			}` |
|     1821 |  540 | `		}else{` |
|   392953 |  541 | `			break;` |
|        - |  542 | `		}` |
|        5 |  543 | `	}` |
|        - |  544 | `	/* Synchronize pointers */` |
|   785901 |  545 | `	*ppCur = pIn;` |
|   785901 |  546 |  |
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
|      318 |  580 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  581 |  |
|      323 |  582 | `	SyToken *pIn = *ppCur;` |
|        - |  583 | `	sxu32 nLine;` |
|        - |  584 | `	sxi32 rc;` |
|        - |  585 | `	/* Jump the 'function' keyword */` |
|      323 |  586 | `	nLine = pIn->nLine;` |
|      323 |  587 | `	pIn++;` |
|      323 |  588 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  589 | `		pIn++;` |
|        1 |  590 | `	}` |
|      323 |  591 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  592 | `		/* Syntax error */` |
|        6 |  593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  594 | `		if( rc != SXERR_ABORT ){` |
|        6 |  595 | `			rc = SXERR_SYNTAX;` |
|        2 |  596 | `		}` |
|        6 |  597 | `		goto Synchronize;` |
|        - |  598 | `	}` |
|      319 |  599 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      319 |  600 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      319 |  601 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  602 | `		/* Syntax error */` |
|        6 |  603 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  604 | `		if( rc != SXERR_ABORT ){` |
|        6 |  605 | `			rc = SXERR_SYNTAX;` |
|        2 |  606 | `		}` |
|        6 |  607 | `		goto Synchronize;` |
|        - |  608 | `	}` |
|      315 |  609 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  610 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      315 |  611 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      315 |  638 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       39 |  639 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  640 | `		/* Check if we are dealing with a closure */` |
|       39 |  641 | `		if( nKey == PH7_TKWRD_USE ){` |
|       31 |  642 | `			pIn++; /* Jump the 'use' keyword */` |
|       31 |  643 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  644 | `				/* Syntax error */` |
|        6 |  645 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  646 | `				if( rc != SXERR_ABORT ){` |
|        6 |  647 | `					rc = SXERR_SYNTAX;` |
|        2 |  648 | `				}` |
|        6 |  649 | `				goto Synchronize;` |
|        - |  650 | `			}` |
|       27 |  651 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       27 |  652 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       27 |  653 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  654 | `				/* Syntax error */` |
|        6 |  655 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  656 | `				if( rc != SXERR_ABORT ){` |
|        6 |  657 | `					rc = SXERR_SYNTAX;` |
|        2 |  658 | `				}` |
|        6 |  659 | `				goto Synchronize;` |
|        - |  660 | `			}` |
|       23 |  661 | `			pIn++;` |
|       14 |  662 | `		}else{` |
|        - |  663 | `			/* Syntax error */` |
|       11 |  664 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|       11 |  665 | `			if( rc != SXERR_ABORT ){` |
|       11 |  666 | `				rc = SXERR_SYNTAX;` |
|        4 |  667 | `			}` |
|       11 |  668 | `			goto Synchronize;` |
|        - |  669 | `		}` |
|        9 |  670 | `	}` |
|      299 |  671 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      299 |  672 | `		pIn++; /* Jump the leading curly '{' */` |
|      299 |  673 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      299 |  674 | `		if( pIn < pEnd ){` |
|      299 |  675 | `			pIn++;` |
|      147 |  676 | `		}` |
|      152 |  677 | `	}else{` |
|        - |  678 | `		/* Syntax error */` |
|      ! 0 |  679 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|        - |  683 | `	}` |
|      299 |  684 | `	rc = SXRET_OK;` |
|      159 |  685 | `Synchronize:` |
|        - |  686 | `	/* Synchronize pointers */` |
|      323 |  687 | `	*ppCur = pIn;` |
|      323 |  688 | `	return rc;` |
|      164 |  689 |  |
|        - |  690 | `/*` |
|        - |  691 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|        - |  692 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|        - |  693 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|        - |  694 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|        - |  695 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|        - |  696 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|        - |  697 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|        - |  698 | ` */` |
|       26 |  699 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        3 |  700 |  |
|       29 |  701 | `	SyToken *pIn = *ppCur;` |
|       29 |  702 | `	sxu32 nLine = pIn->nLine;` |
|        - |  703 | `	sxi32 rc;` |
|       29 |  704 | `	pIn++; /* Jump the 'class' keyword */` |
|        - |  705 | `	/* Optional constructor argument list */` |
|       29 |  706 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  707 | `		pIn++; /* Jump '(' */` |
|        7 |  708 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        7 |  709 | `		if( pIn < pEnd ){` |
|        7 |  710 | `			pIn++; /* Jump ')' */` |
|        3 |  711 | `		}` |
|        3 |  712 | `	}` |
|        - |  713 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|        - |  714 | `	 * (no braces appear between ')' and the class body). */` |
|       57 |  715 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       31 |  716 | `		pIn++;` |
|        3 |  717 | `	}` |
|       29 |  718 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|        - |  719 | `		/* Syntax error: missing class body */` |
|      ! 0 |  720 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  721 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|      ! 0 |  722 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  723 | `			rc = SXERR_SYNTAX;` |
|      ! 0 |  724 | `		}` |
|      ! 0 |  725 | `		*ppCur = pIn;` |
|      ! 0 |  726 | `		return rc;` |
|        - |  727 | `	}` |
|       29 |  728 | `	pIn++; /* Jump the leading '{' */` |
|       29 |  729 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       29 |  730 | `	if( pIn < pEnd ){` |
|       29 |  731 | `		pIn++; /* Jump the trailing '}' */` |
|       13 |  732 | `	}` |
|       29 |  733 | `	*ppCur = pIn;` |
|       29 |  734 | `	return SXRET_OK;` |
|       16 |  735 |  |
|        - |  736 | `/*` |
|        - |  737 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  738 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  739 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  740 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  741 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  742 | ` */` |
|      160 |  743 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  744 |  |
|      164 |  745 | `	SyToken *pIn = *ppCur;` |
|        - |  746 | `	sxu32 nLine;` |
|        - |  747 | `	sxi32 rc;` |
|        - |  748 | `	int iNest;` |
|      164 |  749 | `	nLine = pIn->nLine;` |
|        - |  750 | `	/* Optional 'static' prefix */` |
|      160 |  751 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      164 |  752 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  753 | `		pIn++;` |
|        1 |  754 | `	}` |
|        - |  755 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      160 |  756 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      164 |  757 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  758 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  759 | `		goto Synchronize;` |
|        - |  760 | `	}` |
|      164 |  761 | `	pIn++; /* Jump 'fn' */` |
|       80 |  762 | `	SXUNUSED(nLine);` |
|       80 |  763 | `	SXUNUSED(pGen);` |
|        - |  764 | `	/* Optional '&' for return-by-reference */` |
|      164 |  765 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  766 | `		pIn++;` |
|      ! 0 |  767 | `	}` |
|        - |  768 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  769 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  770 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  771 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      164 |  772 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      162 |  773 | `		pIn++; /* '(' */` |
|      162 |  774 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      162 |  775 | `		if( pIn < pEnd ){` |
|      159 |  776 | `			pIn++; /* ')' */` |
|       78 |  777 | `		}` |
|       79 |  778 | `	}` |
|        - |  779 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|      164 |  780 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      164 |  806 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      156 |  807 | `		pIn++;` |
|       77 |  808 | `	}` |
|        - |  809 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      164 |  810 | `	iNest = 0;` |
|      934 |  811 | `	while( pIn < pEnd ){` |
|      837 |  812 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  813 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       65 |  814 | `			break;` |
|        - |  815 | `		}` |
|      773 |  816 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       41 |  817 | `			iNest++;` |
|      753 |  818 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       41 |  819 | `			iNest--;` |
|       20 |  820 | `		}` |
|      773 |  821 | `		pIn++;` |
|        3 |  822 | `	}` |
|      164 |  823 | `	rc = SXRET_OK;` |
|       80 |  824 | `Synchronize:` |
|      164 |  825 | `	*ppCur = pIn;` |
|      164 |  826 | `	return rc;` |
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
|  4200436 |  877 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|        5 |  878 |  |
|        - |  879 | `	ph7_expr_node *pNode;` |
|        - |  880 | `	SyToken *pCur;` |
|        - |  881 | `	sxi32 rc;` |
|        - |  882 | `	/* Allocate a new node */` |
|  4200441 |  883 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4200441 |  884 | `	if( pNode == 0 ){` |
|        - |  885 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  886 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  887 | `		 */` |
|      ! 0 |  888 | `		return SXERR_MEM;` |
|        - |  889 | `	}` |
|        - |  890 | `	/* Zero the structure */` |
|  4200441 |  891 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4200441 |  892 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  893 | `	/* Point to the head of the token stream */` |
|  4200441 |  894 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  895 | `	/* Start collecting tokens */` |
|  4200441 |  896 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|     3699 |  897 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|        - |  898 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|        - |  899 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|        - |  900 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|        - |  901 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       65 |  902 | `			pNode->pEnd = pCur;` |
|       65 |  903 | `			pCur++;` |
|       65 |  904 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       65 |  905 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       65 |  906 | `			pGen->pIn = pCur;` |
|       65 |  907 | `			*ppNode = pNode;` |
|       65 |  908 | `			return SXRET_OK;` |
|        - |  909 | `		}` |
|        - |  910 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  911 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|     3635 |  912 | `		pCur++;` |
|     3635 |  913 | `		pGen->pIn = pCur;` |
|     3635 |  914 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|     3635 |  915 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|     3635 |  916 | `		if( rc == SXRET_OK && *ppNode ){` |
|     3635 |  917 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|     1815 |  918 | `		}` |
|     3635 |  919 | `		return rc;` |
|        - |  920 | `	}` |
|  4196747 |  921 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  922 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  923 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  924 | `		 */` |
|     1065 |  925 | `		pCur++; /* Skip the opening '[' */` |
|     1065 |  926 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1065 |  927 | `		if( pCur < pGen->pEnd ){` |
|     1065 |  928 | `			pCur++; /* Skip past the closing ']' */` |
|      535 |  929 | `		}else{` |
|      ! 0 |  930 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  931 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  932 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  933 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  934 | `			}` |
|      ! 0 |  935 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  936 | `			return rc;` |
|        - |  937 | `		}` |
|        - |  938 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  939 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  940 | `		 */` |
|     1140 |  941 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      154 |  942 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      154 |  943 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       54 |  944 | `				pNode->xCode = PH7_CompileShortList;` |
|       29 |  945 | `			}else{` |
|      101 |  946 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  947 | `			}` |
|       79 |  948 | `		}else{` |
|      915 |  949 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  950 | `		}` |
|  4196217 |  951 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  952 | `		/* Point to the instance that describe this operator */` |
|   948277 |  953 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  954 | `		/* Advance the stream cursor */` |
|   948277 |  955 | `		pCur++;` |
|  3721551 |  956 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  957 | `		/* Isolate variable */` |
|  2269437 |  958 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1134727 |  959 | `			pCur++; /* Variable variable */` |
|        5 |  960 | `		}` |
|  1134715 |  961 | `		if( pCur < pGen->pEnd ){` |
|  1134715 |  962 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  963 | `				/* Variable name */` |
|  1134687 |  964 | `				pCur++;` |
|   567374 |  965 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       24 |  966 | `				pCur++;` |
|        - |  967 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       24 |  968 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       24 |  969 | `				if( pCur < pGen->pEnd ){` |
|       19 |  970 | `					pCur++;` |
|       11 |  971 | `				}else{` |
|        5 |  972 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  973 | `					if( rc != SXERR_ABORT ){` |
|        5 |  974 | `						rc = SXERR_SYNTAX;` |
|        2 |  975 | `					}` |
|        5 |  976 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  977 | `					return rc;` |
|        - |  978 | `				}` |
|        8 |  979 | `			}` |
|   567353 |  980 | `		}` |
|  1134711 |  981 | `		pNode->xCode = PH7_CompileVariable;` |
|  2680058 |  982 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    53047 |  983 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    53047 |  984 | `		 if( bAfterMemberOp ){` |
|        - |  985 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|        - |  986 | `			  * method/property NAME, not a language construct — PHP allows any` |
|        - |  987 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|        - |  988 | `			  * as a plain literal like an ordinary identifier member name. */` |
|      155 |  989 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      155 |  990 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    52972 |  991 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  992 | `			 /* List/Array node */` |
|    30195 |  993 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  994 | `				 /* Assume a literal */` |
|      ! 0 |  995 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  996 | `				 pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  997 | `			 }else{` |
|    30195 |  998 | `				 pCur += 2;` |
|        - |  999 | `				 /* Collect array/list tokens */` |
|    30195 | 1000 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    30195 | 1001 | `				 if( pCur < pGen->pEnd ){` |
|    30193 | 1002 | `					 pCur++;` |
|    15099 | 1003 | `				 }else{` |
|        - | 1004 | `					 /* Syntax error */` |
|        4 | 1005 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 | 1006 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 | 1007 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1008 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1009 | `					 }` |
|        3 | 1010 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1011 | `					 return rc;` |
|        - | 1012 | `				 }` |
|    30193 | 1013 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    30193 | 1014 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       31 | 1015 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       31 | 1016 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - | 1017 | `						 /* Syntax error */` |
|        3 | 1018 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 | 1019 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1020 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1021 | `						 }` |
|        3 | 1022 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1023 | `						 return rc;` |
|        - | 1024 | `					 }` |
|       13 | 1025 | `				 }` |
|        5 | 1026 | `			 }` |
|    37800 | 1027 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1028 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      175 | 1029 | `			 pCur++; /* Skip 'yield' keyword */` |
|      175 | 1030 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1031 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1032 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      175 | 1033 | `			 pNode->xCode = PH7_CompileYield;` |
|    22622 | 1034 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1035 | `			 /* Annonymous function */` |
|      323 | 1036 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1037 | `				 /* Assume a literal */` |
|      ! 0 | 1038 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1039 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1040 | `			 }else{` |
|        - | 1041 | `				 /* Assemble annonymous functions body */` |
|      323 | 1042 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      323 | 1043 | `				 if( rc != SXRET_OK ){` |
|       28 | 1044 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1045 | `					 return rc;` |
|        - | 1046 | `				 }` |
|      299 | 1047 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1048 | `			  }` |
|    22370 | 1049 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       35 | 1050 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       22 | 1051 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       12 | 1052 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        9 | 1053 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1054 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1055 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1056 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1057 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       29 | 1058 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       29 | 1059 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1060 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1061 | `				 return rc;` |
|        - | 1062 | `			 }` |
|       29 | 1063 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    22205 | 1064 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    22114 | 1065 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       30 | 1066 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       15 | 1067 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1068 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      164 | 1069 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      164 | 1070 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1071 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1072 | `				 return rc;` |
|        - | 1073 | `			 }` |
|      164 | 1074 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    22113 | 1075 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1076 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1077 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1078 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1079 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1080 | `				 return rc;` |
|        - | 1081 | `			 }` |
|       75 | 1082 | `			 pNode->xCode = PH7_CompileMatch;` |
|    21998 | 1083 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1084 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1085 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1086 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1087 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1088 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1089 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1090 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1091 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    21945 | 1092 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1093 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       91 | 1094 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       91 | 1095 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       48 | 1096 | `		 }else{` |
|        - | 1097 | `			 /* Assume a literal */` |
|    21841 | 1098 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    21841 | 1099 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1100 | `		 }` |
|  2086170 | 1101 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1102 | `		 /* Constants,function name,namespace path,class name... */` |
|   763915 | 1103 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   763915 | 1104 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   381960 | 1105 | `	 }else{` |
|  1295753 | 1106 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1107 | `			 /* Point to the code generator routine */` |
|   248779 | 1108 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   248779 | 1109 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1110 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1111 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1112 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1113 | `				 }` |
|        3 | 1114 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1115 | `				 return rc;` |
|        - | 1116 | `			 }` |
|   124386 | 1117 | `		 }` |
|        - | 1118 | `		/* Advance the stream cursor */` |
|  1295751 | 1119 | `		pCur++;` |
|        - | 1120 | `	 }` |
|        - | 1121 | `	/* Point to the end of the token stream */` |
|  4196713 | 1122 | `	pNode->pEnd = pCur;` |
|        - | 1123 | `	/* Save the node for later processing */` |
|  4196713 | 1124 | `	*ppNode = pNode;` |
|        - | 1125 | `	/* Synchronize cursors */` |
|  4196713 | 1126 | `	pGen->pIn = pCur;` |
|  4196713 | 1127 | `	return SXRET_OK;` |
|  2100223 | 1128 |  |
|        - | 1129 | `/*` |
|        - | 1130 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1131 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1132 | ` * level is zero.` |
|        - | 1133 | ` */` |
|    94090 | 1134 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1135 |  |
|    94095 | 1136 | `	SyToken *pCur = pStart;` |
|    94095 | 1137 | `	sxi32 iNest = 0;` |
|    94095 | 1138 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1139 | `		/* Last expression */` |
|    49091 | 1140 | `		return SXERR_EOF;` |
|        - | 1141 | `	}` |
|   185089 | 1142 | `	while( pCur < pEnd ){` |
|   168857 | 1143 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    28777 | 1144 | `			break;` |
|        - | 1145 | `		}` |
|   140085 | 1146 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     9387 | 1147 | `			iNest++;` |
|   135394 | 1148 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     9389 | 1149 | `			iNest--;` |
|     4692 | 1150 | `		}` |
|   140085 | 1151 | `		pCur++;` |
|        5 | 1152 | `	}` |
|    45009 | 1153 | `	*ppNext = pCur;` |
|    45009 | 1154 | `	return SXRET_OK;` |
|    47050 | 1155 |  |
|        - | 1156 | `/*` |
|        - | 1157 | ` * Free an expression tree.` |
|        - | 1158 | ` */` |
|  3587208 | 1159 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1160 |  |
|  3587213 | 1161 | `	if( pNode->pLeft ){` |
|        - | 1162 | `		/* Release the left tree */` |
|  1324765 | 1163 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   662380 | 1164 | `	}` |
|  3587213 | 1165 | `	if( pNode->pRight ){` |
|        - | 1166 | `		/* Release the right tree */` |
|   716523 | 1167 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   358259 | 1168 | `	}` |
|  3587213 | 1169 | `	if( pNode->pCond ){` |
|        - | 1170 | `		/* Release the conditional tree used by the ternary operator */` |
|     2667 | 1171 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1331 | 1172 | `	}` |
|  3587213 | 1173 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1174 | `		ph7_expr_node **apArg;` |
|        - | 1175 | `		sxu32 n;` |
|        - | 1176 | `		/* Release node arguments */` |
|   462931 | 1177 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   995335 | 1178 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   532409 | 1179 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   266207 | 1180 | `		}` |
|   462931 | 1181 | `		SySetRelease(&pNode->aNodeArgs);` |
|   231463 | 1182 | `	}` |
|        - | 1183 | `	/* Finally,release this node */` |
|  3587213 | 1184 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3587213 | 1185 |  |
|        - | 1186 | `/*` |
|        - | 1187 | ` * Free an expression tree.` |
|        - | 1188 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1189 | ` */` |
|   949714 | 1190 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1191 |  |
|        - | 1192 | `	ph7_expr_node **apNode;` |
|        - | 1193 | `	sxu32 n;` |
|   949719 | 1194 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5146491 | 1195 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4196777 | 1196 | `		if( apNode[n] ){` |
|   950053 | 1197 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   475024 | 1198 | `		}` |
|  2098391 | 1199 | `	}` |
|   949719 | 1200 | `	return SXRET_OK;` |
|        5 | 1201 |  |
|        - | 1202 | `/*` |
|        - | 1203 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1204 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1205 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1206 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1207 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1208 | ` */` |
|  1298234 | 1209 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1210 |  |
|  1298239 | 1211 | `	if( pNode == 0 ){` |
|   801083 | 1212 | `		return 0;` |
|        - | 1213 | `	}` |
|   497161 | 1214 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1215 | `		return 1;` |
|        - | 1216 | `	}` |
|   497149 | 1217 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1218 | `		return 1;` |
|        - | 1219 | `	}` |
|   497145 | 1220 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1221 | `		return 1;` |
|        - | 1222 | `	}` |
|   497145 | 1223 | `	return 0;` |
|   649122 | 1224 |  |
|        - | 1225 | `/*` |
|        - | 1226 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1227 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1228 | ` */` |
|   297108 | 1229 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1230 |  |
|        - | 1231 | `	sxi32 iExprOp;` |
|   297113 | 1232 | `	if( pNode->pOp == 0 ){` |
|   182695 | 1233 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1234 | `	}` |
|   114423 | 1235 | `	iExprOp = pNode->pOp->iOp;` |
|   114423 | 1236 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    78501 | 1237 | `			return TRUE;` |
|        - | 1238 | `	}` |
|    35927 | 1239 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    35923 | 1240 | `		if( pNode->pLeft->pOp ) {` |
|       66 | 1241 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       31 | 1242 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1243 | `				return FALSE;` |
|        5 | 1244 | `			}` |
|    35890 | 1245 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1246 | `			return FALSE;` |
|        - | 1247 | `		}` |
|    35923 | 1248 | `		return TRUE;` |
|        - | 1249 | `	}` |
|        5 | 1250 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1251 | `		return TRUE;` |
|        - | 1252 | `	}` |
|        - | 1253 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1254 | `	return FALSE;` |
|   148559 | 1255 |  |
|        - | 1256 | `/* Forward declaration */` |
|        - | 1257 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1258 | `/* Macro to check if the given node is a terminal.` |
|        - | 1259 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1260 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1261 | ` * linked ternary/elvis node). */` |
|        - | 1262 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1263 | `/*` |
|        - | 1264 | ` * Buid an expression tree for each given function argument.` |
|        - | 1265 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1266 | ` */` |
|   389542 | 1267 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1268 |  |
|        - | 1269 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1270 | `	sxi32 rc;` |
|        - | 1271 | `	/* Process function arguments from left to right */` |
|   389547 | 1272 | `	iCur = 0;` |
|   424269 | 1273 | `	for(;;){` |
|   848543 | 1274 | `		if( iCur >= nToken ){` |
|        - | 1275 | `			/* No more arguments to process */` |
|   389521 | 1276 | `			break;` |
|        - | 1277 | `		}` |
|   459027 | 1278 | `		iNode = iCur;` |
|   459027 | 1279 | `		iNest = 0;` |
|  1134697 | 1280 | `		while( iCur < nToken ){` |
|   745179 | 1281 | `			if( apNode[iCur] ){` |
|   731003 | 1282 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    34757 | 1283 | `					break;` |
|   679951 | 1284 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   349355 | 1285 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    37070 | 1286 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1287 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1288 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1289 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1290 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1291 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1292 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    36919 | 1293 | `					iNest++;` |
|   643042 | 1294 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    36919 | 1295 | `					iNest--;` |
|    18457 | 1296 | `				}` |
|   330747 | 1297 | `			}` |
|   675675 | 1298 | `			iCur++;` |
|        5 | 1299 | `		}` |
|   459027 | 1300 | `		if( iCur > iNode ){` |
|   459021 | 1301 | `			SyString sArgName = {0, 0};` |
|        - | 1302 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1303 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1304 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   463061 | 1305 | `			if( (iCur - iNode) >= 2` |
|   253781 | 1306 | `				&& apNode[iNode]` |
|    48546 | 1307 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    28442 | 1308 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     8214 | 1309 | `				&& apNode[iNode+1]` |
|     8095 | 1310 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1311 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      219 | 1312 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      219 | 1313 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      219 | 1314 | `				apNode[iNode] = 0;` |
|      219 | 1315 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      219 | 1316 | `				apNode[iNode+1] = 0;` |
|      219 | 1317 | `				iNode += 2;` |
|        - | 1318 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1319 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      219 | 1320 | `				if( iNode >= iCur ){` |
|        4 | 1321 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1322 | `						pOp->pStart->nLine,` |
|        - | 1323 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1324 | `						&sArgName);` |
|        3 | 1325 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1326 | `						rc = SXERR_SYNTAX;` |
|        1 | 1327 | `					}` |
|        3 | 1328 | `					return rc;` |
|        - | 1329 | `				}` |
|      106 | 1330 | `			}` |
|   459014 | 1331 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1332 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1333 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1334 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1335 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1336 | `					apNode[iNode] = 0;` |
|      ! 0 | 1337 | `			}` |
|   459019 | 1338 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   459019 | 1339 | `			if( apNode[iNode] ){` |
|   459019 | 1340 | `				if( sArgName.nByte > 0 ){` |
|      216 | 1341 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      216 | 1342 | `					apNode[iNode]->sArgName = sArgName;` |
|      106 | 1343 | `				}` |
|        - | 1344 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   459019 | 1345 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   229512 | 1346 | `			}else{` |
|        - | 1347 | `				/* No expression before comma */` |
|      ! 0 | 1348 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1349 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1350 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1351 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1352 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1353 | `				}` |
|      ! 0 | 1354 | `				return rc;` |
|        - | 1355 | `			}` |
|   229512 | 1356 | `		}else{` |
|        - | 1357 | `			/* Comma with no preceding argument */` |
|        8 | 1358 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        8 | 1359 | `			if( rc != SXERR_ABORT ){` |
|        8 | 1360 | `				rc = SXERR_SYNTAX;` |
|        3 | 1361 | `			}` |
|        8 | 1362 | `			return rc;` |
|        - | 1363 | `		}` |
|        - | 1364 | `		/* Jump trailing comma */` |
|   459019 | 1365 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    69503 | 1366 | `			iCur++;` |
|    69503 | 1367 | `			if( iCur >= nToken ){` |
|        - | 1368 | `				/* Trailing comma after last argument */` |
|       19 | 1369 | `				break;` |
|        - | 1370 | `			}` |
|    34740 | 1371 | `		}` |
|        5 | 1372 | `	}` |
|   389539 | 1373 | `	return SXRET_OK;` |
|   194776 | 1374 |  |
|        - | 1375 | ` /*` |
|        - | 1376 | `  * Create an expression tree from an array of tokens.` |
|        - | 1377 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1378 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1379 | `  */` |
|  1521360 | 1380 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1381 | ` {` |
|        - | 1382 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1383 | `	 ph7_expr_node *pNode;` |
|        - | 1384 | `	 sxi32 iCur;` |
|        - | 1385 | `	 sxi32 rc;` |
|  1521365 | 1386 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1387 | `		 /* TICKET 1433-17: self evaluating node */` |
|   713059 | 1388 | `		 return SXRET_OK;` |
|        - | 1389 | `	 }` |
|        - | 1390 | `	 /* Process expressions enclosed in parenthesis first */` |
|  5016427 | 1391 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1392 | `		 sxi32 iNest;` |
|        - | 1393 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1394 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1395 | `		  */` |
|  4208123 | 1396 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4177935 | 1397 | `			 continue;` |
|        - | 1398 | `		 }` |
|    30193 | 1399 | `		 iNest = 1;` |
|    30193 | 1400 | `		 iLeft = iCur;` |
|        - | 1401 | `		 /* Find the closing parenthesis */` |
|    30193 | 1402 | `		 iCur++;` |
|   201431 | 1403 | `		 while( iCur < nToken ){` |
|   201431 | 1404 | `			 if( apNode[iCur] ){` |
|   201431 | 1405 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1406 | `					 /* Decrement nesting level */` |
|    52399 | 1407 | `					 iNest--;` |
|    52399 | 1408 | `					 if( iNest <= 0 ){` |
|    30193 | 1409 | `						 break;` |
|        5 | 1410 | `					 }` |
|   160140 | 1411 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1412 | `					 /* Increment nesting level */` |
|    22211 | 1413 | `					 iNest++;` |
|    11103 | 1414 | `				 }` |
|    85619 | 1415 | `			 }` |
|   171243 | 1416 | `			 iCur++;` |
|        5 | 1417 | `		 }` |
|    30193 | 1418 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1419 | `			 sxi32 j;` |
|        - | 1420 | `			 /* Recurse and process this expression */` |
|    30193 | 1421 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    30193 | 1422 | `			 if( rc != SXRET_OK ){` |
|        3 | 1423 | `				 return rc;` |
|        - | 1424 | `			 }` |
|        - | 1425 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1426 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1427 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    30191 | 1428 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    30191 | 1429 | `				 if( apNode[j] ){` |
|    30191 | 1430 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    30191 | 1431 | `					 break;` |
|        - | 1432 | `				 }` |
|      ! 0 | 1433 | `			 }` |
|    15093 | 1434 | `		 }` |
|        - | 1435 | `		 /* Free the left and right nodes */` |
|    30191 | 1436 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    30191 | 1437 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    30191 | 1438 | `		 apNode[iLeft] = 0;` |
|    30191 | 1439 | `		 apNode[iCur] = 0;` |
|    15098 | 1440 | `	 }` |
|        - | 1441 | `	  /* Process expressions enclosed in braces */` |
|  5210075 | 1442 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1443 | `		 sxi32 iNest;` |
|        - | 1444 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1445 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1446 | `		  */` |
|  4409523 | 1447 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4409515 | 1448 | `			 continue;` |
|        - | 1449 | `		 }` |
|       10 | 1450 | `		 iNest = 1;` |
|       10 | 1451 | `		 iLeft = iCur;` |
|        - | 1452 | `		 /* Find the closing parenthesis */` |
|       10 | 1453 | `		 iCur++;` |
|       16 | 1454 | `		 while( iCur < nToken ){` |
|       16 | 1455 | `			 if( apNode[iCur] ){` |
|       16 | 1456 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1457 | `					 /* Decrement nesting level */` |
|       10 | 1458 | `					 iNest--;` |
|       10 | 1459 | `					 if( iNest <= 0 ){` |
|       10 | 1460 | `						 break;` |
|      ! 0 | 1461 | `					 }` |
|        7 | 1462 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1463 | `					 /* Increment nesting level */` |
|      ! 0 | 1464 | `					 iNest++;` |
|      ! 0 | 1465 | `				 }` |
|        3 | 1466 | `			 }` |
|        7 | 1467 | `			 iCur++;` |
|        1 | 1468 | `		 }` |
|       10 | 1469 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1470 | `			 /* Recurse and process this expression */` |
|        7 | 1471 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        7 | 1472 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1473 | `				 return rc;` |
|        - | 1474 | `			 }` |
|        3 | 1475 | `		 }` |
|        - | 1476 | `		 /* Free the left and right nodes */` |
|       10 | 1477 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       10 | 1478 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       10 | 1479 | `		 apNode[iLeft] = 0;` |
|       10 | 1480 | `		 apNode[iCur] = 0;` |
|        6 | 1481 | `	 }` |
|        - | 1482 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   800557 | 1483 | `	 iLeft = -1;` |
|  5210053 | 1484 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4409513 | 1485 | `		 if( apNode[iCur] == 0 ){` |
|  1734995 | 1486 | `			 continue;` |
|        - | 1487 | `		 }` |
|  2674523 | 1488 | `		 pNode = apNode[iCur];` |
|  2674523 | 1489 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   714103 | 1490 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1491 | `				 /* Collect function arguments */` |
|   446211 | 1492 | `				 sxi32 iPtr = 0;` |
|   446211 | 1493 | `				 sxi32 nFuncTok = 0;` |
|  1637593 | 1494 | `				 while( nFuncTok + iCur < nToken ){` |
|  1637593 | 1495 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1623417 | 1496 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   464963 | 1497 | `							 iPtr++;` |
|  1390938 | 1498 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   464963 | 1499 | `							 iPtr--;` |
|   464963 | 1500 | `							 if( iPtr <= 0 ){` |
|   446211 | 1501 | `								 break;` |
|        - | 1502 | `							 }` |
|     9376 | 1503 | `						 }` |
|   588603 | 1504 | `					 }` |
|  1191387 | 1505 | `					 nFuncTok++;` |
|        5 | 1506 | `				 }` |
|   446211 | 1507 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1508 | `					 /* Syntax error */` |
|      ! 0 | 1509 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1510 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1511 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1512 | `					 }` |
|      ! 0 | 1513 | `					 return rc;` |
|        - | 1514 | `				 }` |
|   446211 | 1515 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1516 | `					 /* Syntax error */` |
|      ! 0 | 1517 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1518 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1519 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1520 | `					 }` |
|      ! 0 | 1521 | `					 return rc;` |
|        - | 1522 | `				 }` |
|   446211 | 1523 | `				 if( nFuncTok > 1 ){` |
|        - | 1524 | `					 /* Process function arguments */` |
|   389547 | 1525 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   389547 | 1526 | `					 if( rc != SXRET_OK ){` |
|       10 | 1527 | `						 return rc;` |
|        - | 1528 | `					 }` |
|   194767 | 1529 | `				 }` |
|        - | 1530 | `				 /* Link the node to the tree */` |
|   446203 | 1531 | `				 pNode->pLeft = apNode[iLeft];` |
|   446203 | 1532 | `				 apNode[iLeft] = 0;` |
|  1637561 | 1533 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1191363 | 1534 | `					 apNode[iCur+iPtr] = 0;` |
|   595684 | 1535 | `				 }` |
|   490996 | 1536 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1537 | `				 /* Subscripting */` |
|    91215 | 1538 | `				 sxi32 iArrTok = iCur + 1;` |
|    91215 | 1539 | `				 sxi32 iNest = 1;` |
|    91421 | 1540 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1541 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1542 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       13 | 1543 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    91210 | 1544 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1545 | `						 /* Syntax error */` |
|      ! 0 | 1546 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1547 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1548 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1549 | `						 }` |
|      ! 0 | 1550 | `						 return rc;` |
|        - | 1551 | `				 }` |
|        - | 1552 | `				 /* Collect index tokens */` |
|   164727 | 1553 | `				 while( iArrTok < nToken ){` |
|   164727 | 1554 | `					 if( apNode[iArrTok] ){` |
|   164695 | 1555 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1556 | `							 /* Increment nesting level */` |
|      ! 0 | 1557 | `							 iNest++;` |
|   164695 | 1558 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1559 | `							 /* Decrement nesting level */` |
|    91215 | 1560 | `							 iNest--;` |
|    91215 | 1561 | `							 if( iNest <= 0 ){` |
|    91215 | 1562 | `								 break;` |
|        - | 1563 | `							 }` |
|      ! 0 | 1564 | `						 }` |
|    36740 | 1565 | `					 }` |
|    73517 | 1566 | `					 ++iArrTok;` |
|        5 | 1567 | `				 }` |
|    91215 | 1568 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1569 | `					 /* Recurse and process this expression */` |
|    73395 | 1570 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    73395 | 1571 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1572 | `						 return rc;` |
|        - | 1573 | `					 }` |
|        - | 1574 | `					 /* Link the node to it's index */` |
|    73395 | 1575 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    36695 | 1576 | `				 }` |
|        - | 1577 | `				 /* Link the node to the tree */` |
|    91215 | 1578 | `				 pNode->pLeft = apNode[iLeft];` |
|    91215 | 1579 | `				 pNode->pRight = 0;` |
|    91215 | 1580 | `				 apNode[iLeft] = 0;` |
|   255937 | 1581 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   164727 | 1582 | `					 apNode[iNest] = 0;` |
|    82366 | 1583 | `				 }` |
|    45610 | 1584 | `			 }else{` |
|        - | 1585 | `				 /* Member access operators [i.e: '->','::'] */` |
|   176687 | 1586 | `				  iRight = iCur + 1;` |
|   176693 | 1587 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        7 | 1588 | `					 iRight++;` |
|        1 | 1589 | `				 }` |
|   176687 | 1590 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1591 | `					 /* Syntax error */` |
|        5 | 1592 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1593 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1594 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1595 | `					 }` |
|        5 | 1596 | `					 return rc;` |
|        - | 1597 | `				 }` |
|        - | 1598 | `				 /* Link the node to the tree */` |
|   176683 | 1599 | `				 pNode->pLeft = apNode[iLeft];` |
|   264804 | 1600 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   176524 | 1601 | `					 && pNode->pLeft->pOp == 0 &&` |
|   176252 | 1602 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1603 | `						 /* Syntax error */` |
|      ! 0 | 1604 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1605 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1606 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1607 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1608 | `						 }` |
|      ! 0 | 1609 | `						 return rc;` |
|        - | 1610 | `				 }` |
|   176683 | 1611 | `				 pNode->pRight = apNode[iRight];` |
|   176683 | 1612 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1613 | `			 }` |
|   357043 | 1614 | `		 }` |
|  2674511 | 1615 | `		 iLeft = iCur;` |
|  1337258 | 1616 | `	 }` |
|        - | 1617 | `	 /* Handle left associative (new, clone) operators */` |
|  5210021 | 1618 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4409481 | 1619 | `		 if( apNode[iCur] == 0 ){` |
|  2471943 | 1620 | `			 continue;` |
|        - | 1621 | `		 }` |
|  1937543 | 1622 | `		 pNode = apNode[iCur];` |
|  1937543 | 1623 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1624 | `			 SyToken *pToken;` |
|        - | 1625 | `			 /* Get the left node */` |
|    22867 | 1626 | `			 iLeft = iCur + 1;` |
|    45479 | 1627 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    22617 | 1628 | `				 iLeft++;` |
|        5 | 1629 | `			 }` |
|    22867 | 1630 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1631 | `				  /* Syntax error */` |
|      ! 0 | 1632 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1633 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1634 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1635 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1636 | `				 }` |
|      ! 0 | 1637 | `				 return rc;` |
|        - | 1638 | `			 }` |
|        - | 1639 | `			 /* Make sure the operand are of a valid type */` |
|    22867 | 1640 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1641 | `				 /* Clone:` |
|        - | 1642 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1643 | `				  *  ++ function call (including annonymous)` |
|        - | 1644 | `				  *  ++ array member` |
|        - | 1645 | `				  *  ++ 'new' operator` |
|        - | 1646 | `				  * Example:` |
|        - | 1647 | `				  *   clone $pObj;` |
|        - | 1648 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1649 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1650 | `				  */` |
|       38 | 1651 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       36 | 1652 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1653 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1654 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1655 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1656 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1657 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1658 | `						 }` |
|      ! 0 | 1659 | `						 return rc;` |
|        - | 1660 | `					 }` |
|       16 | 1661 | `				 }` |
|       21 | 1662 | `			 }else{` |
|        - | 1663 | `				 /* New */` |
|    22833 | 1664 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      223 | 1665 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      218 | 1666 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       31 | 1667 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1668 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1669 | `						 /* Syntax error */` |
|      ! 0 | 1670 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1671 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1672 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1673 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1674 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1675 | `						 }` |
|      ! 0 | 1676 | `						 return rc;` |
|        - | 1677 | `					 }` |
|      109 | 1678 | `				 }` |
|        - | 1679 | `			 }` |
|        - | 1680 | `			  /* Link the node to the tree */` |
|    22867 | 1681 | `			 pNode->pLeft = apNode[iLeft];` |
|    22867 | 1682 | `			 apNode[iLeft] = 0;` |
|    22867 | 1683 | `			 pNode->pRight = 0; /* Paranoid */` |
|    11431 | 1684 | `		 }` |
|   968774 | 1685 | `	 }` |
|        - | 1686 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   800545 | 1687 | `	 iLeft = -1;` |
|  5213897 | 1688 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4409481 | 1689 | `		 if( apNode[iCur] == 0 ){` |
|  2471943 | 1690 | `			 continue;` |
|        - | 1691 | `		 }` |
|  1937543 | 1692 | `		 pNode = apNode[iCur];` |
|  1937543 | 1693 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    10699 | 1694 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3912 | 1695 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1696 | `					 /* Link the node to the tree */` |
|     3923 | 1697 | `					 pNode->pLeft = apNode[iLeft];` |
|     3923 | 1698 | `					 apNode[iLeft] = 0;` |
|     1959 | 1699 | `			 }` |
|     7285 | 1700 | `		  }` |
|  1941419 | 1701 | `		 iLeft = iCur;` |
|   972650 | 1702 | `	  }` |
|   804421 | 1703 | `	 iLeft = -1;` |
|  5213897 | 1704 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4409481 | 1705 | `		 if( apNode[iCur] == 0 ){` |
|  2475861 | 1706 | `			 continue;` |
|        - | 1707 | `		 }` |
|  1933625 | 1708 | `		 pNode = apNode[iCur];` |
|  1933625 | 1709 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    10655 | 1710 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    10657 | 1711 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1712 | `					 /* Syntax error */` |
|      ! 0 | 1713 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1714 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1715 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1716 | `					 }` |
|      ! 0 | 1717 | `					 return rc;` |
|        - | 1718 | `			 }` |
|        - | 1719 | `			 /* Link the node to the tree */` |
|    10657 | 1720 | `			 pNode->pLeft = apNode[iLeft];` |
|    10657 | 1721 | `			 apNode[iLeft] = 0;` |
|        - | 1722 | `			 /* Mark as pre-increment/decrement node */` |
|    10657 | 1723 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5326 | 1724 | `		  }` |
|  1933625 | 1725 | `		 iLeft = iCur;` |
|   966815 | 1726 | `	 }` |
|        - | 1727 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   804421 | 1728 | `	  iLeft = 0;` |
|  5213891 | 1729 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4409477 | 1730 | `		  if( apNode[iCur] ){` |
|  1922969 | 1731 | `			  pNode = apNode[iCur];` |
|  1922969 | 1732 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    33477 | 1733 | `				  if( iLeft > 0 ){` |
|        - | 1734 | `					  /* Link the node to the tree */` |
|    33475 | 1735 | `					  pNode->pLeft = apNode[iLeft];` |
|    33475 | 1736 | `					  apNode[iLeft] = 0;` |
|    33475 | 1737 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1738 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1739 | `							   /* Syntax error */` |
|      ! 0 | 1740 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1741 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1742 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1743 | `							  }` |
|      ! 0 | 1744 | `							  return rc;` |
|        - | 1745 | `						  }` |
|       36 | 1746 | `					  }` |
|    16740 | 1747 | `				  }else{` |
|        - | 1748 | `					  /* Syntax error */` |
|        3 | 1749 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1750 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1751 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1752 | `					  }` |
|        3 | 1753 | `					  return rc;` |
|        - | 1754 | `				  }` |
|    16735 | 1755 | `			  }` |
|        - | 1756 | `			  /* Save terminal position */` |
|  1922967 | 1757 | `			  iLeft = iCur;` |
|   961481 | 1758 | `		  }` |
|  2204740 | 1759 | `	  }` |
|        - | 1760 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1761 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1762 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1763 | `	  * yielding a right-leaning tree. */` |
|  5213889 | 1764 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4409475 | 1765 | `		 if( apNode[iCur] == 0 ){` |
|  2520095 | 1766 | `			 continue;` |
|        - | 1767 | `		 }` |
|  1889385 | 1768 | `		 pNode = apNode[iCur];` |
|  1889385 | 1769 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1770 | `			 sxi32 iL, iR;` |
|        - | 1771 | `			 /* Find the right operand */` |
|      113 | 1772 | `			 iR = -1;` |
|        - | 1773 | `			 {` |
|        - | 1774 | `				 sxi32 j;` |
|      125 | 1775 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1776 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1777 | `				 }` |
|        - | 1778 | `			 }` |
|        - | 1779 | `			 /* Find the left operand */` |
|      113 | 1780 | `			 iL = -1;` |
|        - | 1781 | `			 {` |
|        - | 1782 | `				 sxi32 j;` |
|      181 | 1783 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1784 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1785 | `				 }` |
|        - | 1786 | `			 }` |
|      113 | 1787 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1788 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1789 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1790 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1791 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1792 | `				 }` |
|      ! 0 | 1793 | `				 return rc;` |
|        - | 1794 | `			 }` |
|      113 | 1795 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1796 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1797 | `			 apNode[iL] = 0;` |
|      113 | 1798 | `			 apNode[iR] = 0;` |
|        - | 1799 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1800 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1801 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1802 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1803 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1804 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1805 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1806 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1807 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1808 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1809 | `			  * operands are respected. */` |
|      129 | 1810 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1811 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1812 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1813 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1814 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1815 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1816 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1817 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1818 | `				 while( pTail->pLeft` |
|       34 | 1819 | `					 && pTail->pLeft->pOp` |
|       23 | 1820 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1821 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1822 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1823 | `					 pTail = pTail->pLeft;` |
|        1 | 1824 | `				 }` |
|        - | 1825 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1826 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1827 | `				 pTail->pLeft = pNode;` |
|       27 | 1828 | `				 apNode[iCur] = pHead;` |
|       13 | 1829 | `			 }` |
|       56 | 1830 | `		 }` |
|   944695 | 1831 | `	 }` |
|        - | 1832 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  8848473 | 1833 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  8044069 | 1834 | `		 iLeft = -1;` |
| 52138475 | 1835 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 44094421 | 1836 | `			 if( apNode[iCur] == 0 ){` |
| 28411785 | 1837 | `				 continue;` |
|        - | 1838 | `			 }` |
| 15682641 | 1839 | `			 pNode = apNode[iCur];` |
| 15682641 | 1840 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1841 | `				 /* Get the right node */` |
|   239995 | 1842 | `				 iRight = iCur + 1;` |
|   342871 | 1843 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   102881 | 1844 | `					 iRight++;` |
|        5 | 1845 | `				 }` |
|   239995 | 1846 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1847 | `					 /* Syntax error */` |
|       10 | 1848 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       10 | 1849 | `					 if( rc != SXERR_ABORT ){` |
|       10 | 1850 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1851 | `					 }` |
|       10 | 1852 | `					 return rc;` |
|        - | 1853 | `				 }` |
|   239987 | 1854 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1855 | `					 sxi32  iTmp;` |
|        - | 1856 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1857 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1858 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1859 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1860 | `					  * is swapped below. */` |
|       57 | 1861 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1862 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1863 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1864 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1865 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1866 | `						 }` |
|        3 | 1867 | `						 return rc;` |
|        - | 1868 | `					 }` |
|       54 | 1869 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1870 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1871 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1872 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1873 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1874 | `						 }` |
|      ! 0 | 1875 | `						 return rc;` |
|        - | 1876 | `					 }` |
|       54 | 1877 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       38 | 1878 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1879 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1880 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1881 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1882 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1883 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1884 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1885 | `									 }` |
|      ! 0 | 1886 | `									 return rc;` |
|        - | 1887 | `							 }` |
|      ! 0 | 1888 | `						 }` |
|       18 | 1889 | `					 }` |
|        - | 1890 | `					 /* Swap operands */` |
|       54 | 1891 | `					 iTmp = iRight;` |
|       54 | 1892 | `					 iRight = iLeft;` |
|       54 | 1893 | `					 iLeft = iTmp;` |
|       26 | 1894 | `				 }` |
|        - | 1895 | `				 /* Link the node to the tree */` |
|   239985 | 1896 | `				 pNode->pLeft = apNode[iLeft];` |
|   239985 | 1897 | `				 pNode->pRight = apNode[iRight];` |
|   239985 | 1898 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   119990 | 1899 | `			 }` |
| 15682631 | 1900 | `			 iLeft = iCur;` |
|  7841318 | 1901 | `		 }` |
|  4022032 | 1902 | `	 }` |
|        - | 1903 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1904 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1905 | `	  * we are dealing with a single operator.` |
|        - | 1906 | `	  */` |
|   804409 | 1907 | `	  iLeft = -1;` |
|  5202439 | 1908 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4400699 | 1909 | `		  if( apNode[iCur] == 0 ){` |
|  2999367 | 1910 | `			  continue;` |
|        - | 1911 | `		  }` |
|  1401337 | 1912 | `		  pNode = apNode[iCur];` |
|  1401337 | 1913 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2669 | 1914 | `			  sxi32 iNest = 1;` |
|     2669 | 1915 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1916 | `				  /* Missing condition */` |
|        3 | 1917 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1918 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1919 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1920 | `				  }` |
|        3 | 1921 | `				  return rc;` |
|        - | 1922 | `			  }` |
|        - | 1923 | `			  /* Get the right node */` |
|     2667 | 1924 | `			  iRight = iCur + 1;` |
|     5589 | 1925 | `			  while( iRight < nToken  ){` |
|     5589 | 1926 | `				  if( apNode[iRight] ){` |
|     5261 | 1927 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1928 | `						  /* Increment nesting level */` |
|      ! 0 | 1929 | `						  ++iNest;` |
|     5261 | 1930 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1931 | `						  /* Decrement nesting level */` |
|     2667 | 1932 | `						  --iNest;` |
|     2667 | 1933 | `						  if( iNest <= 0 ){` |
|     2667 | 1934 | `							  break;` |
|        - | 1935 | `						  }` |
|      ! 0 | 1936 | `					  }` |
|     1297 | 1937 | `				  }` |
|     2927 | 1938 | `				  iRight++;` |
|        5 | 1939 | `			  }` |
|     2667 | 1940 | `			  if( iRight > iCur + 1 ){` |
|        - | 1941 | `				  /* Recurse and process the then expression */` |
|     2599 | 1942 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2599 | 1943 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1944 | `					  return rc;` |
|        - | 1945 | `				  }` |
|        - | 1946 | `				  /* Link the node to the tree */` |
|     2599 | 1947 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1297 | 1948 | `			  }else{` |
|        - | 1949 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1950 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1951 | `			  }` |
|     2667 | 1952 | `			  apNode[iCur + 1] = 0;` |
|     2667 | 1953 | `			  if( iRight + 1 < nToken ){` |
|        - | 1954 | `				  /* Recurse and process the else expression */` |
|     2667 | 1955 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2667 | 1956 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1957 | `					  return rc;` |
|        - | 1958 | `				  }` |
|        - | 1959 | `				  /* Link the node to the tree */` |
|     2667 | 1960 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2667 | 1961 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1336 | 1962 | `			  }else{` |
|      ! 0 | 1963 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1964 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1965 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1966 | `				 }` |
|      ! 0 | 1967 | `				 return rc;` |
|        - | 1968 | `			  }` |
|        - | 1969 | `			  /* Point to the condition */` |
|     2667 | 1970 | `			  pNode->pCond  = apNode[iLeft];` |
|     2667 | 1971 | `			  apNode[iLeft] = 0;` |
|     2667 | 1972 | `			  break;` |
|        - | 1973 | `		  }` |
|  1398673 | 1974 | `		  iLeft = iCur;` |
|   699339 | 1975 | `	  }` |
|        - | 1976 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1977 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1978 | `	  * so there is no need for a precedence loop here.` |
|        - | 1979 | `	  */` |
|   804407 | 1980 | `	 iRight = -1;` |
|  5213693 | 1981 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4409345 | 1982 | `		 if( apNode[iCur] == 0 ){` |
|  3307751 | 1983 | `			 continue;` |
|        - | 1984 | `		 }` |
|  1101599 | 1985 | `		 pNode = apNode[iCur];` |
|  1101599 | 1986 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1987 | `			 /* Get the left node */` |
|   297075 | 1988 | `			 iLeft = iCur - 1;` |
|   429653 | 1989 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   132583 | 1990 | `				 iLeft--;` |
|        5 | 1991 | `			 }` |
|   297075 | 1992 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1993 | `				 /* Syntax error */` |
|       45 | 1994 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1995 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 1996 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1997 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 1998 | `				 }else{` |
|       41 | 1999 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 2000 | `				 }` |
|       45 | 2001 | `				 if( rc != SXERR_ABORT ){` |
|       43 | 2002 | `					 rc = SXERR_SYNTAX;` |
|       20 | 2003 | `				 }` |
|       45 | 2004 | `				 return rc;` |
|        - | 2005 | `			 }` |
|        - | 2006 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 2007 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 2008 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 2009 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 2010 | `			  * a write. */` |
|   297033 | 2011 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 2012 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 2013 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 2014 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 2015 | `					 rc = SXERR_SYNTAX;` |
|        4 | 2016 | `				 }` |
|       11 | 2017 | `				 return rc;` |
|        - | 2018 | `			 }` |
|   297025 | 2019 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      111 | 2020 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       78 | 2021 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2022 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2023 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2024 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2025 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2026 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2027 | `					 }else{` |
|        4 | 2028 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2029 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2030 | `					 }` |
|        6 | 2031 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2032 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2033 | `					 }` |
|        6 | 2034 | `					 return rc;` |
|        - | 2035 | `				 }` |
|       38 | 2036 | `			 }` |
|        - | 2037 | `			 /* Link the node to the tree (Reverse) */` |
|   297021 | 2038 | `			 pNode->pLeft = apNode[iRight];` |
|   297021 | 2039 | `			 pNode->pRight = apNode[iLeft];` |
|   297021 | 2040 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   148508 | 2041 | `		 }` |
|  1101545 | 2042 | `		 iRight = iCur;` |
|   550775 | 2043 | `	 }` |
|        - | 2044 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  4021745 | 2045 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3217397 | 2046 | `		 iLeft = -1;` |
| 20854485 | 2047 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 17637093 | 2048 | `			 if( apNode[iCur] == 0 ){` |
| 14419295 | 2049 | `				 continue;` |
|        - | 2050 | `			 }` |
|  3217803 | 2051 | `			 pNode = apNode[iCur];` |
|  3217803 | 2052 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2053 | `				 /* Get the right node */` |
|       72 | 2054 | `				 iRight = iCur + 1;` |
|      110 | 2055 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2056 | `					 iRight++;` |
|        2 | 2057 | `				 }` |
|       72 | 2058 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2059 | `					 /* Syntax error */` |
|      ! 0 | 2060 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2061 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2062 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2063 | `					 }` |
|      ! 0 | 2064 | `					 return rc;` |
|        - | 2065 | `				 }` |
|        - | 2066 | `				 /* Link the node to the tree */` |
|       72 | 2067 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2068 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2069 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2070 | `			 }` |
|  3217803 | 2071 | `			 iLeft = iCur;` |
|  1608904 | 2072 | `		 }` |
|  1608701 | 2073 | `	 }` |
|        - | 2074 | `	 /* Point to the root of the expression tree */` |
|  4409249 | 2075 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3604919 | 2076 | `		 if( apNode[iCur] ){` |
|   742591 | 2077 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       22 | 2078 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       22 | 2079 | `				  if( rc != SXERR_ABORT ){` |
|       22 | 2080 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2081 | `				  }` |
|       22 | 2082 | `				  return rc;` |
|        - | 2083 | `			 }` |
|   742573 | 2084 | `			 apNode[0] = apNode[iCur];` |
|   742573 | 2085 | `			 apNode[iCur] = 0;` |
|   371284 | 2086 | `		 }` |
|  1802453 | 2087 | `	 }` |
|   804335 | 2088 | `	 return SXRET_OK;` |
|   758747 | 2089 | ` }` |
|        - | 2090 | ` /*` |
|        - | 2091 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2092 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2093 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2094 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2095 | `  */` |
|   949714 | 2096 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2097 |  |
|        - | 2098 | `	ph7_expr_node **apNode;` |
|        - | 2099 | `	ph7_expr_node *pNode;` |
|        - | 2100 | `	sxi32 rc;` |
|        - | 2101 | `	/* Reset node container */` |
|   949719 | 2102 | `	SySetReset(pExprNode);` |
|   949719 | 2103 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2104 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2105 | `	{` |
|   949719 | 2106 | `		int iLastWasTerm = 0;` |
|   949719 | 2107 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  5146491 | 2108 | `		while( pGen->pIn < pGen->pEnd ){` |
|  4196811 | 2109 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  4196811 | 2110 | `			if( rc != SXRET_OK ){` |
|       38 | 2111 | `				return rc;` |
|        - | 2112 | `			}` |
|        - | 2113 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  4196777 | 2114 | `			if( pNode->xCode ){` |
|        - | 2115 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2201531 | 2116 | `				iLastWasTerm = 1;` |
|  3096014 | 2117 | `			}else if( pNode->pOp ){` |
|        - | 2118 | `				/* Operator node */` |
|   948277 | 2119 | `				iLastWasTerm = 0;` |
|   474141 | 2120 | `			}else{` |
|        - | 2121 | `				/* Delimiter: ')' and ']' end terms */` |
|  1046979 | 2122 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2123 | `			}` |
|        - | 2124 | `			/* A keyword in the next node is a member name only right after a member` |
|        - | 2125 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|        - | 2126 | `			 * node kind, so this single test covers all branches. */` |
|  4196777 | 2127 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|        - | 2128 | `			/* Save the extracted node */` |
|  4196777 | 2129 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2130 | `		}` |
|        - | 2131 | `	}` |
|   949685 | 2132 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2133 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2134 | `		*ppRoot = 0;` |
|      ! 0 | 2135 | `		return SXRET_OK;` |
|        - | 2136 | `	}` |
|   949685 | 2137 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2138 | `	/* Make sure we are dealing with valid nodes */` |
|   949685 | 2139 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   949685 | 2140 | `	if( rc != SXRET_OK ){` |
|        - | 2141 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2142 | `		 * cleanup the mess left behind.` |
|        - | 2143 | `		 */` |
|       54 | 2144 | `		*ppRoot = 0;` |
|       54 | 2145 | `		return rc;` |
|        - | 2146 | `	}` |
|        - | 2147 | `	/* Build the tree */` |
|   949635 | 2148 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   949635 | 2149 | `	if( rc != SXRET_OK ){` |
|        - | 2150 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2151 | `		*ppRoot = 0;` |
|      103 | 2152 | `		return rc;` |
|        - | 2153 | `	}` |
|        - | 2154 | `	/* Point to the root of the tree */` |
|   949537 | 2155 | `	*ppRoot = apNode[0];` |
|   949537 | 2156 | `	return SXRET_OK;` |
|   474862 | 2157 |  |
|        - | 2158 |  |
