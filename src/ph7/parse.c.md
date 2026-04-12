# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 917/1091 lines (84.05%)

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
|        - |  158 | `	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|        - |  159 | `	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},` |
|        - |  160 | `	/* Precedence 3,non-associative  */` |
|        - |  161 | `	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},` |
|        - |  162 | `	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},` |
|        - |  163 | `	                              /* Unary operators */` |
|        - |  164 | `	/* Precedence 4,right-associative  */` |
|        - |  165 | `	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },` |
|        - |  166 | `	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },` |
|        - |  167 | `	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },` |
|        - |  168 | `	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },` |
|        - |  169 | `	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},` |
|        - |  170 | `	                             /* Cast operators */` |
|        - |  171 | `	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },` |
|        - |  172 | `	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },` |
|        - |  173 | `	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },` |
|        - |  174 | `	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|        - |  175 | `	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},` |
|        - |  176 | `	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },` |
|        - |  177 | `	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },` |
|        - |  178 | `	                           /* Binary operators */` |
|        - |  179 | `	/* Precedence 7,left-associative */` |
|        - |  180 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|        - |  181 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|        - |  182 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|        - |  183 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|        - |  184 | `	/* Precedence 8,left-associative */` |
|        - |  185 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|        - |  186 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|        - |  187 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|        - |  188 | `	/* Precedence 9,left-associative */` |
|        - |  189 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|        - |  190 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|        - |  191 | `	/* Precedence 10,non-associative */` |
|        - |  192 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|        - |  193 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|        - |  194 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|        - |  195 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|        - |  196 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|        - |  197 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  198 | `	/* Precedence 11,non-associative */` |
|        - |  199 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|        - |  200 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  201 | `	{ {"eq",sizeof(char)*2},  EXPR_OP_SEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_SEQ}, /* IMP-0137-EQ: Symisc eXtension */` |
|        - |  202 | `	{ {"ne",sizeof(char)*2},  EXPR_OP_SNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_SNE}, /* IMP-0138-NE: Symisc eXtension */` |
|        - |  203 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|        - |  204 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|        - |  205 | `	/* Precedence 12,left-associative */` |
|        - |  206 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|        - |  207 | `	/* Precedence 12,left-associative */` |
|        - |  208 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|        - |  209 | `	                         /* Binary operators */` |
|        - |  210 | `	/* Precedence 13,left-associative */` |
|        - |  211 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|        - |  212 | `	/* Precedence 14,left-associative */` |
|        - |  213 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|        - |  214 | `	/* Precedence 15,left-associative */` |
|        - |  215 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  216 | `	/* Precedence 16,left-associative */` |
|        - |  217 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  218 | `	                      /* Null coalescing operator */` |
|        - |  219 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|        - |  220 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|        - |  221 | `	                      /* Ternary operator */` |
|        - |  222 | `	/* Precedence 17,left-associative */` |
|        - |  223 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  224 | `	                     /* Combined binary operators */` |
|        - |  225 | `	/* Precedence 18,right-associative */` |
|        - |  226 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  227 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  228 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  229 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  230 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  231 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  232 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  233 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  234 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  235 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  236 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  237 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  238 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|        - |  239 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|        - |  240 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|        - |  241 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|        - |  242 | `	 * in this file: keep one of the question marks escaped. */` |
|        - |  243 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|        - |  244 | `	/* Precedence 19,left-associative */` |
|        - |  245 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  246 | `	/* Precedence 20,left-associative */` |
|        - |  247 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  248 | `	/* Precedence 21,left-associative */` |
|        - |  249 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  250 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  251 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  252 | `};` |
|        - |  253 | `/* Function call operator need special handling */` |
|        - |  254 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  255 | `/*` |
|        - |  256 | ` * Check if the given token is a potential operator or not.` |
|        - |  257 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  258 | ` * look like an operator.` |
|        - |  259 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  260 | ` * Otherwise NULL.` |
|        - |  261 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  262 | ` * a binary minus or unary minus.]` |
|        - |  263 | ` */` |
|   773378 |  264 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  265 |  |
|   773380 |  266 | `	sxu32 n = 0;` |
|        - |  267 | `	sxi32 rc;` |
|        - |  268 | `	/* Do a linear lookup on the operators table */` |
| 12670310 |  269 | `	for(;;){` |
| 25340622 |  270 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  271 | `			break;` |
|        - |  272 | `		}` |
| 25340622 |  273 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  274 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3083240 |  275 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1541621 |  276 | `		}else{` |
| 22257384 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  278 | `		}` |
| 25340622 |  279 | `		if( rc == 0 ){` |
|   776788 |  280 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  281 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   773050 |  282 | `				return &aOpTable[n];` |
|        - |  283 | `			}` |
|        - |  284 | `			/* Handle ambiguity */` |
|     3740 |  285 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  286 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  287 | `				return &aOpTable[n];` |
|        - |  288 | `			}` |
|     3520 |  289 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  290 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  291 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  292 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  293 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  294 | `					return &aOpTable[n];` |
|        - |  295 | `				}` |
|        - |  296 |  |
|        4 |  297 | `			}` |
|     1704 |  298 | `		}` |
| 24567244 |  299 | `		++n; /* Next operator in the table */` |
|        2 |  300 | `	}` |
|        - |  301 | `	/* No such operator */` |
|      ! 0 |  302 | `	return 0;` |
|   386691 |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * Delimit a set of token stream.` |
|        - |  306 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  307 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  308 | ` */` |
|   397750 |  309 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  310 |  |
|   397752 |  311 | `	SyToken *pCur = pIn;` |
|   397752 |  312 | `	sxi32 iNest = 1;` |
|  2260220 |  313 | `	for(;;){` |
|  4520442 |  314 | `		if( pCur >= pEnd ){` |
|      128 |  315 | `			break;` |
|        - |  316 | `		}` |
|  4520316 |  317 | `		if( pCur->nType & nTokStart ){` |
|        - |  318 | `			/* Increment nesting level */` |
|   249938 |  319 | `			iNest++;` |
|  4395348 |  320 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  321 | `			/* Decrement nesting level */` |
|   647562 |  322 | `			iNest--;` |
|   647562 |  323 | `			if( iNest <= 0 ){` |
|   397626 |  324 | `				break;` |
|        - |  325 | `			}` |
|   124968 |  326 | `		}` |
|        - |  327 | `		/* Advance cursor */` |
|  4122692 |  328 | `		pCur++;` |
|        2 |  329 | `	}` |
|        - |  330 | `	/* Point to the end of the chunk */` |
|   397752 |  331 | `	*ppEnd = pCur;` |
|   397752 |  332 |  |
|        - |  333 | `/*` |
|        - |  334 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  335 | ` * Note on reserved keywords.` |
|        - |  336 | ` *  According to the PHP language reference manual:` |
|        - |  337 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  338 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  339 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  340 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  341 | ` */` |
|    11944 |  342 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  343 |  |
|    17850 |  344 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11851 |  345 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  346 | `		){` |
|      146 |  347 | `			return TRUE;` |
|        - |  348 | `	}` |
|    11802 |  349 | `	if( bCheckFunc ){` |
|       92 |  350 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  351 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  352 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  353 | `				return TRUE;` |
|        - |  354 | `		}` |
|       20 |  355 | `	}` |
|        - |  356 | `	/* Not a language construct */` |
|    11770 |  357 | `	return FALSE;` |
|     5974 |  358 |  |
|        - |  359 | `/*` |
|        - |  360 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  361 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  362 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  363 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  364 | ` */` |
|   680794 |  365 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  366 |  |
|        - |  367 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  368 | `	sxi32 i,rc;` |
|        - |  369 |  |
|   680796 |  370 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  371 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  372 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  373 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  374 | `	}` |
|   680796 |  375 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3681804 |  376 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3001044 |  377 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  378 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      314 |  379 | `			continue;` |
|        - |  380 | `		}` |
|  3000732 |  381 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   344688 |  382 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    17732 |  383 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  384 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   320976 |  385 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  386 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  387 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  388 | `						 */` |
|   320976 |  389 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   320976 |  390 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   320976 |  391 | `						apNode[i]->pOp = &sFCallOp;` |
|   160487 |  392 | `					}` |
|   160487 |  393 | `			}` |
|   344688 |  394 | `			iParen++;` |
|  2828389 |  395 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   344688 |  396 | `			if( iParen <= 0 ){` |
|       13 |  397 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  398 | `				if( rc != SXERR_ABORT ){` |
|       13 |  399 | `					rc = SXERR_SYNTAX;` |
|        6 |  400 | `				}` |
|       13 |  401 | `				return rc;` |
|        - |  402 | `			}` |
|   344676 |  403 | `			iParen--;` |
|  2483697 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    71918 |  405 | `			iSquare++;` |
|  2275402 |  406 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    71932 |  407 | `			if( iSquare <= 0 ){` |
|        7 |  408 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  409 | `				if( rc != SXERR_ABORT ){` |
|        7 |  410 | `					rc = SXERR_SYNTAX;` |
|        3 |  411 | `				}` |
|        7 |  412 | `				return rc;` |
|        - |  413 | `			}` |
|    71926 |  414 | `			iSquare--;` |
|  2203476 |  415 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       11 |  416 | `			iBraces++;` |
|       11 |  417 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  418 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  419 | `				int iNest = 1;` |
|       11 |  420 | `				sxi32 j=i+1;` |
|        - |  421 | `				/*` |
|        - |  422 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  423 | `				 */` |
|       11 |  424 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  425 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  426 | `				pOp = aOpTable;` |
|       11 |  427 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       51 |  428 | `				while( pOp < pEnd ){` |
|       51 |  429 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  430 | `						break;` |
|        - |  431 | `					}` |
|       41 |  432 | `					pOp++;` |
|        1 |  433 | `				}` |
|       11 |  434 | `				if( pOp >= pEnd ){` |
|      ! 0 |  435 | `					pOp = 0;` |
|      ! 0 |  436 | `				}` |
|       11 |  437 | `				if( pOp ){` |
|       11 |  438 | `					apNode[i]->pOp = pOp;` |
|       11 |  439 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  440 | `				}` |
|       11 |  441 | `				iBraces--;` |
|       11 |  442 | `				iSquare++;` |
|       21 |  443 | `				while( j < nNode ){` |
|       21 |  444 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  445 | `						/* Increment nesting level */` |
|      ! 0 |  446 | `						iNest++;` |
|       21 |  447 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  448 | `						/* Decrement nesting level */` |
|       11 |  449 | `						iNest--;` |
|       11 |  450 | `						if( iNest < 1 ){` |
|       11 |  451 | `							break;` |
|        - |  452 | `						}` |
|      ! 0 |  453 | `					}` |
|       11 |  454 | `					j++;` |
|        1 |  455 | `				}` |
|       11 |  456 | `				if( j < nNode ){` |
|       11 |  457 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  458 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  459 | `				}` |
|        - |  460 |  |
|        6 |  461 | `			}` |
|  2167509 |  462 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  463 | `			if( iBraces <= 0 ){` |
|       13 |  464 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  465 | `				if( rc != SXERR_ABORT ){` |
|       13 |  466 | `					rc = SXERR_SYNTAX;` |
|        6 |  467 | `				}` |
|       13 |  468 | `				return rc;` |
|        - |  469 | `			}` |
|      ! 0 |  470 | `			iBraces--;` |
|  2167492 |  471 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1958 |  472 | `			if( iQuesty <= 0 ){` |
|        5 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  474 | `				if( rc != SXERR_ABORT ){` |
|        5 |  475 | `					rc = SXERR_SYNTAX;` |
|        2 |  476 | `				}` |
|        5 |  477 | `				return rc;` |
|        - |  478 | `			}` |
|     1954 |  479 | `			iQuesty--;` |
|  2166512 |  480 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   603420 |  481 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   603420 |  482 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1956 |  483 | `				iQuesty++;` |
|   602443 |  484 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      318 |  485 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  486 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  487 | `					sxu32 n = 0;` |
|       11 |  488 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  489 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  490 | `					}` |
|        - |  491 | `					/*` |
|        - |  492 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  493 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  494 | `					 */` |
|      245 |  495 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      235 |  496 | `						++n;` |
|        1 |  497 | `					}` |
|       11 |  498 | `					pOp = &aOpTable[n];` |
|        - |  499 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  500 | `					apNode[i]->pOp = pOp;` |
|       11 |  501 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  502 | `				}` |
|      158 |  503 | `			}` |
|   301709 |  504 | `		}` |
|  1500350 |  505 | `	}` |
|   680762 |  506 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  507 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  508 | `		if( rc != SXERR_ABORT ){` |
|       17 |  509 | `			rc = SXERR_SYNTAX;` |
|        8 |  510 | `		}` |
|       17 |  511 | `		return rc;` |
|        - |  512 | `	}` |
|   680746 |  513 | `	return SXRET_OK;` |
|   340399 |  514 |  |
|        - |  515 | `/*` |
|        - |  516 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  517 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  518 | ` */` |
|   549338 |  519 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  520 |  |
|   549340 |  521 | `	SyToken *pIn = *ppCur;` |
|        - |  522 | `	/* Jump the first literal seen */` |
|   549340 |  523 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   549316 |  524 | `		pIn++;` |
|   274657 |  525 | `	}` |
|   274701 |  526 | `	for(;;){` |
|   549404 |  527 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       66 |  528 | `			pIn++;` |
|       66 |  529 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       66 |  530 | `				pIn++;` |
|       32 |  531 | `			}` |
|       34 |  532 | `		}else{` |
|   274671 |  533 | `			break;` |
|        - |  534 | `		}` |
|        2 |  535 | `	}` |
|        - |  536 | `	/* Synchronize pointers */` |
|   549340 |  537 | `	*ppCur = pIn;` |
|   549340 |  538 |  |
|        - |  539 | `/*` |
|        - |  540 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  541 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  542 | ` * Note on annonymous functions.` |
|        - |  543 | ` *  According to the PHP language reference manual:` |
|        - |  544 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  545 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  546 | ` *  parameters, but they have many other uses.` |
|        - |  547 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  548 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  549 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  550 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  551 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  552 | ` *` |
|        - |  553 | ` * Some example:` |
|        - |  554 | ` *  $greet = function($name)` |
|        - |  555 | ` * {` |
|        - |  556 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  557 | ` * };` |
|        - |  558 | ` *  $greet('World');` |
|        - |  559 | ` *  $greet('PHP');` |
|        - |  560 | ` *` |
|        - |  561 | ` * $double = function($a) {` |
|        - |  562 | ` *   return $a * 2;` |
|        - |  563 | ` * };` |
|        - |  564 | ` * // This is our range of numbers` |
|        - |  565 | ` * $numbers = range(1, 5);` |
|        - |  566 | ` * // Use the Annonymous function as a callback here to` |
|        - |  567 | ` * // double the size of each element in our` |
|        - |  568 | ` * // range` |
|        - |  569 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  570 | ` * print implode(' ', $new_numbers);` |
|        - |  571 | ` */` |
|      194 |  572 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  573 |  |
|      196 |  574 | `	SyToken *pIn = *ppCur;` |
|        - |  575 | `	sxu32 nLine;` |
|        - |  576 | `	sxi32 rc;` |
|        - |  577 | `	/* Jump the 'function' keyword */` |
|      196 |  578 | `	nLine = pIn->nLine;` |
|      196 |  579 | `	pIn++;` |
|      196 |  580 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  581 | `		pIn++;` |
|        1 |  582 | `	}` |
|      196 |  583 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  584 | `		/* Syntax error */` |
|        5 |  585 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  586 | `		if( rc != SXERR_ABORT ){` |
|        5 |  587 | `			rc = SXERR_SYNTAX;` |
|        2 |  588 | `		}` |
|        5 |  589 | `		goto Synchronize;` |
|        - |  590 | `	}` |
|      192 |  591 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      192 |  592 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      192 |  593 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  594 | `		/* Syntax error */` |
|        5 |  595 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  596 | `		if( rc != SXERR_ABORT ){` |
|        5 |  597 | `			rc = SXERR_SYNTAX;` |
|        2 |  598 | `		}` |
|        5 |  599 | `		goto Synchronize;` |
|        - |  600 | `	}` |
|      188 |  601 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  602 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      188 |  603 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  604 | `		pIn++; /* Skip ':' */` |
|        - |  605 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  606 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  607 | `			pIn++;` |
|      ! 0 |  608 | `		}` |
|        - |  609 | `		/* Skip the first type (allow leading '\' and namespace path) */` |
|        5 |  610 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        5 |  611 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  612 | `			pIn++;` |
|        5 |  613 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  614 | `				pIn += 2;` |
|      ! 0 |  615 | `			}` |
|        2 |  616 | `		}` |
|        - |  617 | `		/* Skip union alternatives ( \| type )* */` |
|        6 |  618 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        3 |  619 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  620 | `			pIn++;` |
|      ! 0 |  621 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  622 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  623 | `				pIn++;` |
|      ! 0 |  624 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  625 | `					pIn += 2;` |
|      ! 0 |  626 | `				}` |
|      ! 0 |  627 | `			}` |
|      ! 0 |  628 | `		}` |
|        2 |  629 | `	}` |
|      188 |  630 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       32 |  631 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  632 | `		/* Check if we are dealing with a closure */` |
|       32 |  633 | `		if( nKey == PH7_TKWRD_USE ){` |
|       24 |  634 | `			pIn++; /* Jump the 'use' keyword */` |
|       24 |  635 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  636 | `				/* Syntax error */` |
|        5 |  637 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  638 | `				if( rc != SXERR_ABORT ){` |
|        5 |  639 | `					rc = SXERR_SYNTAX;` |
|        2 |  640 | `				}` |
|        5 |  641 | `				goto Synchronize;` |
|        - |  642 | `			}` |
|       20 |  643 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       20 |  644 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       20 |  645 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  646 | `				/* Syntax error */` |
|        5 |  647 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  648 | `				if( rc != SXERR_ABORT ){` |
|        5 |  649 | `					rc = SXERR_SYNTAX;` |
|        2 |  650 | `				}` |
|        5 |  651 | `				goto Synchronize;` |
|        - |  652 | `			}` |
|       16 |  653 | `			pIn++;` |
|        9 |  654 | `		}else{` |
|        - |  655 | `			/* Syntax error */` |
|        9 |  656 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  657 | `			if( rc != SXERR_ABORT ){` |
|        9 |  658 | `				rc = SXERR_SYNTAX;` |
|        4 |  659 | `			}` |
|        9 |  660 | `			goto Synchronize;` |
|        - |  661 | `		}` |
|        7 |  662 | `	}` |
|      172 |  663 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      172 |  664 | `		pIn++; /* Jump the leading curly '{' */` |
|      172 |  665 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      172 |  666 | `		if( pIn < pEnd ){` |
|      172 |  667 | `			pIn++;` |
|       85 |  668 | `		}` |
|       87 |  669 | `	}else{` |
|        - |  670 | `		/* Syntax error */` |
|      ! 0 |  671 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  672 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  673 | `			return SXERR_ABORT;` |
|        - |  674 | `		}` |
|        - |  675 | `	}` |
|      172 |  676 | `	rc = SXRET_OK;` |
|       97 |  677 | `Synchronize:` |
|        - |  678 | `	/* Synchronize pointers */` |
|      196 |  679 | `	*ppCur = pIn;` |
|      196 |  680 | `	return rc;` |
|       99 |  681 |  |
|        - |  682 | `/*` |
|        - |  683 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  684 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  685 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  686 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  687 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  688 | ` */` |
|       84 |  689 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  690 |  |
|       86 |  691 | `	SyToken *pIn = *ppCur;` |
|        - |  692 | `	sxu32 nLine;` |
|        - |  693 | `	sxi32 rc;` |
|        - |  694 | `	int iNest;` |
|       86 |  695 | `	nLine = pIn->nLine;` |
|        - |  696 | `	/* Optional 'static' prefix */` |
|       84 |  697 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       86 |  698 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  699 | `		pIn++;` |
|        1 |  700 | `	}` |
|        - |  701 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       84 |  702 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       86 |  703 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  704 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  705 | `		goto Synchronize;` |
|        - |  706 | `	}` |
|       86 |  707 | `	pIn++; /* Jump 'fn' */` |
|       42 |  708 | `	SXUNUSED(nLine);` |
|       42 |  709 | `	SXUNUSED(pGen);` |
|        - |  710 | `	/* Optional '&' for return-by-reference */` |
|       86 |  711 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  712 | `		pIn++;` |
|      ! 0 |  713 | `	}` |
|        - |  714 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  715 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  716 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  717 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       86 |  718 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       84 |  719 | `		pIn++; /* '(' */` |
|       84 |  720 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       84 |  721 | `		if( pIn < pEnd ){` |
|       82 |  722 | `			pIn++; /* ')' */` |
|       40 |  723 | `		}` |
|       41 |  724 | `	}` |
|        - |  725 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|       86 |  726 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  727 | `		pIn++;` |
|        7 |  728 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  729 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  730 | `			pIn++;` |
|        1 |  731 | `		}` |
|        7 |  732 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        7 |  733 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  734 | `			pIn++;` |
|        7 |  735 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  736 | `				pIn += 2;` |
|      ! 0 |  737 | `			}` |
|        3 |  738 | `		}` |
|        9 |  739 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        4 |  740 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  741 | `			pIn++;` |
|      ! 0 |  742 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  743 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  744 | `				pIn++;` |
|      ! 0 |  745 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  746 | `					pIn += 2;` |
|      ! 0 |  747 | `				}` |
|      ! 0 |  748 | `			}` |
|      ! 0 |  749 | `		}` |
|        3 |  750 | `	}` |
|        - |  751 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       86 |  752 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       79 |  753 | `		pIn++;` |
|       39 |  754 | `	}` |
|        - |  755 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       86 |  756 | `	iNest = 0;` |
|      566 |  757 | `	while( pIn < pEnd ){` |
|      488 |  758 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  759 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  760 | `			break;` |
|        - |  761 | `		}` |
|      482 |  762 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       21 |  763 | `			iNest++;` |
|      472 |  764 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       21 |  765 | `			iNest--;` |
|       10 |  766 | `		}` |
|      482 |  767 | `		pIn++;` |
|        2 |  768 | `	}` |
|       86 |  769 | `	rc = SXRET_OK;` |
|       42 |  770 | `Synchronize:` |
|       86 |  771 | `	*ppCur = pIn;` |
|       86 |  772 | `	return rc;` |
|        2 |  773 |  |
|        - |  774 | `/*` |
|        - |  775 | ` * Extract a single expression node from the input.` |
|        - |  776 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  777 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  778 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  779 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  780 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  781 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  782 | ` */` |
|  3001206 |  783 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  784 |  |
|        - |  785 | `	ph7_expr_node *pNode;` |
|        - |  786 | `	SyToken *pCur;` |
|        - |  787 | `	sxi32 rc;` |
|        - |  788 | `	/* Allocate a new node */` |
|  3001208 |  789 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3001208 |  790 | `	if( pNode == 0 ){` |
|        - |  791 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  792 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  793 | `		 */` |
|      ! 0 |  794 | `		return SXERR_MEM;` |
|        - |  795 | `	}` |
|        - |  796 | `	/* Zero the structure */` |
|  3001208 |  797 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3001208 |  798 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  799 | `	/* Point to the head of the token stream */` |
|  3001208 |  800 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  801 | `	/* Start collecting tokens */` |
|  3001208 |  802 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  803 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  804 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       15 |  805 | `		pCur++;` |
|       15 |  806 | `		pGen->pIn = pCur;` |
|       15 |  807 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       15 |  808 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       15 |  809 | `		if( rc == SXRET_OK && *ppNode ){` |
|       15 |  810 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        7 |  811 | `		}` |
|       15 |  812 | `		return rc;` |
|        - |  813 | `	}` |
|  3001194 |  814 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  815 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  816 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  817 | `		 */` |
|      316 |  818 | `		pCur++; /* Skip the opening '[' */` |
|      316 |  819 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      316 |  820 | `		if( pCur < pGen->pEnd ){` |
|      316 |  821 | `			pCur++; /* Skip past the closing ']' */` |
|      159 |  822 | `		}else{` |
|      ! 0 |  823 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  824 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  825 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  826 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  827 | `			}` |
|      ! 0 |  828 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  829 | `			return rc;` |
|        - |  830 | `		}` |
|        - |  831 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  832 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  833 | `		 */` |
|      339 |  834 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  835 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  836 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  837 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  838 | `			}else{` |
|       19 |  839 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  840 | `			}` |
|       25 |  841 | `		}else{` |
|      270 |  842 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  843 | `		}` |
|  3001037 |  844 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  845 | `		/* Point to the instance that describe this operator */` |
|   675370 |  846 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  847 | `		/* Advance the stream cursor */` |
|   675370 |  848 | `		pCur++;` |
|  2663196 |  849 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  850 | `		/* Isolate variable */` |
|  1637890 |  851 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   818952 |  852 | `			pCur++; /* Variable variable */` |
|        2 |  853 | `		}` |
|   818940 |  854 | `		if( pCur < pGen->pEnd ){` |
|   818940 |  855 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  856 | `				/* Variable name */` |
|   818912 |  857 | `				pCur++;` |
|   409485 |  858 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  859 | `				pCur++;` |
|        - |  860 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  861 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  862 | `				if( pCur < pGen->pEnd ){` |
|       18 |  863 | `					pCur++;` |
|       10 |  864 | `				}else{` |
|        5 |  865 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  866 | `					if( rc != SXERR_ABORT ){` |
|        5 |  867 | `						rc = SXERR_SYNTAX;` |
|        2 |  868 | `					}` |
|        5 |  869 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  870 | `					return rc;` |
|        - |  871 | `				}` |
|        8 |  872 | `			}` |
|   409467 |  873 | `		}` |
|   818936 |  874 | `		pNode->xCode = PH7_CompileVariable;` |
|  1916041 |  875 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    36338 |  876 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    36338 |  877 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  878 | `			 /* List/Array node */` |
|    24218 |  879 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  880 | `				 /* Assume a literal */` |
|       17 |  881 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  882 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  883 | `			 }else{` |
|    24202 |  884 | `				 pCur += 2;` |
|        - |  885 | `				 /* Collect array/list tokens */` |
|    24202 |  886 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    24202 |  887 | `				 if( pCur < pGen->pEnd ){` |
|    24200 |  888 | `					 pCur++;` |
|    12101 |  889 | `				 }else{` |
|        - |  890 | `					 /* Syntax error */` |
|        4 |  891 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  892 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  893 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  894 | `						 rc = SXERR_SYNTAX;` |
|        1 |  895 | `					 }` |
|        3 |  896 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  897 | `					 return rc;` |
|        - |  898 | `				 }` |
|    24200 |  899 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    24200 |  900 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  901 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  902 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  903 | `						 /* Syntax error */` |
|        3 |  904 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  905 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  906 | `							 rc = SXERR_SYNTAX;` |
|        1 |  907 | `						 }` |
|        3 |  908 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  909 | `						 return rc;` |
|        - |  910 | `					 }` |
|       12 |  911 | `				 }` |
|        2 |  912 | `			 }` |
|    24228 |  913 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  914 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  915 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  916 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  917 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  918 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  919 | `			 pNode->xCode = PH7_CompileYield;` |
|    12106 |  920 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  921 | `			 /* Annonymous function */` |
|      196 |  922 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  923 | `				 /* Assume a literal */` |
|      ! 0 |  924 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  925 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  926 | `			 }else{` |
|        - |  927 | `				 /* Assemble annonymous functions body */` |
|      196 |  928 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      196 |  929 | `				 if( rc != SXRET_OK ){` |
|       25 |  930 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  931 | `					 return rc;` |
|        - |  932 | `				 }` |
|      172 |  933 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  934 | `			  }` |
|    11982 |  935 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    11855 |  936 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  937 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  938 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  939 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       86 |  940 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       86 |  941 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  942 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  943 | `				 return rc;` |
|        - |  944 | `			 }` |
|       86 |  945 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    11854 |  946 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  947 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 |  948 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 |  949 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 |  950 | `		 }else{` |
|        - |  951 | `			 /* Assume a literal */` |
|    11734 |  952 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11734 |  953 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  954 | `		 }` |
|  1488392 |  955 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  956 | `		 /* Constants,function name,namespace path,class name... */` |
|   537592 |  957 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   537592 |  958 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   268797 |  959 | `	 }else{` |
|   932648 |  960 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  961 | `			 /* Point to the code generator routine */` |
|   169346 |  962 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   169346 |  963 | `			 if( pNode->xCode == 0 ){` |
|        3 |  964 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  965 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  966 | `					 rc = SXERR_SYNTAX;` |
|        1 |  967 | `				 }` |
|        3 |  968 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  969 | `				 return rc;` |
|        - |  970 | `			 }` |
|    84671 |  971 | `		 }` |
|        - |  972 | `		/* Advance the stream cursor */` |
|   932646 |  973 | `		pCur++;` |
|        - |  974 | `	 }` |
|        - |  975 | `	/* Point to the end of the token stream */` |
|  3001160 |  976 | `	pNode->pEnd = pCur;` |
|        - |  977 | `	/* Save the node for later processing */` |
|  3001160 |  978 | `	*ppNode = pNode;` |
|        - |  979 | `	/* Synchronize cursors */` |
|  3001160 |  980 | `	pGen->pIn = pCur;` |
|  3001160 |  981 | `	return SXRET_OK;` |
|  1500605 |  982 |  |
|        - |  983 | `/*` |
|        - |  984 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  985 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  986 | ` * level is zero.` |
|        - |  987 | ` */` |
|    72572 |  988 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  989 |  |
|    72574 |  990 | `	SyToken *pCur = pStart;` |
|    72574 |  991 | `	sxi32 iNest = 0;` |
|    72574 |  992 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  993 | `		/* Last expression */` |
|    38684 |  994 | `		return SXERR_EOF;` |
|        - |  995 | `	}` |
|   136840 |  996 | `	while( pCur < pEnd ){` |
|   123930 |  997 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    20982 |  998 | `			break;` |
|        - |  999 | `		}` |
|   102950 | 1000 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5872 | 1001 | `			iNest++;` |
|   100015 | 1002 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5874 | 1003 | `			iNest--;` |
|     2936 | 1004 | `		}` |
|   102950 | 1005 | `		pCur++;` |
|        2 | 1006 | `	}` |
|    33892 | 1007 | `	*ppNext = pCur;` |
|    33892 | 1008 | `	return SXRET_OK;` |
|    36288 | 1009 |  |
|        - | 1010 | `/*` |
|        - | 1011 | ` * Free an expression tree.` |
|        - | 1012 | ` */` |
|  2568558 | 1013 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1014 |  |
|  2568560 | 1015 | `	if( pNode->pLeft ){` |
|        - | 1016 | `		/* Release the left tree */` |
|   958366 | 1017 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   479182 | 1018 | `	}` |
|  2568560 | 1019 | `	if( pNode->pRight ){` |
|        - | 1020 | `		/* Release the right tree */` |
|   501784 | 1021 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   250891 | 1022 | `	}` |
|  2568560 | 1023 | `	if( pNode->pCond ){` |
|        - | 1024 | `		/* Release the conditional tree used by the ternary operator */` |
|     1952 | 1025 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      975 | 1026 | `	}` |
|  2568560 | 1027 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1028 | `		ph7_expr_node **apArg;` |
|        - | 1029 | `		sxu32 n;` |
|        - | 1030 | `		/* Release node arguments */` |
|   340176 | 1031 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   718100 | 1032 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   377926 | 1033 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   188964 | 1034 | `		}` |
|   340176 | 1035 | `		SySetRelease(&pNode->aNodeArgs);` |
|   170087 | 1036 | `	}` |
|        - | 1037 | `	/* Finally,release this node */` |
|  2568560 | 1038 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2568560 | 1039 |  |
|        - | 1040 | `/*` |
|        - | 1041 | ` * Free an expression tree.` |
|        - | 1042 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1043 | ` */` |
|   680828 | 1044 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1045 |  |
|        - | 1046 | `	ph7_expr_node **apNode;` |
|        - | 1047 | `	sxu32 n;` |
|   680830 | 1048 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3681988 | 1049 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3001160 | 1050 | `		if( apNode[n] ){` |
|   681140 | 1051 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   340569 | 1052 | `		}` |
|  1500581 | 1053 | `	}` |
|   680830 | 1054 | `	return SXRET_OK;` |
|        2 | 1055 |  |
|        - | 1056 | `/*` |
|        - | 1057 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1058 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1059 | ` */` |
|   218160 | 1060 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1061 |  |
|        - | 1062 | `	sxi32 iExprOp;` |
|   218162 | 1063 | `	if( pNode->pOp == 0 ){` |
|   141900 | 1064 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1065 | `	}` |
|    76264 | 1066 | `	iExprOp = pNode->pOp->iOp;` |
|    76264 | 1067 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    47872 | 1068 | `			return TRUE;` |
|        - | 1069 | `	}` |
|    28394 | 1070 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    28390 | 1071 | `		if( pNode->pLeft->pOp ) {` |
|       18 | 1072 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        5 | 1073 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1074 | `				return FALSE;` |
|        1 | 1075 | `			}` |
|    28381 | 1076 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1077 | `			return FALSE;` |
|        - | 1078 | `		}` |
|    28390 | 1079 | `		return TRUE;` |
|        - | 1080 | `	}` |
|        5 | 1081 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1082 | `		return TRUE;` |
|        - | 1083 | `	}` |
|        - | 1084 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1085 | `	return FALSE;` |
|   109082 | 1086 |  |
|        - | 1087 | `/* Forward declaration */` |
|        - | 1088 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1089 | `/* Macro to check if the given node is a terminal.` |
|        - | 1090 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1091 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1092 | ` * linked ternary/elvis node). */` |
|        - | 1093 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1094 | `/*` |
|        - | 1095 | ` * Buid an expression tree for each given function argument.` |
|        - | 1096 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1097 | ` */` |
|   282330 | 1098 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1099 |  |
|        - | 1100 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1101 | `	sxi32 rc;` |
|        - | 1102 | `	/* Process function arguments from left to right */` |
|   282332 | 1103 | `	iCur = 0;` |
|   301196 | 1104 | `	for(;;){` |
|   602394 | 1105 | `		if( iCur >= nToken ){` |
|        - | 1106 | `			/* No more arguments to process */` |
|   282312 | 1107 | `			break;` |
|        - | 1108 | `		}` |
|   320084 | 1109 | `		iNode = iCur;` |
|   320084 | 1110 | `		iNest = 0;` |
|   800752 | 1111 | `		while( iCur < nToken ){` |
|   518442 | 1112 | `			if( apNode[iCur] ){` |
|   507234 | 1113 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    18888 | 1114 | `					break;` |
|   469462 | 1115 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    26040 | 1116 | `					iNest++;` |
|   456443 | 1117 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    26020 | 1118 | `					iNest--;` |
|    13009 | 1119 | `				}` |
|   234730 | 1120 | `			}` |
|   480670 | 1121 | `			iCur++;` |
|        2 | 1122 | `		}` |
|   320084 | 1123 | `		if( iCur > iNode ){` |
|   320076 | 1124 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1125 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1126 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1127 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1128 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1129 | `					apNode[iNode] = 0;` |
|      ! 0 | 1130 | `			}` |
|   320078 | 1131 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   320078 | 1132 | `			if( apNode[iNode] ){` |
|        - | 1133 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   320078 | 1134 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   160040 | 1135 | `			}else{` |
|        - | 1136 | `				/* No expression before comma */` |
|      ! 0 | 1137 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1138 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1139 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1140 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1141 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1142 | `				}` |
|      ! 0 | 1143 | `				return rc;` |
|        - | 1144 | `			}` |
|   160040 | 1145 | `		}else{` |
|        - | 1146 | `			/* Comma with no preceding argument */` |
|        7 | 1147 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1148 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1149 | `				rc = SXERR_SYNTAX;` |
|        3 | 1150 | `			}` |
|        7 | 1151 | `			return rc;` |
|        - | 1152 | `		}` |
|        - | 1153 | `		/* Jump trailing comma */` |
|   320078 | 1154 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    37768 | 1155 | `			iCur++;` |
|    37768 | 1156 | `			if( iCur >= nToken ){` |
|        - | 1157 | `				/* Trailing comma after last argument */` |
|       15 | 1158 | `				break;` |
|        - | 1159 | `			}` |
|    18876 | 1160 | `		}` |
|        2 | 1161 | `	}` |
|   282326 | 1162 | `	return SXRET_OK;` |
|   141167 | 1163 |  |
|        - | 1164 | ` /*` |
|        - | 1165 | `  * Create an expression tree from an array of tokens.` |
|        - | 1166 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1167 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1168 | `  */` |
|  1089234 | 1169 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1170 | ` {` |
|        - | 1171 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1172 | `	 ph7_expr_node *pNode;` |
|        - | 1173 | `	 sxi32 iCur;` |
|        - | 1174 | `	 sxi32 rc;` |
|  1089236 | 1175 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1176 | `		 /* TICKET 1433-17: self evaluating node */` |
|   502232 | 1177 | `		 return SXRET_OK;` |
|        - | 1178 | `	 }` |
|        - | 1179 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3605116 | 1180 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1181 | `		 sxi32 iNest;` |
|        - | 1182 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1183 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1184 | `		  */` |
|  3018114 | 1185 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2994412 | 1186 | `			 continue;` |
|        - | 1187 | `		 }` |
|    23704 | 1188 | `		 iNest = 1;` |
|    23704 | 1189 | `		 iLeft = iCur;` |
|        - | 1190 | `		 /* Find the closing parenthesis */` |
|    23704 | 1191 | `		 iCur++;` |
|   157538 | 1192 | `		 while( iCur < nToken ){` |
|   157538 | 1193 | `			 if( apNode[iCur] ){` |
|   157538 | 1194 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1195 | `					 /* Decrement nesting level */` |
|    41078 | 1196 | `					 iNest--;` |
|    41078 | 1197 | `					 if( iNest <= 0 ){` |
|    23704 | 1198 | `						 break;` |
|        2 | 1199 | `					 }` |
|   125149 | 1200 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1201 | `					 /* Increment nesting level */` |
|    17376 | 1202 | `					 iNest++;` |
|     8687 | 1203 | `				 }` |
|    66917 | 1204 | `			 }` |
|   133836 | 1205 | `			 iCur++;` |
|        2 | 1206 | `		 }` |
|    23704 | 1207 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1208 | `			 /* Recurse and process this expression */` |
|    23704 | 1209 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    23704 | 1210 | `			 if( rc != SXRET_OK ){` |
|        3 | 1211 | `				 return rc;` |
|        - | 1212 | `			 }` |
|    11850 | 1213 | `		 }` |
|        - | 1214 | `		 /* Free the left and right nodes */` |
|    23702 | 1215 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    23702 | 1216 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    23702 | 1217 | `		 apNode[iLeft] = 0;` |
|    23702 | 1218 | `		 apNode[iCur] = 0;` |
|    11852 | 1219 | `	 }` |
|        - | 1220 | `	  /* Process expressions enclosed in braces */` |
|  3756576 | 1221 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1222 | `		 sxi32 iNest;` |
|        - | 1223 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1224 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1225 | `		  */` |
|  3175638 | 1226 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3175638 | 1227 | `			 continue;` |
|        - | 1228 | `		 }` |
|      ! 0 | 1229 | `		 iNest = 1;` |
|      ! 0 | 1230 | `		 iLeft = iCur;` |
|        - | 1231 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1232 | `		 iCur++;` |
|      ! 0 | 1233 | `		 while( iCur < nToken ){` |
|      ! 0 | 1234 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1235 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1236 | `					 /* Decrement nesting level */` |
|      ! 0 | 1237 | `					 iNest--;` |
|      ! 0 | 1238 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1239 | `						 break;` |
|      ! 0 | 1240 | `					 }` |
|      ! 0 | 1241 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1242 | `					 /* Increment nesting level */` |
|      ! 0 | 1243 | `					 iNest++;` |
|      ! 0 | 1244 | `				 }` |
|      ! 0 | 1245 | `			 }` |
|      ! 0 | 1246 | `			 iCur++;` |
|      ! 0 | 1247 | `		 }` |
|      ! 0 | 1248 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1249 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1250 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1251 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1252 | `				 return rc;` |
|        - | 1253 | `			 }` |
|      ! 0 | 1254 | `		 }` |
|        - | 1255 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1256 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1257 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1258 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1259 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1260 | `	 }` |
|        - | 1261 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   580940 | 1262 | `	 iLeft = -1;` |
|  3756548 | 1263 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3175620 | 1264 | `		 if( apNode[iCur] == 0 ){` |
|  1235388 | 1265 | `			 continue;` |
|        - | 1266 | `		 }` |
|  1940234 | 1267 | `		 pNode = apNode[iCur];` |
|  1940234 | 1268 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   500828 | 1269 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1270 | `				 /* Collect function arguments */` |
|   320972 | 1271 | `				 sxi32 iPtr = 0;` |
|   320972 | 1272 | `				 sxi32 nFuncTok = 0;` |
|  1160384 | 1273 | `				 while( nFuncTok + iCur < nToken ){` |
|  1160384 | 1274 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1149176 | 1275 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   332672 | 1276 | `							 iPtr++;` |
|   982841 | 1277 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   332672 | 1278 | `							 iPtr--;` |
|   332672 | 1279 | `							 if( iPtr <= 0 ){` |
|   320972 | 1280 | `								 break;` |
|        - | 1281 | `							 }` |
|     5850 | 1282 | `						 }` |
|   414102 | 1283 | `					 }` |
|   839414 | 1284 | `					 nFuncTok++;` |
|        2 | 1285 | `				 }` |
|   320972 | 1286 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1287 | `					 /* Syntax error */` |
|      ! 0 | 1288 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1289 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1290 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1291 | `					 }` |
|      ! 0 | 1292 | `					 return rc;` |
|        - | 1293 | `				 }` |
|   320972 | 1294 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1295 | `					 /* Syntax error */` |
|      ! 0 | 1296 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1297 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1298 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1299 | `					 }` |
|      ! 0 | 1300 | `					 return rc;` |
|        - | 1301 | `				 }` |
|   320972 | 1302 | `				 if( nFuncTok > 1 ){` |
|        - | 1303 | `					 /* Process function arguments */` |
|   282332 | 1304 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   282332 | 1305 | `					 if( rc != SXRET_OK ){` |
|        7 | 1306 | `						 return rc;` |
|        - | 1307 | `					 }` |
|   141162 | 1308 | `				 }` |
|        - | 1309 | `				 /* Link the node to the tree */` |
|   320966 | 1310 | `				 pNode->pLeft = apNode[iLeft];` |
|   320966 | 1311 | `				 apNode[iLeft] = 0;` |
|  1160360 | 1312 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   839396 | 1313 | `					 apNode[iCur+iPtr] = 0;` |
|   419699 | 1314 | `				 }` |
|   340340 | 1315 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1316 | `				 /* Subscripting */` |
|    71926 | 1317 | `				 sxi32 iArrTok = iCur + 1;` |
|    71926 | 1318 | `				 sxi32 iNest = 1;` |
|    72005 | 1319 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1320 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1321 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1322 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    71924 | 1323 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1324 | `						 /* Syntax error */` |
|      ! 0 | 1325 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1326 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1327 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1328 | `						 }` |
|      ! 0 | 1329 | `						 return rc;` |
|        - | 1330 | `				 }` |
|        - | 1331 | `				 /* Collect index tokens */` |
|   129884 | 1332 | `				 while( iArrTok < nToken ){` |
|   129884 | 1333 | `					 if( apNode[iArrTok] ){` |
|   129852 | 1334 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1335 | `							 /* Increment nesting level */` |
|      ! 0 | 1336 | `							 iNest++;` |
|   129852 | 1337 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1338 | `							 /* Decrement nesting level */` |
|    71926 | 1339 | `							 iNest--;` |
|    71926 | 1340 | `							 if( iNest <= 0 ){` |
|    71926 | 1341 | `								 break;` |
|        - | 1342 | `							 }` |
|      ! 0 | 1343 | `						 }` |
|    28963 | 1344 | `					 }` |
|    57960 | 1345 | `					 ++iArrTok;` |
|        2 | 1346 | `				 }` |
|    71926 | 1347 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1348 | `					 /* Recurse and process this expression */` |
|    57850 | 1349 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    57850 | 1350 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1351 | `						 return rc;` |
|        - | 1352 | `					 }` |
|        - | 1353 | `					 /* Link the node to it's index */` |
|    57850 | 1354 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    28924 | 1355 | `				 }` |
|        - | 1356 | `				 /* Link the node to the tree */` |
|    71926 | 1357 | `				 pNode->pLeft = apNode[iLeft];` |
|    71926 | 1358 | `				 pNode->pRight = 0;` |
|    71926 | 1359 | `				 apNode[iLeft] = 0;` |
|   201808 | 1360 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   129884 | 1361 | `					 apNode[iNest] = 0;` |
|    64943 | 1362 | `				 }` |
|    35964 | 1363 | `			 }else{` |
|        - | 1364 | `				 /* Member access operators [i.e: '->','::'] */` |
|   107934 | 1365 | `				  iRight = iCur + 1;` |
|   107934 | 1366 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1367 | `					 iRight++;` |
|      ! 0 | 1368 | `				 }` |
|   107934 | 1369 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1370 | `					 /* Syntax error */` |
|        5 | 1371 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1372 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1373 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1374 | `					 }` |
|        5 | 1375 | `					 return rc;` |
|        - | 1376 | `				 }` |
|        - | 1377 | `				 /* Link the node to the tree */` |
|   107930 | 1378 | `				 pNode->pLeft = apNode[iLeft];` |
|   107930 | 1379 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   107656 | 1380 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1381 | `						 /* Syntax error */` |
|      ! 0 | 1382 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1383 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1384 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1385 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1386 | `						 }` |
|      ! 0 | 1387 | `						 return rc;` |
|        - | 1388 | `				 }` |
|   107930 | 1389 | `				 pNode->pRight = apNode[iRight];` |
|   107930 | 1390 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1391 | `			 }` |
|   250408 | 1392 | `		 }` |
|  1940224 | 1393 | `		 iLeft = iCur;` |
|   970113 | 1394 | `	 }` |
|        - | 1395 | `	 /* Handle left associative (new, clone) operators */` |
|  3756520 | 1396 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3175592 | 1397 | `		 if( apNode[iCur] == 0 ){` |
|  1750798 | 1398 | `			 continue;` |
|        - | 1399 | `		 }` |
|  1424796 | 1400 | `		 pNode = apNode[iCur];` |
|  1424796 | 1401 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1402 | `			 SyToken *pToken;` |
|        - | 1403 | `			 /* Get the left node */` |
|    14596 | 1404 | `			 iLeft = iCur + 1;` |
|    29160 | 1405 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    14566 | 1406 | `				 iLeft++;` |
|        2 | 1407 | `			 }` |
|    14596 | 1408 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1409 | `				  /* Syntax error */` |
|      ! 0 | 1410 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1411 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1412 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1413 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1414 | `				 }` |
|      ! 0 | 1415 | `				 return rc;` |
|        - | 1416 | `			 }` |
|        - | 1417 | `			 /* Make sure the operand are of a valid type */` |
|    14596 | 1418 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1419 | `				 /* Clone:` |
|        - | 1420 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1421 | `				  *  ++ function call (including annonymous)` |
|        - | 1422 | `				  *  ++ array member` |
|        - | 1423 | `				  *  ++ 'new' operator` |
|        - | 1424 | `				  * Example:` |
|        - | 1425 | `				  *   clone $pObj;` |
|        - | 1426 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1427 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1428 | `				  */` |
|       18 | 1429 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1430 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1431 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1432 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1433 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1434 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1435 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1436 | `						 }` |
|      ! 0 | 1437 | `						 return rc;` |
|        - | 1438 | `					 }` |
|        7 | 1439 | `				 }` |
|       10 | 1440 | `			 }else{` |
|        - | 1441 | `				 /* New */` |
|    14580 | 1442 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1443 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1444 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1445 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1446 | `						 /* Syntax error */` |
|      ! 0 | 1447 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1448 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1449 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1450 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1451 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1452 | `						 }` |
|      ! 0 | 1453 | `						 return rc;` |
|        - | 1454 | `					 }` |
|        8 | 1455 | `				 }` |
|        - | 1456 | `			 }` |
|        - | 1457 | `			  /* Link the node to the tree */` |
|    14596 | 1458 | `			 pNode->pLeft = apNode[iLeft];` |
|    14596 | 1459 | `			 apNode[iLeft] = 0;` |
|    14596 | 1460 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7297 | 1461 | `		 }` |
|   712399 | 1462 | `	 }` |
|        - | 1463 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   580930 | 1464 | `	 iLeft = -1;` |
|  3756520 | 1465 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3172560 | 1466 | `		 if( apNode[iCur] == 0 ){` |
|  1750798 | 1467 | `			 continue;` |
|        - | 1468 | `		 }` |
|  1421764 | 1469 | `		 pNode = apNode[iCur];` |
|  1421764 | 1470 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8434 | 1471 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3050 | 1472 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1473 | `					 /* Link the node to the tree */` |
|     3052 | 1474 | `					 pNode->pLeft = apNode[iLeft];` |
|     3052 | 1475 | `					 apNode[iLeft] = 0;` |
|     1525 | 1476 | `			 }` |
|     5732 | 1477 | `		  }` |
|  1424796 | 1478 | `		 iLeft = iCur;` |
|   712399 | 1479 | `	  }` |
|   583962 | 1480 | `	 iLeft = -1;` |
|  3759552 | 1481 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3175592 | 1482 | `		 if( apNode[iCur] == 0 ){` |
|  1753848 | 1483 | `			 continue;` |
|        - | 1484 | `		 }` |
|  1421746 | 1485 | `		 pNode = apNode[iCur];` |
|  1421746 | 1486 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8415 | 1487 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8416 | 1488 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1489 | `					 /* Syntax error */` |
|      ! 0 | 1490 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1491 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1492 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1493 | `					 }` |
|      ! 0 | 1494 | `					 return rc;` |
|        - | 1495 | `			 }` |
|        - | 1496 | `			 /* Link the node to the tree */` |
|     8416 | 1497 | `			 pNode->pLeft = apNode[iLeft];` |
|     8416 | 1498 | `			 apNode[iLeft] = 0;` |
|        - | 1499 | `			 /* Mark as pre-increment/decrement node */` |
|     8416 | 1500 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4207 | 1501 | `		  }` |
|  1421746 | 1502 | `		 iLeft = iCur;` |
|   710874 | 1503 | `	 }` |
|        - | 1504 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   583962 | 1505 | `	  iLeft = 0;` |
|  3759546 | 1506 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3175588 | 1507 | `		  if( apNode[iCur] ){` |
|  1413328 | 1508 | `			  pNode = apNode[iCur];` |
|  1413328 | 1509 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    37708 | 1510 | `				  if( iLeft > 0 ){` |
|        - | 1511 | `					  /* Link the node to the tree */` |
|    37706 | 1512 | `					  pNode->pLeft = apNode[iLeft];` |
|    37706 | 1513 | `					  apNode[iLeft] = 0;` |
|    37706 | 1514 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1515 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1516 | `							   /* Syntax error */` |
|      ! 0 | 1517 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1518 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1519 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1520 | `							  }` |
|      ! 0 | 1521 | `							  return rc;` |
|        - | 1522 | `						  }` |
|       36 | 1523 | `					  }` |
|    18854 | 1524 | `				  }else{` |
|        - | 1525 | `					  /* Syntax error */` |
|        3 | 1526 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1527 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1528 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1529 | `					  }` |
|        3 | 1530 | `					  return rc;` |
|        - | 1531 | `				  }` |
|    18852 | 1532 | `			  }` |
|        - | 1533 | `			  /* Save terminal position */` |
|  1413326 | 1534 | `			  iLeft = iCur;` |
|   706662 | 1535 | `		  }` |
|  1587794 | 1536 | `	  }` |
|        - | 1537 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6423464 | 1538 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5839514 | 1539 | `		 iLeft = -1;` |
| 37595108 | 1540 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 31755604 | 1541 | `			 if( apNode[iCur] == 0 ){` |
| 20265114 | 1542 | `				 continue;` |
|        - | 1543 | `			 }` |
| 11490492 | 1544 | `			 pNode = apNode[iCur];` |
| 11490492 | 1545 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1546 | `				 /* Get the right node */` |
|   173766 | 1547 | `				 iRight = iCur + 1;` |
|   246762 | 1548 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    72998 | 1549 | `					 iRight++;` |
|        2 | 1550 | `				 }` |
|   173766 | 1551 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1552 | `					 /* Syntax error */` |
|        9 | 1553 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1554 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1555 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1556 | `					 }` |
|        9 | 1557 | `					 return rc;` |
|        - | 1558 | `				 }` |
|   173758 | 1559 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1560 | `					 sxi32  iTmp;` |
|        - | 1561 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       48 | 1562 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1563 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1564 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1565 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1566 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1567 | `						 }` |
|      ! 0 | 1568 | `						 return rc;` |
|        - | 1569 | `					 }` |
|       48 | 1570 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1571 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1572 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1573 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1574 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1575 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1576 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1577 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1578 | `									 }` |
|      ! 0 | 1579 | `									 return rc;` |
|        - | 1580 | `							 }` |
|      ! 0 | 1581 | `						 }` |
|       16 | 1582 | `					 }` |
|        - | 1583 | `					 /* Swap operands */` |
|       48 | 1584 | `					 iTmp = iRight;` |
|       48 | 1585 | `					 iRight = iLeft;` |
|       48 | 1586 | `					 iLeft = iTmp;` |
|       23 | 1587 | `				 }` |
|        - | 1588 | `				 /* Link the node to the tree */` |
|   173758 | 1589 | `				 pNode->pLeft = apNode[iLeft];` |
|   173758 | 1590 | `				 pNode->pRight = apNode[iRight];` |
|   173758 | 1591 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    86878 | 1592 | `			 }` |
| 11490484 | 1593 | `			 iLeft = iCur;` |
|  5745243 | 1594 | `		 }` |
|  2919754 | 1595 | `	 }` |
|        - | 1596 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1597 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1598 | `	  * we are dealing with a single operator.` |
|        - | 1599 | `	  */` |
|   583952 | 1600 | `	  iLeft = -1;` |
|  3751098 | 1601 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3169100 | 1602 | `		  if( apNode[iCur] == 0 ){` |
|  2146826 | 1603 | `			  continue;` |
|        - | 1604 | `		  }` |
|  1022276 | 1605 | `		  pNode = apNode[iCur];` |
|  1022276 | 1606 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1954 | 1607 | `			  sxi32 iNest = 1;` |
|     1954 | 1608 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1609 | `				  /* Missing condition */` |
|        3 | 1610 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1611 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1612 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1613 | `				  }` |
|        3 | 1614 | `				  return rc;` |
|        - | 1615 | `			  }` |
|        - | 1616 | `			  /* Get the right node */` |
|     1952 | 1617 | `			  iRight = iCur + 1;` |
|     4138 | 1618 | `			  while( iRight < nToken  ){` |
|     4138 | 1619 | `				  if( apNode[iRight] ){` |
|     3834 | 1620 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1621 | `						  /* Increment nesting level */` |
|      ! 0 | 1622 | `						  ++iNest;` |
|     3834 | 1623 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1624 | `						  /* Decrement nesting level */` |
|     1952 | 1625 | `						  --iNest;` |
|     1952 | 1626 | `						  if( iNest <= 0 ){` |
|     1952 | 1627 | `							  break;` |
|        - | 1628 | `						  }` |
|      ! 0 | 1629 | `					  }` |
|      941 | 1630 | `				  }` |
|     2188 | 1631 | `				  iRight++;` |
|        2 | 1632 | `			  }` |
|     1952 | 1633 | `			  if( iRight > iCur + 1 ){` |
|        - | 1634 | `				  /* Recurse and process the then expression */` |
|     1884 | 1635 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1884 | 1636 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1637 | `					  return rc;` |
|        - | 1638 | `				  }` |
|        - | 1639 | `				  /* Link the node to the tree */` |
|     1884 | 1640 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      941 | 1641 | `			  }else{` |
|        - | 1642 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1643 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1644 | `			  }` |
|     1952 | 1645 | `			  apNode[iCur + 1] = 0;` |
|     1952 | 1646 | `			  if( iRight + 1 < nToken ){` |
|        - | 1647 | `				  /* Recurse and process the else expression */` |
|     1952 | 1648 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1952 | 1649 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1650 | `					  return rc;` |
|        - | 1651 | `				  }` |
|        - | 1652 | `				  /* Link the node to the tree */` |
|     1952 | 1653 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1952 | 1654 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      977 | 1655 | `			  }else{` |
|      ! 0 | 1656 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1657 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1658 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1659 | `				 }` |
|      ! 0 | 1660 | `				 return rc;` |
|        - | 1661 | `			  }` |
|        - | 1662 | `			  /* Point to the condition */` |
|     1952 | 1663 | `			  pNode->pCond  = apNode[iLeft];` |
|     1952 | 1664 | `			  apNode[iLeft] = 0;` |
|     1952 | 1665 | `			  break;` |
|        - | 1666 | `		  }` |
|  1020324 | 1667 | `		  iLeft = iCur;` |
|   510163 | 1668 | `	  }` |
|        - | 1669 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1670 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1671 | `	  * so there is no need for a precedence loop here.` |
|        - | 1672 | `	  */` |
|   583950 | 1673 | `	 iRight = -1;` |
|  3759402 | 1674 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3175500 | 1675 | `		 if( apNode[iCur] == 0 ){` |
|  2373306 | 1676 | `			 continue;` |
|        - | 1677 | `		 }` |
|   802196 | 1678 | `		 pNode = apNode[iCur];` |
|   802196 | 1679 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1680 | `			 /* Get the left node */` |
|   218126 | 1681 | `			 iLeft = iCur - 1;` |
|   308756 | 1682 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    90632 | 1683 | `				 iLeft--;` |
|        2 | 1684 | `			 }` |
|   218126 | 1685 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1686 | `				 /* Syntax error */` |
|       43 | 1687 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1688 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1689 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1690 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1691 | `				 }else{` |
|       39 | 1692 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1693 | `				 }` |
|       43 | 1694 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1695 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1696 | `				 }` |
|       43 | 1697 | `				 return rc;` |
|        - | 1698 | `			 }` |
|   218084 | 1699 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1700 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1701 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1702 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1703 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1704 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1705 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1706 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1707 | `					 }else{` |
|        4 | 1708 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1709 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1710 | `					 }` |
|        5 | 1711 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1712 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1713 | `					 }` |
|        5 | 1714 | `					 return rc;` |
|        - | 1715 | `				 }` |
|       26 | 1716 | `			 }` |
|        - | 1717 | `			 /* Link the node to the tree (Reverse) */` |
|   218080 | 1718 | `			 pNode->pLeft = apNode[iRight];` |
|   218080 | 1719 | `			 pNode->pRight = apNode[iLeft];` |
|   218080 | 1720 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   109039 | 1721 | `		 }` |
|   802150 | 1722 | `		 iRight = iCur;` |
|   401076 | 1723 | `	 }` |
|        - | 1724 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2919512 | 1725 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2335610 | 1726 | `		 iLeft = -1;` |
| 15037394 | 1727 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12701786 | 1728 | `			 if( apNode[iCur] == 0 ){` |
| 10365772 | 1729 | `				 continue;` |
|        - | 1730 | `			 }` |
|  2336016 | 1731 | `			 pNode = apNode[iCur];` |
|  2336016 | 1732 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1733 | `				 /* Get the right node */` |
|       72 | 1734 | `				 iRight = iCur + 1;` |
|      110 | 1735 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1736 | `					 iRight++;` |
|        2 | 1737 | `				 }` |
|       72 | 1738 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1739 | `					 /* Syntax error */` |
|      ! 0 | 1740 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1741 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1742 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1743 | `					 }` |
|      ! 0 | 1744 | `					 return rc;` |
|        - | 1745 | `				 }` |
|        - | 1746 | `				 /* Link the node to the tree */` |
|       72 | 1747 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1748 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1749 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1750 | `			 }` |
|  2336016 | 1751 | `			 iLeft = iCur;` |
|  1168009 | 1752 | `		 }` |
|  1167806 | 1753 | `	 }` |
|        - | 1754 | `	 /* Point to the root of the expression tree */` |
|  3175420 | 1755 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2591536 | 1756 | `		 if( apNode[iCur] ){` |
|   527068 | 1757 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1758 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1759 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1760 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1761 | `				  }` |
|       20 | 1762 | `				  return rc;` |
|        - | 1763 | `			 }` |
|   527050 | 1764 | `			 apNode[0] = apNode[iCur];` |
|   527050 | 1765 | `			 apNode[iCur] = 0;` |
|   263524 | 1766 | `		 }` |
|  1295760 | 1767 | `	 }` |
|   583886 | 1768 | `	 return SXRET_OK;` |
|   543103 | 1769 | ` }` |
|        - | 1770 | ` /*` |
|        - | 1771 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1772 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1773 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1774 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1775 | `  */` |
|   680828 | 1776 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1777 |  |
|        - | 1778 | `	ph7_expr_node **apNode;` |
|        - | 1779 | `	ph7_expr_node *pNode;` |
|        - | 1780 | `	sxi32 rc;` |
|        - | 1781 | `	/* Reset node container */` |
|   680830 | 1782 | `	SySetReset(pExprNode);` |
|   680830 | 1783 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1784 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1785 | `	{` |
|   680830 | 1786 | `		int iLastWasTerm = 0;` |
|  3681988 | 1787 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3001194 | 1788 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3001194 | 1789 | `			if( rc != SXRET_OK ){` |
|       35 | 1790 | `				return rc;` |
|        - | 1791 | `			}` |
|        - | 1792 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3001160 | 1793 | `			if( pNode->xCode ){` |
|        - | 1794 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1562490 | 1795 | `				iLastWasTerm = 1;` |
|  2219916 | 1796 | `			}else if( pNode->pOp ){` |
|        - | 1797 | `				/* Operator node */` |
|   675370 | 1798 | `				iLastWasTerm = 0;` |
|   337686 | 1799 | `			}else{` |
|        - | 1800 | `				/* Delimiter: ')' and ']' end terms */` |
|   763304 | 1801 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1802 | `			}` |
|        - | 1803 | `			/* Save the extracted node */` |
|  3001160 | 1804 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1805 | `		}` |
|        - | 1806 | `	}` |
|   680796 | 1807 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1808 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1809 | `		*ppRoot = 0;` |
|      ! 0 | 1810 | `		return SXRET_OK;` |
|        - | 1811 | `	}` |
|   680796 | 1812 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1813 | `	/* Make sure we are dealing with valid nodes */` |
|   680796 | 1814 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   680796 | 1815 | `	if( rc != SXRET_OK ){` |
|        - | 1816 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1817 | `		 * cleanup the mess left behind.` |
|        - | 1818 | `		 */` |
|       51 | 1819 | `		*ppRoot = 0;` |
|       51 | 1820 | `		return rc;` |
|        - | 1821 | `	}` |
|        - | 1822 | `	/* Build the tree */` |
|   680746 | 1823 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   680746 | 1824 | `	if( rc != SXRET_OK ){` |
|        - | 1825 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       88 | 1826 | `		*ppRoot = 0;` |
|       88 | 1827 | `		return rc;` |
|        - | 1828 | `	}` |
|        - | 1829 | `	/* Point to the root of the tree */` |
|   680660 | 1830 | `	*ppRoot = apNode[0];` |
|   680660 | 1831 | `	return SXRET_OK;` |
|   340416 | 1832 |  |
|        - | 1833 |  |
