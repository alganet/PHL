# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1105/1277 lines (86.53%)

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
|  1102920 |  268 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  269 |  |
|  1102925 |  270 | `	sxu32 n = 0;` |
|        - |  271 | `	sxi32 rc;` |
|        - |  272 | `	/* Do a linear lookup on the operators table */` |
| 18510658 |  273 | `	for(;;){` |
| 37021321 |  274 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  275 | `			break;` |
|        - |  276 | `		}` |
| 37021321 |  277 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  278 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  4349137 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  2174571 |  280 | `		}else{` |
| 32672189 |  281 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  282 | `		}` |
| 37021321 |  283 | `		if( rc == 0 ){` |
|  1107167 |  284 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  285 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1102529 |  286 | `				return &aOpTable[n];` |
|        - |  287 | `			}` |
|        - |  288 | `			/* Handle ambiguity */` |
|     4643 |  289 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  290 | `				/* Unary opertors have prcedence here over binary operators */` |
|      269 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|     4379 |  293 | `			if( pLast->nType & PH7_TK_OP ){` |
|      143 |  294 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  295 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      143 |  296 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  297 | `					/* Unary opertors have prcedence here over binary operators */` |
|      135 |  298 | `					return &aOpTable[n];` |
|        - |  299 | `				}` |
|        - |  300 |  |
|        4 |  301 | `			}` |
|     2121 |  302 | `		}` |
| 35918401 |  303 | `		++n; /* Next operator in the table */` |
|        5 |  304 | `	}` |
|        - |  305 | `	/* No such operator */` |
|      ! 0 |  306 | `	return 0;` |
|   551465 |  307 |  |
|        - |  308 | `/*` |
|        - |  309 | ` * Delimit a set of token stream.` |
|        - |  310 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  311 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  312 | ` */` |
|   677498 |  313 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  314 |  |
|   677503 |  315 | `	SyToken *pCur = pIn;` |
|   677503 |  316 | `	sxi32 iNest = 1;` |
|  3658181 |  317 | `	for(;;){` |
|  7316367 |  318 | `		if( pCur >= pEnd ){` |
|      297 |  319 | `			break;` |
|        - |  320 | `		}` |
|  7316075 |  321 | `		if( pCur->nType & nTokStart ){` |
|        - |  322 | `			/* Increment nesting level */` |
|   360499 |  323 | `			iNest++;` |
|  7135828 |  324 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  325 | `			/* Decrement nesting level */` |
|  1037705 |  326 | `			iNest--;` |
|  1037705 |  327 | `			if( iNest <= 0 ){` |
|   677211 |  328 | `				break;` |
|        - |  329 | `			}` |
|   180247 |  330 | `		}` |
|        - |  331 | `		/* Advance cursor */` |
|  6638869 |  332 | `		pCur++;` |
|        5 |  333 | `	}` |
|        - |  334 | `	/* Point to the end of the chunk */` |
|   677503 |  335 | `	*ppEnd = pCur;` |
|   677503 |  336 |  |
|        - |  337 | `/*` |
|        - |  338 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  339 | ` * Note on reserved keywords.` |
|        - |  340 | ` *  According to the PHP language reference manual:` |
|        - |  341 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  342 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  343 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  344 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  345 | ` */` |
|    22026 |  346 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  347 |  |
|    22026 |  348 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    21930 |  349 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  350 | `		){` |
|      165 |  351 | `			return TRUE;` |
|        - |  352 | `	}` |
|    21871 |  353 | `	if( bCheckFunc ){` |
|      198 |  354 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      191 |  355 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      175 |  356 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       45 |  357 | `				return TRUE;` |
|        - |  358 | `		}` |
|       79 |  359 | `	}` |
|        - |  360 | `	/* Not a language construct */` |
|    21831 |  361 | `	return FALSE;` |
|    11018 |  362 |  |
|        - |  363 | `/*` |
|        - |  364 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  365 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  366 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  367 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  368 | ` */` |
|   932352 |  369 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  370 |  |
|        - |  371 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  372 | `	sxi32 i,rc;` |
|        - |  373 |  |
|   932357 |  374 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  375 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  376 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  377 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  378 | `	}` |
|   932357 |  379 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5019147 |  380 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4086829 |  381 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  382 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1011 |  383 | `			continue;` |
|        - |  384 | `		}` |
|  4085823 |  385 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   448413 |  386 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    22344 |  387 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  388 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   418381 |  389 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  390 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  391 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  392 | `						 */` |
|   418381 |  393 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   418381 |  394 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   418381 |  395 | `						apNode[i]->pOp = &sFCallOp;` |
|   209188 |  396 | `					}` |
|   209188 |  397 | `			}` |
|   448413 |  398 | `			iParen++;` |
|  3861619 |  399 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   448413 |  400 | `			if( iParen <= 0 ){` |
|       16 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       16 |  402 | `				if( rc != SXERR_ABORT ){` |
|       16 |  403 | `					rc = SXERR_SYNTAX;` |
|        6 |  404 | `				}` |
|       16 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|   448401 |  407 | `			iParen--;` |
|  3413205 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    90681 |  409 | `			iSquare++;` |
|  3143669 |  410 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    90695 |  411 | `			if( iSquare <= 0 ){` |
|        9 |  412 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        9 |  413 | `				if( rc != SXERR_ABORT ){` |
|        9 |  414 | `					rc = SXERR_SYNTAX;` |
|        3 |  415 | `				}` |
|        9 |  416 | `				return rc;` |
|        - |  417 | `			}` |
|    90689 |  418 | `			iSquare--;` |
|  3052983 |  419 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  3007634 |  466 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       20 |  467 | `			if( iBraces <= 0 ){` |
|       15 |  468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       15 |  469 | `				if( rc != SXERR_ABORT ){` |
|       15 |  470 | `					rc = SXERR_SYNTAX;` |
|        6 |  471 | `				}` |
|       15 |  472 | `				return rc;` |
|        - |  473 | `			}` |
|        6 |  474 | `			iBraces--;` |
|  3007613 |  475 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2843 |  476 | `			if( iQuesty > 0 ){` |
|     2653 |  477 | `				iQuesty--;` |
|     1519 |  478 | `			}else if( iParen <= 0 ){` |
|        - |  479 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  480 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  481 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  482 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  483 | `				if( rc != SXERR_ABORT ){` |
|        5 |  484 | `					rc = SXERR_SYNTAX;` |
|        2 |  485 | `				}` |
|        5 |  486 | `				return rc;` |
|        5 |  487 | `			}` |
|  3006190 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   858243 |  489 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   858243 |  490 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2655 |  491 | `				iQuesty++;` |
|   856918 |  492 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   429119 |  512 | `		}` |
|  2042897 |  513 | `	}` |
|   932323 |  514 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       19 |  515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       19 |  516 | `		if( rc != SXERR_ABORT ){` |
|       19 |  517 | `			rc = SXERR_SYNTAX;` |
|        8 |  518 | `		}` |
|       19 |  519 | `		return rc;` |
|        - |  520 | `	}` |
|   932307 |  521 | `	return SXRET_OK;` |
|   466181 |  522 |  |
|        - |  523 | `/*` |
|        - |  524 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  525 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  526 | ` */` |
|   776984 |  527 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  528 |  |
|   776989 |  529 | `	SyToken *pIn = *ppCur;` |
|        - |  530 | `	/* Jump the first literal seen */` |
|   776989 |  531 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   776949 |  532 | `		pIn++;` |
|   388472 |  533 | `	}` |
|   388536 |  534 | `	for(;;){` |
|   777077 |  535 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       93 |  536 | `			pIn++;` |
|       93 |  537 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       91 |  538 | `				pIn++;` |
|       43 |  539 | `			}` |
|       49 |  540 | `		}else{` |
|   388497 |  541 | `			break;` |
|        - |  542 | `		}` |
|        5 |  543 | `	}` |
|        - |  544 | `	/* Synchronize pointers */` |
|   776989 |  545 | `	*ppCur = pIn;` |
|   776989 |  546 |  |
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
|      282 |  580 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  581 |  |
|      287 |  582 | `	SyToken *pIn = *ppCur;` |
|        - |  583 | `	sxu32 nLine;` |
|        - |  584 | `	sxi32 rc;` |
|        - |  585 | `	/* Jump the 'function' keyword */` |
|      287 |  586 | `	nLine = pIn->nLine;` |
|      287 |  587 | `	pIn++;` |
|      287 |  588 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  589 | `		pIn++;` |
|        1 |  590 | `	}` |
|      287 |  591 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  592 | `		/* Syntax error */` |
|        6 |  593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  594 | `		if( rc != SXERR_ABORT ){` |
|        6 |  595 | `			rc = SXERR_SYNTAX;` |
|        2 |  596 | `		}` |
|        6 |  597 | `		goto Synchronize;` |
|        - |  598 | `	}` |
|      283 |  599 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      283 |  600 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      283 |  601 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  602 | `		/* Syntax error */` |
|        6 |  603 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  604 | `		if( rc != SXERR_ABORT ){` |
|        6 |  605 | `			rc = SXERR_SYNTAX;` |
|        2 |  606 | `		}` |
|        6 |  607 | `		goto Synchronize;` |
|        - |  608 | `	}` |
|      279 |  609 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  610 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      279 |  611 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      279 |  638 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
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
|      263 |  671 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      263 |  672 | `		pIn++; /* Jump the leading curly '{' */` |
|      263 |  673 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      263 |  674 | `		if( pIn < pEnd ){` |
|      263 |  675 | `			pIn++;` |
|      129 |  676 | `		}` |
|      134 |  677 | `	}else{` |
|        - |  678 | `		/* Syntax error */` |
|      ! 0 |  679 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|        - |  683 | `	}` |
|      263 |  684 | `	rc = SXRET_OK;` |
|      141 |  685 | `Synchronize:` |
|        - |  686 | `	/* Synchronize pointers */` |
|      287 |  687 | `	*ppCur = pIn;` |
|      287 |  688 | `	return rc;` |
|      146 |  689 |  |
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
|      122 |  773 | `		pIn++; /* '(' */` |
|      122 |  774 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      122 |  775 | `		if( pIn < pEnd ){` |
|      119 |  776 | `			pIn++; /* ')' */` |
|       58 |  777 | `		}` |
|       59 |  778 | `	}` |
|        - |  779 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|      124 |  780 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  781 | `		pIn++;` |
|        6 |  782 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
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
|  4087056 |  877 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|        5 |  878 |  |
|        - |  879 | `	ph7_expr_node *pNode;` |
|        - |  880 | `	SyToken *pCur;` |
|        - |  881 | `	sxi32 rc;` |
|        - |  882 | `	/* Allocate a new node */` |
|  4087061 |  883 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4087061 |  884 | `	if( pNode == 0 ){` |
|        - |  885 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  886 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  887 | `		 */` |
|      ! 0 |  888 | `		return SXERR_MEM;` |
|        - |  889 | `	}` |
|        - |  890 | `	/* Zero the structure */` |
|  4087061 |  891 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4087061 |  892 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  893 | `	/* Point to the head of the token stream */` |
|  4087061 |  894 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  895 | `	/* Start collecting tokens */` |
|  4087061 |  896 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  897 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  898 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       86 |  899 | `		pCur++;` |
|       86 |  900 | `		pGen->pIn = pCur;` |
|       86 |  901 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       86 |  902 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       86 |  903 | `		if( rc == SXRET_OK && *ppNode ){` |
|       86 |  904 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       41 |  905 | `		}` |
|       86 |  906 | `		return rc;` |
|        - |  907 | `	}` |
|  4086979 |  908 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  909 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  910 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  911 | `		 */` |
|     1013 |  912 | `		pCur++; /* Skip the opening '[' */` |
|     1013 |  913 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1013 |  914 | `		if( pCur < pGen->pEnd ){` |
|     1013 |  915 | `			pCur++; /* Skip past the closing ']' */` |
|      509 |  916 | `		}else{` |
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
|     1081 |  928 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      139 |  929 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      139 |  930 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       47 |  931 | `				pNode->xCode = PH7_CompileShortList;` |
|       25 |  932 | `			}else{` |
|       93 |  933 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  934 | `			}` |
|       71 |  935 | `		}else{` |
|      877 |  936 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  937 | `		}` |
|  4086475 |  938 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  939 | `		/* Point to the instance that describe this operator */` |
|   948953 |  940 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  941 | `		/* Advance the stream cursor */` |
|   948953 |  942 | `		pCur++;` |
|  3611497 |  943 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  944 | `		/* Isolate variable */` |
|  2205689 |  945 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1102853 |  946 | `			pCur++; /* Variable variable */` |
|        5 |  947 | `		}` |
|  1102841 |  948 | `		if( pCur < pGen->pEnd ){` |
|  1102841 |  949 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  950 | `				/* Variable name */` |
|  1102813 |  951 | `				pCur++;` |
|   551437 |  952 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       24 |  953 | `				pCur++;` |
|        - |  954 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       24 |  955 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       24 |  956 | `				if( pCur < pGen->pEnd ){` |
|       19 |  957 | `					pCur++;` |
|       11 |  958 | `				}else{` |
|        5 |  959 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  960 | `					if( rc != SXERR_ABORT ){` |
|        5 |  961 | `						rc = SXERR_SYNTAX;` |
|        2 |  962 | `					}` |
|        5 |  963 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  964 | `					return rc;` |
|        - |  965 | `				}` |
|        8 |  966 | `			}` |
|   551416 |  967 | `		}` |
|  1102837 |  968 | `		pNode->xCode = PH7_CompileVariable;` |
|  2585603 |  969 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    52621 |  970 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    52621 |  971 | `		 if( bAfterMemberOp ){` |
|        - |  972 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|        - |  973 | `			  * method/property NAME, not a language construct — PHP allows any` |
|        - |  974 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|        - |  975 | `			  * as a plain literal like an ordinary identifier member name. */` |
|      145 |  976 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      145 |  977 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    52551 |  978 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  979 | `			 /* List/Array node */` |
|    30035 |  980 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  981 | `				 /* Assume a literal */` |
|      ! 0 |  982 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  983 | `				 pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  984 | `			 }else{` |
|    30035 |  985 | `				 pCur += 2;` |
|        - |  986 | `				 /* Collect array/list tokens */` |
|    30035 |  987 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    30035 |  988 | `				 if( pCur < pGen->pEnd ){` |
|    30033 |  989 | `					 pCur++;` |
|    15019 |  990 | `				 }else{` |
|        - |  991 | `					 /* Syntax error */` |
|        4 |  992 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  993 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  994 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  995 | `						 rc = SXERR_SYNTAX;` |
|        1 |  996 | `					 }` |
|        3 |  997 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  998 | `					 return rc;` |
|        - |  999 | `				 }` |
|    30033 | 1000 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    30033 | 1001 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       31 | 1002 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       31 | 1003 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - | 1004 | `						 /* Syntax error */` |
|        3 | 1005 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 | 1006 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1007 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1008 | `						 }` |
|        3 | 1009 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1010 | `						 return rc;` |
|        - | 1011 | `					 }` |
|       13 | 1012 | `				 }` |
|        5 | 1013 | `			 }` |
|    37464 | 1014 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1015 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      159 | 1016 | `			 pCur++; /* Skip 'yield' keyword */` |
|      159 | 1017 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1018 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1019 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      159 | 1020 | `			 pNode->xCode = PH7_CompileYield;` |
|    22374 | 1021 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1022 | `			 /* Annonymous function */` |
|      287 | 1023 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1024 | `				 /* Assume a literal */` |
|      ! 0 | 1025 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1026 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1027 | `			 }else{` |
|        - | 1028 | `				 /* Assemble annonymous functions body */` |
|      287 | 1029 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      287 | 1030 | `				 if( rc != SXRET_OK ){` |
|       28 | 1031 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1032 | `					 return rc;` |
|        - | 1033 | `				 }` |
|      263 | 1034 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1035 | `			  }` |
|    22144 | 1036 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       39 | 1037 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       22 | 1038 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       12 | 1039 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        9 | 1040 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1041 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1042 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1043 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1044 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       29 | 1045 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       29 | 1046 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1047 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1048 | `				 return rc;` |
|        - | 1049 | `			 }` |
|       29 | 1050 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    22000 | 1051 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    21931 | 1052 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 | 1053 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 | 1054 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1055 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      124 | 1056 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      124 | 1057 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1058 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1059 | `				 return rc;` |
|        - | 1060 | `			 }` |
|      124 | 1061 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    21929 | 1062 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1063 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1064 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1065 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1066 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1067 | `				 return rc;` |
|        - | 1068 | `			 }` |
|       75 | 1069 | `			 pNode->xCode = PH7_CompileMatch;` |
|    21834 | 1070 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1071 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1072 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1073 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1074 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1075 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1076 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1077 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1078 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    21781 | 1079 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1080 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       91 | 1081 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       91 | 1082 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       48 | 1083 | `		 }else{` |
|        - | 1084 | `			 /* Assume a literal */` |
|    21677 | 1085 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    21677 | 1086 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1087 | `		 }` |
|  2007865 | 1088 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1089 | `		 /* Constants,function name,namespace path,class name... */` |
|   755177 | 1090 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   755177 | 1091 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   377591 | 1092 | `	 }else{` |
|  1226399 | 1093 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1094 | `			 /* Point to the code generator routine */` |
|   236003 | 1095 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   236003 | 1096 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1097 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1098 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1099 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1100 | `				 }` |
|        3 | 1101 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1102 | `				 return rc;` |
|        - | 1103 | `			 }` |
|   117998 | 1104 | `		 }` |
|        - | 1105 | `		/* Advance the stream cursor */` |
|  1226397 | 1106 | `		pCur++;` |
|        - | 1107 | `	 }` |
|        - | 1108 | `	/* Point to the end of the token stream */` |
|  4086945 | 1109 | `	pNode->pEnd = pCur;` |
|        - | 1110 | `	/* Save the node for later processing */` |
|  4086945 | 1111 | `	*ppNode = pNode;` |
|        - | 1112 | `	/* Synchronize cursors */` |
|  4086945 | 1113 | `	pGen->pIn = pCur;` |
|  4086945 | 1114 | `	return SXRET_OK;` |
|  2043533 | 1115 |  |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1118 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1119 | ` * level is zero.` |
|        - | 1120 | ` */` |
|    92348 | 1121 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1122 |  |
|    92353 | 1123 | `	SyToken *pCur = pStart;` |
|    92353 | 1124 | `	sxi32 iNest = 0;` |
|    92353 | 1125 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1126 | `		/* Last expression */` |
|    48427 | 1127 | `		return SXERR_EOF;` |
|        - | 1128 | `	}` |
|   179639 | 1129 | `	while( pCur < pEnd ){` |
|   163559 | 1130 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    27851 | 1131 | `			break;` |
|        - | 1132 | `		}` |
|   135713 | 1133 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     8837 | 1134 | `			iNest++;` |
|   131297 | 1135 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     8839 | 1136 | `			iNest--;` |
|     4417 | 1137 | `		}` |
|   135713 | 1138 | `		pCur++;` |
|        5 | 1139 | `	}` |
|    43931 | 1140 | `	*ppNext = pCur;` |
|    43931 | 1141 | `	return SXRET_OK;` |
|    46179 | 1142 |  |
|        - | 1143 | `/*` |
|        - | 1144 | ` * Free an expression tree.` |
|        - | 1145 | ` */` |
|  3527434 | 1146 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1147 |  |
|  3527439 | 1148 | `	if( pNode->pLeft ){` |
|        - | 1149 | `		/* Release the left tree */` |
|  1319289 | 1150 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   659642 | 1151 | `	}` |
|  3527439 | 1152 | `	if( pNode->pRight ){` |
|        - | 1153 | `		/* Release the right tree */` |
|   729417 | 1154 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   364706 | 1155 | `	}` |
|  3527439 | 1156 | `	if( pNode->pCond ){` |
|        - | 1157 | `		/* Release the conditional tree used by the ternary operator */` |
|     2651 | 1158 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1323 | 1159 | `	}` |
|  3527439 | 1160 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1161 | `		ph7_expr_node **apArg;` |
|        - | 1162 | `		sxu32 n;` |
|        - | 1163 | `		/* Release node arguments */` |
|   435161 | 1164 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   918113 | 1165 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   482957 | 1166 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   241481 | 1167 | `		}` |
|   435161 | 1168 | `		SySetRelease(&pNode->aNodeArgs);` |
|   217578 | 1169 | `	}` |
|        - | 1170 | `	/* Finally,release this node */` |
|  3527439 | 1171 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3527439 | 1172 |  |
|        - | 1173 | `/*` |
|        - | 1174 | ` * Free an expression tree.` |
|        - | 1175 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1176 | ` */` |
|   932386 | 1177 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1178 |  |
|        - | 1179 | `	ph7_expr_node **apNode;` |
|        - | 1180 | `	sxu32 n;` |
|   932391 | 1181 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5019331 | 1182 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4086945 | 1183 | `		if( apNode[n] ){` |
|   932725 | 1184 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   466360 | 1185 | `		}` |
|  2043475 | 1186 | `	}` |
|   932391 | 1187 | `	return SXRET_OK;` |
|        5 | 1188 |  |
|        - | 1189 | `/*` |
|        - | 1190 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1191 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1192 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1193 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1194 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1195 | ` */` |
|  1303716 | 1196 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1197 |  |
|  1303721 | 1198 | `	if( pNode == 0 ){` |
|   802827 | 1199 | `		return 0;` |
|        - | 1200 | `	}` |
|   500899 | 1201 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1202 | `		return 1;` |
|        - | 1203 | `	}` |
|   500887 | 1204 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1205 | `		return 1;` |
|        - | 1206 | `	}` |
|   500883 | 1207 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1208 | `		return 1;` |
|        - | 1209 | `	}` |
|   500883 | 1210 | `	return 0;` |
|   651863 | 1211 |  |
|        - | 1212 | `/*` |
|        - | 1213 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1214 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1215 | ` */` |
|   295160 | 1216 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1217 |  |
|        - | 1218 | `	sxi32 iExprOp;` |
|   295165 | 1219 | `	if( pNode->pOp == 0 ){` |
|   177941 | 1220 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1221 | `	}` |
|   117229 | 1222 | `	iExprOp = pNode->pOp->iOp;` |
|   117229 | 1223 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    81523 | 1224 | `			return TRUE;` |
|        - | 1225 | `	}` |
|    35711 | 1226 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    35707 | 1227 | `		if( pNode->pLeft->pOp ) {` |
|       50 | 1228 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       24 | 1229 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1230 | `				return FALSE;` |
|        5 | 1231 | `			}` |
|    35682 | 1232 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1233 | `			return FALSE;` |
|        - | 1234 | `		}` |
|    35707 | 1235 | `		return TRUE;` |
|        - | 1236 | `	}` |
|        5 | 1237 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1238 | `		return TRUE;` |
|        - | 1239 | `	}` |
|        - | 1240 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1241 | `	return FALSE;` |
|   147585 | 1242 |  |
|        - | 1243 | `/* Forward declaration */` |
|        - | 1244 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1245 | `/* Macro to check if the given node is a terminal.` |
|        - | 1246 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1247 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1248 | ` * linked ternary/elvis node). */` |
|        - | 1249 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1250 | `/*` |
|        - | 1251 | ` * Buid an expression tree for each given function argument.` |
|        - | 1252 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1253 | ` */` |
|   362192 | 1254 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1255 |  |
|        - | 1256 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1257 | `	sxi32 rc;` |
|        - | 1258 | `	/* Process function arguments from left to right */` |
|   362197 | 1259 | `	iCur = 0;` |
|   386078 | 1260 | `	for(;;){` |
|   772161 | 1261 | `		if( iCur >= nToken ){` |
|        - | 1262 | `			/* No more arguments to process */` |
|   362171 | 1263 | `			break;` |
|        - | 1264 | `		}` |
|   409995 | 1265 | `		iNode = iCur;` |
|   409995 | 1266 | `		iNest = 0;` |
|  1024065 | 1267 | `		while( iCur < nToken ){` |
|   661897 | 1268 | `			if( apNode[iCur] ){` |
|   647801 | 1269 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    23916 | 1270 | `					break;` |
|   599974 | 1271 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   316652 | 1272 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    33201 | 1273 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1274 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1275 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1276 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1277 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1278 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1279 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    33067 | 1280 | `					iNest++;` |
|   583448 | 1281 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    33067 | 1282 | `					iNest--;` |
|    16531 | 1283 | `				}` |
|   299987 | 1284 | `			}` |
|   614075 | 1285 | `			iCur++;` |
|        5 | 1286 | `		}` |
|   409995 | 1287 | `		if( iCur > iNode ){` |
|   409989 | 1288 | `			SyString sArgName = {0, 0};` |
|        - | 1289 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1290 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1291 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   409984 | 1292 | `			if( (iCur - iNode) >= 2` |
|   227242 | 1293 | `				&& apNode[iNode]` |
|    44500 | 1294 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    24558 | 1295 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     4522 | 1296 | `				&& apNode[iNode+1]` |
|     4433 | 1297 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1298 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      191 | 1299 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      191 | 1300 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      191 | 1301 | `				apNode[iNode] = 0;` |
|      191 | 1302 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      191 | 1303 | `				apNode[iNode+1] = 0;` |
|      191 | 1304 | `				iNode += 2;` |
|        - | 1305 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1306 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      191 | 1307 | `				if( iNode >= iCur ){` |
|        4 | 1308 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1309 | `						pOp->pStart->nLine,` |
|        - | 1310 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1311 | `						&sArgName);` |
|        3 | 1312 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1313 | `						rc = SXERR_SYNTAX;` |
|        1 | 1314 | `					}` |
|        3 | 1315 | `					return rc;` |
|        - | 1316 | `				}` |
|       92 | 1317 | `			}` |
|   409982 | 1318 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1319 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1320 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1321 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1322 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1323 | `					apNode[iNode] = 0;` |
|      ! 0 | 1324 | `			}` |
|   409987 | 1325 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   409987 | 1326 | `			if( apNode[iNode] ){` |
|   409987 | 1327 | `				if( sArgName.nByte > 0 ){` |
|      188 | 1328 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      188 | 1329 | `					apNode[iNode]->sArgName = sArgName;` |
|       92 | 1330 | `				}` |
|        - | 1331 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   409987 | 1332 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   204996 | 1333 | `			}else{` |
|        - | 1334 | `				/* No expression before comma */` |
|      ! 0 | 1335 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1336 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1337 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1338 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1339 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1340 | `				}` |
|      ! 0 | 1341 | `				return rc;` |
|        - | 1342 | `			}` |
|   204996 | 1343 | `		}else{` |
|        - | 1344 | `			/* Comma with no preceding argument */` |
|        8 | 1345 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        8 | 1346 | `			if( rc != SXERR_ABORT ){` |
|        8 | 1347 | `				rc = SXERR_SYNTAX;` |
|        3 | 1348 | `			}` |
|        8 | 1349 | `			return rc;` |
|        - | 1350 | `		}` |
|        - | 1351 | `		/* Jump trailing comma */` |
|   409987 | 1352 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    47821 | 1353 | `			iCur++;` |
|    47821 | 1354 | `			if( iCur >= nToken ){` |
|        - | 1355 | `				/* Trailing comma after last argument */` |
|       19 | 1356 | `				break;` |
|        - | 1357 | `			}` |
|    23899 | 1358 | `		}` |
|        5 | 1359 | `	}` |
|   362189 | 1360 | `	return SXRET_OK;` |
|   181101 | 1361 |  |
|        - | 1362 | ` /*` |
|        - | 1363 | `  * Create an expression tree from an array of tokens.` |
|        - | 1364 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1365 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1366 | `  */` |
|  1454358 | 1367 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1368 | ` {` |
|        - | 1369 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1370 | `	 ph7_expr_node *pNode;` |
|        - | 1371 | `	 sxi32 iCur;` |
|        - | 1372 | `	 sxi32 rc;` |
|  1454363 | 1373 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1374 | `		 /* TICKET 1433-17: self evaluating node */` |
|   658645 | 1375 | `		 return SXRET_OK;` |
|        - | 1376 | `	 }` |
|        - | 1377 | `	 /* Process expressions enclosed in parenthesis first */` |
|  4886575 | 1378 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1379 | `		 sxi32 iNest;` |
|        - | 1380 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1381 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1382 | `		  */` |
|  4090859 | 1383 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4060837 | 1384 | `			 continue;` |
|        - | 1385 | `		 }` |
|    30027 | 1386 | `		 iNest = 1;` |
|    30027 | 1387 | `		 iLeft = iCur;` |
|        - | 1388 | `		 /* Find the closing parenthesis */` |
|    30027 | 1389 | `		 iCur++;` |
|   200323 | 1390 | `		 while( iCur < nToken ){` |
|   200323 | 1391 | `			 if( apNode[iCur] ){` |
|   200323 | 1392 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1393 | `					 /* Decrement nesting level */` |
|    52107 | 1394 | `					 iNest--;` |
|    52107 | 1395 | `					 if( iNest <= 0 ){` |
|    30027 | 1396 | `						 break;` |
|        5 | 1397 | `					 }` |
|   159261 | 1398 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1399 | `					 /* Increment nesting level */` |
|    22085 | 1400 | `					 iNest++;` |
|    11040 | 1401 | `				 }` |
|    85148 | 1402 | `			 }` |
|   170301 | 1403 | `			 iCur++;` |
|        5 | 1404 | `		 }` |
|    30027 | 1405 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1406 | `			 sxi32 j;` |
|        - | 1407 | `			 /* Recurse and process this expression */` |
|    30027 | 1408 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    30027 | 1409 | `			 if( rc != SXRET_OK ){` |
|        3 | 1410 | `				 return rc;` |
|        - | 1411 | `			 }` |
|        - | 1412 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1413 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1414 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    30025 | 1415 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    30025 | 1416 | `				 if( apNode[j] ){` |
|    30025 | 1417 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    30025 | 1418 | `					 break;` |
|        - | 1419 | `				 }` |
|      ! 0 | 1420 | `			 }` |
|    15010 | 1421 | `		 }` |
|        - | 1422 | `		 /* Free the left and right nodes */` |
|    30025 | 1423 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    30025 | 1424 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    30025 | 1425 | `		 apNode[iLeft] = 0;` |
|    30025 | 1426 | `		 apNode[iCur] = 0;` |
|    15015 | 1427 | `	 }` |
|        - | 1428 | `	  /* Process expressions enclosed in braces */` |
|  5079163 | 1429 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1430 | `		 sxi32 iNest;` |
|        - | 1431 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1432 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1433 | `		  */` |
|  4291159 | 1434 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4291155 | 1435 | `			 continue;` |
|        - | 1436 | `		 }` |
|        6 | 1437 | `		 iNest = 1;` |
|        6 | 1438 | `		 iLeft = iCur;` |
|        - | 1439 | `		 /* Find the closing parenthesis */` |
|        6 | 1440 | `		 iCur++;` |
|        8 | 1441 | `		 while( iCur < nToken ){` |
|        8 | 1442 | `			 if( apNode[iCur] ){` |
|        8 | 1443 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1444 | `					 /* Decrement nesting level */` |
|        6 | 1445 | `					 iNest--;` |
|        6 | 1446 | `					 if( iNest <= 0 ){` |
|        6 | 1447 | `						 break;` |
|      ! 0 | 1448 | `					 }` |
|        3 | 1449 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1450 | `					 /* Increment nesting level */` |
|      ! 0 | 1451 | `					 iNest++;` |
|      ! 0 | 1452 | `				 }` |
|        1 | 1453 | `			 }` |
|        3 | 1454 | `			 iCur++;` |
|        1 | 1455 | `		 }` |
|        6 | 1456 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1457 | `			 /* Recurse and process this expression */` |
|        3 | 1458 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        3 | 1459 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1460 | `				 return rc;` |
|        - | 1461 | `			 }` |
|        1 | 1462 | `		 }` |
|        - | 1463 | `		 /* Free the left and right nodes */` |
|        6 | 1464 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        6 | 1465 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        6 | 1466 | `		 apNode[iLeft] = 0;` |
|        6 | 1467 | `		 apNode[iCur] = 0;` |
|        4 | 1468 | `	 }` |
|        - | 1469 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   788009 | 1470 | `	 iLeft = -1;` |
|  5079133 | 1471 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4291141 | 1472 | `		 if( apNode[iCur] == 0 ){` |
|  1638097 | 1473 | `			 continue;` |
|        - | 1474 | `		 }` |
|  2653049 | 1475 | `		 pNode = apNode[iCur];` |
|  2653049 | 1476 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   702035 | 1477 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1478 | `				 /* Collect function arguments */` |
|   418377 | 1479 | `				 sxi32 iPtr = 0;` |
|   418377 | 1480 | `				 sxi32 nFuncTok = 0;` |
|  1498643 | 1481 | `				 while( nFuncTok + iCur < nToken ){` |
|  1498643 | 1482 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1484547 | 1483 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   433377 | 1484 | `							 iPtr++;` |
|  1267861 | 1485 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   433377 | 1486 | `							 iPtr--;` |
|   433377 | 1487 | `							 if( iPtr <= 0 ){` |
|   418377 | 1488 | `								 break;` |
|        - | 1489 | `							 }` |
|     7500 | 1490 | `						 }` |
|   533085 | 1491 | `					 }` |
|  1080271 | 1492 | `					 nFuncTok++;` |
|        5 | 1493 | `				 }` |
|   418377 | 1494 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1495 | `					 /* Syntax error */` |
|      ! 0 | 1496 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1497 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1498 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1499 | `					 }` |
|      ! 0 | 1500 | `					 return rc;` |
|        - | 1501 | `				 }` |
|   418377 | 1502 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1503 | `					 /* Syntax error */` |
|      ! 0 | 1504 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1505 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1506 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1507 | `					 }` |
|      ! 0 | 1508 | `					 return rc;` |
|        - | 1509 | `				 }` |
|   418377 | 1510 | `				 if( nFuncTok > 1 ){` |
|        - | 1511 | `					 /* Process function arguments */` |
|   362197 | 1512 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   362197 | 1513 | `					 if( rc != SXRET_OK ){` |
|       10 | 1514 | `						 return rc;` |
|        - | 1515 | `					 }` |
|   181092 | 1516 | `				 }` |
|        - | 1517 | `				 /* Link the node to the tree */` |
|   418369 | 1518 | `				 pNode->pLeft = apNode[iLeft];` |
|   418369 | 1519 | `				 apNode[iLeft] = 0;` |
|  1498611 | 1520 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1080247 | 1521 | `					 apNode[iCur+iPtr] = 0;` |
|   540126 | 1522 | `				 }` |
|   492845 | 1523 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1524 | `				 /* Subscripting */` |
|    90689 | 1525 | `				 sxi32 iArrTok = iCur + 1;` |
|    90689 | 1526 | `				 sxi32 iNest = 1;` |
|    90684 | 1527 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1528 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1529 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|      214 | 1530 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    90684 | 1531 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1532 | `						 /* Syntax error */` |
|      ! 0 | 1533 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1534 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1535 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1536 | `						 }` |
|      ! 0 | 1537 | `						 return rc;` |
|        - | 1538 | `				 }` |
|        - | 1539 | `				 /* Collect index tokens */` |
|   163781 | 1540 | `				 while( iArrTok < nToken ){` |
|   163781 | 1541 | `					 if( apNode[iArrTok] ){` |
|   163749 | 1542 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1543 | `							 /* Increment nesting level */` |
|      ! 0 | 1544 | `							 iNest++;` |
|   163749 | 1545 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1546 | `							 /* Decrement nesting level */` |
|    90689 | 1547 | `							 iNest--;` |
|    90689 | 1548 | `							 if( iNest <= 0 ){` |
|    90689 | 1549 | `								 break;` |
|        - | 1550 | `							 }` |
|      ! 0 | 1551 | `						 }` |
|    36530 | 1552 | `					 }` |
|    73097 | 1553 | `					 ++iArrTok;` |
|        5 | 1554 | `				 }` |
|    90689 | 1555 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1556 | `					 /* Recurse and process this expression */` |
|    72975 | 1557 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    72975 | 1558 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1559 | `						 return rc;` |
|        - | 1560 | `					 }` |
|        - | 1561 | `					 /* Link the node to it's index */` |
|    72975 | 1562 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    36485 | 1563 | `				 }` |
|        - | 1564 | `				 /* Link the node to the tree */` |
|    90689 | 1565 | `				 pNode->pLeft = apNode[iLeft];` |
|    90689 | 1566 | `				 pNode->pRight = 0;` |
|    90689 | 1567 | `				 apNode[iLeft] = 0;` |
|   254465 | 1568 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   163781 | 1569 | `					 apNode[iNest] = 0;` |
|    81893 | 1570 | `				 }` |
|    45347 | 1571 | `			 }else{` |
|        - | 1572 | `				 /* Member access operators [i.e: '->','::'] */` |
|   192979 | 1573 | `				  iRight = iCur + 1;` |
|   192981 | 1574 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        3 | 1575 | `					 iRight++;` |
|        1 | 1576 | `				 }` |
|   192979 | 1577 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1578 | `					 /* Syntax error */` |
|        5 | 1579 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1580 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1581 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1582 | `					 }` |
|        5 | 1583 | `					 return rc;` |
|        - | 1584 | `				 }` |
|        - | 1585 | `				 /* Link the node to the tree */` |
|   192975 | 1586 | `				 pNode->pLeft = apNode[iLeft];` |
|   192970 | 1587 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   192830 | 1588 | `					 && pNode->pLeft->pOp == 0 &&` |
|   192590 | 1589 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1590 | `						 /* Syntax error */` |
|      ! 0 | 1591 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1592 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1593 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1594 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1595 | `						 }` |
|      ! 0 | 1596 | `						 return rc;` |
|        - | 1597 | `				 }` |
|   192975 | 1598 | `				 pNode->pRight = apNode[iRight];` |
|   192975 | 1599 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1600 | `			 }` |
|   351009 | 1601 | `		 }` |
|  2653037 | 1602 | `		 iLeft = iCur;` |
|  1326521 | 1603 | `	 }` |
|        - | 1604 | `	 /* Handle left associative (new, clone) operators */` |
|  5079101 | 1605 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4291109 | 1606 | `		 if( apNode[iCur] == 0 ){` |
|  2359155 | 1607 | `			 continue;` |
|        - | 1608 | `		 }` |
|  1931959 | 1609 | `		 pNode = apNode[iCur];` |
|  1931959 | 1610 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1611 | `			 SyToken *pToken;` |
|        - | 1612 | `			 /* Get the left node */` |
|    19045 | 1613 | `			 iLeft = iCur + 1;` |
|    37903 | 1614 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    18863 | 1615 | `				 iLeft++;` |
|        5 | 1616 | `			 }` |
|    19045 | 1617 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1618 | `				  /* Syntax error */` |
|      ! 0 | 1619 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1620 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1621 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1622 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1623 | `				 }` |
|      ! 0 | 1624 | `				 return rc;` |
|        - | 1625 | `			 }` |
|        - | 1626 | `			 /* Make sure the operand are of a valid type */` |
|    19045 | 1627 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1628 | `				 /* Clone:` |
|        - | 1629 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1630 | `				  *  ++ function call (including annonymous)` |
|        - | 1631 | `				  *  ++ array member` |
|        - | 1632 | `				  *  ++ 'new' operator` |
|        - | 1633 | `				  * Example:` |
|        - | 1634 | `				  *   clone $pObj;` |
|        - | 1635 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1636 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1637 | `				  */` |
|       30 | 1638 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       28 | 1639 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1640 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1641 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1642 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1643 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1644 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1645 | `						 }` |
|      ! 0 | 1646 | `						 return rc;` |
|        - | 1647 | `					 }` |
|       12 | 1648 | `				 }` |
|       17 | 1649 | `			 }else{` |
|        - | 1650 | `				 /* New */` |
|    19019 | 1651 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      163 | 1652 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      158 | 1653 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       31 | 1654 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1655 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1656 | `						 /* Syntax error */` |
|      ! 0 | 1657 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1658 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1659 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1660 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1661 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1662 | `						 }` |
|      ! 0 | 1663 | `						 return rc;` |
|        - | 1664 | `					 }` |
|       79 | 1665 | `				 }` |
|        - | 1666 | `			 }` |
|        - | 1667 | `			  /* Link the node to the tree */` |
|    19045 | 1668 | `			 pNode->pLeft = apNode[iLeft];` |
|    19045 | 1669 | `			 apNode[iLeft] = 0;` |
|    19045 | 1670 | `			 pNode->pRight = 0; /* Paranoid */` |
|     9520 | 1671 | `		 }` |
|   965982 | 1672 | `	 }` |
|        - | 1673 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   787997 | 1674 | `	 iLeft = -1;` |
|  5082957 | 1675 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4291109 | 1676 | `		 if( apNode[iCur] == 0 ){` |
|  2359155 | 1677 | `			 continue;` |
|        - | 1678 | `		 }` |
|  1931959 | 1679 | `		 pNode = apNode[iCur];` |
|  1931959 | 1680 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    10639 | 1681 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3892 | 1682 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1683 | `					 /* Link the node to the tree */` |
|     3903 | 1684 | `					 pNode->pLeft = apNode[iLeft];` |
|     3903 | 1685 | `					 apNode[iLeft] = 0;` |
|     1949 | 1686 | `			 }` |
|     7245 | 1687 | `		  }` |
|  1935815 | 1688 | `		 iLeft = iCur;` |
|   969838 | 1689 | `	  }` |
|   791853 | 1690 | `	 iLeft = -1;` |
|  5082957 | 1691 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4291109 | 1692 | `		 if( apNode[iCur] == 0 ){` |
|  2363053 | 1693 | `			 continue;` |
|        - | 1694 | `		 }` |
|  1928061 | 1695 | `		 pNode = apNode[iCur];` |
|  1928061 | 1696 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    10592 | 1697 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    10597 | 1698 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1699 | `					 /* Syntax error */` |
|      ! 0 | 1700 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1701 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1702 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1703 | `					 }` |
|      ! 0 | 1704 | `					 return rc;` |
|        - | 1705 | `			 }` |
|        - | 1706 | `			 /* Link the node to the tree */` |
|    10597 | 1707 | `			 pNode->pLeft = apNode[iLeft];` |
|    10597 | 1708 | `			 apNode[iLeft] = 0;` |
|        - | 1709 | `			 /* Mark as pre-increment/decrement node */` |
|    10597 | 1710 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5296 | 1711 | `		  }` |
|  1928061 | 1712 | `		 iLeft = iCur;` |
|   964033 | 1713 | `	 }` |
|        - | 1714 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   791853 | 1715 | `	  iLeft = 0;` |
|  5082951 | 1716 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4291105 | 1717 | `		  if( apNode[iCur] ){` |
|  1917465 | 1718 | `			  pNode = apNode[iCur];` |
|  1917465 | 1719 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    47369 | 1720 | `				  if( iLeft > 0 ){` |
|        - | 1721 | `					  /* Link the node to the tree */` |
|    47367 | 1722 | `					  pNode->pLeft = apNode[iLeft];` |
|    47367 | 1723 | `					  apNode[iLeft] = 0;` |
|    47367 | 1724 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1725 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1726 | `							   /* Syntax error */` |
|      ! 0 | 1727 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1728 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1729 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1730 | `							  }` |
|      ! 0 | 1731 | `							  return rc;` |
|        - | 1732 | `						  }` |
|       36 | 1733 | `					  }` |
|    23686 | 1734 | `				  }else{` |
|        - | 1735 | `					  /* Syntax error */` |
|        3 | 1736 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1737 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1738 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1739 | `					  }` |
|        3 | 1740 | `					  return rc;` |
|        - | 1741 | `				  }` |
|    23681 | 1742 | `			  }` |
|        - | 1743 | `			  /* Save terminal position */` |
|  1917463 | 1744 | `			  iLeft = iCur;` |
|   958729 | 1745 | `		  }` |
|  2145554 | 1746 | `	  }` |
|        - | 1747 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1748 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1749 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1750 | `	  * yielding a right-leaning tree. */` |
|  5082949 | 1751 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4291103 | 1752 | `		 if( apNode[iCur] == 0 ){` |
|  2421119 | 1753 | `			 continue;` |
|        - | 1754 | `		 }` |
|  1869989 | 1755 | `		 pNode = apNode[iCur];` |
|  1869989 | 1756 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1757 | `			 sxi32 iL, iR;` |
|        - | 1758 | `			 /* Find the right operand */` |
|      113 | 1759 | `			 iR = -1;` |
|        - | 1760 | `			 {` |
|        - | 1761 | `				 sxi32 j;` |
|      125 | 1762 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1763 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1764 | `				 }` |
|        - | 1765 | `			 }` |
|        - | 1766 | `			 /* Find the left operand */` |
|      113 | 1767 | `			 iL = -1;` |
|        - | 1768 | `			 {` |
|        - | 1769 | `				 sxi32 j;` |
|      181 | 1770 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1771 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1772 | `				 }` |
|        - | 1773 | `			 }` |
|      113 | 1774 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1775 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1776 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1777 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1778 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1779 | `				 }` |
|      ! 0 | 1780 | `				 return rc;` |
|        - | 1781 | `			 }` |
|      113 | 1782 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1783 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1784 | `			 apNode[iL] = 0;` |
|      113 | 1785 | `			 apNode[iR] = 0;` |
|        - | 1786 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1787 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1788 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1789 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1790 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1791 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1792 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1793 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1794 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1795 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1796 | `			  * operands are respected. */` |
|      112 | 1797 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1798 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1799 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1800 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1801 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1802 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1803 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1804 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1805 | `				 while( pTail->pLeft` |
|       34 | 1806 | `					 && pTail->pLeft->pOp` |
|       23 | 1807 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1808 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1809 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1810 | `					 pTail = pTail->pLeft;` |
|        1 | 1811 | `				 }` |
|        - | 1812 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1813 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1814 | `				 pTail->pLeft = pNode;` |
|       27 | 1815 | `				 apNode[iCur] = pHead;` |
|       13 | 1816 | `			 }` |
|       56 | 1817 | `		 }` |
|   934997 | 1818 | `	 }` |
|        - | 1819 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  8710225 | 1820 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  7918389 | 1821 | `		 iLeft = -1;` |
| 50829075 | 1822 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 42910701 | 1823 | `			 if( apNode[iCur] == 0 ){` |
| 27402511 | 1824 | `				 continue;` |
|        - | 1825 | `			 }` |
| 15508195 | 1826 | `			 pNode = apNode[iCur];` |
| 15508195 | 1827 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1828 | `				 /* Get the right node */` |
|   238561 | 1829 | `				 iRight = iCur + 1;` |
|   340815 | 1830 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   102259 | 1831 | `					 iRight++;` |
|        5 | 1832 | `				 }` |
|   238561 | 1833 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1834 | `					 /* Syntax error */` |
|       10 | 1835 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       10 | 1836 | `					 if( rc != SXERR_ABORT ){` |
|       10 | 1837 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1838 | `					 }` |
|       10 | 1839 | `					 return rc;` |
|        - | 1840 | `				 }` |
|   238553 | 1841 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1842 | `					 sxi32  iTmp;` |
|        - | 1843 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1844 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1845 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1846 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1847 | `					  * is swapped below. */` |
|       57 | 1848 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1849 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1850 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1851 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1852 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1853 | `						 }` |
|        3 | 1854 | `						 return rc;` |
|        - | 1855 | `					 }` |
|       54 | 1856 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1857 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1858 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1859 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1860 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1861 | `						 }` |
|      ! 0 | 1862 | `						 return rc;` |
|        - | 1863 | `					 }` |
|       54 | 1864 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       38 | 1865 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1866 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1867 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1868 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1869 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1870 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1871 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1872 | `									 }` |
|      ! 0 | 1873 | `									 return rc;` |
|        - | 1874 | `							 }` |
|      ! 0 | 1875 | `						 }` |
|       18 | 1876 | `					 }` |
|        - | 1877 | `					 /* Swap operands */` |
|       54 | 1878 | `					 iTmp = iRight;` |
|       54 | 1879 | `					 iRight = iLeft;` |
|       54 | 1880 | `					 iLeft = iTmp;` |
|       26 | 1881 | `				 }` |
|        - | 1882 | `				 /* Link the node to the tree */` |
|   238551 | 1883 | `				 pNode->pLeft = apNode[iLeft];` |
|   238551 | 1884 | `				 pNode->pRight = apNode[iRight];` |
|   238551 | 1885 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   119273 | 1886 | `			 }` |
| 15508185 | 1887 | `			 iLeft = iCur;` |
|  7754095 | 1888 | `		 }` |
|  3959192 | 1889 | `	 }` |
|        - | 1890 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1891 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1892 | `	  * we are dealing with a single operator.` |
|        - | 1893 | `	  */` |
|   791841 | 1894 | `	  iLeft = -1;` |
|  5071575 | 1895 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4282387 | 1896 | `		  if( apNode[iCur] == 0 ){` |
|  2897535 | 1897 | `			  continue;` |
|        - | 1898 | `		  }` |
|  1384857 | 1899 | `		  pNode = apNode[iCur];` |
|  1384857 | 1900 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2653 | 1901 | `			  sxi32 iNest = 1;` |
|     2653 | 1902 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1903 | `				  /* Missing condition */` |
|        3 | 1904 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1905 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1906 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1907 | `				  }` |
|        3 | 1908 | `				  return rc;` |
|        - | 1909 | `			  }` |
|        - | 1910 | `			  /* Get the right node */` |
|     2651 | 1911 | `			  iRight = iCur + 1;` |
|     5551 | 1912 | `			  while( iRight < nToken  ){` |
|     5551 | 1913 | `				  if( apNode[iRight] ){` |
|     5229 | 1914 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1915 | `						  /* Increment nesting level */` |
|      ! 0 | 1916 | `						  ++iNest;` |
|     5229 | 1917 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1918 | `						  /* Decrement nesting level */` |
|     2651 | 1919 | `						  --iNest;` |
|     2651 | 1920 | `						  if( iNest <= 0 ){` |
|     2651 | 1921 | `							  break;` |
|        - | 1922 | `						  }` |
|      ! 0 | 1923 | `					  }` |
|     1289 | 1924 | `				  }` |
|     2905 | 1925 | `				  iRight++;` |
|        5 | 1926 | `			  }` |
|     2651 | 1927 | `			  if( iRight > iCur + 1 ){` |
|        - | 1928 | `				  /* Recurse and process the then expression */` |
|     2583 | 1929 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2583 | 1930 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1931 | `					  return rc;` |
|        - | 1932 | `				  }` |
|        - | 1933 | `				  /* Link the node to the tree */` |
|     2583 | 1934 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1289 | 1935 | `			  }else{` |
|        - | 1936 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1937 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1938 | `			  }` |
|     2651 | 1939 | `			  apNode[iCur + 1] = 0;` |
|     2651 | 1940 | `			  if( iRight + 1 < nToken ){` |
|        - | 1941 | `				  /* Recurse and process the else expression */` |
|     2651 | 1942 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2651 | 1943 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1944 | `					  return rc;` |
|        - | 1945 | `				  }` |
|        - | 1946 | `				  /* Link the node to the tree */` |
|     2651 | 1947 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2651 | 1948 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1328 | 1949 | `			  }else{` |
|      ! 0 | 1950 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1951 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1952 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1953 | `				 }` |
|      ! 0 | 1954 | `				 return rc;` |
|        - | 1955 | `			  }` |
|        - | 1956 | `			  /* Point to the condition */` |
|     2651 | 1957 | `			  pNode->pCond  = apNode[iLeft];` |
|     2651 | 1958 | `			  apNode[iLeft] = 0;` |
|     2651 | 1959 | `			  break;` |
|        - | 1960 | `		  }` |
|  1382209 | 1961 | `		  iLeft = iCur;` |
|   691107 | 1962 | `	  }` |
|        - | 1963 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1964 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1965 | `	  * so there is no need for a precedence loop here.` |
|        - | 1966 | `	  */` |
|   791839 | 1967 | `	 iRight = -1;` |
|  5082753 | 1968 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4290973 | 1969 | `		 if( apNode[iCur] == 0 ){` |
|  3203895 | 1970 | `			 continue;` |
|        - | 1971 | `		 }` |
|  1087083 | 1972 | `		 pNode = apNode[iCur];` |
|  1087083 | 1973 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1974 | `			 /* Get the left node */` |
|   295127 | 1975 | `			 iLeft = iCur - 1;` |
|   430401 | 1976 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   135279 | 1977 | `				 iLeft--;` |
|        5 | 1978 | `			 }` |
|   295127 | 1979 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1980 | `				 /* Syntax error */` |
|       45 | 1981 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1982 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 1983 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1984 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 1985 | `				 }else{` |
|       41 | 1986 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1987 | `				 }` |
|       45 | 1988 | `				 if( rc != SXERR_ABORT ){` |
|       43 | 1989 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1990 | `				 }` |
|       45 | 1991 | `				 return rc;` |
|        - | 1992 | `			 }` |
|        - | 1993 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1994 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1995 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1996 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1997 | `			  * a write. */` |
|   295085 | 1998 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 1999 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 2000 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 2001 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 2002 | `					 rc = SXERR_SYNTAX;` |
|        4 | 2003 | `				 }` |
|       11 | 2004 | `				 return rc;` |
|        - | 2005 | `			 }` |
|   295077 | 2006 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      102 | 2007 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       72 | 2008 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2009 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2010 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2011 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2012 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2013 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2014 | `					 }else{` |
|        4 | 2015 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2016 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2017 | `					 }` |
|        6 | 2018 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2019 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2020 | `					 }` |
|        6 | 2021 | `					 return rc;` |
|        - | 2022 | `				 }` |
|       35 | 2023 | `			 }` |
|        - | 2024 | `			 /* Link the node to the tree (Reverse) */` |
|   295073 | 2025 | `			 pNode->pLeft = apNode[iRight];` |
|   295073 | 2026 | `			 pNode->pRight = apNode[iLeft];` |
|   295073 | 2027 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   147534 | 2028 | `		 }` |
|  1087029 | 2029 | `		 iRight = iCur;` |
|   543517 | 2030 | `	 }` |
|        - | 2031 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3958905 | 2032 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3167125 | 2033 | `		 iLeft = -1;` |
| 20330725 | 2034 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 17163605 | 2035 | `			 if( apNode[iCur] == 0 ){` |
| 13996079 | 2036 | `				 continue;` |
|        - | 2037 | `			 }` |
|  3167531 | 2038 | `			 pNode = apNode[iCur];` |
|  3167531 | 2039 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2040 | `				 /* Get the right node */` |
|       72 | 2041 | `				 iRight = iCur + 1;` |
|      110 | 2042 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2043 | `					 iRight++;` |
|        2 | 2044 | `				 }` |
|       72 | 2045 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2046 | `					 /* Syntax error */` |
|      ! 0 | 2047 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2048 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2049 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2050 | `					 }` |
|      ! 0 | 2051 | `					 return rc;` |
|        - | 2052 | `				 }` |
|        - | 2053 | `				 /* Link the node to the tree */` |
|       72 | 2054 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2055 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2056 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2057 | `			 }` |
|  3167531 | 2058 | `			 iLeft = iCur;` |
|  1583768 | 2059 | `		 }` |
|  1583565 | 2060 | `	 }` |
|        - | 2061 | `	 /* Point to the root of the expression tree */` |
|  4290877 | 2062 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3499115 | 2063 | `		 if( apNode[iCur] ){` |
|   719881 | 2064 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       22 | 2065 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       22 | 2066 | `				  if( rc != SXERR_ABORT ){` |
|       22 | 2067 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2068 | `				  }` |
|       22 | 2069 | `				  return rc;` |
|        - | 2070 | `			 }` |
|   719863 | 2071 | `			 apNode[0] = apNode[iCur];` |
|   719863 | 2072 | `			 apNode[iCur] = 0;` |
|   359929 | 2073 | `		 }` |
|  1749551 | 2074 | `	 }` |
|   791767 | 2075 | `	 return SXRET_OK;` |
|   725256 | 2076 | ` }` |
|        - | 2077 | ` /*` |
|        - | 2078 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2079 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2080 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2081 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2082 | `  */` |
|   932386 | 2083 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2084 |  |
|        - | 2085 | `	ph7_expr_node **apNode;` |
|        - | 2086 | `	ph7_expr_node *pNode;` |
|        - | 2087 | `	sxi32 rc;` |
|        - | 2088 | `	/* Reset node container */` |
|   932391 | 2089 | `	SySetReset(pExprNode);` |
|   932391 | 2090 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2091 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2092 | `	{` |
|   932391 | 2093 | `		int iLastWasTerm = 0;` |
|   932391 | 2094 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  5019331 | 2095 | `		while( pGen->pIn < pGen->pEnd ){` |
|  4086979 | 2096 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  4086979 | 2097 | `			if( rc != SXRET_OK ){` |
|       38 | 2098 | `				return rc;` |
|        - | 2099 | `			}` |
|        - | 2100 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  4086945 | 2101 | `			if( pNode->xCode ){` |
|        - | 2102 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2147601 | 2103 | `				iLastWasTerm = 1;` |
|  3013147 | 2104 | `			}else if( pNode->pOp ){` |
|        - | 2105 | `				/* Operator node */` |
|   948953 | 2106 | `				iLastWasTerm = 0;` |
|   474479 | 2107 | `			}else{` |
|        - | 2108 | `				/* Delimiter: ')' and ']' end terms */` |
|   990401 | 2109 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2110 | `			}` |
|        - | 2111 | `			/* A keyword in the next node is a member name only right after a member` |
|        - | 2112 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|        - | 2113 | `			 * node kind, so this single test covers all branches. */` |
|  4086945 | 2114 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|        - | 2115 | `			/* Save the extracted node */` |
|  4086945 | 2116 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2117 | `		}` |
|        - | 2118 | `	}` |
|   932357 | 2119 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2120 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2121 | `		*ppRoot = 0;` |
|      ! 0 | 2122 | `		return SXRET_OK;` |
|        - | 2123 | `	}` |
|   932357 | 2124 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2125 | `	/* Make sure we are dealing with valid nodes */` |
|   932357 | 2126 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   932357 | 2127 | `	if( rc != SXRET_OK ){` |
|        - | 2128 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2129 | `		 * cleanup the mess left behind.` |
|        - | 2130 | `		 */` |
|       54 | 2131 | `		*ppRoot = 0;` |
|       54 | 2132 | `		return rc;` |
|        - | 2133 | `	}` |
|        - | 2134 | `	/* Build the tree */` |
|   932307 | 2135 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   932307 | 2136 | `	if( rc != SXRET_OK ){` |
|        - | 2137 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2138 | `		*ppRoot = 0;` |
|      103 | 2139 | `		return rc;` |
|        - | 2140 | `	}` |
|        - | 2141 | `	/* Point to the root of the tree */` |
|   932209 | 2142 | `	*ppRoot = apNode[0];` |
|   932209 | 2143 | `	return SXRET_OK;` |
|   466198 | 2144 |  |
|        - | 2145 |  |
