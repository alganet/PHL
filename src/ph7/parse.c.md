# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 998/1177 lines (84.79%)

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
|        - |  180 | `	/* Precedence 7,left-associative */` |
|        - |  181 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|        - |  182 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|        - |  183 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|        - |  184 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|        - |  185 | `	/* Precedence 8,left-associative */` |
|        - |  186 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|        - |  187 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|        - |  188 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|        - |  189 | `	/* Precedence 9,left-associative */` |
|        - |  190 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|        - |  191 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|        - |  192 | `	/* Precedence 10,non-associative */` |
|        - |  193 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|        - |  194 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|        - |  195 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|        - |  196 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|        - |  197 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|        - |  198 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  199 | `	/* Precedence 11,non-associative */` |
|        - |  200 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|        - |  201 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  202 | `	{ {"eq",sizeof(char)*2},  EXPR_OP_SEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_SEQ}, /* IMP-0137-EQ: Symisc eXtension */` |
|        - |  203 | `	{ {"ne",sizeof(char)*2},  EXPR_OP_SNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_SNE}, /* IMP-0138-NE: Symisc eXtension */` |
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
|        - |  234 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  235 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  236 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  237 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  238 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  239 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|        - |  240 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|        - |  241 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|        - |  242 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|        - |  243 | `	 * in this file: keep one of the question marks escaped. */` |
|        - |  244 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|        - |  245 | `	/* Precedence 19,left-associative */` |
|        - |  246 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  247 | `	/* Precedence 20,left-associative */` |
|        - |  248 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  249 | `	/* Precedence 21,left-associative */` |
|        - |  250 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  251 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  252 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  253 | `};` |
|        - |  254 | `/* Function call operator need special handling */` |
|        - |  255 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  256 | `/*` |
|        - |  257 | ` * Check if the given token is a potential operator or not.` |
|        - |  258 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  259 | ` * look like an operator.` |
|        - |  260 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  261 | ` * Otherwise NULL.` |
|        - |  262 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  263 | ` * a binary minus or unary minus.]` |
|        - |  264 | ` */` |
|   800296 |  265 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  266 |  |
|   800298 |  267 | `	sxu32 n = 0;` |
|        - |  268 | `	sxi32 rc;` |
|        - |  269 | `	/* Do a linear lookup on the operators table */` |
| 13450908 |  270 | `	for(;;){` |
| 26901818 |  271 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  272 | `			break;` |
|        - |  273 | `		}` |
| 26901818 |  274 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  275 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3191612 |  276 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1595807 |  277 | `		}else{` |
| 23710208 |  278 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  279 | `		}` |
| 26901818 |  280 | `		if( rc == 0 ){` |
|   803812 |  281 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  282 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   799964 |  283 | `				return &aOpTable[n];` |
|        - |  284 | `			}` |
|        - |  285 | `			/* Handle ambiguity */` |
|     3850 |  286 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  287 | `				/* Unary opertors have prcedence here over binary operators */` |
|      226 |  288 | `				return &aOpTable[n];` |
|        - |  289 | `			}` |
|     3626 |  290 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  291 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  292 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  293 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  294 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  295 | `					return &aOpTable[n];` |
|        - |  296 | `				}` |
|        - |  297 |  |
|        4 |  298 | `			}` |
|     1757 |  299 | `		}` |
| 26101522 |  300 | `		++n; /* Next operator in the table */` |
|        2 |  301 | `	}` |
|        - |  302 | `	/* No such operator */` |
|      ! 0 |  303 | `	return 0;` |
|   400150 |  304 |  |
|        - |  305 | `/*` |
|        - |  306 | ` * Delimit a set of token stream.` |
|        - |  307 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  308 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  309 | ` */` |
|   411660 |  310 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  311 |  |
|   411662 |  312 | `	SyToken *pCur = pIn;` |
|   411662 |  313 | `	sxi32 iNest = 1;` |
|  2339266 |  314 | `	for(;;){` |
|  4678534 |  315 | `		if( pCur >= pEnd ){` |
|      130 |  316 | `			break;` |
|        - |  317 | `		}` |
|  4678406 |  318 | `		if( pCur->nType & nTokStart ){` |
|        - |  319 | `			/* Increment nesting level */` |
|   258486 |  320 | `			iNest++;` |
|  4549164 |  321 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  322 | `			/* Decrement nesting level */` |
|   670018 |  323 | `			iNest--;` |
|   670018 |  324 | `			if( iNest <= 0 ){` |
|   411534 |  325 | `				break;` |
|        - |  326 | `			}` |
|   129242 |  327 | `		}` |
|        - |  328 | `		/* Advance cursor */` |
|  4266874 |  329 | `		pCur++;` |
|        2 |  330 | `	}` |
|        - |  331 | `	/* Point to the end of the chunk */` |
|   411662 |  332 | `	*ppEnd = pCur;` |
|   411662 |  333 |  |
|        - |  334 | `/*` |
|        - |  335 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  336 | ` * Note on reserved keywords.` |
|        - |  337 | ` *  According to the PHP language reference manual:` |
|        - |  338 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  339 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  340 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  341 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  342 | ` */` |
|    12342 |  343 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  344 |  |
|    18447 |  345 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    12249 |  346 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  347 | `		){` |
|      146 |  348 | `			return TRUE;` |
|        - |  349 | `	}` |
|    12200 |  350 | `	if( bCheckFunc ){` |
|       98 |  351 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       72 |  352 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       57 |  353 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  354 | `				return TRUE;` |
|        - |  355 | `		}` |
|       22 |  356 | `	}` |
|        - |  357 | `	/* Not a language construct */` |
|    12168 |  358 | `	return FALSE;` |
|     6173 |  359 |  |
|        - |  360 | `/*` |
|        - |  361 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  362 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  363 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  364 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  365 | ` */` |
|   704566 |  366 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  367 |  |
|        - |  368 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  369 | `	sxi32 i,rc;` |
|        - |  370 |  |
|   704568 |  371 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  372 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  373 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  374 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  375 | `	}` |
|   704568 |  376 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3808822 |  377 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3104290 |  378 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  379 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      326 |  380 | `			continue;` |
|        - |  381 | `		}` |
|  3103966 |  382 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   356410 |  383 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    18334 |  384 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  385 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   331866 |  386 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  387 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  388 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  389 | `						 */` |
|   331866 |  390 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   331866 |  391 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   331866 |  392 | `						apNode[i]->pOp = &sFCallOp;` |
|   165932 |  393 | `					}` |
|   165932 |  394 | `			}` |
|   356410 |  395 | `			iParen++;` |
|  2925762 |  396 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   356410 |  397 | `			if( iParen <= 0 ){` |
|       13 |  398 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  399 | `				if( rc != SXERR_ABORT ){` |
|       13 |  400 | `					rc = SXERR_SYNTAX;` |
|        6 |  401 | `				}` |
|       13 |  402 | `				return rc;` |
|        - |  403 | `			}` |
|   356398 |  404 | `			iParen--;` |
|  2569348 |  405 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    74338 |  406 | `			iSquare++;` |
|  2353982 |  407 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    74352 |  408 | `			if( iSquare <= 0 ){` |
|        7 |  409 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  410 | `				if( rc != SXERR_ABORT ){` |
|        7 |  411 | `					rc = SXERR_SYNTAX;` |
|        3 |  412 | `				}` |
|        7 |  413 | `				return rc;` |
|        - |  414 | `			}` |
|    74346 |  415 | `			iSquare--;` |
|  2279636 |  416 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       11 |  417 | `			iBraces++;` |
|       11 |  418 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  419 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  420 | `				int iNest = 1;` |
|       11 |  421 | `				sxi32 j=i+1;` |
|        - |  422 | `				/*` |
|        - |  423 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  424 | `				 */` |
|       11 |  425 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  426 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  427 | `				pOp = aOpTable;` |
|       11 |  428 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       61 |  429 | `				while( pOp < pEnd ){` |
|       61 |  430 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  431 | `						break;` |
|        - |  432 | `					}` |
|       51 |  433 | `					pOp++;` |
|        1 |  434 | `				}` |
|       11 |  435 | `				if( pOp >= pEnd ){` |
|      ! 0 |  436 | `					pOp = 0;` |
|      ! 0 |  437 | `				}` |
|       11 |  438 | `				if( pOp ){` |
|       11 |  439 | `					apNode[i]->pOp = pOp;` |
|       11 |  440 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  441 | `				}` |
|       11 |  442 | `				iBraces--;` |
|       11 |  443 | `				iSquare++;` |
|       21 |  444 | `				while( j < nNode ){` |
|       21 |  445 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  446 | `						/* Increment nesting level */` |
|      ! 0 |  447 | `						iNest++;` |
|       21 |  448 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  449 | `						/* Decrement nesting level */` |
|       11 |  450 | `						iNest--;` |
|       11 |  451 | `						if( iNest < 1 ){` |
|       11 |  452 | `							break;` |
|        - |  453 | `						}` |
|      ! 0 |  454 | `					}` |
|       11 |  455 | `					j++;` |
|        1 |  456 | `				}` |
|       11 |  457 | `				if( j < nNode ){` |
|       11 |  458 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  459 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  460 | `				}` |
|        - |  461 |  |
|        6 |  462 | `			}` |
|  2242459 |  463 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  464 | `			if( iBraces <= 0 ){` |
|       13 |  465 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  466 | `				if( rc != SXERR_ABORT ){` |
|       13 |  467 | `					rc = SXERR_SYNTAX;` |
|        6 |  468 | `				}` |
|       13 |  469 | `				return rc;` |
|        - |  470 | `			}` |
|      ! 0 |  471 | `			iBraces--;` |
|  2242442 |  472 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2204 |  473 | `			if( iQuesty > 0 ){` |
|     2024 |  474 | `				iQuesty--;` |
|     1193 |  475 | `			}else if( iParen <= 0 ){` |
|        - |  476 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  477 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  478 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  479 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  480 | `				if( rc != SXERR_ABORT ){` |
|        5 |  481 | `					rc = SXERR_SYNTAX;` |
|        2 |  482 | `				}` |
|        5 |  483 | `				return rc;` |
|        2 |  484 | `			}` |
|  2241339 |  485 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   624152 |  486 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   624152 |  487 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2026 |  488 | `				iQuesty++;` |
|   623140 |  489 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      322 |  490 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  491 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  492 | `					sxu32 n = 0;` |
|       11 |  493 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  494 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  495 | `					}` |
|        - |  496 | `					/*` |
|        - |  497 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  498 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  499 | `					 */` |
|      255 |  500 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      245 |  501 | `						++n;` |
|        1 |  502 | `					}` |
|       11 |  503 | `					pOp = &aOpTable[n];` |
|        - |  504 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  505 | `					apNode[i]->pOp = pOp;` |
|       11 |  506 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  507 | `				}` |
|      160 |  508 | `			}` |
|   312075 |  509 | `		}` |
|  1551967 |  510 | `	}` |
|   704534 |  511 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  512 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  513 | `		if( rc != SXERR_ABORT ){` |
|       17 |  514 | `			rc = SXERR_SYNTAX;` |
|        8 |  515 | `		}` |
|       17 |  516 | `		return rc;` |
|        - |  517 | `	}` |
|   704518 |  518 | `	return SXRET_OK;` |
|   352285 |  519 |  |
|        - |  520 | `/*` |
|        - |  521 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  522 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  523 | ` */` |
|   568442 |  524 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  525 |  |
|   568444 |  526 | `	SyToken *pIn = *ppCur;` |
|        - |  527 | `	/* Jump the first literal seen */` |
|   568444 |  528 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   568418 |  529 | `		pIn++;` |
|   284208 |  530 | `	}` |
|   284254 |  531 | `	for(;;){` |
|   568510 |  532 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       68 |  533 | `			pIn++;` |
|       68 |  534 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       68 |  535 | `				pIn++;` |
|       33 |  536 | `			}` |
|       35 |  537 | `		}else{` |
|   284223 |  538 | `			break;` |
|        - |  539 | `		}` |
|        2 |  540 | `	}` |
|        - |  541 | `	/* Synchronize pointers */` |
|   568444 |  542 | `	*ppCur = pIn;` |
|   568444 |  543 |  |
|        - |  544 | `/*` |
|        - |  545 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  546 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  547 | ` * Note on annonymous functions.` |
|        - |  548 | ` *  According to the PHP language reference manual:` |
|        - |  549 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  550 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  551 | ` *  parameters, but they have many other uses.` |
|        - |  552 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  553 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  554 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  555 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  556 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  557 | ` *` |
|        - |  558 | ` * Some example:` |
|        - |  559 | ` *  $greet = function($name)` |
|        - |  560 | ` * {` |
|        - |  561 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  562 | ` * };` |
|        - |  563 | ` *  $greet('World');` |
|        - |  564 | ` *  $greet('PHP');` |
|        - |  565 | ` *` |
|        - |  566 | ` * $double = function($a) {` |
|        - |  567 | ` *   return $a * 2;` |
|        - |  568 | ` * };` |
|        - |  569 | ` * // This is our range of numbers` |
|        - |  570 | ` * $numbers = range(1, 5);` |
|        - |  571 | ` * // Use the Annonymous function as a callback here to` |
|        - |  572 | ` * // double the size of each element in our` |
|        - |  573 | ` * // range` |
|        - |  574 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  575 | ` * print implode(' ', $new_numbers);` |
|        - |  576 | ` */` |
|      202 |  577 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  578 |  |
|      204 |  579 | `	SyToken *pIn = *ppCur;` |
|        - |  580 | `	sxu32 nLine;` |
|        - |  581 | `	sxi32 rc;` |
|        - |  582 | `	/* Jump the 'function' keyword */` |
|      204 |  583 | `	nLine = pIn->nLine;` |
|      204 |  584 | `	pIn++;` |
|      204 |  585 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  586 | `		pIn++;` |
|        1 |  587 | `	}` |
|      204 |  588 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  589 | `		/* Syntax error */` |
|        5 |  590 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  591 | `		if( rc != SXERR_ABORT ){` |
|        5 |  592 | `			rc = SXERR_SYNTAX;` |
|        2 |  593 | `		}` |
|        5 |  594 | `		goto Synchronize;` |
|        - |  595 | `	}` |
|      200 |  596 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      200 |  597 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      200 |  598 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  599 | `		/* Syntax error */` |
|        5 |  600 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  601 | `		if( rc != SXERR_ABORT ){` |
|        5 |  602 | `			rc = SXERR_SYNTAX;` |
|        2 |  603 | `		}` |
|        5 |  604 | `		goto Synchronize;` |
|        - |  605 | `	}` |
|      196 |  606 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  607 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      196 |  608 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  609 | `		pIn++; /* Skip ':' */` |
|        - |  610 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  611 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  612 | `			pIn++;` |
|      ! 0 |  613 | `		}` |
|        - |  614 | `		/* Skip the first type (allow leading '\' and namespace path) */` |
|        5 |  615 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        5 |  616 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  617 | `			pIn++;` |
|        5 |  618 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  619 | `				pIn += 2;` |
|      ! 0 |  620 | `			}` |
|        2 |  621 | `		}` |
|        - |  622 | `		/* Skip union alternatives ( \| type )* */` |
|        6 |  623 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        3 |  624 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  625 | `			pIn++;` |
|      ! 0 |  626 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  627 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  628 | `				pIn++;` |
|      ! 0 |  629 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  630 | `					pIn += 2;` |
|      ! 0 |  631 | `				}` |
|      ! 0 |  632 | `			}` |
|      ! 0 |  633 | `		}` |
|        2 |  634 | `	}` |
|      196 |  635 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       32 |  636 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  637 | `		/* Check if we are dealing with a closure */` |
|       32 |  638 | `		if( nKey == PH7_TKWRD_USE ){` |
|       24 |  639 | `			pIn++; /* Jump the 'use' keyword */` |
|       24 |  640 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  641 | `				/* Syntax error */` |
|        5 |  642 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  643 | `				if( rc != SXERR_ABORT ){` |
|        5 |  644 | `					rc = SXERR_SYNTAX;` |
|        2 |  645 | `				}` |
|        5 |  646 | `				goto Synchronize;` |
|        - |  647 | `			}` |
|       20 |  648 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       20 |  649 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       20 |  650 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  651 | `				/* Syntax error */` |
|        5 |  652 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  653 | `				if( rc != SXERR_ABORT ){` |
|        5 |  654 | `					rc = SXERR_SYNTAX;` |
|        2 |  655 | `				}` |
|        5 |  656 | `				goto Synchronize;` |
|        - |  657 | `			}` |
|       16 |  658 | `			pIn++;` |
|        9 |  659 | `		}else{` |
|        - |  660 | `			/* Syntax error */` |
|        9 |  661 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  662 | `			if( rc != SXERR_ABORT ){` |
|        9 |  663 | `				rc = SXERR_SYNTAX;` |
|        4 |  664 | `			}` |
|        9 |  665 | `			goto Synchronize;` |
|        - |  666 | `		}` |
|        7 |  667 | `	}` |
|      180 |  668 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      180 |  669 | `		pIn++; /* Jump the leading curly '{' */` |
|      180 |  670 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      180 |  671 | `		if( pIn < pEnd ){` |
|      180 |  672 | `			pIn++;` |
|       89 |  673 | `		}` |
|       91 |  674 | `	}else{` |
|        - |  675 | `		/* Syntax error */` |
|      ! 0 |  676 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  677 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  678 | `			return SXERR_ABORT;` |
|        - |  679 | `		}` |
|        - |  680 | `	}` |
|      180 |  681 | `	rc = SXRET_OK;` |
|      101 |  682 | `Synchronize:` |
|        - |  683 | `	/* Synchronize pointers */` |
|      204 |  684 | `	*ppCur = pIn;` |
|      204 |  685 | `	return rc;` |
|      103 |  686 |  |
|        - |  687 | `/*` |
|        - |  688 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  689 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  690 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  691 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  692 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  693 | ` */` |
|       84 |  694 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  695 |  |
|       86 |  696 | `	SyToken *pIn = *ppCur;` |
|        - |  697 | `	sxu32 nLine;` |
|        - |  698 | `	sxi32 rc;` |
|        - |  699 | `	int iNest;` |
|       86 |  700 | `	nLine = pIn->nLine;` |
|        - |  701 | `	/* Optional 'static' prefix */` |
|       84 |  702 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       86 |  703 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  704 | `		pIn++;` |
|        1 |  705 | `	}` |
|        - |  706 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       84 |  707 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       86 |  708 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  709 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  710 | `		goto Synchronize;` |
|        - |  711 | `	}` |
|       86 |  712 | `	pIn++; /* Jump 'fn' */` |
|       42 |  713 | `	SXUNUSED(nLine);` |
|       42 |  714 | `	SXUNUSED(pGen);` |
|        - |  715 | `	/* Optional '&' for return-by-reference */` |
|       86 |  716 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  717 | `		pIn++;` |
|      ! 0 |  718 | `	}` |
|        - |  719 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  720 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  721 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  722 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       86 |  723 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       84 |  724 | `		pIn++; /* '(' */` |
|       84 |  725 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       84 |  726 | `		if( pIn < pEnd ){` |
|       82 |  727 | `			pIn++; /* ')' */` |
|       40 |  728 | `		}` |
|       41 |  729 | `	}` |
|        - |  730 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|       86 |  731 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  732 | `		pIn++;` |
|        7 |  733 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  734 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  735 | `			pIn++;` |
|        1 |  736 | `		}` |
|        7 |  737 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        7 |  738 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  739 | `			pIn++;` |
|        7 |  740 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  741 | `				pIn += 2;` |
|      ! 0 |  742 | `			}` |
|        3 |  743 | `		}` |
|        9 |  744 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        4 |  745 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  746 | `			pIn++;` |
|      ! 0 |  747 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  748 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  749 | `				pIn++;` |
|      ! 0 |  750 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  751 | `					pIn += 2;` |
|      ! 0 |  752 | `				}` |
|      ! 0 |  753 | `			}` |
|      ! 0 |  754 | `		}` |
|        3 |  755 | `	}` |
|        - |  756 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       86 |  757 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       79 |  758 | `		pIn++;` |
|       39 |  759 | `	}` |
|        - |  760 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       86 |  761 | `	iNest = 0;` |
|      566 |  762 | `	while( pIn < pEnd ){` |
|      488 |  763 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  764 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  765 | `			break;` |
|        - |  766 | `		}` |
|      482 |  767 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       21 |  768 | `			iNest++;` |
|      472 |  769 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       21 |  770 | `			iNest--;` |
|       10 |  771 | `		}` |
|      482 |  772 | `		pIn++;` |
|        2 |  773 | `	}` |
|       86 |  774 | `	rc = SXRET_OK;` |
|       42 |  775 | `Synchronize:` |
|       86 |  776 | `	*ppCur = pIn;` |
|       86 |  777 | `	return rc;` |
|        2 |  778 |  |
|        - |  779 | `/*` |
|        - |  780 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  781 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  782 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  783 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  784 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  785 | ` */` |
|       68 |  786 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  787 |  |
|       70 |  788 | `	SyToken *pIn = *ppCur;` |
|        - |  789 | `	sxi32 rc;` |
|       34 |  790 | `	SXUNUSED(pGen);` |
|        - |  791 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       68 |  792 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       70 |  793 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  794 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  795 | `		goto Synchronize;` |
|        - |  796 | `	}` |
|       70 |  797 | `	pIn++; /* Jump 'match' */` |
|        - |  798 | `	/* Optional '(' subject ')' */` |
|       70 |  799 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       70 |  800 | `		pIn++;` |
|       70 |  801 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       70 |  802 | `		if( pIn < pEnd ){` |
|       70 |  803 | `			pIn++; /* ')' */` |
|       34 |  804 | `		}` |
|       34 |  805 | `	}` |
|        - |  806 | `	/* Optional '{' arms '}' */` |
|       70 |  807 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       70 |  808 | `		pIn++;` |
|       70 |  809 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       70 |  810 | `		if( pIn < pEnd ){` |
|       70 |  811 | `			pIn++; /* '}' */` |
|       34 |  812 | `		}` |
|       34 |  813 | `	}` |
|       70 |  814 | `	rc = SXRET_OK;` |
|       34 |  815 | `Synchronize:` |
|       70 |  816 | `	*ppCur = pIn;` |
|       70 |  817 | `	return rc;` |
|        2 |  818 |  |
|        - |  819 | `/*` |
|        - |  820 | ` * Extract a single expression node from the input.` |
|        - |  821 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  822 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  823 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  824 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  825 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  826 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  827 | ` */` |
|  3104456 |  828 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  829 |  |
|        - |  830 | `	ph7_expr_node *pNode;` |
|        - |  831 | `	SyToken *pCur;` |
|        - |  832 | `	sxi32 rc;` |
|        - |  833 | `	/* Allocate a new node */` |
|  3104458 |  834 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3104458 |  835 | `	if( pNode == 0 ){` |
|        - |  836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  838 | `		 */` |
|      ! 0 |  839 | `		return SXERR_MEM;` |
|        - |  840 | `	}` |
|        - |  841 | `	/* Zero the structure */` |
|  3104458 |  842 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3104458 |  843 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  844 | `	/* Point to the head of the token stream */` |
|  3104458 |  845 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  846 | `	/* Start collecting tokens */` |
|  3104458 |  847 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  848 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  849 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       20 |  850 | `		pCur++;` |
|       20 |  851 | `		pGen->pIn = pCur;` |
|       20 |  852 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       20 |  853 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       20 |  854 | `		if( rc == SXRET_OK && *ppNode ){` |
|       20 |  855 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        9 |  856 | `		}` |
|       20 |  857 | `		return rc;` |
|        - |  858 | `	}` |
|  3104440 |  859 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  860 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  861 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  862 | `		 */` |
|      328 |  863 | `		pCur++; /* Skip the opening '[' */` |
|      328 |  864 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      328 |  865 | `		if( pCur < pGen->pEnd ){` |
|      328 |  866 | `			pCur++; /* Skip past the closing ']' */` |
|      165 |  867 | `		}else{` |
|      ! 0 |  868 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  869 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  870 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  871 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  872 | `			}` |
|      ! 0 |  873 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  874 | `			return rc;` |
|        - |  875 | `		}` |
|        - |  876 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  877 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  878 | `		 */` |
|      351 |  879 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  880 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  881 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  882 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  883 | `			}else{` |
|       19 |  884 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  885 | `			}` |
|       25 |  886 | `		}else{` |
|      282 |  887 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  888 | `		}` |
|  3104277 |  889 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  890 | `		/* Point to the instance that describe this operator */` |
|   698522 |  891 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  892 | `		/* Advance the stream cursor */` |
|   698522 |  893 | `		pCur++;` |
|  2754854 |  894 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  895 | `		/* Isolate variable */` |
|  1694210 |  896 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   847112 |  897 | `			pCur++; /* Variable variable */` |
|        2 |  898 | `		}` |
|   847100 |  899 | `		if( pCur < pGen->pEnd ){` |
|   847100 |  900 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  901 | `				/* Variable name */` |
|   847072 |  902 | `				pCur++;` |
|   423565 |  903 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  904 | `				pCur++;` |
|        - |  905 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  906 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  907 | `				if( pCur < pGen->pEnd ){` |
|       18 |  908 | `					pCur++;` |
|       10 |  909 | `				}else{` |
|        5 |  910 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  911 | `					if( rc != SXERR_ABORT ){` |
|        5 |  912 | `						rc = SXERR_SYNTAX;` |
|        2 |  913 | `					}` |
|        5 |  914 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  915 | `					return rc;` |
|        - |  916 | `				}` |
|        8 |  917 | `			}` |
|   423547 |  918 | `		}` |
|   847096 |  919 | `		pNode->xCode = PH7_CompileVariable;` |
|  1982043 |  920 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    37586 |  921 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    37586 |  922 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  923 | `			 /* List/Array node */` |
|    24994 |  924 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  925 | `				 /* Assume a literal */` |
|       17 |  926 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  927 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  928 | `			 }else{` |
|    24978 |  929 | `				 pCur += 2;` |
|        - |  930 | `				 /* Collect array/list tokens */` |
|    24978 |  931 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    24978 |  932 | `				 if( pCur < pGen->pEnd ){` |
|    24976 |  933 | `					 pCur++;` |
|    12489 |  934 | `				 }else{` |
|        - |  935 | `					 /* Syntax error */` |
|        4 |  936 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  937 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  938 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  939 | `						 rc = SXERR_SYNTAX;` |
|        1 |  940 | `					 }` |
|        3 |  941 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  942 | `					 return rc;` |
|        - |  943 | `				 }` |
|    24976 |  944 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    24976 |  945 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  946 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  947 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  948 | `						 /* Syntax error */` |
|        3 |  949 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  950 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  951 | `							 rc = SXERR_SYNTAX;` |
|        1 |  952 | `						 }` |
|        3 |  953 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  954 | `						 return rc;` |
|        - |  955 | `					 }` |
|       12 |  956 | `				 }` |
|        2 |  957 | `			 }` |
|    25088 |  958 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  959 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  960 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  961 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  962 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  963 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  964 | `			 pNode->xCode = PH7_CompileYield;` |
|    12577 |  965 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  966 | `			 /* Annonymous function */` |
|      204 |  967 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  968 | `				 /* Assume a literal */` |
|      ! 0 |  969 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  970 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  971 | `			 }else{` |
|        - |  972 | `				 /* Assemble annonymous functions body */` |
|      204 |  973 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      204 |  974 | `				 if( rc != SXRET_OK ){` |
|       25 |  975 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  976 | `					 return rc;` |
|        - |  977 | `				 }` |
|      180 |  978 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  979 | `			  }` |
|    12448 |  980 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    12317 |  981 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  982 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  983 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  984 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       86 |  985 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       86 |  986 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  987 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  988 | `				 return rc;` |
|        - |  989 | `			 }` |
|       86 |  990 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    12316 |  991 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - |  992 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       70 |  993 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       70 |  994 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  995 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  996 | `				 return rc;` |
|        - |  997 | `			 }` |
|       70 |  998 | `			 pNode->xCode = PH7_CompileMatch;` |
|    12240 |  999 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1000 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 | 1001 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 | 1002 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 | 1003 | `		 }else{` |
|        - | 1004 | `			 /* Assume a literal */` |
|    12128 | 1005 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    12128 | 1006 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 | 1007 | `		 }` |
|  1539690 | 1008 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1009 | `		 /* Constants,function name,namespace path,class name... */` |
|   556302 | 1010 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   556302 | 1011 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   278152 | 1012 | `	 }else{` |
|   964612 | 1013 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1014 | `			 /* Point to the code generator routine */` |
|   175200 | 1015 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   175200 | 1016 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1017 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1018 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1019 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1020 | `				 }` |
|        3 | 1021 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1022 | `				 return rc;` |
|        - | 1023 | `			 }` |
|    87598 | 1024 | `		 }` |
|        - | 1025 | `		/* Advance the stream cursor */` |
|   964610 | 1026 | `		pCur++;` |
|        - | 1027 | `	 }` |
|        - | 1028 | `	/* Point to the end of the token stream */` |
|  3104406 | 1029 | `	pNode->pEnd = pCur;` |
|        - | 1030 | `	/* Save the node for later processing */` |
|  3104406 | 1031 | `	*ppNode = pNode;` |
|        - | 1032 | `	/* Synchronize cursors */` |
|  3104406 | 1033 | `	pGen->pIn = pCur;` |
|  3104406 | 1034 | `	return SXRET_OK;` |
|  1552230 | 1035 |  |
|        - | 1036 | `/*` |
|        - | 1037 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1038 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1039 | ` * level is zero.` |
|        - | 1040 | ` */` |
|    74944 | 1041 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 | 1042 |  |
|    74946 | 1043 | `	SyToken *pCur = pStart;` |
|    74946 | 1044 | `	sxi32 iNest = 0;` |
|    74946 | 1045 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1046 | `		/* Last expression */` |
|    39876 | 1047 | `		return SXERR_EOF;` |
|        - | 1048 | `	}` |
|   141362 | 1049 | `	while( pCur < pEnd ){` |
|   128062 | 1050 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    21772 | 1051 | `			break;` |
|        - | 1052 | `		}` |
|   106292 | 1053 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     6072 | 1054 | `			iNest++;` |
|   103257 | 1055 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     6074 | 1056 | `			iNest--;` |
|     3036 | 1057 | `		}` |
|   106292 | 1058 | `		pCur++;` |
|        2 | 1059 | `	}` |
|    35072 | 1060 | `	*ppNext = pCur;` |
|    35072 | 1061 | `	return SXRET_OK;` |
|    37474 | 1062 |  |
|        - | 1063 | `/*` |
|        - | 1064 | ` * Free an expression tree.` |
|        - | 1065 | ` */` |
|  2657150 | 1066 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1067 |  |
|  2657152 | 1068 | `	if( pNode->pLeft ){` |
|        - | 1069 | `		/* Release the left tree */` |
|   991120 | 1070 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   495559 | 1071 | `	}` |
|  2657152 | 1072 | `	if( pNode->pRight ){` |
|        - | 1073 | `		/* Release the right tree */` |
|   518988 | 1074 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   259493 | 1075 | `	}` |
|  2657152 | 1076 | `	if( pNode->pCond ){` |
|        - | 1077 | `		/* Release the conditional tree used by the ternary operator */` |
|     2022 | 1078 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1010 | 1079 | `	}` |
|  2657152 | 1080 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1081 | `		ph7_expr_node **apArg;` |
|        - | 1082 | `		sxu32 n;` |
|        - | 1083 | `		/* Release node arguments */` |
|   351656 | 1084 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   742332 | 1085 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   390678 | 1086 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   195340 | 1087 | `		}` |
|   351656 | 1088 | `		SySetRelease(&pNode->aNodeArgs);` |
|   175827 | 1089 | `	}` |
|        - | 1090 | `	/* Finally,release this node */` |
|  2657152 | 1091 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2657152 | 1092 |  |
|        - | 1093 | `/*` |
|        - | 1094 | ` * Free an expression tree.` |
|        - | 1095 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1096 | ` */` |
|   704600 | 1097 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1098 |  |
|        - | 1099 | `	ph7_expr_node **apNode;` |
|        - | 1100 | `	sxu32 n;` |
|   704602 | 1101 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3809006 | 1102 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3104406 | 1103 | `		if( apNode[n] ){` |
|   704936 | 1104 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   352467 | 1105 | `		}` |
|  1552204 | 1106 | `	}` |
|   704602 | 1107 | `	return SXRET_OK;` |
|        2 | 1108 |  |
|        - | 1109 | `/*` |
|        - | 1110 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1111 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1112 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1113 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1114 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1115 | ` */` |
|   953486 | 1116 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        2 | 1117 |  |
|   953488 | 1118 | `	if( pNode == 0 ){` |
|   592786 | 1119 | `		return 0;` |
|        - | 1120 | `	}` |
|   360704 | 1121 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       13 | 1122 | `		return 1;` |
|        - | 1123 | `	}` |
|   360692 | 1124 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        5 | 1125 | `		return 1;` |
|        - | 1126 | `	}` |
|   360688 | 1127 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1128 | `		return 1;` |
|        - | 1129 | `	}` |
|   360688 | 1130 | `	return 0;` |
|   476745 | 1131 |  |
|        - | 1132 | `/*` |
|        - | 1133 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1134 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1135 | ` */` |
|   225678 | 1136 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1137 |  |
|        - | 1138 | `	sxi32 iExprOp;` |
|   225680 | 1139 | `	if( pNode->pOp == 0 ){` |
|   146752 | 1140 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1141 | `	}` |
|    78930 | 1142 | `	iExprOp = pNode->pOp->iOp;` |
|    78930 | 1143 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    49574 | 1144 | `			return TRUE;` |
|        - | 1145 | `	}` |
|    29358 | 1146 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    29354 | 1147 | `		if( pNode->pLeft->pOp ) {` |
|       18 | 1148 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        5 | 1149 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1150 | `				return FALSE;` |
|        1 | 1151 | `			}` |
|    29345 | 1152 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1153 | `			return FALSE;` |
|        - | 1154 | `		}` |
|    29354 | 1155 | `		return TRUE;` |
|        - | 1156 | `	}` |
|        5 | 1157 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1158 | `		return TRUE;` |
|        - | 1159 | `	}` |
|        - | 1160 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1161 | `	return FALSE;` |
|   112841 | 1162 |  |
|        - | 1163 | `/* Forward declaration */` |
|        - | 1164 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1165 | `/* Macro to check if the given node is a terminal.` |
|        - | 1166 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1167 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1168 | ` * linked ternary/elvis node). */` |
|        - | 1169 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1170 | `/*` |
|        - | 1171 | ` * Buid an expression tree for each given function argument.` |
|        - | 1172 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1173 | ` */` |
|   291876 | 1174 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1175 |  |
|        - | 1176 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1177 | `	sxi32 rc;` |
|        - | 1178 | `	/* Process function arguments from left to right */` |
|   291878 | 1179 | `	iCur = 0;` |
|   311375 | 1180 | `	for(;;){` |
|   622752 | 1181 | `		if( iCur >= nToken ){` |
|        - | 1182 | `			/* No more arguments to process */` |
|   291852 | 1183 | `			break;` |
|        - | 1184 | `		}` |
|   330902 | 1185 | `		iNode = iCur;` |
|   330902 | 1186 | `		iNest = 0;` |
|   828236 | 1187 | `		while( iCur < nToken ){` |
|   536384 | 1188 | `			if( apNode[iCur] ){` |
|   524792 | 1189 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    19526 | 1190 | `					break;` |
|   485744 | 1191 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    26916 | 1192 | `					iNest++;` |
|   472287 | 1193 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    26896 | 1194 | `					iNest--;` |
|    13447 | 1195 | `				}` |
|   242871 | 1196 | `			}` |
|   497336 | 1197 | `			iCur++;` |
|        2 | 1198 | `		}` |
|   330902 | 1199 | `		if( iCur > iNode ){` |
|   330896 | 1200 | `			SyString sArgName = {0, 0};` |
|        - | 1201 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1202 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1203 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   332662 | 1204 | `			if( (iCur - iNode) >= 2` |
|   183600 | 1205 | `				&& apNode[iNode]` |
|    36306 | 1206 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    19948 | 1207 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3563 | 1208 | `				&& apNode[iNode+1]` |
|     3538 | 1209 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1210 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      178 | 1211 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      178 | 1212 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      178 | 1213 | `				apNode[iNode] = 0;` |
|      178 | 1214 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      178 | 1215 | `				apNode[iNode+1] = 0;` |
|      178 | 1216 | `				iNode += 2;` |
|        - | 1217 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1218 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      178 | 1219 | `				if( iNode >= iCur ){` |
|        4 | 1220 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1221 | `						pOp->pStart->nLine,` |
|        - | 1222 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1223 | `						&sArgName);` |
|        3 | 1224 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1225 | `						rc = SXERR_SYNTAX;` |
|        1 | 1226 | `					}` |
|        3 | 1227 | `					return rc;` |
|        - | 1228 | `				}` |
|       87 | 1229 | `			}` |
|   330892 | 1230 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1231 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1232 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1233 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1234 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1235 | `					apNode[iNode] = 0;` |
|      ! 0 | 1236 | `			}` |
|   330894 | 1237 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   330894 | 1238 | `			if( apNode[iNode] ){` |
|   330894 | 1239 | `				if( sArgName.nByte > 0 ){` |
|      176 | 1240 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      176 | 1241 | `					apNode[iNode]->sArgName = sArgName;` |
|       87 | 1242 | `				}` |
|        - | 1243 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   330894 | 1244 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   165448 | 1245 | `			}else{` |
|        - | 1246 | `				/* No expression before comma */` |
|      ! 0 | 1247 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1248 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1249 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1250 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1251 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1252 | `				}` |
|      ! 0 | 1253 | `				return rc;` |
|        - | 1254 | `			}` |
|   165448 | 1255 | `		}else{` |
|        - | 1256 | `			/* Comma with no preceding argument */` |
|        7 | 1257 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1258 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1259 | `				rc = SXERR_SYNTAX;` |
|        3 | 1260 | `			}` |
|        7 | 1261 | `			return rc;` |
|        - | 1262 | `		}` |
|        - | 1263 | `		/* Jump trailing comma */` |
|   330894 | 1264 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    39044 | 1265 | `			iCur++;` |
|    39044 | 1266 | `			if( iCur >= nToken ){` |
|        - | 1267 | `				/* Trailing comma after last argument */` |
|       19 | 1268 | `				break;` |
|        - | 1269 | `			}` |
|    19512 | 1270 | `		}` |
|        2 | 1271 | `	}` |
|   291870 | 1272 | `	return SXRET_OK;` |
|   145940 | 1273 |  |
|        - | 1274 | ` /*` |
|        - | 1275 | `  * Create an expression tree from an array of tokens.` |
|        - | 1276 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1277 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1278 | `  */` |
|  1126832 | 1279 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1280 | ` {` |
|        - | 1281 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1282 | `	 ph7_expr_node *pNode;` |
|        - | 1283 | `	 sxi32 iCur;` |
|        - | 1284 | `	 sxi32 rc;` |
|  1126834 | 1285 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1286 | `		 /* TICKET 1433-17: self evaluating node */` |
|   519718 | 1287 | `		 return SXRET_OK;` |
|        - | 1288 | `	 }` |
|        - | 1289 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3728606 | 1290 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1291 | `		 sxi32 iNest;` |
|        - | 1292 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1293 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1294 | `		  */` |
|  3121492 | 1295 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3096958 | 1296 | `			 continue;` |
|        - | 1297 | `		 }` |
|    24536 | 1298 | `		 iNest = 1;` |
|    24536 | 1299 | `		 iLeft = iCur;` |
|        - | 1300 | `		 /* Find the closing parenthesis */` |
|    24536 | 1301 | `		 iCur++;` |
|   163126 | 1302 | `		 while( iCur < nToken ){` |
|   163126 | 1303 | `			 if( apNode[iCur] ){` |
|   163126 | 1304 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1305 | `					 /* Decrement nesting level */` |
|    42500 | 1306 | `					 iNest--;` |
|    42500 | 1307 | `					 if( iNest <= 0 ){` |
|    24536 | 1308 | `						 break;` |
|        2 | 1309 | `					 }` |
|   129610 | 1310 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1311 | `					 /* Increment nesting level */` |
|    17966 | 1312 | `					 iNest++;` |
|     8982 | 1313 | `				 }` |
|    69295 | 1314 | `			 }` |
|   138592 | 1315 | `			 iCur++;` |
|        2 | 1316 | `		 }` |
|    24536 | 1317 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1318 | `			 /* Recurse and process this expression */` |
|    24536 | 1319 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    24536 | 1320 | `			 if( rc != SXRET_OK ){` |
|        3 | 1321 | `				 return rc;` |
|        - | 1322 | `			 }` |
|    12266 | 1323 | `		 }` |
|        - | 1324 | `		 /* Free the left and right nodes */` |
|    24534 | 1325 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    24534 | 1326 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    24534 | 1327 | `		 apNode[iLeft] = 0;` |
|    24534 | 1328 | `		 apNode[iCur] = 0;` |
|    12268 | 1329 | `	 }` |
|        - | 1330 | `	  /* Process expressions enclosed in braces */` |
|  3885450 | 1331 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1332 | `		 sxi32 iNest;` |
|        - | 1333 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1334 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1335 | `		  */` |
|  3284604 | 1336 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3284604 | 1337 | `			 continue;` |
|        - | 1338 | `		 }` |
|      ! 0 | 1339 | `		 iNest = 1;` |
|      ! 0 | 1340 | `		 iLeft = iCur;` |
|        - | 1341 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1342 | `		 iCur++;` |
|      ! 0 | 1343 | `		 while( iCur < nToken ){` |
|      ! 0 | 1344 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1345 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1346 | `					 /* Decrement nesting level */` |
|      ! 0 | 1347 | `					 iNest--;` |
|      ! 0 | 1348 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1349 | `						 break;` |
|      ! 0 | 1350 | `					 }` |
|      ! 0 | 1351 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1352 | `					 /* Increment nesting level */` |
|      ! 0 | 1353 | `					 iNest++;` |
|      ! 0 | 1354 | `				 }` |
|      ! 0 | 1355 | `			 }` |
|      ! 0 | 1356 | `			 iCur++;` |
|      ! 0 | 1357 | `		 }` |
|      ! 0 | 1358 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1359 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1360 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1361 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1362 | `				 return rc;` |
|        - | 1363 | `			 }` |
|      ! 0 | 1364 | `		 }` |
|        - | 1365 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1366 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1367 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1368 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1369 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1370 | `	 }` |
|        - | 1371 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   600848 | 1372 | `	 iLeft = -1;` |
|  3885414 | 1373 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3284580 | 1374 | `		 if( apNode[iCur] == 0 ){` |
|  1278160 | 1375 | `			 continue;` |
|        - | 1376 | `		 }` |
|  2006422 | 1377 | `		 pNode = apNode[iCur];` |
|  2006422 | 1378 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   518080 | 1379 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1380 | `				 /* Collect function arguments */` |
|   331862 | 1381 | `				 sxi32 iPtr = 0;` |
|   331862 | 1382 | `				 sxi32 nFuncTok = 0;` |
|  1200106 | 1383 | `				 while( nFuncTok + iCur < nToken ){` |
|  1200106 | 1384 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1188514 | 1385 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   343958 | 1386 | `							 iPtr++;` |
|  1016536 | 1387 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   343958 | 1388 | `							 iPtr--;` |
|   343958 | 1389 | `							 if( iPtr <= 0 ){` |
|   331862 | 1390 | `								 break;` |
|        - | 1391 | `							 }` |
|     6048 | 1392 | `						 }` |
|   428326 | 1393 | `					 }` |
|   868246 | 1394 | `					 nFuncTok++;` |
|        2 | 1395 | `				 }` |
|   331862 | 1396 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1397 | `					 /* Syntax error */` |
|      ! 0 | 1398 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1399 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1400 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1401 | `					 }` |
|      ! 0 | 1402 | `					 return rc;` |
|        - | 1403 | `				 }` |
|   331862 | 1404 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1405 | `					 /* Syntax error */` |
|      ! 0 | 1406 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1407 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1408 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1409 | `					 }` |
|      ! 0 | 1410 | `					 return rc;` |
|        - | 1411 | `				 }` |
|   331862 | 1412 | `				 if( nFuncTok > 1 ){` |
|        - | 1413 | `					 /* Process function arguments */` |
|   291878 | 1414 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   291878 | 1415 | `					 if( rc != SXRET_OK ){` |
|        9 | 1416 | `						 return rc;` |
|        - | 1417 | `					 }` |
|   145934 | 1418 | `				 }` |
|        - | 1419 | `				 /* Link the node to the tree */` |
|   331854 | 1420 | `				 pNode->pLeft = apNode[iLeft];` |
|   331854 | 1421 | `				 apNode[iLeft] = 0;` |
|  1200074 | 1422 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   868222 | 1423 | `					 apNode[iCur+iPtr] = 0;` |
|   434112 | 1424 | `				 }` |
|   352146 | 1425 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1426 | `				 /* Subscripting */` |
|    74346 | 1427 | `				 sxi32 iArrTok = iCur + 1;` |
|    74346 | 1428 | `				 sxi32 iNest = 1;` |
|    74428 | 1429 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1430 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1431 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1432 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    74344 | 1433 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1434 | `						 /* Syntax error */` |
|      ! 0 | 1435 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1436 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1437 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1438 | `						 }` |
|      ! 0 | 1439 | `						 return rc;` |
|        - | 1440 | `				 }` |
|        - | 1441 | `				 /* Collect index tokens */` |
|   134240 | 1442 | `				 while( iArrTok < nToken ){` |
|   134240 | 1443 | `					 if( apNode[iArrTok] ){` |
|   134208 | 1444 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1445 | `							 /* Increment nesting level */` |
|      ! 0 | 1446 | `							 iNest++;` |
|   134208 | 1447 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1448 | `							 /* Decrement nesting level */` |
|    74346 | 1449 | `							 iNest--;` |
|    74346 | 1450 | `							 if( iNest <= 0 ){` |
|    74346 | 1451 | `								 break;` |
|        - | 1452 | `							 }` |
|      ! 0 | 1453 | `						 }` |
|    29931 | 1454 | `					 }` |
|    59896 | 1455 | `					 ++iArrTok;` |
|        2 | 1456 | `				 }` |
|    74346 | 1457 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1458 | `					 /* Recurse and process this expression */` |
|    59786 | 1459 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    59786 | 1460 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1461 | `						 return rc;` |
|        - | 1462 | `					 }` |
|        - | 1463 | `					 /* Link the node to it's index */` |
|    59786 | 1464 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    29892 | 1465 | `				 }` |
|        - | 1466 | `				 /* Link the node to the tree */` |
|    74346 | 1467 | `				 pNode->pLeft = apNode[iLeft];` |
|    74346 | 1468 | `				 pNode->pRight = 0;` |
|    74346 | 1469 | `				 apNode[iLeft] = 0;` |
|   208584 | 1470 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   134240 | 1471 | `					 apNode[iNest] = 0;` |
|    67121 | 1472 | `				 }` |
|    37174 | 1473 | `			 }else{` |
|        - | 1474 | `				 /* Member access operators [i.e: '->','::'] */` |
|   111876 | 1475 | `				  iRight = iCur + 1;` |
|   111876 | 1476 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1477 | `					 iRight++;` |
|      ! 0 | 1478 | `				 }` |
|   111876 | 1479 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1480 | `					 /* Syntax error */` |
|        5 | 1481 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1482 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1483 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1484 | `					 }` |
|        5 | 1485 | `					 return rc;` |
|        - | 1486 | `				 }` |
|        - | 1487 | `				 /* Link the node to the tree */` |
|   111872 | 1488 | `				 pNode->pLeft = apNode[iLeft];` |
|   167653 | 1489 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   111758 | 1490 | `					 && pNode->pLeft->pOp == 0 &&` |
|   111566 | 1491 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1492 | `						 /* Syntax error */` |
|      ! 0 | 1493 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1494 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1495 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1496 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1497 | `						 }` |
|      ! 0 | 1498 | `						 return rc;` |
|        - | 1499 | `				 }` |
|   111872 | 1500 | `				 pNode->pRight = apNode[iRight];` |
|   111872 | 1501 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1502 | `			 }` |
|   259033 | 1503 | `		 }` |
|  2006410 | 1504 | `		 iLeft = iCur;` |
|  1003206 | 1505 | `	 }` |
|        - | 1506 | `	 /* Handle left associative (new, clone) operators */` |
|  3885382 | 1507 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3284548 | 1508 | `		 if( apNode[iCur] == 0 ){` |
|  1811418 | 1509 | `			 continue;` |
|        - | 1510 | `		 }` |
|  1473132 | 1511 | `		 pNode = apNode[iCur];` |
|  1473132 | 1512 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1513 | `			 SyToken *pToken;` |
|        - | 1514 | `			 /* Get the left node */` |
|    15194 | 1515 | `			 iLeft = iCur + 1;` |
|    30354 | 1516 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    15162 | 1517 | `				 iLeft++;` |
|        2 | 1518 | `			 }` |
|    15194 | 1519 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1520 | `				  /* Syntax error */` |
|      ! 0 | 1521 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1522 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1523 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1524 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1525 | `				 }` |
|      ! 0 | 1526 | `				 return rc;` |
|        - | 1527 | `			 }` |
|        - | 1528 | `			 /* Make sure the operand are of a valid type */` |
|    15194 | 1529 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1530 | `				 /* Clone:` |
|        - | 1531 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1532 | `				  *  ++ function call (including annonymous)` |
|        - | 1533 | `				  *  ++ array member` |
|        - | 1534 | `				  *  ++ 'new' operator` |
|        - | 1535 | `				  * Example:` |
|        - | 1536 | `				  *   clone $pObj;` |
|        - | 1537 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1538 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1539 | `				  */` |
|       20 | 1540 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1541 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1542 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1543 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1544 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1545 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1546 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1547 | `						 }` |
|      ! 0 | 1548 | `						 return rc;` |
|        - | 1549 | `					 }` |
|        8 | 1550 | `				 }` |
|       11 | 1551 | `			 }else{` |
|        - | 1552 | `				 /* New */` |
|    15176 | 1553 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1554 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1555 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1556 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1557 | `						 /* Syntax error */` |
|      ! 0 | 1558 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1559 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1560 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1561 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1562 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1563 | `						 }` |
|      ! 0 | 1564 | `						 return rc;` |
|        - | 1565 | `					 }` |
|        8 | 1566 | `				 }` |
|        - | 1567 | `			 }` |
|        - | 1568 | `			  /* Link the node to the tree */` |
|    15194 | 1569 | `			 pNode->pLeft = apNode[iLeft];` |
|    15194 | 1570 | `			 apNode[iLeft] = 0;` |
|    15194 | 1571 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7596 | 1572 | `		 }` |
|   736567 | 1573 | `	 }` |
|        - | 1574 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   600836 | 1575 | `	 iLeft = -1;` |
|  3888516 | 1576 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3284548 | 1577 | `		 if( apNode[iCur] == 0 ){` |
|  1811418 | 1578 | `			 continue;` |
|        - | 1579 | `		 }` |
|  1473132 | 1580 | `		 pNode = apNode[iCur];` |
|  1473132 | 1581 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8722 | 1582 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3152 | 1583 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1584 | `					 /* Link the node to the tree */` |
|     3154 | 1585 | `					 pNode->pLeft = apNode[iLeft];` |
|     3154 | 1586 | `					 apNode[iLeft] = 0;` |
|     1576 | 1587 | `			 }` |
|     5927 | 1588 | `		  }` |
|  1476266 | 1589 | `		 iLeft = iCur;` |
|   739701 | 1590 | `	  }` |
|   603970 | 1591 | `	 iLeft = -1;` |
|  3888516 | 1592 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3284548 | 1593 | `		 if( apNode[iCur] == 0 ){` |
|  1814570 | 1594 | `			 continue;` |
|        - | 1595 | `		 }` |
|  1469980 | 1596 | `		 pNode = apNode[iCur];` |
|  1469980 | 1597 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8703 | 1598 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8704 | 1599 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1600 | `					 /* Syntax error */` |
|      ! 0 | 1601 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1602 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1603 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1604 | `					 }` |
|      ! 0 | 1605 | `					 return rc;` |
|        - | 1606 | `			 }` |
|        - | 1607 | `			 /* Link the node to the tree */` |
|     8704 | 1608 | `			 pNode->pLeft = apNode[iLeft];` |
|     8704 | 1609 | `			 apNode[iLeft] = 0;` |
|        - | 1610 | `			 /* Mark as pre-increment/decrement node */` |
|     8704 | 1611 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4351 | 1612 | `		  }` |
|  1469980 | 1613 | `		 iLeft = iCur;` |
|   734991 | 1614 | `	 }` |
|        - | 1615 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   603970 | 1616 | `	  iLeft = 0;` |
|  3888510 | 1617 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3284544 | 1618 | `		  if( apNode[iCur] ){` |
|  1461274 | 1619 | `			  pNode = apNode[iCur];` |
|  1461274 | 1620 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    38962 | 1621 | `				  if( iLeft > 0 ){` |
|        - | 1622 | `					  /* Link the node to the tree */` |
|    38960 | 1623 | `					  pNode->pLeft = apNode[iLeft];` |
|    38960 | 1624 | `					  apNode[iLeft] = 0;` |
|    38960 | 1625 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1626 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1627 | `							   /* Syntax error */` |
|      ! 0 | 1628 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1629 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1630 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1631 | `							  }` |
|      ! 0 | 1632 | `							  return rc;` |
|        - | 1633 | `						  }` |
|       36 | 1634 | `					  }` |
|    19481 | 1635 | `				  }else{` |
|        - | 1636 | `					  /* Syntax error */` |
|        3 | 1637 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1638 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1639 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1640 | `					  }` |
|        3 | 1641 | `					  return rc;` |
|        - | 1642 | `				  }` |
|    19479 | 1643 | `			  }` |
|        - | 1644 | `			  /* Save terminal position */` |
|  1461272 | 1645 | `			  iLeft = iCur;` |
|   730635 | 1646 | `		  }` |
|  1642272 | 1647 | `	  }` |
|        - | 1648 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6643542 | 1649 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  6039586 | 1650 | `		 iLeft = -1;` |
| 38884692 | 1651 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 32845118 | 1652 | `			 if( apNode[iCur] == 0 ){` |
| 20960796 | 1653 | `				 continue;` |
|        - | 1654 | `			 }` |
| 11884324 | 1655 | `			 pNode = apNode[iCur];` |
| 11884324 | 1656 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1657 | `				 /* Get the right node */` |
|   179442 | 1658 | `				 iRight = iCur + 1;` |
|   254856 | 1659 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    75416 | 1660 | `					 iRight++;` |
|        2 | 1661 | `				 }` |
|   179442 | 1662 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1663 | `					 /* Syntax error */` |
|        9 | 1664 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1665 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1666 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1667 | `					 }` |
|        9 | 1668 | `					 return rc;` |
|        - | 1669 | `				 }` |
|   179434 | 1670 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1671 | `					 sxi32  iTmp;` |
|        - | 1672 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1673 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1674 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1675 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1676 | `					  * is swapped below. */` |
|       50 | 1677 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1678 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1679 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1680 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1681 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1682 | `						 }` |
|        3 | 1683 | `						 return rc;` |
|        - | 1684 | `					 }` |
|       48 | 1685 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1686 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1687 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1688 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1689 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1690 | `						 }` |
|      ! 0 | 1691 | `						 return rc;` |
|        - | 1692 | `					 }` |
|       48 | 1693 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1694 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1695 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1696 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1697 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1698 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1699 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1700 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1701 | `									 }` |
|      ! 0 | 1702 | `									 return rc;` |
|        - | 1703 | `							 }` |
|      ! 0 | 1704 | `						 }` |
|       16 | 1705 | `					 }` |
|        - | 1706 | `					 /* Swap operands */` |
|       48 | 1707 | `					 iTmp = iRight;` |
|       48 | 1708 | `					 iRight = iLeft;` |
|       48 | 1709 | `					 iLeft = iTmp;` |
|       23 | 1710 | `				 }` |
|        - | 1711 | `				 /* Link the node to the tree */` |
|   179432 | 1712 | `				 pNode->pLeft = apNode[iLeft];` |
|   179432 | 1713 | `				 pNode->pRight = apNode[iRight];` |
|   179432 | 1714 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    89715 | 1715 | `			 }` |
| 11884314 | 1716 | `			 iLeft = iCur;` |
|  5942158 | 1717 | `		 }` |
|  3019789 | 1718 | `	 }` |
|        - | 1719 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1720 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1721 | `	  * we are dealing with a single operator.` |
|        - | 1722 | `	  */` |
|   603958 | 1723 | `	  iLeft = -1;` |
|  3879710 | 1724 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3277776 | 1725 | `		  if( apNode[iCur] == 0 ){` |
|  2220374 | 1726 | `			  continue;` |
|        - | 1727 | `		  }` |
|  1057404 | 1728 | `		  pNode = apNode[iCur];` |
|  1057404 | 1729 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2024 | 1730 | `			  sxi32 iNest = 1;` |
|     2024 | 1731 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1732 | `				  /* Missing condition */` |
|        3 | 1733 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1734 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1735 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1736 | `				  }` |
|        3 | 1737 | `				  return rc;` |
|        - | 1738 | `			  }` |
|        - | 1739 | `			  /* Get the right node */` |
|     2022 | 1740 | `			  iRight = iCur + 1;` |
|     4286 | 1741 | `			  while( iRight < nToken  ){` |
|     4286 | 1742 | `				  if( apNode[iRight] ){` |
|     3974 | 1743 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1744 | `						  /* Increment nesting level */` |
|      ! 0 | 1745 | `						  ++iNest;` |
|     3974 | 1746 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1747 | `						  /* Decrement nesting level */` |
|     2022 | 1748 | `						  --iNest;` |
|     2022 | 1749 | `						  if( iNest <= 0 ){` |
|     2022 | 1750 | `							  break;` |
|        - | 1751 | `						  }` |
|      ! 0 | 1752 | `					  }` |
|      976 | 1753 | `				  }` |
|     2266 | 1754 | `				  iRight++;` |
|        2 | 1755 | `			  }` |
|     2022 | 1756 | `			  if( iRight > iCur + 1 ){` |
|        - | 1757 | `				  /* Recurse and process the then expression */` |
|     1954 | 1758 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1954 | 1759 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1760 | `					  return rc;` |
|        - | 1761 | `				  }` |
|        - | 1762 | `				  /* Link the node to the tree */` |
|     1954 | 1763 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      976 | 1764 | `			  }else{` |
|        - | 1765 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1766 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1767 | `			  }` |
|     2022 | 1768 | `			  apNode[iCur + 1] = 0;` |
|     2022 | 1769 | `			  if( iRight + 1 < nToken ){` |
|        - | 1770 | `				  /* Recurse and process the else expression */` |
|     2022 | 1771 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2022 | 1772 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1773 | `					  return rc;` |
|        - | 1774 | `				  }` |
|        - | 1775 | `				  /* Link the node to the tree */` |
|     2022 | 1776 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2022 | 1777 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1012 | 1778 | `			  }else{` |
|      ! 0 | 1779 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1780 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1781 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1782 | `				 }` |
|      ! 0 | 1783 | `				 return rc;` |
|        - | 1784 | `			  }` |
|        - | 1785 | `			  /* Point to the condition */` |
|     2022 | 1786 | `			  pNode->pCond  = apNode[iLeft];` |
|     2022 | 1787 | `			  apNode[iLeft] = 0;` |
|     2022 | 1788 | `			  break;` |
|        - | 1789 | `		  }` |
|  1055382 | 1790 | `		  iLeft = iCur;` |
|   527692 | 1791 | `	  }` |
|        - | 1792 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1793 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1794 | `	  * so there is no need for a precedence loop here.` |
|        - | 1795 | `	  */` |
|   603956 | 1796 | `	 iRight = -1;` |
|  3888312 | 1797 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3284412 | 1798 | `		 if( apNode[iCur] == 0 ){` |
|  2454686 | 1799 | `			 continue;` |
|        - | 1800 | `		 }` |
|   829728 | 1801 | `		 pNode = apNode[iCur];` |
|   829728 | 1802 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1803 | `			 /* Get the left node */` |
|   225652 | 1804 | `			 iLeft = iCur - 1;` |
|   319438 | 1805 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    93788 | 1806 | `				 iLeft--;` |
|        2 | 1807 | `			 }` |
|   225652 | 1808 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1809 | `				 /* Syntax error */` |
|       43 | 1810 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1811 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1812 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1813 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1814 | `				 }else{` |
|       39 | 1815 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1816 | `				 }` |
|       43 | 1817 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1818 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1819 | `				 }` |
|       43 | 1820 | `				 return rc;` |
|        - | 1821 | `			 }` |
|        - | 1822 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1823 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1824 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1825 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1826 | `			  * a write. */` |
|   225610 | 1827 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        9 | 1828 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1829 | `					 "Can't use nullsafe operator in write context");` |
|        9 | 1830 | `				 if( rc != SXERR_ABORT ){` |
|        9 | 1831 | `					 rc = SXERR_SYNTAX;` |
|        4 | 1832 | `				 }` |
|        9 | 1833 | `				 return rc;` |
|        - | 1834 | `			 }` |
|   225602 | 1835 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1836 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1837 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1838 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1839 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1840 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1841 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1842 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1843 | `					 }else{` |
|        4 | 1844 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1845 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1846 | `					 }` |
|        5 | 1847 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1848 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1849 | `					 }` |
|        5 | 1850 | `					 return rc;` |
|        - | 1851 | `				 }` |
|       26 | 1852 | `			 }` |
|        - | 1853 | `			 /* Link the node to the tree (Reverse) */` |
|   225598 | 1854 | `			 pNode->pLeft = apNode[iRight];` |
|   225598 | 1855 | `			 pNode->pRight = apNode[iLeft];` |
|   225598 | 1856 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   112798 | 1857 | `		 }` |
|   829674 | 1858 | `		 iRight = iCur;` |
|   414838 | 1859 | `	 }` |
|        - | 1860 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3019502 | 1861 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2415602 | 1862 | `		 iLeft = -1;` |
| 15552970 | 1863 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 13137370 | 1864 | `			 if( apNode[iCur] == 0 ){` |
| 10721364 | 1865 | `				 continue;` |
|        - | 1866 | `			 }` |
|  2416008 | 1867 | `			 pNode = apNode[iCur];` |
|  2416008 | 1868 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1869 | `				 /* Get the right node */` |
|       72 | 1870 | `				 iRight = iCur + 1;` |
|      110 | 1871 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1872 | `					 iRight++;` |
|        2 | 1873 | `				 }` |
|       72 | 1874 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1875 | `					 /* Syntax error */` |
|      ! 0 | 1876 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1877 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1878 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1879 | `					 }` |
|      ! 0 | 1880 | `					 return rc;` |
|        - | 1881 | `				 }` |
|        - | 1882 | `				 /* Link the node to the tree */` |
|       72 | 1883 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1884 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1885 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1886 | `			 }` |
|  2416008 | 1887 | `			 iLeft = iCur;` |
|  1208005 | 1888 | `		 }` |
|  1207802 | 1889 | `	 }` |
|        - | 1890 | `	 /* Point to the root of the expression tree */` |
|  3284316 | 1891 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2680434 | 1892 | `		 if( apNode[iCur] ){` |
|   545130 | 1893 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1894 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1895 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1896 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1897 | `				  }` |
|       20 | 1898 | `				  return rc;` |
|        - | 1899 | `			 }` |
|   545112 | 1900 | `			 apNode[0] = apNode[iCur];` |
|   545112 | 1901 | `			 apNode[iCur] = 0;` |
|   272555 | 1902 | `		 }` |
|  1340209 | 1903 | `	 }` |
|   603884 | 1904 | `	 return SXRET_OK;` |
|   561851 | 1905 | ` }` |
|        - | 1906 | ` /*` |
|        - | 1907 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1908 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1909 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1910 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1911 | `  */` |
|   704600 | 1912 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1913 |  |
|        - | 1914 | `	ph7_expr_node **apNode;` |
|        - | 1915 | `	ph7_expr_node *pNode;` |
|        - | 1916 | `	sxi32 rc;` |
|        - | 1917 | `	/* Reset node container */` |
|   704602 | 1918 | `	SySetReset(pExprNode);` |
|   704602 | 1919 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1920 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1921 | `	{` |
|   704602 | 1922 | `		int iLastWasTerm = 0;` |
|  3809006 | 1923 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3104440 | 1924 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3104440 | 1925 | `			if( rc != SXRET_OK ){` |
|       35 | 1926 | `				return rc;` |
|        - | 1927 | `			}` |
|        - | 1928 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3104406 | 1929 | `			if( pNode->xCode ){` |
|        - | 1930 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1616474 | 1931 | `				iLastWasTerm = 1;` |
|  2296170 | 1932 | `			}else if( pNode->pOp ){` |
|        - | 1933 | `				/* Operator node */` |
|   698522 | 1934 | `				iLastWasTerm = 0;` |
|   349262 | 1935 | `			}else{` |
|        - | 1936 | `				/* Delimiter: ')' and ']' end terms */` |
|   789414 | 1937 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1938 | `			}` |
|        - | 1939 | `			/* Save the extracted node */` |
|  3104406 | 1940 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1941 | `		}` |
|        - | 1942 | `	}` |
|   704568 | 1943 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1944 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1945 | `		*ppRoot = 0;` |
|      ! 0 | 1946 | `		return SXRET_OK;` |
|        - | 1947 | `	}` |
|   704568 | 1948 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1949 | `	/* Make sure we are dealing with valid nodes */` |
|   704568 | 1950 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   704568 | 1951 | `	if( rc != SXRET_OK ){` |
|        - | 1952 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1953 | `		 * cleanup the mess left behind.` |
|        - | 1954 | `		 */` |
|       51 | 1955 | `		*ppRoot = 0;` |
|       51 | 1956 | `		return rc;` |
|        - | 1957 | `	}` |
|        - | 1958 | `	/* Build the tree */` |
|   704518 | 1959 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   704518 | 1960 | `	if( rc != SXRET_OK ){` |
|        - | 1961 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      100 | 1962 | `		*ppRoot = 0;` |
|      100 | 1963 | `		return rc;` |
|        - | 1964 | `	}` |
|        - | 1965 | `	/* Point to the root of the tree */` |
|   704420 | 1966 | `	*ppRoot = apNode[0];` |
|   704420 | 1967 | `	return SXRET_OK;` |
|   352302 | 1968 |  |
|        - | 1969 |  |
