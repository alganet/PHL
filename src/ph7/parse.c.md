# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1118/1275 lines (87.69%)

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
|  1214818 |  266 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  267 | `{` |
|  1214823 |  268 | `	sxu32 n = 0;` |
|        - |  269 | `	sxi32 rc;` |
|        - |  270 | `	/* Do a linear lookup on the operators table */` |
| 20846224 |  271 | `	for(;;){` |
| 41692453 |  272 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  273 | `			break;` |
|        - |  274 | `		}` |
| 41692453 |  275 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  276 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3724069 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1862037 |  278 | `		}else{` |
| 37968389 |  279 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  280 | `		}` |
| 41692453 |  281 | `		if( rc == 0 ){` |
|  1219461 |  282 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  283 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1214367 |  284 | `				return &aOpTable[n];` |
|        - |  285 | `			}` |
|        - |  286 | `			/* Handle ambiguity */` |
|     5099 |  287 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  288 | `				/* Unary opertors have prcedence here over binary operators */` |
|      341 |  289 | `				return &aOpTable[n];` |
|        - |  290 | `			}` |
|     4763 |  291 | `			if( pLast->nType & PH7_TK_OP ){` |
|      131 |  292 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  293 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      131 |  294 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  295 | `					/* Unary opertors have prcedence here over binary operators */` |
|      123 |  296 | `					return &aOpTable[n];` |
|        - |  297 | `				}` |
|        - |  298 |  |
|        4 |  299 | `			}` |
|     2319 |  300 | `		}` |
| 40477635 |  301 | `		++n; /* Next operator in the table */` |
|        5 |  302 | `	}` |
|        - |  303 | `	/* No such operator */` |
|      ! 0 |  304 | `	return 0;` |
|   607414 |  305 | `}` |
|        - |  306 | `/*` |
|        - |  307 | ` * Delimit a set of token stream.` |
|        - |  308 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  309 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  310 | ` */` |
|   743394 |  311 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  312 | `{` |
|   743399 |  313 | `	SyToken *pCur = pIn;` |
|   743399 |  314 | `	sxi32 iNest = 1;` |
|  4147677 |  315 | `	for(;;){` |
|  8295359 |  316 | `		if( pCur >= pEnd ){` |
|      471 |  317 | `			break;` |
|        - |  318 | `		}` |
|  8294893 |  319 | `		if( pCur->nType & nTokStart ){` |
|        - |  320 | `			/* Increment nesting level */` |
|   391081 |  321 | `			iNest++;` |
|  8099355 |  322 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  323 | `			/* Decrement nesting level */` |
|  1134009 |  324 | `			iNest--;` |
|  1134009 |  325 | `			if( iNest <= 0 ){` |
|   742933 |  326 | `				break;` |
|        - |  327 | `			}` |
|   195538 |  328 | `		}` |
|        - |  329 | `		/* Advance cursor */` |
|  7551965 |  330 | `		pCur++;` |
|        5 |  331 | `	}` |
|        - |  332 | `	/* Point to the end of the chunk */` |
|   743399 |  333 | `	*ppEnd = pCur;` |
|   743399 |  334 | `}` |
|        - |  335 | `/*` |
|        - |  336 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  337 | ` * Note on reserved keywords.` |
|        - |  338 | ` *  According to the PHP language reference manual:` |
|        - |  339 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  340 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  341 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  342 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  343 | ` */` |
|    24090 |  344 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  345 | `{` |
|    24090 |  346 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    23992 |  347 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  348 | `		){` |
|      167 |  349 | `			return TRUE;` |
|        - |  350 | `	}` |
|    23933 |  351 | `	if( bCheckFunc ){` |
|      356 |  352 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      349 |  353 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      331 |  354 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       49 |  355 | `				return TRUE;` |
|        - |  356 | `		}` |
|      156 |  357 | `	}` |
|        - |  358 | `	/* Not a language construct */` |
|    23889 |  359 | `	return FALSE;` |
|    12050 |  360 | `}` |
|        - |  361 | `/*` |
|        - |  362 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  363 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  364 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  365 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  366 | ` */` |
|  1024560 |  367 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  368 | `{` |
|        - |  369 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  370 | `	sxi32 i,rc;` |
|        - |  371 |  |
|  1024565 |  372 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  373 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  374 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  375 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  376 | `	}` |
|  1024565 |  377 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5553871 |  378 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4529345 |  379 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  380 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1271 |  381 | `			continue;` |
|        - |  382 | `		}` |
|  4528079 |  383 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   514839 |  384 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    24116 |  385 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  386 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   482407 |  387 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  388 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  389 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  390 | `						 */` |
|   482407 |  391 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   482407 |  392 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   482407 |  393 | `						apNode[i]->pOp = &sFCallOp;` |
|   241201 |  394 | `					}` |
|   241201 |  395 | `			}` |
|   514839 |  396 | `			iParen++;` |
|  4270662 |  397 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   514839 |  398 | `			if( iParen <= 0 ){` |
|       16 |  399 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       16 |  400 | `				if( rc != SXERR_ABORT ){` |
|       16 |  401 | `					rc = SXERR_SYNTAX;` |
|        6 |  402 | `				}` |
|       16 |  403 | `				return rc;` |
|        - |  404 | `			}` |
|   514827 |  405 | `			iParen--;` |
|  3755822 |  406 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    98413 |  407 | `			iSquare++;` |
|  3449207 |  408 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    98427 |  409 | `			if( iSquare <= 0 ){` |
|        8 |  410 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        8 |  411 | `				if( rc != SXERR_ABORT ){` |
|        8 |  412 | `					rc = SXERR_SYNTAX;` |
|        3 |  413 | `				}` |
|        8 |  414 | `				return rc;` |
|        - |  415 | `			}` |
|    98421 |  416 | `			iSquare--;` |
|  3350789 |  417 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  3301572 |  464 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       24 |  465 | `			if( iBraces <= 0 ){` |
|       15 |  466 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       15 |  467 | `				if( rc != SXERR_ABORT ){` |
|       15 |  468 | `					rc = SXERR_SYNTAX;` |
|        6 |  469 | `				}` |
|       15 |  470 | `				return rc;` |
|        - |  471 | `			}` |
|       10 |  472 | `			iBraces--;` |
|  3301547 |  473 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2903 |  474 | `			if( iQuesty > 0 ){` |
|     2649 |  475 | `				iQuesty--;` |
|     1581 |  476 | `			}else if( iParen <= 0 ){` |
|        - |  477 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  478 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  479 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        6 |  480 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        6 |  481 | `				if( rc != SXERR_ABORT ){` |
|        6 |  482 | `					rc = SXERR_SYNTAX;` |
|        2 |  483 | `				}` |
|        6 |  484 | `				return rc;` |
|        5 |  485 | `			}` |
|  3300092 |  486 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   924697 |  487 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   924697 |  488 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2651 |  489 | `				iQuesty++;` |
|   923374 |  490 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      401 |  491 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      198 |  509 | `			}` |
|   462346 |  510 | `		}` |
|  2264025 |  511 | `	}` |
|  1024531 |  512 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       20 |  513 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       20 |  514 | `		if( rc != SXERR_ABORT ){` |
|       20 |  515 | `			rc = SXERR_SYNTAX;` |
|        8 |  516 | `		}` |
|       20 |  517 | `		return rc;` |
|        - |  518 | `	}` |
|  1024515 |  519 | `	return SXRET_OK;` |
|   512285 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  523 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  524 | ` */` |
|   849268 |  525 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  526 | `{` |
|   849273 |  527 | `	SyToken *pIn = *ppCur;` |
|        - |  528 | `	/* Jump the first literal seen */` |
|   849273 |  529 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   845395 |  530 | `		pIn++;` |
|   422695 |  531 | `	}` |
|   426600 |  532 | `	for(;;){` |
|   853205 |  533 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|     3937 |  534 | `			pIn++;` |
|     3937 |  535 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     3935 |  536 | `				pIn++;` |
|     1965 |  537 | `			}` |
|     1971 |  538 | `		}else{` |
|   424639 |  539 | `			break;` |
|        - |  540 | `		}` |
|        5 |  541 | `	}` |
|        - |  542 | `	/* Synchronize pointers */` |
|   849273 |  543 | `	*ppCur = pIn;` |
|   849273 |  544 | `}` |
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
|        - |  578 | `/*` |
|        - |  579 | ` * Skip an optional return-type declaration at *ppIn:` |
|        - |  580 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|        - |  581 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|        - |  582 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|        - |  583 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|        - |  584 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|        - |  585 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|        - |  586 | ` * authoritative type parse, so this must accept every shape it does` |
|        - |  587 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|        - |  588 | ` */` |
|      542 |  589 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|        5 |  590 | `{` |
|      547 |  591 | `	SyToken *pIn = *ppIn;` |
|      547 |  592 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|       25 |  593 | `		pIn++; /* Skip ':' */` |
|       11 |  594 | `		for(;;){` |
|        - |  595 | `			/* Optional '?' nullable prefix */` |
|       29 |  596 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        6 |  597 | `				pIn++;` |
|        2 |  598 | `			}` |
|       29 |  599 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - |  600 | `				/* Parenthesized DNF group '(A&B)' */` |
|      ! 0 |  601 | `				pIn++;` |
|      ! 0 |  602 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      ! 0 |  603 | `				if( pIn < pEnd ){` |
|      ! 0 |  604 | `					pIn++; /* ')' */` |
|      ! 0 |  605 | `				}` |
|       26 |  606 | `			}else if( pIn < pEnd` |
|       29 |  607 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|        - |  608 | `				/* ['\']Name('\'Name)* */` |
|       29 |  609 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|       29 |  610 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       29 |  611 | `					pIn++;` |
|       29 |  612 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  613 | `						pIn += 2;` |
|      ! 0 |  614 | `					}` |
|       13 |  615 | `				}` |
|       16 |  616 | `			}else{` |
|        - |  617 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|      ! 0 |  618 | `				break;` |
|        - |  619 | `			}` |
|        - |  620 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|       26 |  621 | `			if( pIn < pEnd` |
|       29 |  622 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|       26 |  623 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|        5 |  624 | `				pIn++;` |
|        5 |  625 | `				continue;` |
|        - |  626 | `			}` |
|       25 |  627 | `			break;` |
|      ! 0 |  628 | `		}` |
|       11 |  629 | `	}` |
|      547 |  630 | `	*ppIn = pIn;` |
|      547 |  631 | `}` |
|      346 |  632 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  633 | `{` |
|      351 |  634 | `	SyToken *pIn = *ppCur;` |
|        - |  635 | `	sxu32 nLine;` |
|        - |  636 | `	sxi32 rc;` |
|        - |  637 | `	/* Jump the 'function' keyword */` |
|      351 |  638 | `	nLine = pIn->nLine;` |
|      351 |  639 | `	pIn++;` |
|      351 |  640 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  641 | `		pIn++;` |
|        1 |  642 | `	}` |
|      351 |  643 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  644 | `		/* Syntax error */` |
|        6 |  645 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  646 | `		if( rc != SXERR_ABORT ){` |
|        6 |  647 | `			rc = SXERR_SYNTAX;` |
|        2 |  648 | `		}` |
|        6 |  649 | `		goto Synchronize;` |
|        - |  650 | `	}` |
|      347 |  651 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      347 |  652 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      347 |  653 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  654 | `		/* Syntax error */` |
|        6 |  655 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  656 | `		if( rc != SXERR_ABORT ){` |
|        6 |  657 | `			rc = SXERR_SYNTAX;` |
|        2 |  658 | `		}` |
|        6 |  659 | `		goto Synchronize;` |
|        - |  660 | `	}` |
|      343 |  661 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  662 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      343 |  663 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      343 |  664 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       45 |  665 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  666 | `		/* Check if we are dealing with a closure */` |
|       45 |  667 | `		if( nKey == PH7_TKWRD_USE ){` |
|       37 |  668 | `			pIn++; /* Jump the 'use' keyword */` |
|       37 |  669 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  670 | `				/* Syntax error */` |
|        6 |  671 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  672 | `				if( rc != SXERR_ABORT ){` |
|        6 |  673 | `					rc = SXERR_SYNTAX;` |
|        2 |  674 | `				}` |
|        6 |  675 | `				goto Synchronize;` |
|        - |  676 | `			}` |
|       33 |  677 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       33 |  678 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       33 |  679 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  680 | `				/* Syntax error */` |
|        6 |  681 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  682 | `				if( rc != SXERR_ABORT ){` |
|        6 |  683 | `					rc = SXERR_SYNTAX;` |
|        2 |  684 | `				}` |
|        6 |  685 | `				goto Synchronize;` |
|        - |  686 | `			}` |
|       29 |  687 | `			pIn++;` |
|        - |  688 | `			/* php 7.1+: the return type may also follow the use clause —` |
|        - |  689 | ``			 * `function (...) use (...) : int {` */`` |
|       29 |  690 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       17 |  691 | `		}else{` |
|        - |  692 | `			/* Syntax error */` |
|       11 |  693 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|       11 |  694 | `			if( rc != SXERR_ABORT ){` |
|       11 |  695 | `				rc = SXERR_SYNTAX;` |
|        4 |  696 | `			}` |
|       11 |  697 | `			goto Synchronize;` |
|        - |  698 | `		}` |
|       12 |  699 | `	}` |
|        - |  700 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|        - |  701 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|        - |  702 | `	 * the type), and pEnd is one past the last token. */` |
|      327 |  703 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      327 |  704 | `		pIn++; /* Jump the leading curly '{' */` |
|      327 |  705 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      327 |  706 | `		if( pIn < pEnd ){` |
|      327 |  707 | `			pIn++;` |
|      161 |  708 | `		}` |
|      166 |  709 | `	}else{` |
|        - |  710 | `		/* Syntax error */` |
|      ! 0 |  711 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  712 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  713 | `			return SXERR_ABORT;` |
|        - |  714 | `		}` |
|        - |  715 | `	}` |
|      327 |  716 | `	rc = SXRET_OK;` |
|      173 |  717 | `Synchronize:` |
|        - |  718 | `	/* Synchronize pointers */` |
|      351 |  719 | `	*ppCur = pIn;` |
|      351 |  720 | `	return rc;` |
|      178 |  721 | `}` |
|        - |  722 | `/*` |
|        - |  723 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|        - |  724 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|        - |  725 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|        - |  726 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|        - |  727 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|        - |  728 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|        - |  729 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|        - |  730 | ` */` |
|       26 |  731 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  732 | `{` |
|       30 |  733 | `	SyToken *pIn = *ppCur;` |
|       30 |  734 | `	sxu32 nLine = pIn->nLine;` |
|        - |  735 | `	sxi32 rc;` |
|       30 |  736 | `	pIn++; /* Jump the 'class' keyword */` |
|        - |  737 | `	/* Optional constructor argument list */` |
|       30 |  738 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  739 | `		pIn++; /* Jump '(' */` |
|        7 |  740 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        7 |  741 | `		if( pIn < pEnd ){` |
|        7 |  742 | `			pIn++; /* Jump ')' */` |
|        3 |  743 | `		}` |
|        3 |  744 | `	}` |
|        - |  745 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|        - |  746 | `	 * (no braces appear between ')' and the class body). */` |
|       58 |  747 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       32 |  748 | `		pIn++;` |
|        4 |  749 | `	}` |
|       30 |  750 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|        - |  751 | `		/* Syntax error: missing class body */` |
|      ! 0 |  752 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  753 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|      ! 0 |  754 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  755 | `			rc = SXERR_SYNTAX;` |
|      ! 0 |  756 | `		}` |
|      ! 0 |  757 | `		*ppCur = pIn;` |
|      ! 0 |  758 | `		return rc;` |
|        - |  759 | `	}` |
|       30 |  760 | `	pIn++; /* Jump the leading '{' */` |
|       30 |  761 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       30 |  762 | `	if( pIn < pEnd ){` |
|       30 |  763 | `		pIn++; /* Jump the trailing '}' */` |
|       13 |  764 | `	}` |
|       30 |  765 | `	*ppCur = pIn;` |
|       30 |  766 | `	return SXRET_OK;` |
|       17 |  767 | `}` |
|        - |  768 | `/*` |
|        - |  769 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  770 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  771 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  772 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  773 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  774 | ` */` |
|      180 |  775 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  776 | `{` |
|      184 |  777 | `	SyToken *pIn = *ppCur;` |
|        - |  778 | `	sxu32 nLine;` |
|        - |  779 | `	sxi32 rc;` |
|        - |  780 | `	int iNest;` |
|      184 |  781 | `	nLine = pIn->nLine;` |
|        - |  782 | `	/* Optional 'static' prefix */` |
|      180 |  783 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      184 |  784 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  785 | `		pIn++;` |
|        1 |  786 | `	}` |
|        - |  787 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      180 |  788 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      184 |  789 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  790 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  791 | `		goto Synchronize;` |
|        - |  792 | `	}` |
|      184 |  793 | `	pIn++; /* Jump 'fn' */` |
|       90 |  794 | `	SXUNUSED(nLine);` |
|       90 |  795 | `	SXUNUSED(pGen);` |
|        - |  796 | `	/* Optional '&' for return-by-reference */` |
|      184 |  797 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  798 | `		pIn++;` |
|      ! 0 |  799 | `	}` |
|        - |  800 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  801 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  802 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  803 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      184 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      181 |  805 | `		pIn++; /* '(' */` |
|      181 |  806 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      181 |  807 | `		if( pIn < pEnd ){` |
|      179 |  808 | `			pIn++; /* ')' */` |
|       88 |  809 | `		}` |
|       89 |  810 | `	}` |
|        - |  811 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      184 |  812 | `	ExprSkipReturnType(&pIn,pEnd);` |
|        - |  813 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      184 |  814 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      177 |  815 | `		pIn++;` |
|       87 |  816 | `	}` |
|        - |  817 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      184 |  818 | `	iNest = 0;` |
|     1090 |  819 | `	while( pIn < pEnd ){` |
|      988 |  820 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  821 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       79 |  822 | `			break;` |
|        - |  823 | `		}` |
|      910 |  824 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       65 |  825 | `			iNest++;` |
|      879 |  826 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       65 |  827 | `			iNest--;` |
|       31 |  828 | `		}` |
|      910 |  829 | `		pIn++;` |
|        4 |  830 | `	}` |
|      184 |  831 | `	rc = SXRET_OK;` |
|       90 |  832 | `Synchronize:` |
|      184 |  833 | `	*ppCur = pIn;` |
|      184 |  834 | `	return rc;` |
|        4 |  835 | `}` |
|        - |  836 | `/*` |
|        - |  837 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  838 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  839 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  840 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  841 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  842 | ` */` |
|       70 |  843 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  844 | `{` |
|       75 |  845 | `	SyToken *pIn = *ppCur;` |
|        - |  846 | `	sxi32 rc;` |
|       35 |  847 | `	SXUNUSED(pGen);` |
|        - |  848 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  849 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       75 |  850 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  851 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  852 | `		goto Synchronize;` |
|        - |  853 | `	}` |
|       75 |  854 | `	pIn++; /* Jump 'match' */` |
|        - |  855 | `	/* Optional '(' subject ')' */` |
|       75 |  856 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       75 |  857 | `		pIn++;` |
|       75 |  858 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       75 |  859 | `		if( pIn < pEnd ){` |
|       75 |  860 | `			pIn++; /* ')' */` |
|       35 |  861 | `		}` |
|       35 |  862 | `	}` |
|        - |  863 | `	/* Optional '{' arms '}' */` |
|       75 |  864 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       75 |  865 | `		pIn++;` |
|       75 |  866 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       75 |  867 | `		if( pIn < pEnd ){` |
|       75 |  868 | `			pIn++; /* '}' */` |
|       35 |  869 | `		}` |
|       35 |  870 | `	}` |
|       75 |  871 | `	rc = SXRET_OK;` |
|       35 |  872 | `Synchronize:` |
|       75 |  873 | `	*ppCur = pIn;` |
|       75 |  874 | `	return rc;` |
|        5 |  875 | `}` |
|        - |  876 | `/*` |
|        - |  877 | ` * Extract a single expression node from the input.` |
|        - |  878 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  879 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  880 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  881 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  882 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  883 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  884 | ` */` |
|  4533408 |  885 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|        5 |  886 | `{` |
|        - |  887 | `	ph7_expr_node *pNode;` |
|        - |  888 | `	SyToken *pCur;` |
|        - |  889 | `	sxi32 rc;` |
|        - |  890 | `	/* Allocate a new node */` |
|  4533413 |  891 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4533413 |  892 | `	if( pNode == 0 ){` |
|        - |  893 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  894 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  895 | `		 */` |
|      ! 0 |  896 | `		return SXERR_MEM;` |
|        - |  897 | `	}` |
|        - |  898 | `	/* Zero the structure */` |
|  4533413 |  899 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4533413 |  900 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  901 | `	/* Point to the head of the token stream */` |
|  4533413 |  902 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  903 | `	/* Start collecting tokens */` |
|  4533413 |  904 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|     3987 |  905 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|        - |  906 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|        - |  907 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|        - |  908 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|        - |  909 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       65 |  910 | `			pNode->pEnd = pCur;` |
|       65 |  911 | `			pCur++;` |
|       65 |  912 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       65 |  913 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       65 |  914 | `			pGen->pIn = pCur;` |
|       65 |  915 | `			*ppNode = pNode;` |
|       65 |  916 | `			return SXRET_OK;` |
|        - |  917 | `		}` |
|        - |  918 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  919 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|     3923 |  920 | `		pCur++;` |
|     3923 |  921 | `		pGen->pIn = pCur;` |
|     3923 |  922 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|     3923 |  923 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|     3923 |  924 | `		if( rc == SXRET_OK && *ppNode ){` |
|     3923 |  925 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|     1959 |  926 | `		}` |
|     3923 |  927 | `		return rc;` |
|        - |  928 | `	}` |
|  4529431 |  929 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  930 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  931 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  932 | `		 */` |
|     1273 |  933 | `		pCur++; /* Skip the opening '[' */` |
|     1273 |  934 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1273 |  935 | `		if( pCur < pGen->pEnd ){` |
|     1273 |  936 | `			pCur++; /* Skip past the closing ']' */` |
|      639 |  937 | `		}else{` |
|      ! 0 |  938 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  939 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  940 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  941 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  942 | `			}` |
|      ! 0 |  943 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  944 | `			return rc;` |
|        - |  945 | `		}` |
|        - |  946 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  947 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  948 | `		 */` |
|     1359 |  949 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      176 |  950 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      176 |  951 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       54 |  952 | `				pNode->xCode = PH7_CompileShortList;` |
|       29 |  953 | `			}else{` |
|      123 |  954 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  955 | `			}` |
|       90 |  956 | `		}else{` |
|     1101 |  957 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  958 | `		}` |
|  4528797 |  959 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  960 | `		/* Point to the instance that describe this operator */` |
|  1023139 |  961 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  962 | `		/* Advance the stream cursor */` |
|  1023139 |  963 | `		pCur++;` |
|  4016596 |  964 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  965 | `		/* Isolate variable */` |
|  2449129 |  966 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1224573 |  967 | `			pCur++; /* Variable variable */` |
|        5 |  968 | `		}` |
|  1224561 |  969 | `		if( pCur < pGen->pEnd ){` |
|  1224561 |  970 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  971 | `				/* Variable name */` |
|  1224533 |  972 | `				pCur++;` |
|   612296 |  973 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       24 |  974 | `				pCur++;` |
|        - |  975 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       24 |  976 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       24 |  977 | `				if( pCur < pGen->pEnd ){` |
|       19 |  978 | `					pCur++;` |
|       11 |  979 | `				}else{` |
|        6 |  980 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        6 |  981 | `					if( rc != SXERR_ABORT ){` |
|        6 |  982 | `						rc = SXERR_SYNTAX;` |
|        2 |  983 | `					}` |
|        6 |  984 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        6 |  985 | `					return rc;` |
|        - |  986 | `				}` |
|        8 |  987 | `			}` |
|   612276 |  988 | `		}` |
|  1224557 |  989 | `		pNode->xCode = PH7_CompileVariable;` |
|  2892749 |  990 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    57333 |  991 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    57333 |  992 | `		 if( bAfterMemberOp ){` |
|        - |  993 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|        - |  994 | `			  * method/property NAME, not a language construct — PHP allows any` |
|        - |  995 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|        - |  996 | `			  * as a plain literal like an ordinary identifier member name. */` |
|      183 |  997 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      183 |  998 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    57244 |  999 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - | 1000 | `			 /* List/Array node */` |
|    32505 | 1001 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1002 | `				 /* Assume a literal */` |
|      ! 0 | 1003 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1004 | `				 pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1005 | `			 }else{` |
|    32505 | 1006 | `				 pCur += 2;` |
|        - | 1007 | `				 /* Collect array/list tokens */` |
|    32505 | 1008 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    32505 | 1009 | `				 if( pCur < pGen->pEnd ){` |
|    32503 | 1010 | `					 pCur++;` |
|    16254 | 1011 | `				 }else{` |
|        - | 1012 | `					 /* Syntax error */` |
|        4 | 1013 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 | 1014 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 | 1015 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1016 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1017 | `					 }` |
|        3 | 1018 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1019 | `					 return rc;` |
|        - | 1020 | `				 }` |
|    32503 | 1021 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    32503 | 1022 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       37 | 1023 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       37 | 1024 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - | 1025 | `						 /* Syntax error */` |
|        3 | 1026 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 | 1027 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1028 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1029 | `						 }` |
|        3 | 1030 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1031 | `						 return rc;` |
|        - | 1032 | `					 }` |
|       15 | 1033 | `				 }` |
|        5 | 1034 | `			 }` |
|    40903 | 1035 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1036 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      333 | 1037 | `			 pCur++; /* Skip 'yield' keyword */` |
|      333 | 1038 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1039 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1040 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      333 | 1041 | `			 pNode->xCode = PH7_CompileYield;` |
|    24491 | 1042 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1043 | `			 /* Annonymous function */` |
|      351 | 1044 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1045 | `				 /* Assume a literal */` |
|      ! 0 | 1046 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1047 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1048 | `			 }else{` |
|        - | 1049 | `				 /* Assemble annonymous functions body */` |
|      351 | 1050 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      351 | 1051 | `				 if( rc != SXRET_OK ){` |
|       28 | 1052 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1053 | `					 return rc;` |
|        - | 1054 | `				 }` |
|      327 | 1055 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1056 | `			  }` |
|    24142 | 1057 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       39 | 1058 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       22 | 1059 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       12 | 1060 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        9 | 1061 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1062 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1063 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1064 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1065 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       30 | 1066 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       30 | 1067 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1068 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1069 | `				 return rc;` |
|        - | 1070 | `			 }` |
|       30 | 1071 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    23967 | 1072 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    23867 | 1073 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       30 | 1074 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       15 | 1075 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1076 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      184 | 1077 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      184 | 1078 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1079 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1080 | `				 return rc;` |
|        - | 1081 | `			 }` |
|      184 | 1082 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    23865 | 1083 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1084 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1085 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1086 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1087 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1088 | `				 return rc;` |
|        - | 1089 | `			 }` |
|       75 | 1090 | `			 pNode->xCode = PH7_CompileMatch;` |
|    23740 | 1091 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1092 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1093 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1094 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1095 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1096 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1097 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1098 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1099 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    23687 | 1100 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1101 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       93 | 1102 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       93 | 1103 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       49 | 1104 | `		 }else{` |
|        - | 1105 | `			 /* Assume a literal */` |
|    23581 | 1106 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    23581 | 1107 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1108 | `		 }` |
|  2251795 | 1109 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1110 | `		 /* Constants,function name,namespace path,class name... */` |
|   825519 | 1111 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   825519 | 1112 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   412762 | 1113 | `	 }else{` |
|  1397631 | 1114 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1115 | `			 /* Point to the code generator routine */` |
|   266583 | 1116 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   266583 | 1117 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1118 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1119 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1120 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1121 | `				 }` |
|        3 | 1122 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1123 | `				 return rc;` |
|        - | 1124 | `			 }` |
|   133288 | 1125 | `		 }` |
|        - | 1126 | `		/* Advance the stream cursor */` |
|  1397629 | 1127 | `		pCur++;` |
|        - | 1128 | `	 }` |
|        - | 1129 | `	/* Point to the end of the token stream */` |
|  4529397 | 1130 | `	pNode->pEnd = pCur;` |
|        - | 1131 | `	/* Save the node for later processing */` |
|  4529397 | 1132 | `	*ppNode = pNode;` |
|        - | 1133 | `	/* Synchronize cursors */` |
|  4529397 | 1134 | `	pGen->pIn = pCur;` |
|  4529397 | 1135 | `	return SXRET_OK;` |
|  2266709 | 1136 | `}` |
|        - | 1137 | `/*` |
|        - | 1138 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1139 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1140 | ` * level is zero.` |
|        - | 1141 | ` */` |
|    99328 | 1142 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1143 | `{` |
|    99333 | 1144 | `	SyToken *pCur = pStart;` |
|    99333 | 1145 | `	sxi32 iNest = 0;` |
|    99333 | 1146 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1147 | `		/* Last expression */` |
|    51529 | 1148 | `		return SXERR_EOF;` |
|        - | 1149 | `	}` |
|   196129 | 1150 | `	while( pCur < pEnd ){` |
|   178917 | 1151 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    30597 | 1152 | `			break;` |
|        - | 1153 | `		}` |
|   148325 | 1154 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    10311 | 1155 | `			iNest++;` |
|   143172 | 1156 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    10313 | 1157 | `			iNest--;` |
|     5154 | 1158 | `		}` |
|   148325 | 1159 | `		pCur++;` |
|        5 | 1160 | `	}` |
|    47809 | 1161 | `	*ppNext = pCur;` |
|    47809 | 1162 | `	return SXRET_OK;` |
|    49669 | 1163 | `}` |
|        - | 1164 | `/*` |
|        - | 1165 | ` * Free an expression tree.` |
|        - | 1166 | ` */` |
|  3870576 | 1167 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1168 | `{` |
|  3870581 | 1169 | `	if( pNode->pLeft ){` |
|        - | 1170 | `		/* Release the left tree */` |
|  1429881 | 1171 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   714938 | 1172 | `	}` |
|  3870581 | 1173 | `	if( pNode->pRight ){` |
|        - | 1174 | `		/* Release the right tree */` |
|   772567 | 1175 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   386281 | 1176 | `	}` |
|  3870581 | 1177 | `	if( pNode->pCond ){` |
|        - | 1178 | `		/* Release the conditional tree used by the ternary operator */` |
|     2647 | 1179 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1321 | 1180 | `	}` |
|  3870581 | 1181 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1182 | `		ph7_expr_node **apArg;` |
|        - | 1183 | `		sxu32 n;` |
|        - | 1184 | `		/* Release node arguments */` |
|   499801 | 1185 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  1075013 | 1186 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   575217 | 1187 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   287611 | 1188 | `		}` |
|   499801 | 1189 | `		SySetRelease(&pNode->aNodeArgs);` |
|   249898 | 1190 | `	}` |
|        - | 1191 | `	/* Finally,release this node */` |
|  3870581 | 1192 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3870581 | 1193 | `}` |
|        - | 1194 | `/*` |
|        - | 1195 | ` * Free an expression tree.` |
|        - | 1196 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1197 | ` */` |
|  1024594 | 1198 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1199 | `{` |
|        - | 1200 | `	ph7_expr_node **apNode;` |
|        - | 1201 | `	sxu32 n;` |
|  1024599 | 1202 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5554055 | 1203 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4529461 | 1204 | `		if( apNode[n] ){` |
|  1024933 | 1205 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   512464 | 1206 | `		}` |
|  2264733 | 1207 | `	}` |
|  1024599 | 1208 | `	return SXRET_OK;` |
|        5 | 1209 | `}` |
|        - | 1210 | `/*` |
|        - | 1211 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1212 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1213 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1214 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1215 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1216 | ` */` |
|  1398614 | 1217 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1218 | `{` |
|  1398619 | 1219 | `	if( pNode == 0 ){` |
|   862847 | 1220 | `		return 0;` |
|        - | 1221 | `	}` |
|   535777 | 1222 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1223 | `		return 1;` |
|        - | 1224 | `	}` |
|   535765 | 1225 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1226 | `		return 1;` |
|        - | 1227 | `	}` |
|   535761 | 1228 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1229 | `		return 1;` |
|        - | 1230 | `	}` |
|   535761 | 1231 | `	return 0;` |
|   699312 | 1232 | `}` |
|        - | 1233 | `/*` |
|        - | 1234 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1235 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1236 | ` */` |
|   320420 | 1237 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1238 | `{` |
|        - | 1239 | `	sxi32 iExprOp;` |
|   320425 | 1240 | `	if( pNode->pOp == 0 ){` |
|   196835 | 1241 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1242 | `	}` |
|   123595 | 1243 | `	iExprOp = pNode->pOp->iOp;` |
|   123595 | 1244 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    84795 | 1245 | `			return TRUE;` |
|        - | 1246 | `	}` |
|    38805 | 1247 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    38799 | 1248 | `		if( pNode->pLeft->pOp ) {` |
|       68 | 1249 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       31 | 1250 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1251 | `				return FALSE;` |
|        5 | 1252 | `			}` |
|    38765 | 1253 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1254 | `			return FALSE;` |
|        - | 1255 | `		}` |
|    38799 | 1256 | `		return TRUE;` |
|        - | 1257 | `	}` |
|        8 | 1258 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        8 | 1259 | `		return TRUE;` |
|        - | 1260 | `	}` |
|        - | 1261 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1262 | `	return FALSE;` |
|   160215 | 1263 | `}` |
|        - | 1264 | `/* Forward declaration */` |
|        - | 1265 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1266 | `/* Macro to check if the given node is a terminal.` |
|        - | 1267 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1268 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1269 | ` * linked ternary/elvis node). */` |
|        - | 1270 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1271 | `/*` |
|        - | 1272 | ` * Buid an expression tree for each given function argument.` |
|        - | 1273 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1274 | ` */` |
|   420646 | 1275 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1276 | `{` |
|        - | 1277 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1278 | `	sxi32 rc;` |
|        - | 1279 | `	/* Process function arguments from left to right */` |
|   420651 | 1280 | `	iCur = 0;` |
|   458342 | 1281 | `	for(;;){` |
|   916689 | 1282 | `		if( iCur >= nToken ){` |
|        - | 1283 | `			/* No more arguments to process */` |
|   420625 | 1284 | `			break;` |
|        - | 1285 | `		}` |
|   496069 | 1286 | `		iNode = iCur;` |
|   496069 | 1287 | `		iNest = 0;` |
|  1227925 | 1288 | `		while( iCur < nToken ){` |
|   807303 | 1289 | `			if( apNode[iCur] ){` |
|   791983 | 1290 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    37726 | 1291 | `					break;` |
|   716536 | 1292 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   378576 | 1293 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    40395 | 1294 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1295 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1296 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1297 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1298 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1299 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1300 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    40169 | 1301 | `					iNest++;` |
|   696459 | 1302 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    40169 | 1303 | `					iNest--;` |
|    20082 | 1304 | `				}` |
|   358268 | 1305 | `			}` |
|   731861 | 1306 | `			iCur++;` |
|        5 | 1307 | `		}` |
|   496069 | 1308 | `		if( iCur > iNode ){` |
|   496063 | 1309 | `			SyString sArgName = {0, 0};` |
|        - | 1310 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1311 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1312 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   496058 | 1313 | `			if( (iCur - iNode) >= 2` |
|   274408 | 1314 | `				&& apNode[iNode]` |
|    52758 | 1315 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    31018 | 1316 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     9136 | 1317 | `				&& apNode[iNode+1]` |
|     8999 | 1318 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1319 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      255 | 1320 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      255 | 1321 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      255 | 1322 | `				apNode[iNode] = 0;` |
|      255 | 1323 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      255 | 1324 | `				apNode[iNode+1] = 0;` |
|      255 | 1325 | `				iNode += 2;` |
|        - | 1326 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1327 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      255 | 1328 | `				if( iNode >= iCur ){` |
|        4 | 1329 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1330 | `						pOp->pStart->nLine,` |
|        - | 1331 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1332 | `						&sArgName);` |
|        3 | 1333 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1334 | `						rc = SXERR_SYNTAX;` |
|        1 | 1335 | `					}` |
|        3 | 1336 | `					return rc;` |
|        - | 1337 | `				}` |
|      124 | 1338 | `			}` |
|   496056 | 1339 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1340 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1341 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1342 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1343 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1344 | `					apNode[iNode] = 0;` |
|      ! 0 | 1345 | `			}` |
|   496061 | 1346 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   496061 | 1347 | `			if( apNode[iNode] ){` |
|   496061 | 1348 | `				if( sArgName.nByte > 0 ){` |
|      253 | 1349 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      253 | 1350 | `					apNode[iNode]->sArgName = sArgName;` |
|      124 | 1351 | `				}` |
|        - | 1352 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   496061 | 1353 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   248033 | 1354 | `			}else{` |
|        - | 1355 | `				/* No expression before comma */` |
|      ! 0 | 1356 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1357 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1358 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1359 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1360 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1361 | `				}` |
|      ! 0 | 1362 | `				return rc;` |
|        - | 1363 | `			}` |
|   248033 | 1364 | `		}else{` |
|        - | 1365 | `			/* Comma with no preceding argument */` |
|        9 | 1366 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        9 | 1367 | `			if( rc != SXERR_ABORT ){` |
|        9 | 1368 | `				rc = SXERR_SYNTAX;` |
|        3 | 1369 | `			}` |
|        9 | 1370 | `			return rc;` |
|        - | 1371 | `		}` |
|        - | 1372 | `		/* Jump trailing comma */` |
|   496061 | 1373 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    75441 | 1374 | `			iCur++;` |
|    75441 | 1375 | `			if( iCur >= nToken ){` |
|        - | 1376 | `				/* Trailing comma after last argument */` |
|       19 | 1377 | `				break;` |
|        - | 1378 | `			}` |
|    37709 | 1379 | `		}` |
|        5 | 1380 | `	}` |
|   420643 | 1381 | `	return SXRET_OK;` |
|   210328 | 1382 | `}` |
|        - | 1383 | ` /*` |
|        - | 1384 | `  * Create an expression tree from an array of tokens.` |
|        - | 1385 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1386 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1387 | `  */` |
|  1641542 | 1388 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1389 | ` {` |
|        - | 1390 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1391 | `	 ph7_expr_node *pNode;` |
|        - | 1392 | `	 sxi32 iCur;` |
|        - | 1393 | `	 sxi32 rc;` |
|  1641547 | 1394 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1395 | `		 /* TICKET 1433-17: self evaluating node */` |
|   768841 | 1396 | `		 return SXRET_OK;` |
|        - | 1397 | `	 }` |
|        - | 1398 | `	 /* Process expressions enclosed in parenthesis first */` |
|  5417483 | 1399 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1400 | `		 sxi32 iNest;` |
|        - | 1401 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1402 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1403 | `		  */` |
|  4544779 | 1404 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4512357 | 1405 | `			 continue;` |
|        - | 1406 | `		 }` |
|    32427 | 1407 | `		 iNest = 1;` |
|    32427 | 1408 | `		 iLeft = iCur;` |
|        - | 1409 | `		 /* Find the closing parenthesis */` |
|    32427 | 1410 | `		 iCur++;` |
|   216155 | 1411 | `		 while( iCur < nToken ){` |
|   216155 | 1412 | `			 if( apNode[iCur] ){` |
|   216155 | 1413 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1414 | `					 /* Decrement nesting level */` |
|    56319 | 1415 | `					 iNest--;` |
|    56319 | 1416 | `					 if( iNest <= 0 ){` |
|    32427 | 1417 | `						 break;` |
|        5 | 1418 | `					 }` |
|   171787 | 1419 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1420 | `					 /* Increment nesting level */` |
|    23897 | 1421 | `					 iNest++;` |
|    11946 | 1422 | `				 }` |
|    91864 | 1423 | `			 }` |
|   183733 | 1424 | `			 iCur++;` |
|        5 | 1425 | `		 }` |
|    32427 | 1426 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1427 | `			 sxi32 j;` |
|        - | 1428 | `			 /* Recurse and process this expression */` |
|    32427 | 1429 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    32427 | 1430 | `			 if( rc != SXRET_OK ){` |
|        3 | 1431 | `				 return rc;` |
|        - | 1432 | `			 }` |
|        - | 1433 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1434 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1435 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    32425 | 1436 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    32425 | 1437 | `				 if( apNode[j] ){` |
|    32425 | 1438 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    32425 | 1439 | `					 break;` |
|        - | 1440 | `				 }` |
|      ! 0 | 1441 | `			 }` |
|    16210 | 1442 | `		 }` |
|        - | 1443 | `		 /* Free the left and right nodes */` |
|    32425 | 1444 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    32425 | 1445 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    32425 | 1446 | `		 apNode[iLeft] = 0;` |
|    32425 | 1447 | `		 apNode[iCur] = 0;` |
|    16215 | 1448 | `	 }` |
|        - | 1449 | `	  /* Process expressions enclosed in braces */` |
|  5625255 | 1450 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1451 | `		 sxi32 iNest;` |
|        - | 1452 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1453 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1454 | `		  */` |
|  4760903 | 1455 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4760895 | 1456 | `			 continue;` |
|        - | 1457 | `		 }` |
|       10 | 1458 | `		 iNest = 1;` |
|       10 | 1459 | `		 iLeft = iCur;` |
|        - | 1460 | `		 /* Find the closing parenthesis */` |
|       10 | 1461 | `		 iCur++;` |
|       16 | 1462 | `		 while( iCur < nToken ){` |
|       16 | 1463 | `			 if( apNode[iCur] ){` |
|       16 | 1464 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1465 | `					 /* Decrement nesting level */` |
|       10 | 1466 | `					 iNest--;` |
|       10 | 1467 | `					 if( iNest <= 0 ){` |
|       10 | 1468 | `						 break;` |
|      ! 0 | 1469 | `					 }` |
|        7 | 1470 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1471 | `					 /* Increment nesting level */` |
|      ! 0 | 1472 | `					 iNest++;` |
|      ! 0 | 1473 | `				 }` |
|        3 | 1474 | `			 }` |
|        7 | 1475 | `			 iCur++;` |
|        1 | 1476 | `		 }` |
|       10 | 1477 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1478 | `			 /* Recurse and process this expression */` |
|        7 | 1479 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        7 | 1480 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1481 | `				 return rc;` |
|        - | 1482 | `			 }` |
|        3 | 1483 | `		 }` |
|        - | 1484 | `		 /* Free the left and right nodes */` |
|       10 | 1485 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       10 | 1486 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       10 | 1487 | `		 apNode[iLeft] = 0;` |
|       10 | 1488 | `		 apNode[iCur] = 0;` |
|        6 | 1489 | `	 }` |
|        - | 1490 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   864357 | 1491 | `	 iLeft = -1;` |
|  5625233 | 1492 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4760893 | 1493 | `		 if( apNode[iCur] == 0 ){` |
|  1875881 | 1494 | `			 continue;` |
|        - | 1495 | `		 }` |
|  2885017 | 1496 | `		 pNode = apNode[iCur];` |
|  2885017 | 1497 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   772227 | 1498 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1499 | `				 /* Collect function arguments */` |
|   482403 | 1500 | `				 sxi32 iPtr = 0;` |
|   482403 | 1501 | `				 sxi32 nFuncTok = 0;` |
|  1772101 | 1502 | `				 while( nFuncTok + iCur < nToken ){` |
|  1772101 | 1503 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1756781 | 1504 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   502965 | 1505 | `							 iPtr++;` |
|  1505301 | 1506 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   502965 | 1507 | `							 iPtr--;` |
|   502965 | 1508 | `							 if( iPtr <= 0 ){` |
|   482403 | 1509 | `								 break;` |
|        - | 1510 | `							 }` |
|    10281 | 1511 | `						 }` |
|   637189 | 1512 | `					 }` |
|  1289703 | 1513 | `					 nFuncTok++;` |
|        5 | 1514 | `				 }` |
|   482403 | 1515 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1516 | `					 /* Syntax error */` |
|      ! 0 | 1517 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1518 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1519 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1520 | `					 }` |
|      ! 0 | 1521 | `					 return rc;` |
|        - | 1522 | `				 }` |
|   482403 | 1523 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1524 | `					 /* Syntax error */` |
|      ! 0 | 1525 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1526 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1527 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1528 | `					 }` |
|      ! 0 | 1529 | `					 return rc;` |
|        - | 1530 | `				 }` |
|   482403 | 1531 | `				 if( nFuncTok > 1 ){` |
|        - | 1532 | `					 /* Process function arguments */` |
|   420651 | 1533 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   420651 | 1534 | `					 if( rc != SXRET_OK ){` |
|       11 | 1535 | `						 return rc;` |
|        - | 1536 | `					 }` |
|   210319 | 1537 | `				 }` |
|        - | 1538 | `				 /* Link the node to the tree */` |
|   482395 | 1539 | `				 pNode->pLeft = apNode[iLeft];` |
|   482395 | 1540 | `				 apNode[iLeft] = 0;` |
|  1772069 | 1541 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1289679 | 1542 | `					 apNode[iCur+iPtr] = 0;` |
|   644842 | 1543 | `				 }` |
|   531024 | 1544 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1545 | `				 /* Subscripting */` |
|    98421 | 1546 | `				 sxi32 iArrTok = iCur + 1;` |
|    98421 | 1547 | `				 sxi32 iNest = 1;` |
|    98416 | 1548 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1549 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1550 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|      226 | 1551 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    98416 | 1552 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1553 | `						 /* Syntax error */` |
|      ! 0 | 1554 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1555 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1556 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1557 | `						 }` |
|      ! 0 | 1558 | `						 return rc;` |
|        - | 1559 | `				 }` |
|        - | 1560 | `				 /* Collect index tokens */` |
|   177713 | 1561 | `				 while( iArrTok < nToken ){` |
|   177713 | 1562 | `					 if( apNode[iArrTok] ){` |
|   177681 | 1563 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1564 | `							 /* Increment nesting level */` |
|      ! 0 | 1565 | `							 iNest++;` |
|   177681 | 1566 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1567 | `							 /* Decrement nesting level */` |
|    98421 | 1568 | `							 iNest--;` |
|    98421 | 1569 | `							 if( iNest <= 0 ){` |
|    98421 | 1570 | `								 break;` |
|        - | 1571 | `							 }` |
|      ! 0 | 1572 | `						 }` |
|    39630 | 1573 | `					 }` |
|    79297 | 1574 | `					 ++iArrTok;` |
|        5 | 1575 | `				 }` |
|    98421 | 1576 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1577 | `					 /* Recurse and process this expression */` |
|    79161 | 1578 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    79161 | 1579 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1580 | `						 return rc;` |
|        - | 1581 | `					 }` |
|        - | 1582 | `					 /* Link the node to it's index */` |
|    79161 | 1583 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    39578 | 1584 | `				 }` |
|        - | 1585 | `				 /* Link the node to the tree */` |
|    98421 | 1586 | `				 pNode->pLeft = apNode[iLeft];` |
|    98421 | 1587 | `				 pNode->pRight = 0;` |
|    98421 | 1588 | `				 apNode[iLeft] = 0;` |
|   276129 | 1589 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   177713 | 1590 | `					 apNode[iNest] = 0;` |
|    88859 | 1591 | `				 }` |
|    49213 | 1592 | `			 }else{` |
|        - | 1593 | `				 /* Member access operators [i.e: '->','::'] */` |
|   191413 | 1594 | `				  iRight = iCur + 1;` |
|   191419 | 1595 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        7 | 1596 | `					 iRight++;` |
|        1 | 1597 | `				 }` |
|   191413 | 1598 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1599 | `					 /* Syntax error */` |
|        5 | 1600 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1601 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1602 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1603 | `					 }` |
|        5 | 1604 | `					 return rc;` |
|        - | 1605 | `				 }` |
|        - | 1606 | `				 /* Link the node to the tree */` |
|   191409 | 1607 | `				 pNode->pLeft = apNode[iLeft];` |
|   191404 | 1608 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   191224 | 1609 | `					 && pNode->pLeft->pOp == 0 &&` |
|   190918 | 1610 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1611 | `						 /* Syntax error */` |
|      ! 0 | 1612 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1613 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1614 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1615 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1616 | `						 }` |
|      ! 0 | 1617 | `						 return rc;` |
|        - | 1618 | `				 }` |
|   191409 | 1619 | `				 pNode->pRight = apNode[iRight];` |
|   191409 | 1620 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1621 | `			 }` |
|   386105 | 1622 | `		 }` |
|  2885005 | 1623 | `		 iLeft = iCur;` |
|  1442505 | 1624 | `	 }` |
|        - | 1625 | `	 /* Handle left associative (new, clone) operators */` |
|  5625201 | 1626 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4760861 | 1627 | `		 if( apNode[iCur] == 0 ){` |
|  2672831 | 1628 | `			 continue;` |
|        - | 1629 | `		 }` |
|  2088035 | 1630 | `		 pNode = apNode[iCur];` |
|  2088035 | 1631 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1632 | `			 SyToken *pToken;` |
|        - | 1633 | `			 /* Get the left node */` |
|    24745 | 1634 | `			 iLeft = iCur + 1;` |
|    49235 | 1635 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    24495 | 1636 | `				 iLeft++;` |
|        5 | 1637 | `			 }` |
|    24745 | 1638 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1639 | `				  /* Syntax error */` |
|      ! 0 | 1640 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1641 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1642 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1643 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1644 | `				 }` |
|      ! 0 | 1645 | `				 return rc;` |
|        - | 1646 | `			 }` |
|        - | 1647 | `			 /* Make sure the operand are of a valid type */` |
|    24745 | 1648 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1649 | `				 /* Clone:` |
|        - | 1650 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1651 | `				  *  ++ function call (including annonymous)` |
|        - | 1652 | `				  *  ++ array member` |
|        - | 1653 | `				  *  ++ 'new' operator` |
|        - | 1654 | `				  * Example:` |
|        - | 1655 | `				  *   clone $pObj;` |
|        - | 1656 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1657 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1658 | `				  */` |
|       38 | 1659 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       36 | 1660 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1661 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1662 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1663 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1664 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1665 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1666 | `						 }` |
|      ! 0 | 1667 | `						 return rc;` |
|        - | 1668 | `					 }` |
|       16 | 1669 | `				 }` |
|       21 | 1670 | `			 }else{` |
|        - | 1671 | `				 /* New */` |
|    24711 | 1672 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      223 | 1673 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      218 | 1674 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       31 | 1675 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1676 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1677 | `						 /* Syntax error */` |
|      ! 0 | 1678 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1679 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1680 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1681 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1682 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1683 | `						 }` |
|      ! 0 | 1684 | `						 return rc;` |
|        - | 1685 | `					 }` |
|      109 | 1686 | `				 }` |
|        - | 1687 | `			 }` |
|        - | 1688 | `			  /* Link the node to the tree */` |
|    24745 | 1689 | `			 pNode->pLeft = apNode[iLeft];` |
|    24745 | 1690 | `			 apNode[iLeft] = 0;` |
|    24745 | 1691 | `			 pNode->pRight = 0; /* Paranoid */` |
|    12370 | 1692 | `		 }` |
|  1044020 | 1693 | `	 }` |
|        - | 1694 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   864345 | 1695 | `	 iLeft = -1;` |
|  5629377 | 1696 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4760861 | 1697 | `		 if( apNode[iCur] == 0 ){` |
|  2672831 | 1698 | `			 continue;` |
|        - | 1699 | `		 }` |
|  2088035 | 1700 | `		 pNode = apNode[iCur];` |
|  2088035 | 1701 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11559 | 1702 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     4213 | 1703 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1704 | `					 /* Link the node to the tree */` |
|     4225 | 1705 | `					 pNode->pLeft = apNode[iLeft];` |
|     4225 | 1706 | `					 apNode[iLeft] = 0;` |
|     2110 | 1707 | `			 }` |
|     7865 | 1708 | `		  }` |
|  2092211 | 1709 | `		 iLeft = iCur;` |
|  1048196 | 1710 | `	  }` |
|   868521 | 1711 | `	 iLeft = -1;` |
|  5629377 | 1712 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4760861 | 1713 | `		 if( apNode[iCur] == 0 ){` |
|  2677051 | 1714 | `			 continue;` |
|        - | 1715 | `		 }` |
|  2083815 | 1716 | `		 pNode = apNode[iCur];` |
|  2083815 | 1717 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11510 | 1718 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    11515 | 1719 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1720 | `					 /* Syntax error */` |
|      ! 0 | 1721 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1722 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1723 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1724 | `					 }` |
|      ! 0 | 1725 | `					 return rc;` |
|        - | 1726 | `			 }` |
|        - | 1727 | `			 /* Link the node to the tree */` |
|    11515 | 1728 | `			 pNode->pLeft = apNode[iLeft];` |
|    11515 | 1729 | `			 apNode[iLeft] = 0;` |
|        - | 1730 | `			 /* Mark as pre-increment/decrement node */` |
|    11515 | 1731 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5755 | 1732 | `		  }` |
|  2083815 | 1733 | `		 iLeft = iCur;` |
|  1041910 | 1734 | `	 }` |
|        - | 1735 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   868521 | 1736 | `	  iLeft = 0;` |
|  5629371 | 1737 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4760857 | 1738 | `		  if( apNode[iCur] ){` |
|  2072301 | 1739 | `			  pNode = apNode[iCur];` |
|  2072301 | 1740 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    36113 | 1741 | `				  if( iLeft > 0 ){` |
|        - | 1742 | `					  /* Link the node to the tree */` |
|    36111 | 1743 | `					  pNode->pLeft = apNode[iLeft];` |
|    36111 | 1744 | `					  apNode[iLeft] = 0;` |
|    36111 | 1745 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1746 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1747 | `							   /* Syntax error */` |
|      ! 0 | 1748 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1749 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1750 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1751 | `							  }` |
|      ! 0 | 1752 | `							  return rc;` |
|        - | 1753 | `						  }` |
|       36 | 1754 | `					  }` |
|    18058 | 1755 | `				  }else{` |
|        - | 1756 | `					  /* Syntax error */` |
|        3 | 1757 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1758 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1759 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1760 | `					  }` |
|        3 | 1761 | `					  return rc;` |
|        - | 1762 | `				  }` |
|    18053 | 1763 | `			  }` |
|        - | 1764 | `			  /* Save terminal position */` |
|  2072299 | 1765 | `			  iLeft = iCur;` |
|  1036147 | 1766 | `		  }` |
|  2380430 | 1767 | `	  }` |
|        - | 1768 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1769 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1770 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1771 | `	  * yielding a right-leaning tree. */` |
|  5629369 | 1772 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4760855 | 1773 | `		 if( apNode[iCur] == 0 ){` |
|  2724779 | 1774 | `			 continue;` |
|        - | 1775 | `		 }` |
|  2036081 | 1776 | `		 pNode = apNode[iCur];` |
|  2036081 | 1777 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1778 | `			 sxi32 iL, iR;` |
|        - | 1779 | `			 /* Find the right operand */` |
|      113 | 1780 | `			 iR = -1;` |
|        - | 1781 | `			 {` |
|        - | 1782 | `				 sxi32 j;` |
|      125 | 1783 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1784 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1785 | `				 }` |
|        - | 1786 | `			 }` |
|        - | 1787 | `			 /* Find the left operand */` |
|      113 | 1788 | `			 iL = -1;` |
|        - | 1789 | `			 {` |
|        - | 1790 | `				 sxi32 j;` |
|      181 | 1791 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1792 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1793 | `				 }` |
|        - | 1794 | `			 }` |
|      113 | 1795 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1796 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1797 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1798 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1799 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1800 | `				 }` |
|      ! 0 | 1801 | `				 return rc;` |
|        - | 1802 | `			 }` |
|      113 | 1803 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1804 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1805 | `			 apNode[iL] = 0;` |
|      113 | 1806 | `			 apNode[iR] = 0;` |
|        - | 1807 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1808 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1809 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1810 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1811 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1812 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1813 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1814 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1815 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1816 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1817 | `			  * operands are respected. */` |
|      112 | 1818 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1819 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1820 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1821 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1822 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1823 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1824 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1825 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1826 | `				 while( pTail->pLeft` |
|       34 | 1827 | `					 && pTail->pLeft->pOp` |
|       23 | 1828 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1829 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1830 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1831 | `					 pTail = pTail->pLeft;` |
|        1 | 1832 | `				 }` |
|        - | 1833 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1834 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1835 | `				 pTail->pLeft = pNode;` |
|       27 | 1836 | `				 apNode[iCur] = pHead;` |
|       13 | 1837 | `			 }` |
|       56 | 1838 | `		 }` |
|  1018043 | 1839 | `	 }` |
|        - | 1840 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  9553573 | 1841 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  8685069 | 1842 | `		 iLeft = -1;` |
| 56293275 | 1843 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 47608221 | 1844 | `			 if( apNode[iCur] == 0 ){` |
| 30698267 | 1845 | `				 continue;` |
|        - | 1846 | `			 }` |
| 16909959 | 1847 | `			 pNode = apNode[iCur];` |
| 16909959 | 1848 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1849 | `				 /* Get the right node */` |
|   258025 | 1850 | `				 iRight = iCur + 1;` |
|   368965 | 1851 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   110945 | 1852 | `					 iRight++;` |
|        5 | 1853 | `				 }` |
|   258025 | 1854 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1855 | `					 /* Syntax error */` |
|       11 | 1856 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       11 | 1857 | `					 if( rc != SXERR_ABORT ){` |
|       11 | 1858 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1859 | `					 }` |
|       11 | 1860 | `					 return rc;` |
|        - | 1861 | `				 }` |
|   258017 | 1862 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1863 | `					 sxi32  iTmp;` |
|        - | 1864 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1865 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1866 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1867 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1868 | `					  * is swapped below. */` |
|       60 | 1869 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1870 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1871 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1872 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1873 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1874 | `						 }` |
|        3 | 1875 | `						 return rc;` |
|        - | 1876 | `					 }` |
|       57 | 1877 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1878 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1879 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1880 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1881 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1882 | `						 }` |
|      ! 0 | 1883 | `						 return rc;` |
|        - | 1884 | `					 }` |
|       57 | 1885 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       41 | 1886 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1887 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1888 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1889 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1890 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1891 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1892 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1893 | `									 }` |
|      ! 0 | 1894 | `									 return rc;` |
|        - | 1895 | `							 }` |
|      ! 0 | 1896 | `						 }` |
|       19 | 1897 | `					 }` |
|        - | 1898 | `					 /* Swap operands */` |
|       57 | 1899 | `					 iTmp = iRight;` |
|       57 | 1900 | `					 iRight = iLeft;` |
|       57 | 1901 | `					 iLeft = iTmp;` |
|       27 | 1902 | `				 }` |
|        - | 1903 | `				 /* Link the node to the tree */` |
|   258015 | 1904 | `				 pNode->pLeft = apNode[iLeft];` |
|   258015 | 1905 | `				 pNode->pRight = apNode[iRight];` |
|   258015 | 1906 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   129005 | 1907 | `			 }` |
| 16909949 | 1908 | `			 iLeft = iCur;` |
|  8454977 | 1909 | `		 }` |
|  4342532 | 1910 | `	 }` |
|        - | 1911 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1912 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1913 | `	  * we are dealing with a single operator.` |
|        - | 1914 | `	  */` |
|   868509 | 1915 | `	  iLeft = -1;` |
|  5617865 | 1916 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4752005 | 1917 | `		  if( apNode[iCur] == 0 ){` |
|  3239977 | 1918 | `			  continue;` |
|        - | 1919 | `		  }` |
|  1512033 | 1920 | `		  pNode = apNode[iCur];` |
|  1512033 | 1921 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2649 | 1922 | `			  sxi32 iNest = 1;` |
|     2649 | 1923 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1924 | `				  /* Missing condition */` |
|        3 | 1925 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1926 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1927 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1928 | `				  }` |
|        3 | 1929 | `				  return rc;` |
|        - | 1930 | `			  }` |
|        - | 1931 | `			  /* Get the right node */` |
|     2647 | 1932 | `			  iRight = iCur + 1;` |
|     5573 | 1933 | `			  while( iRight < nToken  ){` |
|     5573 | 1934 | `				  if( apNode[iRight] ){` |
|     5221 | 1935 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1936 | `						  /* Increment nesting level */` |
|      ! 0 | 1937 | `						  ++iNest;` |
|     5221 | 1938 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1939 | `						  /* Decrement nesting level */` |
|     2647 | 1940 | `						  --iNest;` |
|     2647 | 1941 | `						  if( iNest <= 0 ){` |
|     2647 | 1942 | `							  break;` |
|        - | 1943 | `						  }` |
|      ! 0 | 1944 | `					  }` |
|     1287 | 1945 | `				  }` |
|     2931 | 1946 | `				  iRight++;` |
|        5 | 1947 | `			  }` |
|     2647 | 1948 | `			  if( iRight > iCur + 1 ){` |
|        - | 1949 | `				  /* Recurse and process the then expression */` |
|     2579 | 1950 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2579 | 1951 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1952 | `					  return rc;` |
|        - | 1953 | `				  }` |
|        - | 1954 | `				  /* Link the node to the tree */` |
|     2579 | 1955 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1287 | 1956 | `			  }else{` |
|        - | 1957 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1958 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1959 | `			  }` |
|     2647 | 1960 | `			  apNode[iCur + 1] = 0;` |
|     2647 | 1961 | `			  if( iRight + 1 < nToken ){` |
|        - | 1962 | `				  /* Recurse and process the else expression */` |
|     2647 | 1963 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2647 | 1964 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1965 | `					  return rc;` |
|        - | 1966 | `				  }` |
|        - | 1967 | `				  /* Link the node to the tree */` |
|     2647 | 1968 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2647 | 1969 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1326 | 1970 | `			  }else{` |
|      ! 0 | 1971 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1972 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1973 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1974 | `				 }` |
|      ! 0 | 1975 | `				 return rc;` |
|        - | 1976 | `			  }` |
|        - | 1977 | `			  /* Point to the condition */` |
|     2647 | 1978 | `			  pNode->pCond  = apNode[iLeft];` |
|     2647 | 1979 | `			  apNode[iLeft] = 0;` |
|     2647 | 1980 | `			  break;` |
|        - | 1981 | `		  }` |
|  1509389 | 1982 | `		  iLeft = iCur;` |
|   754697 | 1983 | `	  }` |
|        - | 1984 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1985 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1986 | `	  * so there is no need for a precedence loop here.` |
|        - | 1987 | `	  */` |
|   868507 | 1988 | `	 iRight = -1;` |
|  5629173 | 1989 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4760725 | 1990 | `		 if( apNode[iCur] == 0 ){` |
|  3571723 | 1991 | `			 continue;` |
|        - | 1992 | `		 }` |
|  1189007 | 1993 | `		 pNode = apNode[iCur];` |
|  1189007 | 1994 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1995 | `			 /* Get the left node */` |
|   320383 | 1996 | `			 iLeft = iCur - 1;` |
|   463583 | 1997 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   143205 | 1998 | `				 iLeft--;` |
|        5 | 1999 | `			 }` |
|   320383 | 2000 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2001 | `				 /* Syntax error */` |
|       44 | 2002 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2003 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 2004 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 2005 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 2006 | `				 }else{` |
|       40 | 2007 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 2008 | `				 }` |
|       44 | 2009 | `				 if( rc != SXERR_ABORT ){` |
|       42 | 2010 | `					 rc = SXERR_SYNTAX;` |
|       20 | 2011 | `				 }` |
|       44 | 2012 | `				 return rc;` |
|        - | 2013 | `			 }` |
|        - | 2014 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 2015 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 2016 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 2017 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 2018 | `			  * a write. */` |
|   320341 | 2019 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 2020 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 2021 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 2022 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 2023 | `					 rc = SXERR_SYNTAX;` |
|        4 | 2024 | `				 }` |
|       11 | 2025 | `				 return rc;` |
|        - | 2026 | `			 }` |
|   320333 | 2027 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      115 | 2028 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       82 | 2029 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2030 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2031 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2032 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2033 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2034 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2035 | `					 }else{` |
|        4 | 2036 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2037 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2038 | `					 }` |
|        6 | 2039 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2040 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2041 | `					 }` |
|        6 | 2042 | `					 return rc;` |
|        - | 2043 | `				 }` |
|       40 | 2044 | `			 }` |
|        - | 2045 | `			 /* Link the node to the tree (Reverse) */` |
|   320329 | 2046 | `			 pNode->pLeft = apNode[iRight];` |
|   320329 | 2047 | `			 pNode->pRight = apNode[iLeft];` |
|   320329 | 2048 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   160162 | 2049 | `		 }` |
|  1188953 | 2050 | `		 iRight = iCur;` |
|   594479 | 2051 | `	 }` |
|        - | 2052 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  4342245 | 2053 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3473797 | 2054 | `		 iLeft = -1;` |
| 22516405 | 2055 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 19042613 | 2056 | `			 if( apNode[iCur] == 0 ){` |
| 15568415 | 2057 | `				 continue;` |
|        - | 2058 | `			 }` |
|  3474203 | 2059 | `			 pNode = apNode[iCur];` |
|  3474203 | 2060 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2061 | `				 /* Get the right node */` |
|       72 | 2062 | `				 iRight = iCur + 1;` |
|      110 | 2063 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2064 | `					 iRight++;` |
|        2 | 2065 | `				 }` |
|       72 | 2066 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2067 | `					 /* Syntax error */` |
|      ! 0 | 2068 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2069 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2070 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2071 | `					 }` |
|      ! 0 | 2072 | `					 return rc;` |
|        - | 2073 | `				 }` |
|        - | 2074 | `				 /* Link the node to the tree */` |
|       72 | 2075 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2076 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2077 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2078 | `			 }` |
|  3474203 | 2079 | `			 iLeft = iCur;` |
|  1737104 | 2080 | `		 }` |
|  1736901 | 2081 | `	 }` |
|        - | 2082 | `	 /* Point to the root of the expression tree */` |
|  4760629 | 2083 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3892199 | 2084 | `		 if( apNode[iCur] ){` |
|   801635 | 2085 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       23 | 2086 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       23 | 2087 | `				  if( rc != SXERR_ABORT ){` |
|       23 | 2088 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2089 | `				  }` |
|       23 | 2090 | `				  return rc;` |
|        - | 2091 | `			 }` |
|   801617 | 2092 | `			 apNode[0] = apNode[iCur];` |
|   801617 | 2093 | `			 apNode[iCur] = 0;` |
|   400806 | 2094 | `		 }` |
|  1946093 | 2095 | `	 }` |
|   868435 | 2096 | `	 return SXRET_OK;` |
|   818688 | 2097 | ` }` |
|        - | 2098 | ` /*` |
|        - | 2099 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2100 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2101 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2102 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2103 | `  */` |
|  1024594 | 2104 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2105 | `{` |
|        - | 2106 | `	ph7_expr_node **apNode;` |
|        - | 2107 | `	ph7_expr_node *pNode;` |
|        - | 2108 | `	sxi32 rc;` |
|        - | 2109 | `	/* Reset node container */` |
|  1024599 | 2110 | `	SySetReset(pExprNode);` |
|  1024599 | 2111 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2112 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2113 | `	{` |
|  1024599 | 2114 | `		int iLastWasTerm = 0;` |
|  1024599 | 2115 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  5554055 | 2116 | `		while( pGen->pIn < pGen->pEnd ){` |
|  4529495 | 2117 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  4529495 | 2118 | `			if( rc != SXRET_OK ){` |
|       38 | 2119 | `				return rc;` |
|        - | 2120 | `			}` |
|        - | 2121 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  4529461 | 2122 | `			if( pNode->xCode ){` |
|        - | 2123 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2375279 | 2124 | `				iLastWasTerm = 1;` |
|  3341824 | 2125 | `			}else if( pNode->pOp ){` |
|        - | 2126 | `				/* Operator node */` |
|  1023139 | 2127 | `				iLastWasTerm = 0;` |
|   511572 | 2128 | `			}else{` |
|        - | 2129 | `				/* Delimiter: ')' and ']' end terms */` |
|  1131053 | 2130 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2131 | `			}` |
|        - | 2132 | `			/* A keyword in the next node is a member name only right after a member` |
|        - | 2133 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|        - | 2134 | `			 * node kind, so this single test covers all branches. */` |
|  4529461 | 2135 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|        - | 2136 | `			/* Save the extracted node */` |
|  4529461 | 2137 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2138 | `		}` |
|        - | 2139 | `	}` |
|  1024565 | 2140 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2141 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2142 | `		*ppRoot = 0;` |
|      ! 0 | 2143 | `		return SXRET_OK;` |
|        - | 2144 | `	}` |
|  1024565 | 2145 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2146 | `	/* Make sure we are dealing with valid nodes */` |
|  1024565 | 2147 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1024565 | 2148 | `	if( rc != SXRET_OK ){` |
|        - | 2149 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2150 | `		 * cleanup the mess left behind.` |
|        - | 2151 | `		 */` |
|       54 | 2152 | `		*ppRoot = 0;` |
|       54 | 2153 | `		return rc;` |
|        - | 2154 | `	}` |
|        - | 2155 | `	/* Build the tree */` |
|  1024515 | 2156 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1024515 | 2157 | `	if( rc != SXRET_OK ){` |
|        - | 2158 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2159 | `		*ppRoot = 0;` |
|      103 | 2160 | `		return rc;` |
|        - | 2161 | `	}` |
|        - | 2162 | `	/* Point to the root of the tree */` |
|  1024417 | 2163 | `	*ppRoot = apNode[0];` |
|  1024417 | 2164 | `	return SXRET_OK;` |
|   512302 | 2165 | `}` |
|        - | 2166 |  |
