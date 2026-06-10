# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1068/1229 lines (86.90%)

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
|   976366 |  268 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  269 |  |
|   976368 |  270 | `	sxu32 n = 0;` |
|        - |  271 | `	sxi32 rc;` |
|        - |  272 | `	/* Do a linear lookup on the operators table */` |
| 16390114 |  273 | `	for(;;){` |
| 32780230 |  274 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  275 | `			break;` |
|        - |  276 | `		}` |
| 32780230 |  277 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  278 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3849138 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1924570 |  280 | `		}else{` |
| 28931094 |  281 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  282 | `		}` |
| 32780230 |  283 | `		if( rc == 0 ){` |
|   980144 |  284 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  285 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   975990 |  286 | `				return &aOpTable[n];` |
|        - |  287 | `			}` |
|        - |  288 | `			/* Handle ambiguity */` |
|     4156 |  289 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  290 | `				/* Unary opertors have prcedence here over binary operators */` |
|      248 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|     3910 |  293 | `			if( pLast->nType & PH7_TK_OP ){` |
|      142 |  294 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  295 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      142 |  296 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  297 | `					/* Unary opertors have prcedence here over binary operators */` |
|      134 |  298 | `					return &aOpTable[n];` |
|        - |  299 | `				}` |
|        - |  300 |  |
|        4 |  301 | `			}` |
|     1888 |  302 | `		}` |
| 31803864 |  303 | `		++n; /* Next operator in the table */` |
|        2 |  304 | `	}` |
|        - |  305 | `	/* No such operator */` |
|      ! 0 |  306 | `	return 0;` |
|   488185 |  307 |  |
|        - |  308 | `/*` |
|        - |  309 | ` * Delimit a set of token stream.` |
|        - |  310 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  311 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  312 | ` */` |
|   552870 |  313 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  314 |  |
|   552872 |  315 | `	SyToken *pCur = pIn;` |
|   552872 |  316 | `	sxi32 iNest = 1;` |
|  3202719 |  317 | `	for(;;){` |
|  6405440 |  318 | `		if( pCur >= pEnd ){` |
|      168 |  319 | `			break;` |
|        - |  320 | `		}` |
|  6405274 |  321 | `		if( pCur->nType & nTokStart ){` |
|        - |  322 | `			/* Increment nesting level */` |
|   319208 |  323 | `			iNest++;` |
|  6245671 |  324 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  325 | `			/* Decrement nesting level */` |
|   871912 |  326 | `			iNest--;` |
|   871912 |  327 | `			if( iNest <= 0 ){` |
|   552706 |  328 | `				break;` |
|        - |  329 | `			}` |
|   159603 |  330 | `		}` |
|        - |  331 | `		/* Advance cursor */` |
|  5852570 |  332 | `		pCur++;` |
|        2 |  333 | `	}` |
|        - |  334 | `	/* Point to the end of the chunk */` |
|   552872 |  335 | `	*ppEnd = pCur;` |
|   552872 |  336 |  |
|        - |  337 | `/*` |
|        - |  338 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  339 | ` * Note on reserved keywords.` |
|        - |  340 | ` *  According to the PHP language reference manual:` |
|        - |  341 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  342 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  343 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  344 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  345 | ` */` |
|    19552 |  346 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  347 |  |
|    29260 |  348 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    19457 |  349 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  350 | `		){` |
|      150 |  351 | `			return TRUE;` |
|        - |  352 | `	}` |
|    19406 |  353 | `	if( bCheckFunc ){` |
|       98 |  354 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       72 |  355 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       57 |  356 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  357 | `				return TRUE;` |
|        - |  358 | `		}` |
|       22 |  359 | `	}` |
|        - |  360 | `	/* Not a language construct */` |
|    19374 |  361 | `	return FALSE;` |
|     9778 |  362 |  |
|        - |  363 | `/*` |
|        - |  364 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  365 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  366 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  367 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  368 | ` */` |
|   825276 |  369 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  370 |  |
|        - |  371 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  372 | `	sxi32 i,rc;` |
|        - |  373 |  |
|   825278 |  374 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  375 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       28 |  376 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       28 |  377 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       13 |  378 | `	}` |
|   825278 |  379 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  4445230 |  380 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3619988 |  381 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  382 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      526 |  383 | `			continue;` |
|        - |  384 | `		}` |
|  3619464 |  385 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   396874 |  386 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    19870 |  387 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  388 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   370266 |  389 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  390 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  391 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  392 | `						 */` |
|   370266 |  393 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   370266 |  394 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   370266 |  395 | `						apNode[i]->pOp = &sFCallOp;` |
|   185132 |  396 | `					}` |
|   185132 |  397 | `			}` |
|   396874 |  398 | `			iParen++;` |
|  3421028 |  399 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   396874 |  400 | `			if( iParen <= 0 ){` |
|       13 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  402 | `				if( rc != SXERR_ABORT ){` |
|       13 |  403 | `					rc = SXERR_SYNTAX;` |
|        6 |  404 | `				}` |
|       13 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|   396862 |  407 | `			iParen--;` |
|  3024150 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    80312 |  409 | `			iSquare++;` |
|  2785565 |  410 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    80326 |  411 | `			if( iSquare <= 0 ){` |
|        7 |  412 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  413 | `				if( rc != SXERR_ABORT ){` |
|        7 |  414 | `					rc = SXERR_SYNTAX;` |
|        3 |  415 | `				}` |
|        7 |  416 | `				return rc;` |
|        - |  417 | `			}` |
|    80320 |  418 | `			iSquare--;` |
|  2705245 |  419 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2665079 |  466 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       18 |  467 | `			if( iBraces <= 0 ){` |
|       13 |  468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  469 | `				if( rc != SXERR_ABORT ){` |
|       13 |  470 | `					rc = SXERR_SYNTAX;` |
|        6 |  471 | `				}` |
|       13 |  472 | `				return rc;` |
|        - |  473 | `			}` |
|        6 |  474 | `			iBraces--;` |
|  2665058 |  475 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2590 |  476 | `			if( iQuesty > 0 ){` |
|     2400 |  477 | `				iQuesty--;` |
|     1391 |  478 | `			}else if( iParen <= 0 ){` |
|        - |  479 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  480 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  481 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  482 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  483 | `				if( rc != SXERR_ABORT ){` |
|        5 |  484 | `					rc = SXERR_SYNTAX;` |
|        2 |  485 | `				}` |
|        5 |  486 | `				return rc;` |
|        2 |  487 | `			}` |
|  2663760 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   760680 |  489 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   760680 |  490 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2402 |  491 | `				iQuesty++;` |
|   759480 |  492 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   380339 |  512 | `		}` |
|  1809716 |  513 | `	}` |
|   825244 |  514 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  516 | `		if( rc != SXERR_ABORT ){` |
|       17 |  517 | `			rc = SXERR_SYNTAX;` |
|        8 |  518 | `		}` |
|       17 |  519 | `		return rc;` |
|        - |  520 | `	}` |
|   825228 |  521 | `	return SXRET_OK;` |
|   412640 |  522 |  |
|        - |  523 | `/*` |
|        - |  524 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  525 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  526 | ` */` |
|   687544 |  527 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  528 |  |
|   687546 |  529 | `	SyToken *pIn = *ppCur;` |
|        - |  530 | `	/* Jump the first literal seen */` |
|   687546 |  531 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   687512 |  532 | `		pIn++;` |
|   343755 |  533 | `	}` |
|   343813 |  534 | `	for(;;){` |
|   687628 |  535 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       84 |  536 | `			pIn++;` |
|       84 |  537 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       82 |  538 | `				pIn++;` |
|       40 |  539 | `			}` |
|       43 |  540 | `		}else{` |
|   343774 |  541 | `			break;` |
|        - |  542 | `		}` |
|        2 |  543 | `	}` |
|        - |  544 | `	/* Synchronize pointers */` |
|   687546 |  545 | `	*ppCur = pIn;` |
|   687546 |  546 |  |
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
|      220 |  580 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  581 |  |
|      222 |  582 | `	SyToken *pIn = *ppCur;` |
|        - |  583 | `	sxu32 nLine;` |
|        - |  584 | `	sxi32 rc;` |
|        - |  585 | `	/* Jump the 'function' keyword */` |
|      222 |  586 | `	nLine = pIn->nLine;` |
|      222 |  587 | `	pIn++;` |
|      222 |  588 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  589 | `		pIn++;` |
|        1 |  590 | `	}` |
|      222 |  591 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  592 | `		/* Syntax error */` |
|        5 |  593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  594 | `		if( rc != SXERR_ABORT ){` |
|        5 |  595 | `			rc = SXERR_SYNTAX;` |
|        2 |  596 | `		}` |
|        5 |  597 | `		goto Synchronize;` |
|        - |  598 | `	}` |
|      218 |  599 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      218 |  600 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      218 |  601 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  602 | `		/* Syntax error */` |
|        5 |  603 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  604 | `		if( rc != SXERR_ABORT ){` |
|        5 |  605 | `			rc = SXERR_SYNTAX;` |
|        2 |  606 | `		}` |
|        5 |  607 | `		goto Synchronize;` |
|        - |  608 | `	}` |
|      214 |  609 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  610 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      214 |  611 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      214 |  638 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
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
|      198 |  671 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      198 |  672 | `		pIn++; /* Jump the leading curly '{' */` |
|      198 |  673 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      198 |  674 | `		if( pIn < pEnd ){` |
|      198 |  675 | `			pIn++;` |
|       98 |  676 | `		}` |
|      100 |  677 | `	}else{` |
|        - |  678 | `		/* Syntax error */` |
|      ! 0 |  679 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  680 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|        - |  683 | `	}` |
|      198 |  684 | `	rc = SXRET_OK;` |
|      110 |  685 | `Synchronize:` |
|        - |  686 | `	/* Synchronize pointers */` |
|      222 |  687 | `	*ppCur = pIn;` |
|      222 |  688 | `	return rc;` |
|      112 |  689 |  |
|        - |  690 | `/*` |
|        - |  691 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  692 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  693 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  694 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  695 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  696 | ` */` |
|       88 |  697 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  698 |  |
|       90 |  699 | `	SyToken *pIn = *ppCur;` |
|        - |  700 | `	sxu32 nLine;` |
|        - |  701 | `	sxi32 rc;` |
|        - |  702 | `	int iNest;` |
|       90 |  703 | `	nLine = pIn->nLine;` |
|        - |  704 | `	/* Optional 'static' prefix */` |
|       88 |  705 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       90 |  706 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  707 | `		pIn++;` |
|        1 |  708 | `	}` |
|        - |  709 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       88 |  710 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       90 |  711 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  712 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  713 | `		goto Synchronize;` |
|        - |  714 | `	}` |
|       90 |  715 | `	pIn++; /* Jump 'fn' */` |
|       44 |  716 | `	SXUNUSED(nLine);` |
|       44 |  717 | `	SXUNUSED(pGen);` |
|        - |  718 | `	/* Optional '&' for return-by-reference */` |
|       90 |  719 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  720 | `		pIn++;` |
|      ! 0 |  721 | `	}` |
|        - |  722 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  723 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  724 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  725 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       90 |  726 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       88 |  727 | `		pIn++; /* '(' */` |
|       88 |  728 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       88 |  729 | `		if( pIn < pEnd ){` |
|       86 |  730 | `			pIn++; /* ')' */` |
|       42 |  731 | `		}` |
|       43 |  732 | `	}` |
|        - |  733 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|       90 |  734 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       90 |  760 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       84 |  761 | `		pIn++;` |
|       41 |  762 | `	}` |
|        - |  763 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       90 |  764 | `	iNest = 0;` |
|      596 |  765 | `	while( pIn < pEnd ){` |
|      514 |  766 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  767 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  768 | `			break;` |
|        - |  769 | `		}` |
|      508 |  770 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       23 |  771 | `			iNest++;` |
|      497 |  772 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       23 |  773 | `			iNest--;` |
|       11 |  774 | `		}` |
|      508 |  775 | `		pIn++;` |
|        2 |  776 | `	}` |
|       90 |  777 | `	rc = SXRET_OK;` |
|       44 |  778 | `Synchronize:` |
|       90 |  779 | `	*ppCur = pIn;` |
|       90 |  780 | `	return rc;` |
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
|  3620204 |  831 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  832 |  |
|        - |  833 | `	ph7_expr_node *pNode;` |
|        - |  834 | `	SyToken *pCur;` |
|        - |  835 | `	sxi32 rc;` |
|        - |  836 | `	/* Allocate a new node */` |
|  3620206 |  837 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3620206 |  838 | `	if( pNode == 0 ){` |
|        - |  839 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  840 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  841 | `		 */` |
|      ! 0 |  842 | `		return SXERR_MEM;` |
|        - |  843 | `	}` |
|        - |  844 | `	/* Zero the structure */` |
|  3620206 |  845 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3620206 |  846 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  847 | `	/* Point to the head of the token stream */` |
|  3620206 |  848 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  849 | `	/* Start collecting tokens */` |
|  3620206 |  850 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  851 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  852 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       70 |  853 | `		pCur++;` |
|       70 |  854 | `		pGen->pIn = pCur;` |
|       70 |  855 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       70 |  856 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       70 |  857 | `		if( rc == SXRET_OK && *ppNode ){` |
|       70 |  858 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       34 |  859 | `		}` |
|       70 |  860 | `		return rc;` |
|        - |  861 | `	}` |
|  3620138 |  862 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  863 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  864 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  865 | `		 */` |
|      528 |  866 | `		pCur++; /* Skip the opening '[' */` |
|      528 |  867 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      528 |  868 | `		if( pCur < pGen->pEnd ){` |
|      528 |  869 | `			pCur++; /* Skip past the closing ']' */` |
|      265 |  870 | `		}else{` |
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
|      551 |  882 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  883 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  884 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  885 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  886 | `			}else{` |
|       19 |  887 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  888 | `			}` |
|       25 |  889 | `		}else{` |
|      482 |  890 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  891 | `		}` |
|  3619875 |  892 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  893 | `		/* Point to the instance that describe this operator */` |
|   841024 |  894 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  895 | `		/* Advance the stream cursor */` |
|   841024 |  896 | `		pCur++;` |
|  3199101 |  897 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  898 | `		/* Isolate variable */` |
|  1956266 |  899 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   978140 |  900 | `			pCur++; /* Variable variable */` |
|        2 |  901 | `		}` |
|   978128 |  902 | `		if( pCur < pGen->pEnd ){` |
|   978128 |  903 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  904 | `				/* Variable name */` |
|   978100 |  905 | `				pCur++;` |
|   489079 |  906 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   489061 |  921 | `		}` |
|   978124 |  922 | `		pNode->xCode = PH7_CompileVariable;` |
|  2289525 |  923 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    46680 |  924 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    46680 |  925 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  926 | `			 /* List/Array node */` |
|    26820 |  927 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  928 | `				 /* Assume a literal */` |
|       17 |  929 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  930 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  931 | `			 }else{` |
|    26804 |  932 | `				 pCur += 2;` |
|        - |  933 | `				 /* Collect array/list tokens */` |
|    26804 |  934 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    26804 |  935 | `				 if( pCur < pGen->pEnd ){` |
|    26802 |  936 | `					 pCur++;` |
|    13402 |  937 | `				 }else{` |
|        - |  938 | `					 /* Syntax error */` |
|        4 |  939 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  940 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  941 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  942 | `						 rc = SXERR_SYNTAX;` |
|        1 |  943 | `					 }` |
|        3 |  944 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  945 | `					 return rc;` |
|        - |  946 | `				 }` |
|    26802 |  947 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    26802 |  948 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    33269 |  961 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  962 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  963 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  964 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  965 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  966 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  967 | `			 pNode->xCode = PH7_CompileYield;` |
|    19845 |  968 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  969 | `			 /* Annonymous function */` |
|      222 |  970 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  971 | `				 /* Assume a literal */` |
|      ! 0 |  972 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  973 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  974 | `			 }else{` |
|        - |  975 | `				 /* Assemble annonymous functions body */` |
|      222 |  976 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      222 |  977 | `				 if( rc != SXRET_OK ){` |
|       25 |  978 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  979 | `					 return rc;` |
|        - |  980 | `				 }` |
|      198 |  981 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  982 | `			  }` |
|    19707 |  983 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    19565 |  984 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  985 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  986 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  987 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       90 |  988 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       90 |  989 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  990 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  991 | `				 return rc;` |
|        - |  992 | `			 }` |
|       90 |  993 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    19564 |  994 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - |  995 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       72 |  996 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       72 |  997 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  998 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  999 | `				 return rc;` |
|        - | 1000 | `			 }` |
|       72 | 1001 | `			 pNode->xCode = PH7_CompileMatch;` |
|    19485 | 1002 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1003 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1004 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1005 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1006 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1007 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1008 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1009 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1010 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    19432 | 1011 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1012 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       82 | 1013 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       82 | 1014 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       42 | 1015 | `		 }else{` |
|        - | 1016 | `			 /* Assume a literal */` |
|    19334 | 1017 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    19334 | 1018 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 | 1019 | `		 }` |
|  1777111 | 1020 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1021 | `		 /* Constants,function name,namespace path,class name... */` |
|   668198 | 1022 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   668198 | 1023 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   334100 | 1024 | `	 }else{` |
|  1085590 | 1025 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1026 | `			 /* Point to the code generator routine */` |
|   208882 | 1027 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   208882 | 1028 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1029 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1030 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1031 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1032 | `				 }` |
|        3 | 1033 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1034 | `				 return rc;` |
|        - | 1035 | `			 }` |
|   104439 | 1036 | `		 }` |
|        - | 1037 | `		/* Advance the stream cursor */` |
|  1085588 | 1038 | `		pCur++;` |
|        - | 1039 | `	 }` |
|        - | 1040 | `	/* Point to the end of the token stream */` |
|  3620104 | 1041 | `	pNode->pEnd = pCur;` |
|        - | 1042 | `	/* Save the node for later processing */` |
|  3620104 | 1043 | `	*ppNode = pNode;` |
|        - | 1044 | `	/* Synchronize cursors */` |
|  3620104 | 1045 | `	pGen->pIn = pCur;` |
|  3620104 | 1046 | `	return SXRET_OK;` |
|  1810104 | 1047 |  |
|        - | 1048 | `/*` |
|        - | 1049 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1050 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1051 | ` * level is zero.` |
|        - | 1052 | ` */` |
|    81316 | 1053 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 | 1054 |  |
|    81318 | 1055 | `	SyToken *pCur = pStart;` |
|    81318 | 1056 | `	sxi32 iNest = 0;` |
|    81318 | 1057 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1058 | `		/* Last expression */` |
|    43002 | 1059 | `		return SXERR_EOF;` |
|        - | 1060 | `	}` |
|   156622 | 1061 | `	while( pCur < pEnd ){` |
|   142392 | 1062 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    24088 | 1063 | `			break;` |
|        - | 1064 | `		}` |
|   118306 | 1065 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     7216 | 1066 | `			iNest++;` |
|   114699 | 1067 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     7218 | 1068 | `			iNest--;` |
|     3608 | 1069 | `		}` |
|   118306 | 1070 | `		pCur++;` |
|        2 | 1071 | `	}` |
|    38318 | 1072 | `	*ppNext = pCur;` |
|    38318 | 1073 | `	return SXRET_OK;` |
|    40660 | 1074 |  |
|        - | 1075 | `/*` |
|        - | 1076 | ` * Free an expression tree.` |
|        - | 1077 | ` */` |
|  3124890 | 1078 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1079 |  |
|  3124892 | 1080 | `	if( pNode->pLeft ){` |
|        - | 1081 | `		/* Release the left tree */` |
|  1168814 | 1082 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   584406 | 1083 | `	}` |
|  3124892 | 1084 | `	if( pNode->pRight ){` |
|        - | 1085 | `		/* Release the right tree */` |
|   646878 | 1086 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   323438 | 1087 | `	}` |
|  3124892 | 1088 | `	if( pNode->pCond ){` |
|        - | 1089 | `		/* Release the conditional tree used by the ternary operator */` |
|     2398 | 1090 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1198 | 1091 | `	}` |
|  3124892 | 1092 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1093 | `		ph7_expr_node **apArg;` |
|        - | 1094 | `		sxu32 n;` |
|        - | 1095 | `		/* Release node arguments */` |
|   385362 | 1096 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   812952 | 1097 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   427592 | 1098 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   213797 | 1099 | `		}` |
|   385362 | 1100 | `		SySetRelease(&pNode->aNodeArgs);` |
|   192680 | 1101 | `	}` |
|        - | 1102 | `	/* Finally,release this node */` |
|  3124892 | 1103 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3124892 | 1104 |  |
|        - | 1105 | `/*` |
|        - | 1106 | ` * Free an expression tree.` |
|        - | 1107 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1108 | ` */` |
|   825310 | 1109 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1110 |  |
|        - | 1111 | `	ph7_expr_node **apNode;` |
|        - | 1112 | `	sxu32 n;` |
|   825312 | 1113 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  4445414 | 1114 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3620104 | 1115 | `		if( apNode[n] ){` |
|   825646 | 1116 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   412822 | 1117 | `		}` |
|  1810053 | 1118 | `	}` |
|   825312 | 1119 | `	return SXRET_OK;` |
|        2 | 1120 |  |
|        - | 1121 | `/*` |
|        - | 1122 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1123 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1124 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1125 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1126 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1127 | ` */` |
|  1157418 | 1128 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        2 | 1129 |  |
|  1157420 | 1130 | `	if( pNode == 0 ){` |
|   712896 | 1131 | `		return 0;` |
|        - | 1132 | `	}` |
|   444526 | 1133 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       13 | 1134 | `		return 1;` |
|        - | 1135 | `	}` |
|   444514 | 1136 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        5 | 1137 | `		return 1;` |
|        - | 1138 | `	}` |
|   444510 | 1139 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1140 | `		return 1;` |
|        - | 1141 | `	}` |
|   444510 | 1142 | `	return 0;` |
|   578711 | 1143 |  |
|        - | 1144 | `/*` |
|        - | 1145 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1146 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1147 | ` */` |
|   261814 | 1148 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1149 |  |
|        - | 1150 | `	sxi32 iExprOp;` |
|   261816 | 1151 | `	if( pNode->pOp == 0 ){` |
|   158014 | 1152 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1153 | `	}` |
|   103804 | 1154 | `	iExprOp = pNode->pOp->iOp;` |
|   103804 | 1155 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    72128 | 1156 | `			return TRUE;` |
|        - | 1157 | `	}` |
|    31678 | 1158 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    31674 | 1159 | `		if( pNode->pLeft->pOp ) {` |
|       50 | 1160 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       21 | 1161 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1162 | `				return FALSE;` |
|        2 | 1163 | `			}` |
|    31649 | 1164 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1165 | `			return FALSE;` |
|        - | 1166 | `		}` |
|    31674 | 1167 | `		return TRUE;` |
|        - | 1168 | `	}` |
|        5 | 1169 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1170 | `		return TRUE;` |
|        - | 1171 | `	}` |
|        - | 1172 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1173 | `	return FALSE;` |
|   130909 | 1174 |  |
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
|   320746 | 1186 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1187 |  |
|        - | 1188 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1189 | `	sxi32 rc;` |
|        - | 1190 | `	/* Process function arguments from left to right */` |
|   320748 | 1191 | `	iCur = 0;` |
|   341849 | 1192 | `	for(;;){` |
|   683700 | 1193 | `		if( iCur >= nToken ){` |
|        - | 1194 | `			/* No more arguments to process */` |
|   320722 | 1195 | `			break;` |
|        - | 1196 | `		}` |
|   362980 | 1197 | `		iNode = iCur;` |
|   362980 | 1198 | `		iNest = 0;` |
|   905710 | 1199 | `		while( iCur < nToken ){` |
|   584988 | 1200 | `			if( apNode[iCur] ){` |
|   572500 | 1201 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    21130 | 1202 | `					break;` |
|   530244 | 1203 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    29114 | 1204 | `					iNest++;` |
|   515688 | 1205 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    29084 | 1206 | `					iNest--;` |
|    14541 | 1207 | `				}` |
|   265121 | 1208 | `			}` |
|   542732 | 1209 | `			iCur++;` |
|        2 | 1210 | `		}` |
|   362980 | 1211 | `		if( iCur > iNode ){` |
|   362974 | 1212 | `			SyString sArgName = {0, 0};` |
|        - | 1213 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1214 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1215 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   364863 | 1216 | `			if( (iCur - iNode) >= 2` |
|   201103 | 1217 | `				&& apNode[iNode]` |
|    39234 | 1218 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    21570 | 1219 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3844 | 1220 | `				&& apNode[iNode+1]` |
|     3784 | 1221 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1222 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      188 | 1223 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      188 | 1224 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      188 | 1225 | `				apNode[iNode] = 0;` |
|      188 | 1226 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      188 | 1227 | `				apNode[iNode+1] = 0;` |
|      188 | 1228 | `				iNode += 2;` |
|        - | 1229 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1230 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      188 | 1231 | `				if( iNode >= iCur ){` |
|        4 | 1232 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1233 | `						pOp->pStart->nLine,` |
|        - | 1234 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1235 | `						&sArgName);` |
|        3 | 1236 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1237 | `						rc = SXERR_SYNTAX;` |
|        1 | 1238 | `					}` |
|        3 | 1239 | `					return rc;` |
|        - | 1240 | `				}` |
|       92 | 1241 | `			}` |
|   362970 | 1242 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1243 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1244 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1245 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1246 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1247 | `					apNode[iNode] = 0;` |
|      ! 0 | 1248 | `			}` |
|   362972 | 1249 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   362972 | 1250 | `			if( apNode[iNode] ){` |
|   362972 | 1251 | `				if( sArgName.nByte > 0 ){` |
|      186 | 1252 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      186 | 1253 | `					apNode[iNode]->sArgName = sArgName;` |
|       92 | 1254 | `				}` |
|        - | 1255 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   362972 | 1256 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   181487 | 1257 | `			}else{` |
|        - | 1258 | `				/* No expression before comma */` |
|      ! 0 | 1259 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1260 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1261 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1262 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1263 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1264 | `				}` |
|      ! 0 | 1265 | `				return rc;` |
|        - | 1266 | `			}` |
|   181487 | 1267 | `		}else{` |
|        - | 1268 | `			/* Comma with no preceding argument */` |
|        7 | 1269 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1270 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1271 | `				rc = SXERR_SYNTAX;` |
|        3 | 1272 | `			}` |
|        7 | 1273 | `			return rc;` |
|        - | 1274 | `		}` |
|        - | 1275 | `		/* Jump trailing comma */` |
|   362972 | 1276 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    42252 | 1277 | `			iCur++;` |
|    42252 | 1278 | `			if( iCur >= nToken ){` |
|        - | 1279 | `				/* Trailing comma after last argument */` |
|       19 | 1280 | `				break;` |
|        - | 1281 | `			}` |
|    21116 | 1282 | `		}` |
|        2 | 1283 | `	}` |
|   320740 | 1284 | `	return SXRET_OK;` |
|   160375 | 1285 |  |
|        - | 1286 | ` /*` |
|        - | 1287 | `  * Create an expression tree from an array of tokens.` |
|        - | 1288 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1289 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1290 | `  */` |
|  1287586 | 1291 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1292 | ` {` |
|        - | 1293 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1294 | `	 ph7_expr_node *pNode;` |
|        - | 1295 | `	 sxi32 iCur;` |
|        - | 1296 | `	 sxi32 rc;` |
|  1287588 | 1297 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1298 | `		 /* TICKET 1433-17: self evaluating node */` |
|   582826 | 1299 | `		 return SXRET_OK;` |
|        - | 1300 | `	 }` |
|        - | 1301 | `	 /* Process expressions enclosed in parenthesis first */` |
|  4327744 | 1302 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1303 | `		 sxi32 iNest;` |
|        - | 1304 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1305 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1306 | `		  */` |
|  3622984 | 1307 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3596386 | 1308 | `			 continue;` |
|        - | 1309 | `		 }` |
|    26600 | 1310 | `		 iNest = 1;` |
|    26600 | 1311 | `		 iLeft = iCur;` |
|        - | 1312 | `		 /* Find the closing parenthesis */` |
|    26600 | 1313 | `		 iCur++;` |
|   177584 | 1314 | `		 while( iCur < nToken ){` |
|   177584 | 1315 | `			 if( apNode[iCur] ){` |
|   177584 | 1316 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1317 | `					 /* Decrement nesting level */` |
|    46150 | 1318 | `					 iNest--;` |
|    46150 | 1319 | `					 if( iNest <= 0 ){` |
|    26600 | 1320 | `						 break;` |
|        2 | 1321 | `					 }` |
|   141211 | 1322 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1323 | `					 /* Increment nesting level */` |
|    19552 | 1324 | `					 iNest++;` |
|     9775 | 1325 | `				 }` |
|    75492 | 1326 | `			 }` |
|   150986 | 1327 | `			 iCur++;` |
|        2 | 1328 | `		 }` |
|    26600 | 1329 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1330 | `			 sxi32 j;` |
|        - | 1331 | `			 /* Recurse and process this expression */` |
|    26600 | 1332 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    26600 | 1333 | `			 if( rc != SXRET_OK ){` |
|        3 | 1334 | `				 return rc;` |
|        - | 1335 | `			 }` |
|        - | 1336 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1337 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1338 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    26598 | 1339 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    26598 | 1340 | `				 if( apNode[j] ){` |
|    26598 | 1341 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    26598 | 1342 | `					 break;` |
|        - | 1343 | `				 }` |
|      ! 0 | 1344 | `			 }` |
|    13298 | 1345 | `		 }` |
|        - | 1346 | `		 /* Free the left and right nodes */` |
|    26598 | 1347 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    26598 | 1348 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    26598 | 1349 | `		 apNode[iLeft] = 0;` |
|    26598 | 1350 | `		 apNode[iCur] = 0;` |
|    13300 | 1351 | `	 }` |
|        - | 1352 | `	  /* Process expressions enclosed in braces */` |
|  4498416 | 1353 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1354 | `		 sxi32 iNest;` |
|        - | 1355 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1356 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1357 | `		  */` |
|  3800548 | 1358 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3800544 | 1359 | `			 continue;` |
|        - | 1360 | `		 }` |
|        6 | 1361 | `		 iNest = 1;` |
|        6 | 1362 | `		 iLeft = iCur;` |
|        - | 1363 | `		 /* Find the closing parenthesis */` |
|        6 | 1364 | `		 iCur++;` |
|        8 | 1365 | `		 while( iCur < nToken ){` |
|        8 | 1366 | `			 if( apNode[iCur] ){` |
|        8 | 1367 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1368 | `					 /* Decrement nesting level */` |
|        6 | 1369 | `					 iNest--;` |
|        6 | 1370 | `					 if( iNest <= 0 ){` |
|        6 | 1371 | `						 break;` |
|      ! 0 | 1372 | `					 }` |
|        3 | 1373 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1374 | `					 /* Increment nesting level */` |
|      ! 0 | 1375 | `					 iNest++;` |
|      ! 0 | 1376 | `				 }` |
|        1 | 1377 | `			 }` |
|        3 | 1378 | `			 iCur++;` |
|        1 | 1379 | `		 }` |
|        6 | 1380 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1381 | `			 /* Recurse and process this expression */` |
|        3 | 1382 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        3 | 1383 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1384 | `				 return rc;` |
|        - | 1385 | `			 }` |
|        1 | 1386 | `		 }` |
|        - | 1387 | `		 /* Free the left and right nodes */` |
|        6 | 1388 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        6 | 1389 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        6 | 1390 | `		 apNode[iLeft] = 0;` |
|        6 | 1391 | `		 apNode[iCur] = 0;` |
|        4 | 1392 | `	 }` |
|        - | 1393 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   697870 | 1394 | `	 iLeft = -1;` |
|  4498386 | 1395 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3800530 | 1396 | `		 if( apNode[iCur] == 0 ){` |
|  1449202 | 1397 | `			 continue;` |
|        - | 1398 | `		 }` |
|  2351330 | 1399 | `		 pNode = apNode[iCur];` |
|  2351330 | 1400 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   621216 | 1401 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1402 | `				 /* Collect function arguments */` |
|   370262 | 1403 | `				 sxi32 iPtr = 0;` |
|   370262 | 1404 | `				 sxi32 nFuncTok = 0;` |
|  1325510 | 1405 | `				 while( nFuncTok + iCur < nToken ){` |
|  1325510 | 1406 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1313022 | 1407 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   383346 | 1408 | `							 iPtr++;` |
|  1121350 | 1409 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   383346 | 1410 | `							 iPtr--;` |
|   383346 | 1411 | `							 if( iPtr <= 0 ){` |
|   370262 | 1412 | `								 break;` |
|        - | 1413 | `							 }` |
|     6542 | 1414 | `						 }` |
|   471380 | 1415 | `					 }` |
|   955250 | 1416 | `					 nFuncTok++;` |
|        2 | 1417 | `				 }` |
|   370262 | 1418 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1419 | `					 /* Syntax error */` |
|      ! 0 | 1420 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1421 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1422 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1423 | `					 }` |
|      ! 0 | 1424 | `					 return rc;` |
|        - | 1425 | `				 }` |
|   370262 | 1426 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1427 | `					 /* Syntax error */` |
|      ! 0 | 1428 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1429 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1430 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1431 | `					 }` |
|      ! 0 | 1432 | `					 return rc;` |
|        - | 1433 | `				 }` |
|   370262 | 1434 | `				 if( nFuncTok > 1 ){` |
|        - | 1435 | `					 /* Process function arguments */` |
|   320748 | 1436 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   320748 | 1437 | `					 if( rc != SXRET_OK ){` |
|        9 | 1438 | `						 return rc;` |
|        - | 1439 | `					 }` |
|   160369 | 1440 | `				 }` |
|        - | 1441 | `				 /* Link the node to the tree */` |
|   370254 | 1442 | `				 pNode->pLeft = apNode[iLeft];` |
|   370254 | 1443 | `				 apNode[iLeft] = 0;` |
|  1325478 | 1444 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   955226 | 1445 | `					 apNode[iCur+iPtr] = 0;` |
|   477614 | 1446 | `				 }` |
|   436082 | 1447 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1448 | `				 /* Subscripting */` |
|    80320 | 1449 | `				 sxi32 iArrTok = iCur + 1;` |
|    80320 | 1450 | `				 sxi32 iNest = 1;` |
|    80472 | 1451 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1452 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1453 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1454 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    80318 | 1455 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1456 | `						 /* Syntax error */` |
|      ! 0 | 1457 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1458 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1459 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1460 | `						 }` |
|      ! 0 | 1461 | `						 return rc;` |
|        - | 1462 | `				 }` |
|        - | 1463 | `				 /* Collect index tokens */` |
|   145050 | 1464 | `				 while( iArrTok < nToken ){` |
|   145050 | 1465 | `					 if( apNode[iArrTok] ){` |
|   145018 | 1466 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1467 | `							 /* Increment nesting level */` |
|      ! 0 | 1468 | `							 iNest++;` |
|   145018 | 1469 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1470 | `							 /* Decrement nesting level */` |
|    80320 | 1471 | `							 iNest--;` |
|    80320 | 1472 | `							 if( iNest <= 0 ){` |
|    80320 | 1473 | `								 break;` |
|        - | 1474 | `							 }` |
|      ! 0 | 1475 | `						 }` |
|    32349 | 1476 | `					 }` |
|    64732 | 1477 | `					 ++iArrTok;` |
|        2 | 1478 | `				 }` |
|    80320 | 1479 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1480 | `					 /* Recurse and process this expression */` |
|    64622 | 1481 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    64622 | 1482 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1483 | `						 return rc;` |
|        - | 1484 | `					 }` |
|        - | 1485 | `					 /* Link the node to it's index */` |
|    64622 | 1486 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    32310 | 1487 | `				 }` |
|        - | 1488 | `				 /* Link the node to the tree */` |
|    80320 | 1489 | `				 pNode->pLeft = apNode[iLeft];` |
|    80320 | 1490 | `				 pNode->pRight = 0;` |
|    80320 | 1491 | `				 apNode[iLeft] = 0;` |
|   225368 | 1492 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   145050 | 1493 | `					 apNode[iNest] = 0;` |
|    72526 | 1494 | `				 }` |
|    40161 | 1495 | `			 }else{` |
|        - | 1496 | `				 /* Member access operators [i.e: '->','::'] */` |
|   170638 | 1497 | `				  iRight = iCur + 1;` |
|   170640 | 1498 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        3 | 1499 | `					 iRight++;` |
|        1 | 1500 | `				 }` |
|   170638 | 1501 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1502 | `					 /* Syntax error */` |
|        5 | 1503 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1504 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1505 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1506 | `					 }` |
|        5 | 1507 | `					 return rc;` |
|        - | 1508 | `				 }` |
|        - | 1509 | `				 /* Link the node to the tree */` |
|   170634 | 1510 | `				 pNode->pLeft = apNode[iLeft];` |
|   255794 | 1511 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   170518 | 1512 | `					 && pNode->pLeft->pOp == 0 &&` |
|   170324 | 1513 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1514 | `						 /* Syntax error */` |
|      ! 0 | 1515 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1516 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1517 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1518 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1519 | `						 }` |
|      ! 0 | 1520 | `						 return rc;` |
|        - | 1521 | `				 }` |
|   170634 | 1522 | `				 pNode->pRight = apNode[iRight];` |
|   170634 | 1523 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1524 | `			 }` |
|   310601 | 1525 | `		 }` |
|  2351318 | 1526 | `		 iLeft = iCur;` |
|  1175660 | 1527 | `	 }` |
|        - | 1528 | `	 /* Handle left associative (new, clone) operators */` |
|  4498354 | 1529 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3800498 | 1530 | `		 if( apNode[iCur] == 0 ){` |
|  2086994 | 1531 | `			 continue;` |
|        - | 1532 | `		 }` |
|  1713506 | 1533 | `		 pNode = apNode[iCur];` |
|  1713506 | 1534 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1535 | `			 SyToken *pToken;` |
|        - | 1536 | `			 /* Get the left node */` |
|    16592 | 1537 | `			 iLeft = iCur + 1;` |
|    33148 | 1538 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    16558 | 1539 | `				 iLeft++;` |
|        2 | 1540 | `			 }` |
|    16592 | 1541 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1542 | `				  /* Syntax error */` |
|      ! 0 | 1543 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1544 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1545 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1546 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1547 | `				 }` |
|      ! 0 | 1548 | `				 return rc;` |
|        - | 1549 | `			 }` |
|        - | 1550 | `			 /* Make sure the operand are of a valid type */` |
|    16592 | 1551 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
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
|    16574 | 1575 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       20 | 1576 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       20 | 1577 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
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
|        9 | 1588 | `				 }` |
|        - | 1589 | `			 }` |
|        - | 1590 | `			  /* Link the node to the tree */` |
|    16592 | 1591 | `			 pNode->pLeft = apNode[iLeft];` |
|    16592 | 1592 | `			 apNode[iLeft] = 0;` |
|    16592 | 1593 | `			 pNode->pRight = 0; /* Paranoid */` |
|     8295 | 1594 | `		 }` |
|   856754 | 1595 | `	 }` |
|        - | 1596 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   697858 | 1597 | `	 iLeft = -1;` |
|  4501800 | 1598 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3800498 | 1599 | `		 if( apNode[iCur] == 0 ){` |
|  2086994 | 1600 | `			 continue;` |
|        - | 1601 | `		 }` |
|  1713506 | 1602 | `		 pNode = apNode[iCur];` |
|  1713506 | 1603 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     9406 | 1604 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3473 | 1605 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1606 | `					 /* Link the node to the tree */` |
|     3472 | 1607 | `					 pNode->pLeft = apNode[iLeft];` |
|     3472 | 1608 | `					 apNode[iLeft] = 0;` |
|     1735 | 1609 | `			 }` |
|     6425 | 1610 | `		  }` |
|  1716952 | 1611 | `		 iLeft = iCur;` |
|   860200 | 1612 | `	  }` |
|   701304 | 1613 | `	 iLeft = -1;` |
|  4501800 | 1614 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3800498 | 1615 | `		 if( apNode[iCur] == 0 ){` |
|  2090464 | 1616 | `			 continue;` |
|        - | 1617 | `		 }` |
|  1710036 | 1618 | `		 pNode = apNode[iCur];` |
|  1710036 | 1619 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     9381 | 1620 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     9382 | 1621 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1622 | `					 /* Syntax error */` |
|      ! 0 | 1623 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1624 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1625 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1626 | `					 }` |
|      ! 0 | 1627 | `					 return rc;` |
|        - | 1628 | `			 }` |
|        - | 1629 | `			 /* Link the node to the tree */` |
|     9382 | 1630 | `			 pNode->pLeft = apNode[iLeft];` |
|     9382 | 1631 | `			 apNode[iLeft] = 0;` |
|        - | 1632 | `			 /* Mark as pre-increment/decrement node */` |
|     9382 | 1633 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4690 | 1634 | `		  }` |
|  1710036 | 1635 | `		 iLeft = iCur;` |
|   855019 | 1636 | `	 }` |
|        - | 1637 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   701304 | 1638 | `	  iLeft = 0;` |
|  4501794 | 1639 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3800494 | 1640 | `		  if( apNode[iCur] ){` |
|  1700652 | 1641 | `			  pNode = apNode[iCur];` |
|  1700652 | 1642 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    41998 | 1643 | `				  if( iLeft > 0 ){` |
|        - | 1644 | `					  /* Link the node to the tree */` |
|    41996 | 1645 | `					  pNode->pLeft = apNode[iLeft];` |
|    41996 | 1646 | `					  apNode[iLeft] = 0;` |
|    41996 | 1647 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1648 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1649 | `							   /* Syntax error */` |
|      ! 0 | 1650 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1651 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1652 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1653 | `							  }` |
|      ! 0 | 1654 | `							  return rc;` |
|        - | 1655 | `						  }` |
|       36 | 1656 | `					  }` |
|    20999 | 1657 | `				  }else{` |
|        - | 1658 | `					  /* Syntax error */` |
|        3 | 1659 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1660 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1661 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1662 | `					  }` |
|        3 | 1663 | `					  return rc;` |
|        - | 1664 | `				  }` |
|    20997 | 1665 | `			  }` |
|        - | 1666 | `			  /* Save terminal position */` |
|  1700650 | 1667 | `			  iLeft = iCur;` |
|   850324 | 1668 | `		  }` |
|  1900247 | 1669 | `	  }` |
|        - | 1670 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1671 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1672 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1673 | `	  * yielding a right-leaning tree. */` |
|  4501792 | 1674 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  3800492 | 1675 | `		 if( apNode[iCur] == 0 ){` |
|  2141950 | 1676 | `			 continue;` |
|        - | 1677 | `		 }` |
|  1658544 | 1678 | `		 pNode = apNode[iCur];` |
|  1658544 | 1679 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1680 | `			 sxi32 iL, iR;` |
|        - | 1681 | `			 /* Find the right operand */` |
|      113 | 1682 | `			 iR = -1;` |
|        - | 1683 | `			 {` |
|        - | 1684 | `				 sxi32 j;` |
|      125 | 1685 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1686 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1687 | `				 }` |
|        - | 1688 | `			 }` |
|        - | 1689 | `			 /* Find the left operand */` |
|      113 | 1690 | `			 iL = -1;` |
|        - | 1691 | `			 {` |
|        - | 1692 | `				 sxi32 j;` |
|      181 | 1693 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1694 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1695 | `				 }` |
|        - | 1696 | `			 }` |
|      113 | 1697 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1698 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1699 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1700 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1701 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1702 | `				 }` |
|      ! 0 | 1703 | `				 return rc;` |
|        - | 1704 | `			 }` |
|      113 | 1705 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1706 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1707 | `			 apNode[iL] = 0;` |
|      113 | 1708 | `			 apNode[iR] = 0;` |
|        - | 1709 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1710 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1711 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1712 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1713 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1714 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1715 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1716 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1717 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1718 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1719 | `			  * operands are respected. */` |
|      129 | 1720 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1721 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1722 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1723 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1724 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1725 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1726 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1727 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1728 | `				 while( pTail->pLeft` |
|       34 | 1729 | `					 && pTail->pLeft->pOp` |
|       23 | 1730 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1731 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1732 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1733 | `					 pTail = pTail->pLeft;` |
|        1 | 1734 | `				 }` |
|        - | 1735 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1736 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1737 | `				 pTail->pLeft = pNode;` |
|       27 | 1738 | `				 apNode[iCur] = pHead;` |
|       13 | 1739 | `			 }` |
|       56 | 1740 | `		 }` |
|   829273 | 1741 | `	 }` |
|        - | 1742 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  7714216 | 1743 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  7012926 | 1744 | `		 iLeft = -1;` |
| 45017532 | 1745 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 38004618 | 1746 | `			 if( apNode[iCur] == 0 ){` |
| 24256656 | 1747 | `				 continue;` |
|        - | 1748 | `			 }` |
| 13747964 | 1749 | `			 pNode = apNode[iCur];` |
| 13747964 | 1750 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1751 | `				 /* Get the right node */` |
|   211946 | 1752 | `				 iRight = iCur + 1;` |
|   302680 | 1753 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    90736 | 1754 | `					 iRight++;` |
|        2 | 1755 | `				 }` |
|   211946 | 1756 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1757 | `					 /* Syntax error */` |
|        9 | 1758 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1759 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1760 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1761 | `					 }` |
|        9 | 1762 | `					 return rc;` |
|        - | 1763 | `				 }` |
|   211938 | 1764 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1765 | `					 sxi32  iTmp;` |
|        - | 1766 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1767 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1768 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1769 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1770 | `					  * is swapped below. */` |
|       50 | 1771 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1772 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1773 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1774 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1775 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1776 | `						 }` |
|        3 | 1777 | `						 return rc;` |
|        - | 1778 | `					 }` |
|       48 | 1779 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1780 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1781 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1782 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1783 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1784 | `						 }` |
|      ! 0 | 1785 | `						 return rc;` |
|        - | 1786 | `					 }` |
|       48 | 1787 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1788 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1789 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1790 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1791 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1792 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1793 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1794 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1795 | `									 }` |
|      ! 0 | 1796 | `									 return rc;` |
|        - | 1797 | `							 }` |
|      ! 0 | 1798 | `						 }` |
|       16 | 1799 | `					 }` |
|        - | 1800 | `					 /* Swap operands */` |
|       48 | 1801 | `					 iTmp = iRight;` |
|       48 | 1802 | `					 iRight = iLeft;` |
|       48 | 1803 | `					 iLeft = iTmp;` |
|       23 | 1804 | `				 }` |
|        - | 1805 | `				 /* Link the node to the tree */` |
|   211936 | 1806 | `				 pNode->pLeft = apNode[iLeft];` |
|   211936 | 1807 | `				 pNode->pRight = apNode[iRight];` |
|   211936 | 1808 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   105967 | 1809 | `			 }` |
| 13747954 | 1810 | `			 iLeft = iCur;` |
|  6873978 | 1811 | `		 }` |
|  3506459 | 1812 | `	 }` |
|        - | 1813 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1814 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1815 | `	  * we are dealing with a single operator.` |
|        - | 1816 | `	  */` |
|   701292 | 1817 | `	  iLeft = -1;` |
|  4491490 | 1818 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3792598 | 1819 | `		  if( apNode[iCur] == 0 ){` |
|  2565214 | 1820 | `			  continue;` |
|        - | 1821 | `		  }` |
|  1227386 | 1822 | `		  pNode = apNode[iCur];` |
|  1227386 | 1823 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2400 | 1824 | `			  sxi32 iNest = 1;` |
|     2400 | 1825 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1826 | `				  /* Missing condition */` |
|        3 | 1827 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1828 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1829 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1830 | `				  }` |
|        3 | 1831 | `				  return rc;` |
|        - | 1832 | `			  }` |
|        - | 1833 | `			  /* Get the right node */` |
|     2398 | 1834 | `			  iRight = iCur + 1;` |
|     5038 | 1835 | `			  while( iRight < nToken  ){` |
|     5038 | 1836 | `				  if( apNode[iRight] ){` |
|     4726 | 1837 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1838 | `						  /* Increment nesting level */` |
|      ! 0 | 1839 | `						  ++iNest;` |
|     4726 | 1840 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1841 | `						  /* Decrement nesting level */` |
|     2398 | 1842 | `						  --iNest;` |
|     2398 | 1843 | `						  if( iNest <= 0 ){` |
|     2398 | 1844 | `							  break;` |
|        - | 1845 | `						  }` |
|      ! 0 | 1846 | `					  }` |
|     1164 | 1847 | `				  }` |
|     2642 | 1848 | `				  iRight++;` |
|        2 | 1849 | `			  }` |
|     2398 | 1850 | `			  if( iRight > iCur + 1 ){` |
|        - | 1851 | `				  /* Recurse and process the then expression */` |
|     2330 | 1852 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2330 | 1853 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1854 | `					  return rc;` |
|        - | 1855 | `				  }` |
|        - | 1856 | `				  /* Link the node to the tree */` |
|     2330 | 1857 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1164 | 1858 | `			  }else{` |
|        - | 1859 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1860 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1861 | `			  }` |
|     2398 | 1862 | `			  apNode[iCur + 1] = 0;` |
|     2398 | 1863 | `			  if( iRight + 1 < nToken ){` |
|        - | 1864 | `				  /* Recurse and process the else expression */` |
|     2398 | 1865 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2398 | 1866 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1867 | `					  return rc;` |
|        - | 1868 | `				  }` |
|        - | 1869 | `				  /* Link the node to the tree */` |
|     2398 | 1870 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2398 | 1871 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1200 | 1872 | `			  }else{` |
|      ! 0 | 1873 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1874 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1875 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1876 | `				 }` |
|      ! 0 | 1877 | `				 return rc;` |
|        - | 1878 | `			  }` |
|        - | 1879 | `			  /* Point to the condition */` |
|     2398 | 1880 | `			  pNode->pCond  = apNode[iLeft];` |
|     2398 | 1881 | `			  apNode[iLeft] = 0;` |
|     2398 | 1882 | `			  break;` |
|        - | 1883 | `		  }` |
|  1224988 | 1884 | `		  iLeft = iCur;` |
|   612495 | 1885 | `	  }` |
|        - | 1886 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1887 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1888 | `	  * so there is no need for a precedence loop here.` |
|        - | 1889 | `	  */` |
|   701290 | 1890 | `	 iRight = -1;` |
|  4501596 | 1891 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3800362 | 1892 | `		 if( apNode[iCur] == 0 ){` |
|  2837166 | 1893 | `			 continue;` |
|        - | 1894 | `		 }` |
|   963198 | 1895 | `		 pNode = apNode[iCur];` |
|   963198 | 1896 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1897 | `			 /* Get the left node */` |
|   261788 | 1898 | `			 iLeft = iCur - 1;` |
|   381630 | 1899 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   119844 | 1900 | `				 iLeft--;` |
|        2 | 1901 | `			 }` |
|   261788 | 1902 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1903 | `				 /* Syntax error */` |
|       43 | 1904 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1905 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1906 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1907 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1908 | `				 }else{` |
|       39 | 1909 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1910 | `				 }` |
|       43 | 1911 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1912 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1913 | `				 }` |
|       43 | 1914 | `				 return rc;` |
|        - | 1915 | `			 }` |
|        - | 1916 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1917 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1918 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1919 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1920 | `			  * a write. */` |
|   261746 | 1921 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        9 | 1922 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1923 | `					 "Can't use nullsafe operator in write context");` |
|        9 | 1924 | `				 if( rc != SXERR_ABORT ){` |
|        9 | 1925 | `					 rc = SXERR_SYNTAX;` |
|        4 | 1926 | `				 }` |
|        9 | 1927 | `				 return rc;` |
|        - | 1928 | `			 }` |
|   261738 | 1929 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1930 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1931 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1932 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1933 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1934 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1935 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1936 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1937 | `					 }else{` |
|        4 | 1938 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1939 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1940 | `					 }` |
|        5 | 1941 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1942 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1943 | `					 }` |
|        5 | 1944 | `					 return rc;` |
|        - | 1945 | `				 }` |
|       26 | 1946 | `			 }` |
|        - | 1947 | `			 /* Link the node to the tree (Reverse) */` |
|   261734 | 1948 | `			 pNode->pLeft = apNode[iRight];` |
|   261734 | 1949 | `			 pNode->pRight = apNode[iLeft];` |
|   261734 | 1950 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   130866 | 1951 | `		 }` |
|   963144 | 1952 | `		 iRight = iCur;` |
|   481573 | 1953 | `	 }` |
|        - | 1954 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3506172 | 1955 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2804938 | 1956 | `		 iLeft = -1;` |
| 18006106 | 1957 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 15201170 | 1958 | `			 if( apNode[iCur] == 0 ){` |
| 12395828 | 1959 | `				 continue;` |
|        - | 1960 | `			 }` |
|  2805344 | 1961 | `			 pNode = apNode[iCur];` |
|  2805344 | 1962 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1963 | `				 /* Get the right node */` |
|       72 | 1964 | `				 iRight = iCur + 1;` |
|      110 | 1965 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1966 | `					 iRight++;` |
|        2 | 1967 | `				 }` |
|       72 | 1968 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1969 | `					 /* Syntax error */` |
|      ! 0 | 1970 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1971 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1972 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1973 | `					 }` |
|      ! 0 | 1974 | `					 return rc;` |
|        - | 1975 | `				 }` |
|        - | 1976 | `				 /* Link the node to the tree */` |
|       72 | 1977 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1978 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1979 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1980 | `			 }` |
|  2805344 | 1981 | `			 iLeft = iCur;` |
|  1402673 | 1982 | `		 }` |
|  1402470 | 1983 | `	 }` |
|        - | 1984 | `	 /* Point to the root of the expression tree */` |
|  3800266 | 1985 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3099050 | 1986 | `		 if( apNode[iCur] ){` |
|   637778 | 1987 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1988 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1989 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1990 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1991 | `				  }` |
|       20 | 1992 | `				  return rc;` |
|        - | 1993 | `			 }` |
|   637760 | 1994 | `			 apNode[0] = apNode[iCur];` |
|   637760 | 1995 | `			 apNode[iCur] = 0;` |
|   318879 | 1996 | `		 }` |
|  1549517 | 1997 | `	 }` |
|   701218 | 1998 | `	 return SXRET_OK;` |
|   642072 | 1999 | ` }` |
|        - | 2000 | ` /*` |
|        - | 2001 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2002 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2003 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2004 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2005 | `  */` |
|   825310 | 2006 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 2007 |  |
|        - | 2008 | `	ph7_expr_node **apNode;` |
|        - | 2009 | `	ph7_expr_node *pNode;` |
|        - | 2010 | `	sxi32 rc;` |
|        - | 2011 | `	/* Reset node container */` |
|   825312 | 2012 | `	SySetReset(pExprNode);` |
|   825312 | 2013 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2014 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2015 | `	{` |
|   825312 | 2016 | `		int iLastWasTerm = 0;` |
|  4445414 | 2017 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3620138 | 2018 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3620138 | 2019 | `			if( rc != SXRET_OK ){` |
|       35 | 2020 | `				return rc;` |
|        - | 2021 | `			}` |
|        - | 2022 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3620104 | 2023 | `			if( pNode->xCode ){` |
|        - | 2024 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1902374 | 2025 | `				iLastWasTerm = 1;` |
|  2668918 | 2026 | `			}else if( pNode->pOp ){` |
|        - | 2027 | `				/* Operator node */` |
|   841024 | 2028 | `				iLastWasTerm = 0;` |
|   420513 | 2029 | `			}else{` |
|        - | 2030 | `				/* Delimiter: ')' and ']' end terms */` |
|   876710 | 2031 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2032 | `			}` |
|        - | 2033 | `			/* Save the extracted node */` |
|  3620104 | 2034 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 2035 | `		}` |
|        - | 2036 | `	}` |
|   825278 | 2037 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2038 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2039 | `		*ppRoot = 0;` |
|      ! 0 | 2040 | `		return SXRET_OK;` |
|        - | 2041 | `	}` |
|   825278 | 2042 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2043 | `	/* Make sure we are dealing with valid nodes */` |
|   825278 | 2044 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   825278 | 2045 | `	if( rc != SXRET_OK ){` |
|        - | 2046 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2047 | `		 * cleanup the mess left behind.` |
|        - | 2048 | `		 */` |
|       51 | 2049 | `		*ppRoot = 0;` |
|       51 | 2050 | `		return rc;` |
|        - | 2051 | `	}` |
|        - | 2052 | `	/* Build the tree */` |
|   825228 | 2053 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   825228 | 2054 | `	if( rc != SXRET_OK ){` |
|        - | 2055 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      100 | 2056 | `		*ppRoot = 0;` |
|      100 | 2057 | `		return rc;` |
|        - | 2058 | `	}` |
|        - | 2059 | `	/* Point to the root of the tree */` |
|   825130 | 2060 | `	*ppRoot = apNode[0];` |
|   825130 | 2061 | `	return SXRET_OK;` |
|   412657 | 2062 |  |
|        - | 2063 |  |
