# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 974/1152 lines (84.55%)

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
|   783544 |  264 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  265 |  |
|   783546 |  266 | `	sxu32 n = 0;` |
|        - |  267 | `	sxi32 rc;` |
|        - |  268 | `	/* Do a linear lookup on the operators table */` |
| 12841754 |  269 | `	for(;;){` |
| 25683510 |  270 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  271 | `			break;` |
|        - |  272 | `		}` |
| 25683510 |  273 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  274 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3125018 |  275 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1562510 |  276 | `		}else{` |
| 22558494 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  278 | `		}` |
| 25683510 |  279 | `		if( rc == 0 ){` |
|   786992 |  280 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  281 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   783212 |  282 | `				return &aOpTable[n];` |
|        - |  283 | `			}` |
|        - |  284 | `			/* Handle ambiguity */` |
|     3782 |  285 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  286 | `				/* Unary opertors have prcedence here over binary operators */` |
|      226 |  287 | `				return &aOpTable[n];` |
|        - |  288 | `			}` |
|     3558 |  289 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  290 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  291 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  292 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  293 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  294 | `					return &aOpTable[n];` |
|        - |  295 | `				}` |
|        - |  296 |  |
|        4 |  297 | `			}` |
|     1723 |  298 | `		}` |
| 24899966 |  299 | `		++n; /* Next operator in the table */` |
|        2 |  300 | `	}` |
|        - |  301 | `	/* No such operator */` |
|      ! 0 |  302 | `	return 0;` |
|   391774 |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * Delimit a set of token stream.` |
|        - |  306 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  307 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  308 | ` */` |
|   403130 |  309 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  310 |  |
|   403132 |  311 | `	SyToken *pCur = pIn;` |
|   403132 |  312 | `	sxi32 iNest = 1;` |
|  2290536 |  313 | `	for(;;){` |
|  4581074 |  314 | `		if( pCur >= pEnd ){` |
|      130 |  315 | `			break;` |
|        - |  316 | `		}` |
|  4580946 |  317 | `		if( pCur->nType & nTokStart ){` |
|        - |  318 | `			/* Increment nesting level */` |
|   253142 |  319 | `			iNest++;` |
|  4454376 |  320 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  321 | `			/* Decrement nesting level */` |
|   656144 |  322 | `			iNest--;` |
|   656144 |  323 | `			if( iNest <= 0 ){` |
|   403004 |  324 | `				break;` |
|        - |  325 | `			}` |
|   126570 |  326 | `		}` |
|        - |  327 | `		/* Advance cursor */` |
|  4177944 |  328 | `		pCur++;` |
|        2 |  329 | `	}` |
|        - |  330 | `	/* Point to the end of the chunk */` |
|   403132 |  331 | `	*ppEnd = pCur;` |
|   403132 |  332 |  |
|        - |  333 | `/*` |
|        - |  334 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  335 | ` * Note on reserved keywords.` |
|        - |  336 | ` *  According to the PHP language reference manual:` |
|        - |  337 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  338 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  339 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  340 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  341 | ` */` |
|    12098 |  342 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  343 |  |
|    18081 |  344 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    12005 |  345 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  346 | `		){` |
|      146 |  347 | `			return TRUE;` |
|        - |  348 | `	}` |
|    11956 |  349 | `	if( bCheckFunc ){` |
|       95 |  350 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       70 |  351 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       55 |  352 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  353 | `				return TRUE;` |
|        - |  354 | `		}` |
|       21 |  355 | `	}` |
|        - |  356 | `	/* Not a language construct */` |
|    11924 |  357 | `	return FALSE;` |
|     6051 |  358 |  |
|        - |  359 | `/*` |
|        - |  360 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  361 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  362 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  363 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  364 | ` */` |
|   690092 |  365 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  366 |  |
|        - |  367 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  368 | `	sxi32 i,rc;` |
|        - |  369 |  |
|   690094 |  370 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  371 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  372 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  373 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  374 | `	}` |
|   690094 |  375 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3730552 |  376 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3040494 |  377 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  378 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      326 |  379 | `			continue;` |
|        - |  380 | `		}` |
|  3040170 |  381 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   349132 |  382 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    17956 |  383 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  384 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   325120 |  385 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  386 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  387 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  388 | `						 */` |
|   325120 |  389 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   325120 |  390 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   325120 |  391 | `						apNode[i]->pOp = &sFCallOp;` |
|   162559 |  392 | `					}` |
|   162559 |  393 | `			}` |
|   349132 |  394 | `			iParen++;` |
|  2865605 |  395 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   349132 |  396 | `			if( iParen <= 0 ){` |
|       13 |  397 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  398 | `				if( rc != SXERR_ABORT ){` |
|       13 |  399 | `					rc = SXERR_SYNTAX;` |
|        6 |  400 | `				}` |
|       13 |  401 | `				return rc;` |
|        - |  402 | `			}` |
|   349120 |  403 | `			iParen--;` |
|  2516469 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    72826 |  405 | `			iSquare++;` |
|  2305498 |  406 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    72840 |  407 | `			if( iSquare <= 0 ){` |
|        7 |  408 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  409 | `				if( rc != SXERR_ABORT ){` |
|        7 |  410 | `					rc = SXERR_SYNTAX;` |
|        3 |  411 | `				}` |
|        7 |  412 | `				return rc;` |
|        - |  413 | `			}` |
|    72834 |  414 | `			iSquare--;` |
|  2232664 |  415 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2196243 |  462 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  463 | `			if( iBraces <= 0 ){` |
|       13 |  464 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  465 | `				if( rc != SXERR_ABORT ){` |
|       13 |  466 | `					rc = SXERR_SYNTAX;` |
|        6 |  467 | `				}` |
|       13 |  468 | `				return rc;` |
|        - |  469 | `			}` |
|      ! 0 |  470 | `			iBraces--;` |
|  2196226 |  471 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2160 |  472 | `			if( iQuesty > 0 ){` |
|     1980 |  473 | `				iQuesty--;` |
|     1171 |  474 | `			}else if( iParen <= 0 ){` |
|        - |  475 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  476 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  477 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  478 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  479 | `				if( rc != SXERR_ABORT ){` |
|        5 |  480 | `					rc = SXERR_SYNTAX;` |
|        2 |  481 | `				}` |
|        5 |  482 | `				return rc;` |
|        2 |  483 | `			}` |
|  2195145 |  484 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   611160 |  485 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   611160 |  486 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1982 |  487 | `				iQuesty++;` |
|   610170 |  488 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      322 |  489 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  490 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  491 | `					sxu32 n = 0;` |
|       11 |  492 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  493 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  494 | `					}` |
|        - |  495 | `					/*` |
|        - |  496 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  497 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  498 | `					 */` |
|      245 |  499 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      235 |  500 | `						++n;` |
|        1 |  501 | `					}` |
|       11 |  502 | `					pOp = &aOpTable[n];` |
|        - |  503 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  504 | `					apNode[i]->pOp = pOp;` |
|       11 |  505 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  506 | `				}` |
|      160 |  507 | `			}` |
|   305579 |  508 | `		}` |
|  1520069 |  509 | `	}` |
|   690060 |  510 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  511 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  512 | `		if( rc != SXERR_ABORT ){` |
|       17 |  513 | `			rc = SXERR_SYNTAX;` |
|        8 |  514 | `		}` |
|       17 |  515 | `		return rc;` |
|        - |  516 | `	}` |
|   690044 |  517 | `	return SXRET_OK;` |
|   345048 |  518 |  |
|        - |  519 | `/*` |
|        - |  520 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  521 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  522 | ` */` |
|   556586 |  523 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  524 |  |
|   556588 |  525 | `	SyToken *pIn = *ppCur;` |
|        - |  526 | `	/* Jump the first literal seen */` |
|   556588 |  527 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   556564 |  528 | `		pIn++;` |
|   278281 |  529 | `	}` |
|   278325 |  530 | `	for(;;){` |
|   556652 |  531 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       66 |  532 | `			pIn++;` |
|       66 |  533 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       66 |  534 | `				pIn++;` |
|       32 |  535 | `			}` |
|       34 |  536 | `		}else{` |
|   278295 |  537 | `			break;` |
|        - |  538 | `		}` |
|        2 |  539 | `	}` |
|        - |  540 | `	/* Synchronize pointers */` |
|   556588 |  541 | `	*ppCur = pIn;` |
|   556588 |  542 |  |
|        - |  543 | `/*` |
|        - |  544 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  545 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  546 | ` * Note on annonymous functions.` |
|        - |  547 | ` *  According to the PHP language reference manual:` |
|        - |  548 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  549 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  550 | ` *  parameters, but they have many other uses.` |
|        - |  551 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  552 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  553 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  554 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  555 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  556 | ` *` |
|        - |  557 | ` * Some example:` |
|        - |  558 | ` *  $greet = function($name)` |
|        - |  559 | ` * {` |
|        - |  560 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  561 | ` * };` |
|        - |  562 | ` *  $greet('World');` |
|        - |  563 | ` *  $greet('PHP');` |
|        - |  564 | ` *` |
|        - |  565 | ` * $double = function($a) {` |
|        - |  566 | ` *   return $a * 2;` |
|        - |  567 | ` * };` |
|        - |  568 | ` * // This is our range of numbers` |
|        - |  569 | ` * $numbers = range(1, 5);` |
|        - |  570 | ` * // Use the Annonymous function as a callback here to` |
|        - |  571 | ` * // double the size of each element in our` |
|        - |  572 | ` * // range` |
|        - |  573 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  574 | ` * print implode(' ', $new_numbers);` |
|        - |  575 | ` */` |
|      202 |  576 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  577 |  |
|      204 |  578 | `	SyToken *pIn = *ppCur;` |
|        - |  579 | `	sxu32 nLine;` |
|        - |  580 | `	sxi32 rc;` |
|        - |  581 | `	/* Jump the 'function' keyword */` |
|      204 |  582 | `	nLine = pIn->nLine;` |
|      204 |  583 | `	pIn++;` |
|      204 |  584 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  585 | `		pIn++;` |
|        1 |  586 | `	}` |
|      204 |  587 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  588 | `		/* Syntax error */` |
|        5 |  589 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  590 | `		if( rc != SXERR_ABORT ){` |
|        5 |  591 | `			rc = SXERR_SYNTAX;` |
|        2 |  592 | `		}` |
|        5 |  593 | `		goto Synchronize;` |
|        - |  594 | `	}` |
|      200 |  595 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      200 |  596 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      200 |  597 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  598 | `		/* Syntax error */` |
|        5 |  599 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  600 | `		if( rc != SXERR_ABORT ){` |
|        5 |  601 | `			rc = SXERR_SYNTAX;` |
|        2 |  602 | `		}` |
|        5 |  603 | `		goto Synchronize;` |
|        - |  604 | `	}` |
|      196 |  605 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  606 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      196 |  607 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  608 | `		pIn++; /* Skip ':' */` |
|        - |  609 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  610 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  611 | `			pIn++;` |
|      ! 0 |  612 | `		}` |
|        - |  613 | `		/* Skip the first type (allow leading '\' and namespace path) */` |
|        5 |  614 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        5 |  615 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  616 | `			pIn++;` |
|        5 |  617 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  618 | `				pIn += 2;` |
|      ! 0 |  619 | `			}` |
|        2 |  620 | `		}` |
|        - |  621 | `		/* Skip union alternatives ( \| type )* */` |
|        6 |  622 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        3 |  623 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  624 | `			pIn++;` |
|      ! 0 |  625 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  626 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  627 | `				pIn++;` |
|      ! 0 |  628 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  629 | `					pIn += 2;` |
|      ! 0 |  630 | `				}` |
|      ! 0 |  631 | `			}` |
|      ! 0 |  632 | `		}` |
|        2 |  633 | `	}` |
|      196 |  634 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       32 |  635 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  636 | `		/* Check if we are dealing with a closure */` |
|       32 |  637 | `		if( nKey == PH7_TKWRD_USE ){` |
|       24 |  638 | `			pIn++; /* Jump the 'use' keyword */` |
|       24 |  639 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  640 | `				/* Syntax error */` |
|        5 |  641 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  642 | `				if( rc != SXERR_ABORT ){` |
|        5 |  643 | `					rc = SXERR_SYNTAX;` |
|        2 |  644 | `				}` |
|        5 |  645 | `				goto Synchronize;` |
|        - |  646 | `			}` |
|       20 |  647 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       20 |  648 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       20 |  649 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  650 | `				/* Syntax error */` |
|        5 |  651 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  652 | `				if( rc != SXERR_ABORT ){` |
|        5 |  653 | `					rc = SXERR_SYNTAX;` |
|        2 |  654 | `				}` |
|        5 |  655 | `				goto Synchronize;` |
|        - |  656 | `			}` |
|       16 |  657 | `			pIn++;` |
|        9 |  658 | `		}else{` |
|        - |  659 | `			/* Syntax error */` |
|        9 |  660 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  661 | `			if( rc != SXERR_ABORT ){` |
|        9 |  662 | `				rc = SXERR_SYNTAX;` |
|        4 |  663 | `			}` |
|        9 |  664 | `			goto Synchronize;` |
|        - |  665 | `		}` |
|        7 |  666 | `	}` |
|      180 |  667 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      180 |  668 | `		pIn++; /* Jump the leading curly '{' */` |
|      180 |  669 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      180 |  670 | `		if( pIn < pEnd ){` |
|      180 |  671 | `			pIn++;` |
|       89 |  672 | `		}` |
|       91 |  673 | `	}else{` |
|        - |  674 | `		/* Syntax error */` |
|      ! 0 |  675 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  676 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  677 | `			return SXERR_ABORT;` |
|        - |  678 | `		}` |
|        - |  679 | `	}` |
|      180 |  680 | `	rc = SXRET_OK;` |
|      101 |  681 | `Synchronize:` |
|        - |  682 | `	/* Synchronize pointers */` |
|      204 |  683 | `	*ppCur = pIn;` |
|      204 |  684 | `	return rc;` |
|      103 |  685 |  |
|        - |  686 | `/*` |
|        - |  687 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  688 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  689 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  690 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  691 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  692 | ` */` |
|       84 |  693 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  694 |  |
|       86 |  695 | `	SyToken *pIn = *ppCur;` |
|        - |  696 | `	sxu32 nLine;` |
|        - |  697 | `	sxi32 rc;` |
|        - |  698 | `	int iNest;` |
|       86 |  699 | `	nLine = pIn->nLine;` |
|        - |  700 | `	/* Optional 'static' prefix */` |
|       84 |  701 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       86 |  702 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  703 | `		pIn++;` |
|        1 |  704 | `	}` |
|        - |  705 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       84 |  706 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       86 |  707 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  708 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  709 | `		goto Synchronize;` |
|        - |  710 | `	}` |
|       86 |  711 | `	pIn++; /* Jump 'fn' */` |
|       42 |  712 | `	SXUNUSED(nLine);` |
|       42 |  713 | `	SXUNUSED(pGen);` |
|        - |  714 | `	/* Optional '&' for return-by-reference */` |
|       86 |  715 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  716 | `		pIn++;` |
|      ! 0 |  717 | `	}` |
|        - |  718 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  719 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  720 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  721 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       86 |  722 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       84 |  723 | `		pIn++; /* '(' */` |
|       84 |  724 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       84 |  725 | `		if( pIn < pEnd ){` |
|       82 |  726 | `			pIn++; /* ')' */` |
|       40 |  727 | `		}` |
|       41 |  728 | `	}` |
|        - |  729 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|       86 |  730 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  731 | `		pIn++;` |
|        7 |  732 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  733 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  734 | `			pIn++;` |
|        1 |  735 | `		}` |
|        7 |  736 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|        7 |  737 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  738 | `			pIn++;` |
|        7 |  739 | `			while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  740 | `				pIn += 2;` |
|      ! 0 |  741 | `			}` |
|        3 |  742 | `		}` |
|        9 |  743 | `		while( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|        4 |  744 | `			&& pIn->sData.zString[0] == '\|' ){` |
|      ! 0 |  745 | `			pIn++;` |
|      ! 0 |  746 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){ pIn++; }` |
|      ! 0 |  747 | `			if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  748 | `				pIn++;` |
|      ! 0 |  749 | `				while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  750 | `					pIn += 2;` |
|      ! 0 |  751 | `				}` |
|      ! 0 |  752 | `			}` |
|      ! 0 |  753 | `		}` |
|        3 |  754 | `	}` |
|        - |  755 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       86 |  756 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       79 |  757 | `		pIn++;` |
|       39 |  758 | `	}` |
|        - |  759 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       86 |  760 | `	iNest = 0;` |
|      566 |  761 | `	while( pIn < pEnd ){` |
|      488 |  762 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  763 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  764 | `			break;` |
|        - |  765 | `		}` |
|      482 |  766 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       21 |  767 | `			iNest++;` |
|      472 |  768 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       21 |  769 | `			iNest--;` |
|       10 |  770 | `		}` |
|      482 |  771 | `		pIn++;` |
|        2 |  772 | `	}` |
|       86 |  773 | `	rc = SXRET_OK;` |
|       42 |  774 | `Synchronize:` |
|       86 |  775 | `	*ppCur = pIn;` |
|       86 |  776 | `	return rc;` |
|        2 |  777 |  |
|        - |  778 | `/*` |
|        - |  779 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  780 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  781 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  782 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  783 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  784 | ` */` |
|       68 |  785 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  786 |  |
|       70 |  787 | `	SyToken *pIn = *ppCur;` |
|        - |  788 | `	sxi32 rc;` |
|       34 |  789 | `	SXUNUSED(pGen);` |
|        - |  790 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       68 |  791 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       70 |  792 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  793 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  794 | `		goto Synchronize;` |
|        - |  795 | `	}` |
|       70 |  796 | `	pIn++; /* Jump 'match' */` |
|        - |  797 | `	/* Optional '(' subject ')' */` |
|       70 |  798 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       70 |  799 | `		pIn++;` |
|       70 |  800 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       70 |  801 | `		if( pIn < pEnd ){` |
|       70 |  802 | `			pIn++; /* ')' */` |
|       34 |  803 | `		}` |
|       34 |  804 | `	}` |
|        - |  805 | `	/* Optional '{' arms '}' */` |
|       70 |  806 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       70 |  807 | `		pIn++;` |
|       70 |  808 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       70 |  809 | `		if( pIn < pEnd ){` |
|       70 |  810 | `			pIn++; /* '}' */` |
|       34 |  811 | `		}` |
|       34 |  812 | `	}` |
|       70 |  813 | `	rc = SXRET_OK;` |
|       34 |  814 | `Synchronize:` |
|       70 |  815 | `	*ppCur = pIn;` |
|       70 |  816 | `	return rc;` |
|        2 |  817 |  |
|        - |  818 | `/*` |
|        - |  819 | ` * Extract a single expression node from the input.` |
|        - |  820 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  821 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  822 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  823 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  824 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  825 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  826 | ` */` |
|  3040660 |  827 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  828 |  |
|        - |  829 | `	ph7_expr_node *pNode;` |
|        - |  830 | `	SyToken *pCur;` |
|        - |  831 | `	sxi32 rc;` |
|        - |  832 | `	/* Allocate a new node */` |
|  3040662 |  833 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3040662 |  834 | `	if( pNode == 0 ){` |
|        - |  835 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  836 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  837 | `		 */` |
|      ! 0 |  838 | `		return SXERR_MEM;` |
|        - |  839 | `	}` |
|        - |  840 | `	/* Zero the structure */` |
|  3040662 |  841 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3040662 |  842 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  843 | `	/* Point to the head of the token stream */` |
|  3040662 |  844 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  845 | `	/* Start collecting tokens */` |
|  3040662 |  846 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  847 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  848 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       20 |  849 | `		pCur++;` |
|       20 |  850 | `		pGen->pIn = pCur;` |
|       20 |  851 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       20 |  852 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       20 |  853 | `		if( rc == SXRET_OK && *ppNode ){` |
|       20 |  854 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        9 |  855 | `		}` |
|       20 |  856 | `		return rc;` |
|        - |  857 | `	}` |
|  3040644 |  858 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  859 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  860 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  861 | `		 */` |
|      328 |  862 | `		pCur++; /* Skip the opening '[' */` |
|      328 |  863 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      328 |  864 | `		if( pCur < pGen->pEnd ){` |
|      328 |  865 | `			pCur++; /* Skip past the closing ']' */` |
|      165 |  866 | `		}else{` |
|      ! 0 |  867 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  868 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  869 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  870 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  871 | `			}` |
|      ! 0 |  872 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  873 | `			return rc;` |
|        - |  874 | `		}` |
|        - |  875 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  876 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  877 | `		 */` |
|      351 |  878 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  879 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  880 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  881 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  882 | `			}else{` |
|       19 |  883 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  884 | `			}` |
|       25 |  885 | `		}else{` |
|      282 |  886 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  887 | `		}` |
|  3040481 |  888 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  889 | `		/* Point to the instance that describe this operator */` |
|   684018 |  890 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  891 | `		/* Advance the stream cursor */` |
|   684018 |  892 | `		pCur++;` |
|  2698310 |  893 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  894 | `		/* Isolate variable */` |
|  1659154 |  895 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   829584 |  896 | `			pCur++; /* Variable variable */` |
|        2 |  897 | `		}` |
|   829572 |  898 | `		if( pCur < pGen->pEnd ){` |
|   829572 |  899 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  900 | `				/* Variable name */` |
|   829544 |  901 | `				pCur++;` |
|   414801 |  902 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  903 | `				pCur++;` |
|        - |  904 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  905 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  906 | `				if( pCur < pGen->pEnd ){` |
|       18 |  907 | `					pCur++;` |
|       10 |  908 | `				}else{` |
|        5 |  909 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  910 | `					if( rc != SXERR_ABORT ){` |
|        5 |  911 | `						rc = SXERR_SYNTAX;` |
|        2 |  912 | `					}` |
|        5 |  913 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  914 | `					return rc;` |
|        - |  915 | `				}` |
|        8 |  916 | `			}` |
|   414783 |  917 | `		}` |
|   829568 |  918 | `		pNode->xCode = PH7_CompileVariable;` |
|  1941515 |  919 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    36856 |  920 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    36856 |  921 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  922 | `			 /* List/Array node */` |
|    24506 |  923 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  924 | `				 /* Assume a literal */` |
|       17 |  925 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  926 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  927 | `			 }else{` |
|    24490 |  928 | `				 pCur += 2;` |
|        - |  929 | `				 /* Collect array/list tokens */` |
|    24490 |  930 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    24490 |  931 | `				 if( pCur < pGen->pEnd ){` |
|    24488 |  932 | `					 pCur++;` |
|    12245 |  933 | `				 }else{` |
|        - |  934 | `					 /* Syntax error */` |
|        4 |  935 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  936 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  937 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  938 | `						 rc = SXERR_SYNTAX;` |
|        1 |  939 | `					 }` |
|        3 |  940 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  941 | `					 return rc;` |
|        - |  942 | `				 }` |
|    24488 |  943 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    24488 |  944 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  945 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  946 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  947 | `						 /* Syntax error */` |
|        3 |  948 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  949 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  950 | `							 rc = SXERR_SYNTAX;` |
|        1 |  951 | `						 }` |
|        3 |  952 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  953 | `						 return rc;` |
|        - |  954 | `					 }` |
|       12 |  955 | `				 }` |
|        2 |  956 | `			 }` |
|    24602 |  957 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  958 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  959 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  960 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  961 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  962 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  963 | `			 pNode->xCode = PH7_CompileYield;` |
|    12335 |  964 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  965 | `			 /* Annonymous function */` |
|      204 |  966 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  967 | `				 /* Assume a literal */` |
|      ! 0 |  968 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  969 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  970 | `			 }else{` |
|        - |  971 | `				 /* Assemble annonymous functions body */` |
|      204 |  972 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      204 |  973 | `				 if( rc != SXRET_OK ){` |
|       25 |  974 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  975 | `					 return rc;` |
|        - |  976 | `				 }` |
|      180 |  977 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  978 | `			  }` |
|    12206 |  979 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    12075 |  980 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  981 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  982 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  983 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       86 |  984 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       86 |  985 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  986 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  987 | `				 return rc;` |
|        - |  988 | `			 }` |
|       86 |  989 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    12074 |  990 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - |  991 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       70 |  992 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       70 |  993 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  994 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  995 | `				 return rc;` |
|        - |  996 | `			 }` |
|       70 |  997 | `			 pNode->xCode = PH7_CompileMatch;` |
|    11998 |  998 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  999 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 | 1000 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 | 1001 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 | 1002 | `		 }else{` |
|        - | 1003 | `			 /* Assume a literal */` |
|    11886 | 1004 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11886 | 1005 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 | 1006 | `		 }` |
|  1508291 | 1007 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1008 | `		 /* Constants,function name,namespace path,class name... */` |
|   544688 | 1009 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   544688 | 1010 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   272345 | 1011 | `	 }else{` |
|   945192 | 1012 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1013 | `			 /* Point to the code generator routine */` |
|   171892 | 1014 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   171892 | 1015 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1016 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1017 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1018 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1019 | `				 }` |
|        3 | 1020 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1021 | `				 return rc;` |
|        - | 1022 | `			 }` |
|    85944 | 1023 | `		 }` |
|        - | 1024 | `		/* Advance the stream cursor */` |
|   945190 | 1025 | `		pCur++;` |
|        - | 1026 | `	 }` |
|        - | 1027 | `	/* Point to the end of the token stream */` |
|  3040610 | 1028 | `	pNode->pEnd = pCur;` |
|        - | 1029 | `	/* Save the node for later processing */` |
|  3040610 | 1030 | `	*ppNode = pNode;` |
|        - | 1031 | `	/* Synchronize cursors */` |
|  3040610 | 1032 | `	pGen->pIn = pCur;` |
|  3040610 | 1033 | `	return SXRET_OK;` |
|  1520332 | 1034 |  |
|        - | 1035 | `/*` |
|        - | 1036 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1037 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1038 | ` * level is zero.` |
|        - | 1039 | ` */` |
|    73522 | 1040 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 | 1041 |  |
|    73524 | 1042 | `	SyToken *pCur = pStart;` |
|    73524 | 1043 | `	sxi32 iNest = 0;` |
|    73524 | 1044 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1045 | `		/* Last expression */` |
|    39168 | 1046 | `		return SXERR_EOF;` |
|        - | 1047 | `	}` |
|   138726 | 1048 | `	while( pCur < pEnd ){` |
|   125680 | 1049 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    21312 | 1050 | `			break;` |
|        - | 1051 | `		}` |
|   104370 | 1052 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5990 | 1053 | `			iNest++;` |
|   101376 | 1054 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5992 | 1055 | `			iNest--;` |
|     2995 | 1056 | `		}` |
|   104370 | 1057 | `		pCur++;` |
|        2 | 1058 | `	}` |
|    34358 | 1059 | `	*ppNext = pCur;` |
|    34358 | 1060 | `	return SXRET_OK;` |
|    36763 | 1061 |  |
|        - | 1062 | `/*` |
|        - | 1063 | ` * Free an expression tree.` |
|        - | 1064 | ` */` |
|  2602388 | 1065 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1066 |  |
|  2602390 | 1067 | `	if( pNode->pLeft ){` |
|        - | 1068 | `		/* Release the left tree */` |
|   970612 | 1069 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   485305 | 1070 | `	}` |
|  2602390 | 1071 | `	if( pNode->pRight ){` |
|        - | 1072 | `		/* Release the right tree */` |
|   508160 | 1073 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   254079 | 1074 | `	}` |
|  2602390 | 1075 | `	if( pNode->pCond ){` |
|        - | 1076 | `		/* Release the conditional tree used by the ternary operator */` |
|     1978 | 1077 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      988 | 1078 | `	}` |
|  2602390 | 1079 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1080 | `		ph7_expr_node **apArg;` |
|        - | 1081 | `		sxu32 n;` |
|        - | 1082 | `		/* Release node arguments */` |
|   344564 | 1083 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   727416 | 1084 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   382854 | 1085 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   191428 | 1086 | `		}` |
|   344564 | 1087 | `		SySetRelease(&pNode->aNodeArgs);` |
|   172281 | 1088 | `	}` |
|        - | 1089 | `	/* Finally,release this node */` |
|  2602390 | 1090 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2602390 | 1091 |  |
|        - | 1092 | `/*` |
|        - | 1093 | ` * Free an expression tree.` |
|        - | 1094 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1095 | ` */` |
|   690126 | 1096 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1097 |  |
|        - | 1098 | `	ph7_expr_node **apNode;` |
|        - | 1099 | `	sxu32 n;` |
|   690128 | 1100 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3730736 | 1101 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3040610 | 1102 | `		if( apNode[n] ){` |
|   690442 | 1103 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   345220 | 1104 | `		}` |
|  1520306 | 1105 | `	}` |
|   690128 | 1106 | `	return SXRET_OK;` |
|        2 | 1107 |  |
|        - | 1108 | `/*` |
|        - | 1109 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1110 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1111 | ` */` |
|   220970 | 1112 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1113 |  |
|        - | 1114 | `	sxi32 iExprOp;` |
|   220972 | 1115 | `	if( pNode->pOp == 0 ){` |
|   143722 | 1116 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1117 | `	}` |
|    77252 | 1118 | `	iExprOp = pNode->pOp->iOp;` |
|    77252 | 1119 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    48496 | 1120 | `			return TRUE;` |
|        - | 1121 | `	}` |
|    28758 | 1122 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    28754 | 1123 | `		if( pNode->pLeft->pOp ) {` |
|       18 | 1124 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        5 | 1125 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1126 | `				return FALSE;` |
|        1 | 1127 | `			}` |
|    28745 | 1128 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1129 | `			return FALSE;` |
|        - | 1130 | `		}` |
|    28754 | 1131 | `		return TRUE;` |
|        - | 1132 | `	}` |
|        5 | 1133 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1134 | `		return TRUE;` |
|        - | 1135 | `	}` |
|        - | 1136 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1137 | `	return FALSE;` |
|   110487 | 1138 |  |
|        - | 1139 | `/* Forward declaration */` |
|        - | 1140 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1141 | `/* Macro to check if the given node is a terminal.` |
|        - | 1142 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1143 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1144 | ` * linked ternary/elvis node). */` |
|        - | 1145 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1146 | `/*` |
|        - | 1147 | ` * Buid an expression tree for each given function argument.` |
|        - | 1148 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1149 | ` */` |
|   285996 | 1150 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1151 |  |
|        - | 1152 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1153 | `	sxi32 rc;` |
|        - | 1154 | `	/* Process function arguments from left to right */` |
|   285998 | 1155 | `	iCur = 0;` |
|   305129 | 1156 | `	for(;;){` |
|   610260 | 1157 | `		if( iCur >= nToken ){` |
|        - | 1158 | `			/* No more arguments to process */` |
|   285972 | 1159 | `			break;` |
|        - | 1160 | `		}` |
|   324290 | 1161 | `		iNode = iCur;` |
|   324290 | 1162 | `		iNest = 0;` |
|   811612 | 1163 | `		while( iCur < nToken ){` |
|   525640 | 1164 | `			if( apNode[iCur] ){` |
|   514288 | 1165 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    19160 | 1166 | `					break;` |
|   475972 | 1167 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    26372 | 1168 | `					iNest++;` |
|   462787 | 1169 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    26352 | 1170 | `					iNest--;` |
|    13175 | 1171 | `				}` |
|   237985 | 1172 | `			}` |
|   487324 | 1173 | `			iCur++;` |
|        2 | 1174 | `		}` |
|   324290 | 1175 | `		if( iCur > iNode ){` |
|   324284 | 1176 | `			SyString sArgName = {0, 0};` |
|        - | 1177 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1178 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1179 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   326020 | 1180 | `			if( (iCur - iNode) >= 2` |
|   179925 | 1181 | `				&& apNode[iNode]` |
|    35568 | 1182 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    19547 | 1183 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3501 | 1184 | `				&& apNode[iNode+1]` |
|     3478 | 1185 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1186 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      178 | 1187 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      178 | 1188 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      178 | 1189 | `				apNode[iNode] = 0;` |
|      178 | 1190 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      178 | 1191 | `				apNode[iNode+1] = 0;` |
|      178 | 1192 | `				iNode += 2;` |
|        - | 1193 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1194 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      178 | 1195 | `				if( iNode >= iCur ){` |
|        4 | 1196 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1197 | `						pOp->pStart->nLine,` |
|        - | 1198 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1199 | `						&sArgName);` |
|        3 | 1200 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1201 | `						rc = SXERR_SYNTAX;` |
|        1 | 1202 | `					}` |
|        3 | 1203 | `					return rc;` |
|        - | 1204 | `				}` |
|       87 | 1205 | `			}` |
|   324280 | 1206 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1207 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1208 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1209 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1210 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1211 | `					apNode[iNode] = 0;` |
|      ! 0 | 1212 | `			}` |
|   324282 | 1213 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   324282 | 1214 | `			if( apNode[iNode] ){` |
|   324282 | 1215 | `				if( sArgName.nByte > 0 ){` |
|      176 | 1216 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      176 | 1217 | `					apNode[iNode]->sArgName = sArgName;` |
|       87 | 1218 | `				}` |
|        - | 1219 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   324282 | 1220 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   162142 | 1221 | `			}else{` |
|        - | 1222 | `				/* No expression before comma */` |
|      ! 0 | 1223 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1224 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1225 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1226 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1227 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1228 | `				}` |
|      ! 0 | 1229 | `				return rc;` |
|        - | 1230 | `			}` |
|   162142 | 1231 | `		}else{` |
|        - | 1232 | `			/* Comma with no preceding argument */` |
|        7 | 1233 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1234 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1235 | `				rc = SXERR_SYNTAX;` |
|        3 | 1236 | `			}` |
|        7 | 1237 | `			return rc;` |
|        - | 1238 | `		}` |
|        - | 1239 | `		/* Jump trailing comma */` |
|   324282 | 1240 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    38312 | 1241 | `			iCur++;` |
|    38312 | 1242 | `			if( iCur >= nToken ){` |
|        - | 1243 | `				/* Trailing comma after last argument */` |
|       19 | 1244 | `				break;` |
|        - | 1245 | `			}` |
|    19146 | 1246 | `		}` |
|        2 | 1247 | `	}` |
|   285990 | 1248 | `	return SXRET_OK;` |
|   143000 | 1249 |  |
|        - | 1250 | ` /*` |
|        - | 1251 | `  * Create an expression tree from an array of tokens.` |
|        - | 1252 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1253 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1254 | `  */` |
|  1103854 | 1255 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1256 | ` {` |
|        - | 1257 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1258 | `	 ph7_expr_node *pNode;` |
|        - | 1259 | `	 sxi32 iCur;` |
|        - | 1260 | `	 sxi32 rc;` |
|  1103856 | 1261 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1262 | `		 /* TICKET 1433-17: self evaluating node */` |
|   509264 | 1263 | `		 return SXRET_OK;` |
|        - | 1264 | `	 }` |
|        - | 1265 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3651936 | 1266 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1267 | `		 sxi32 iNest;` |
|        - | 1268 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1269 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1270 | `		  */` |
|  3057346 | 1271 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3033344 | 1272 | `			 continue;` |
|        - | 1273 | `		 }` |
|    24004 | 1274 | `		 iNest = 1;` |
|    24004 | 1275 | `		 iLeft = iCur;` |
|        - | 1276 | `		 /* Find the closing parenthesis */` |
|    24004 | 1277 | `		 iCur++;` |
|   159554 | 1278 | `		 while( iCur < nToken ){` |
|   159554 | 1279 | `			 if( apNode[iCur] ){` |
|   159554 | 1280 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1281 | `					 /* Decrement nesting level */` |
|    41606 | 1282 | `					 iNest--;` |
|    41606 | 1283 | `					 if( iNest <= 0 ){` |
|    24004 | 1284 | `						 break;` |
|        2 | 1285 | `					 }` |
|   126751 | 1286 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1287 | `					 /* Increment nesting level */` |
|    17604 | 1288 | `					 iNest++;` |
|     8801 | 1289 | `				 }` |
|    67775 | 1290 | `			 }` |
|   135552 | 1291 | `			 iCur++;` |
|        2 | 1292 | `		 }` |
|    24004 | 1293 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1294 | `			 /* Recurse and process this expression */` |
|    24004 | 1295 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    24004 | 1296 | `			 if( rc != SXRET_OK ){` |
|        3 | 1297 | `				 return rc;` |
|        - | 1298 | `			 }` |
|    12000 | 1299 | `		 }` |
|        - | 1300 | `		 /* Free the left and right nodes */` |
|    24002 | 1301 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    24002 | 1302 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    24002 | 1303 | `		 apNode[iLeft] = 0;` |
|    24002 | 1304 | `		 apNode[iCur] = 0;` |
|    12002 | 1305 | `	 }` |
|        - | 1306 | `	  /* Process expressions enclosed in braces */` |
|  3805328 | 1307 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1308 | `		 sxi32 iNest;` |
|        - | 1309 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1310 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1311 | `		  */` |
|  3216886 | 1312 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3216886 | 1313 | `			 continue;` |
|        - | 1314 | `		 }` |
|      ! 0 | 1315 | `		 iNest = 1;` |
|      ! 0 | 1316 | `		 iLeft = iCur;` |
|        - | 1317 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1318 | `		 iCur++;` |
|      ! 0 | 1319 | `		 while( iCur < nToken ){` |
|      ! 0 | 1320 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1321 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1322 | `					 /* Decrement nesting level */` |
|      ! 0 | 1323 | `					 iNest--;` |
|      ! 0 | 1324 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1325 | `						 break;` |
|      ! 0 | 1326 | `					 }` |
|      ! 0 | 1327 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1328 | `					 /* Increment nesting level */` |
|      ! 0 | 1329 | `					 iNest++;` |
|      ! 0 | 1330 | `				 }` |
|      ! 0 | 1331 | `			 }` |
|      ! 0 | 1332 | `			 iCur++;` |
|      ! 0 | 1333 | `		 }` |
|      ! 0 | 1334 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1335 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1336 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1337 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1338 | `				 return rc;` |
|        - | 1339 | `			 }` |
|      ! 0 | 1340 | `		 }` |
|        - | 1341 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1342 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1343 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1344 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1345 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1346 | `	 }` |
|        - | 1347 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   588444 | 1348 | `	 iLeft = -1;` |
|  3805292 | 1349 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3216862 | 1350 | `		 if( apNode[iCur] == 0 ){` |
|  1251814 | 1351 | `			 continue;` |
|        - | 1352 | `		 }` |
|  1965050 | 1353 | `		 pNode = apNode[iCur];` |
|  1965050 | 1354 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   507286 | 1355 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1356 | `				 /* Collect function arguments */` |
|   325116 | 1357 | `				 sxi32 iPtr = 0;` |
|   325116 | 1358 | `				 sxi32 nFuncTok = 0;` |
|  1175870 | 1359 | `				 while( nFuncTok + iCur < nToken ){` |
|  1175870 | 1360 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1164518 | 1361 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   336968 | 1362 | `							 iPtr++;` |
|   996035 | 1363 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   336968 | 1364 | `							 iPtr--;` |
|   336968 | 1365 | `							 if( iPtr <= 0 ){` |
|   325116 | 1366 | `								 break;` |
|        - | 1367 | `							 }` |
|     5926 | 1368 | `						 }` |
|   419701 | 1369 | `					 }` |
|   850756 | 1370 | `					 nFuncTok++;` |
|        2 | 1371 | `				 }` |
|   325116 | 1372 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1373 | `					 /* Syntax error */` |
|      ! 0 | 1374 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1375 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1376 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1377 | `					 }` |
|      ! 0 | 1378 | `					 return rc;` |
|        - | 1379 | `				 }` |
|   325116 | 1380 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1381 | `					 /* Syntax error */` |
|      ! 0 | 1382 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1383 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1384 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1385 | `					 }` |
|      ! 0 | 1386 | `					 return rc;` |
|        - | 1387 | `				 }` |
|   325116 | 1388 | `				 if( nFuncTok > 1 ){` |
|        - | 1389 | `					 /* Process function arguments */` |
|   285998 | 1390 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   285998 | 1391 | `					 if( rc != SXRET_OK ){` |
|        9 | 1392 | `						 return rc;` |
|        - | 1393 | `					 }` |
|   142994 | 1394 | `				 }` |
|        - | 1395 | `				 /* Link the node to the tree */` |
|   325108 | 1396 | `				 pNode->pLeft = apNode[iLeft];` |
|   325108 | 1397 | `				 apNode[iLeft] = 0;` |
|  1175838 | 1398 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   850732 | 1399 | `					 apNode[iCur+iPtr] = 0;` |
|   425367 | 1400 | `				 }` |
|   344725 | 1401 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1402 | `				 /* Subscripting */` |
|    72834 | 1403 | `				 sxi32 iArrTok = iCur + 1;` |
|    72834 | 1404 | `				 sxi32 iNest = 1;` |
|    72913 | 1405 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1406 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1407 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1408 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    72832 | 1409 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1410 | `						 /* Syntax error */` |
|      ! 0 | 1411 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1412 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1413 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1414 | `						 }` |
|      ! 0 | 1415 | `						 return rc;` |
|        - | 1416 | `				 }` |
|        - | 1417 | `				 /* Collect index tokens */` |
|   131516 | 1418 | `				 while( iArrTok < nToken ){` |
|   131516 | 1419 | `					 if( apNode[iArrTok] ){` |
|   131484 | 1420 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1421 | `							 /* Increment nesting level */` |
|      ! 0 | 1422 | `							 iNest++;` |
|   131484 | 1423 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1424 | `							 /* Decrement nesting level */` |
|    72834 | 1425 | `							 iNest--;` |
|    72834 | 1426 | `							 if( iNest <= 0 ){` |
|    72834 | 1427 | `								 break;` |
|        - | 1428 | `							 }` |
|      ! 0 | 1429 | `						 }` |
|    29325 | 1430 | `					 }` |
|    58684 | 1431 | `					 ++iArrTok;` |
|        2 | 1432 | `				 }` |
|    72834 | 1433 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1434 | `					 /* Recurse and process this expression */` |
|    58574 | 1435 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    58574 | 1436 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1437 | `						 return rc;` |
|        - | 1438 | `					 }` |
|        - | 1439 | `					 /* Link the node to it's index */` |
|    58574 | 1440 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    29286 | 1441 | `				 }` |
|        - | 1442 | `				 /* Link the node to the tree */` |
|    72834 | 1443 | `				 pNode->pLeft = apNode[iLeft];` |
|    72834 | 1444 | `				 pNode->pRight = 0;` |
|    72834 | 1445 | `				 apNode[iLeft] = 0;` |
|   204348 | 1446 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   131516 | 1447 | `					 apNode[iNest] = 0;` |
|    65759 | 1448 | `				 }` |
|    36418 | 1449 | `			 }else{` |
|        - | 1450 | `				 /* Member access operators [i.e: '->','::'] */` |
|   109340 | 1451 | `				  iRight = iCur + 1;` |
|   109340 | 1452 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1453 | `					 iRight++;` |
|      ! 0 | 1454 | `				 }` |
|   109340 | 1455 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1456 | `					 /* Syntax error */` |
|        5 | 1457 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1458 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1459 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1460 | `					 }` |
|        5 | 1461 | `					 return rc;` |
|        - | 1462 | `				 }` |
|        - | 1463 | `				 /* Link the node to the tree */` |
|   109336 | 1464 | `				 pNode->pLeft = apNode[iLeft];` |
|   109336 | 1465 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   109060 | 1466 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1467 | `						 /* Syntax error */` |
|      ! 0 | 1468 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1469 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1470 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1471 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1472 | `						 }` |
|      ! 0 | 1473 | `						 return rc;` |
|        - | 1474 | `				 }` |
|   109336 | 1475 | `				 pNode->pRight = apNode[iRight];` |
|   109336 | 1476 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1477 | `			 }` |
|   253636 | 1478 | `		 }` |
|  1965038 | 1479 | `		 iLeft = iCur;` |
|   982520 | 1480 | `	 }` |
|        - | 1481 | `	 /* Handle left associative (new, clone) operators */` |
|  3805260 | 1482 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3216830 | 1483 | `		 if( apNode[iCur] == 0 ){` |
|  1773876 | 1484 | `			 continue;` |
|        - | 1485 | `		 }` |
|  1442956 | 1486 | `		 pNode = apNode[iCur];` |
|  1442956 | 1487 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1488 | `			 SyToken *pToken;` |
|        - | 1489 | `			 /* Get the left node */` |
|    14792 | 1490 | `			 iLeft = iCur + 1;` |
|    29552 | 1491 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    14762 | 1492 | `				 iLeft++;` |
|        2 | 1493 | `			 }` |
|    14792 | 1494 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1495 | `				  /* Syntax error */` |
|      ! 0 | 1496 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1497 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1498 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1499 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1500 | `				 }` |
|      ! 0 | 1501 | `				 return rc;` |
|        - | 1502 | `			 }` |
|        - | 1503 | `			 /* Make sure the operand are of a valid type */` |
|    14792 | 1504 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1505 | `				 /* Clone:` |
|        - | 1506 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1507 | `				  *  ++ function call (including annonymous)` |
|        - | 1508 | `				  *  ++ array member` |
|        - | 1509 | `				  *  ++ 'new' operator` |
|        - | 1510 | `				  * Example:` |
|        - | 1511 | `				  *   clone $pObj;` |
|        - | 1512 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1513 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1514 | `				  */` |
|       18 | 1515 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1516 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1517 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1518 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1519 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1520 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1521 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1522 | `						 }` |
|      ! 0 | 1523 | `						 return rc;` |
|        - | 1524 | `					 }` |
|        7 | 1525 | `				 }` |
|       10 | 1526 | `			 }else{` |
|        - | 1527 | `				 /* New */` |
|    14776 | 1528 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1529 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1530 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1531 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1532 | `						 /* Syntax error */` |
|      ! 0 | 1533 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1534 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1535 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1536 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1537 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1538 | `						 }` |
|      ! 0 | 1539 | `						 return rc;` |
|        - | 1540 | `					 }` |
|        8 | 1541 | `				 }` |
|        - | 1542 | `			 }` |
|        - | 1543 | `			  /* Link the node to the tree */` |
|    14792 | 1544 | `			 pNode->pLeft = apNode[iLeft];` |
|    14792 | 1545 | `			 apNode[iLeft] = 0;` |
|    14792 | 1546 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7395 | 1547 | `		 }` |
|   721479 | 1548 | `	 }` |
|        - | 1549 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   588432 | 1550 | `	 iLeft = -1;` |
|  3805260 | 1551 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3213756 | 1552 | `		 if( apNode[iCur] == 0 ){` |
|  1773876 | 1553 | `			 continue;` |
|        - | 1554 | `		 }` |
|  1439882 | 1555 | `		 pNode = apNode[iCur];` |
|  1439882 | 1556 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8542 | 1557 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3092 | 1558 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1559 | `					 /* Link the node to the tree */` |
|     3094 | 1560 | `					 pNode->pLeft = apNode[iLeft];` |
|     3094 | 1561 | `					 apNode[iLeft] = 0;` |
|     1546 | 1562 | `			 }` |
|     5807 | 1563 | `		  }` |
|  1442956 | 1564 | `		 iLeft = iCur;` |
|   721479 | 1565 | `	  }` |
|   591506 | 1566 | `	 iLeft = -1;` |
|  3808334 | 1567 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3216830 | 1568 | `		 if( apNode[iCur] == 0 ){` |
|  1776968 | 1569 | `			 continue;` |
|        - | 1570 | `		 }` |
|  1439864 | 1571 | `		 pNode = apNode[iCur];` |
|  1439864 | 1572 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8523 | 1573 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8524 | 1574 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1575 | `					 /* Syntax error */` |
|      ! 0 | 1576 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1577 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1578 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1579 | `					 }` |
|      ! 0 | 1580 | `					 return rc;` |
|        - | 1581 | `			 }` |
|        - | 1582 | `			 /* Link the node to the tree */` |
|     8524 | 1583 | `			 pNode->pLeft = apNode[iLeft];` |
|     8524 | 1584 | `			 apNode[iLeft] = 0;` |
|        - | 1585 | `			 /* Mark as pre-increment/decrement node */` |
|     8524 | 1586 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4261 | 1587 | `		  }` |
|  1439864 | 1588 | `		 iLeft = iCur;` |
|   719933 | 1589 | `	 }` |
|        - | 1590 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   591506 | 1591 | `	  iLeft = 0;` |
|  3808328 | 1592 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3216826 | 1593 | `		  if( apNode[iCur] ){` |
|  1431338 | 1594 | `			  pNode = apNode[iCur];` |
|  1431338 | 1595 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    38182 | 1596 | `				  if( iLeft > 0 ){` |
|        - | 1597 | `					  /* Link the node to the tree */` |
|    38180 | 1598 | `					  pNode->pLeft = apNode[iLeft];` |
|    38180 | 1599 | `					  apNode[iLeft] = 0;` |
|    38180 | 1600 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1601 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1602 | `							   /* Syntax error */` |
|      ! 0 | 1603 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1604 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1605 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1606 | `							  }` |
|      ! 0 | 1607 | `							  return rc;` |
|        - | 1608 | `						  }` |
|       36 | 1609 | `					  }` |
|    19091 | 1610 | `				  }else{` |
|        - | 1611 | `					  /* Syntax error */` |
|        3 | 1612 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1613 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1614 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1615 | `					  }` |
|        3 | 1616 | `					  return rc;` |
|        - | 1617 | `				  }` |
|    19089 | 1618 | `			  }` |
|        - | 1619 | `			  /* Save terminal position */` |
|  1431336 | 1620 | `			  iLeft = iCur;` |
|   715667 | 1621 | `		  }` |
|  1608413 | 1622 | `	  }` |
|        - | 1623 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6506448 | 1624 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5914954 | 1625 | `		 iLeft = -1;` |
| 38082928 | 1626 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 32167984 | 1627 | `			 if( apNode[iCur] == 0 ){` |
| 20529780 | 1628 | `				 continue;` |
|        - | 1629 | `			 }` |
| 11638206 | 1630 | `			 pNode = apNode[iCur];` |
| 11638206 | 1631 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1632 | `				 /* Get the right node */` |
|   175900 | 1633 | `				 iRight = iCur + 1;` |
|   249804 | 1634 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    73906 | 1635 | `					 iRight++;` |
|        2 | 1636 | `				 }` |
|   175900 | 1637 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1638 | `					 /* Syntax error */` |
|        9 | 1639 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1640 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1641 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1642 | `					 }` |
|        9 | 1643 | `					 return rc;` |
|        - | 1644 | `				 }` |
|   175892 | 1645 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1646 | `					 sxi32  iTmp;` |
|        - | 1647 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       48 | 1648 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1649 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1650 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1651 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1652 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1653 | `						 }` |
|      ! 0 | 1654 | `						 return rc;` |
|        - | 1655 | `					 }` |
|       48 | 1656 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1657 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1658 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1659 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1660 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1661 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1662 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1663 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1664 | `									 }` |
|      ! 0 | 1665 | `									 return rc;` |
|        - | 1666 | `							 }` |
|      ! 0 | 1667 | `						 }` |
|       16 | 1668 | `					 }` |
|        - | 1669 | `					 /* Swap operands */` |
|       48 | 1670 | `					 iTmp = iRight;` |
|       48 | 1671 | `					 iRight = iLeft;` |
|       48 | 1672 | `					 iLeft = iTmp;` |
|       23 | 1673 | `				 }` |
|        - | 1674 | `				 /* Link the node to the tree */` |
|   175892 | 1675 | `				 pNode->pLeft = apNode[iLeft];` |
|   175892 | 1676 | `				 pNode->pRight = apNode[iRight];` |
|   175892 | 1677 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    87945 | 1678 | `			 }` |
| 11638198 | 1679 | `			 iLeft = iCur;` |
|  5819100 | 1680 | `		 }` |
|  2957474 | 1681 | `	 }` |
|        - | 1682 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1683 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1684 | `	  * we are dealing with a single operator.` |
|        - | 1685 | `	  */` |
|   591496 | 1686 | `	  iLeft = -1;` |
|  3799740 | 1687 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3210224 | 1688 | `		  if( apNode[iCur] == 0 ){` |
|  2174760 | 1689 | `			  continue;` |
|        - | 1690 | `		  }` |
|  1035466 | 1691 | `		  pNode = apNode[iCur];` |
|  1035466 | 1692 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1980 | 1693 | `			  sxi32 iNest = 1;` |
|     1980 | 1694 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1695 | `				  /* Missing condition */` |
|        3 | 1696 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1697 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1698 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1699 | `				  }` |
|        3 | 1700 | `				  return rc;` |
|        - | 1701 | `			  }` |
|        - | 1702 | `			  /* Get the right node */` |
|     1978 | 1703 | `			  iRight = iCur + 1;` |
|     4190 | 1704 | `			  while( iRight < nToken  ){` |
|     4190 | 1705 | `				  if( apNode[iRight] ){` |
|     3886 | 1706 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1707 | `						  /* Increment nesting level */` |
|      ! 0 | 1708 | `						  ++iNest;` |
|     3886 | 1709 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1710 | `						  /* Decrement nesting level */` |
|     1978 | 1711 | `						  --iNest;` |
|     1978 | 1712 | `						  if( iNest <= 0 ){` |
|     1978 | 1713 | `							  break;` |
|        - | 1714 | `						  }` |
|      ! 0 | 1715 | `					  }` |
|      954 | 1716 | `				  }` |
|     2214 | 1717 | `				  iRight++;` |
|        2 | 1718 | `			  }` |
|     1978 | 1719 | `			  if( iRight > iCur + 1 ){` |
|        - | 1720 | `				  /* Recurse and process the then expression */` |
|     1910 | 1721 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1910 | 1722 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1723 | `					  return rc;` |
|        - | 1724 | `				  }` |
|        - | 1725 | `				  /* Link the node to the tree */` |
|     1910 | 1726 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      954 | 1727 | `			  }else{` |
|        - | 1728 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1729 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1730 | `			  }` |
|     1978 | 1731 | `			  apNode[iCur + 1] = 0;` |
|     1978 | 1732 | `			  if( iRight + 1 < nToken ){` |
|        - | 1733 | `				  /* Recurse and process the else expression */` |
|     1978 | 1734 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1978 | 1735 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1736 | `					  return rc;` |
|        - | 1737 | `				  }` |
|        - | 1738 | `				  /* Link the node to the tree */` |
|     1978 | 1739 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1978 | 1740 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      990 | 1741 | `			  }else{` |
|      ! 0 | 1742 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1743 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1744 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1745 | `				 }` |
|      ! 0 | 1746 | `				 return rc;` |
|        - | 1747 | `			  }` |
|        - | 1748 | `			  /* Point to the condition */` |
|     1978 | 1749 | `			  pNode->pCond  = apNode[iLeft];` |
|     1978 | 1750 | `			  apNode[iLeft] = 0;` |
|     1978 | 1751 | `			  break;` |
|        - | 1752 | `		  }` |
|  1033488 | 1753 | `		  iLeft = iCur;` |
|   516745 | 1754 | `	  }` |
|        - | 1755 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1756 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1757 | `	  * so there is no need for a precedence loop here.` |
|        - | 1758 | `	  */` |
|   591494 | 1759 | `	 iRight = -1;` |
|  3808184 | 1760 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3216738 | 1761 | `		 if( apNode[iCur] == 0 ){` |
|  2404190 | 1762 | `			 continue;` |
|        - | 1763 | `		 }` |
|   812550 | 1764 | `		 pNode = apNode[iCur];` |
|   812550 | 1765 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1766 | `			 /* Get the left node */` |
|   220936 | 1767 | `			 iLeft = iCur - 1;` |
|   312734 | 1768 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    91800 | 1769 | `				 iLeft--;` |
|        2 | 1770 | `			 }` |
|   220936 | 1771 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1772 | `				 /* Syntax error */` |
|       43 | 1773 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1774 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1775 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1776 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1777 | `				 }else{` |
|       39 | 1778 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1779 | `				 }` |
|       43 | 1780 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1781 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1782 | `				 }` |
|       43 | 1783 | `				 return rc;` |
|        - | 1784 | `			 }` |
|   220894 | 1785 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1786 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1787 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1788 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1789 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1790 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1791 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1792 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1793 | `					 }else{` |
|        4 | 1794 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1795 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1796 | `					 }` |
|        5 | 1797 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1798 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1799 | `					 }` |
|        5 | 1800 | `					 return rc;` |
|        - | 1801 | `				 }` |
|       26 | 1802 | `			 }` |
|        - | 1803 | `			 /* Link the node to the tree (Reverse) */` |
|   220890 | 1804 | `			 pNode->pLeft = apNode[iRight];` |
|   220890 | 1805 | `			 pNode->pRight = apNode[iLeft];` |
|   220890 | 1806 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   110444 | 1807 | `		 }` |
|   812504 | 1808 | `		 iRight = iCur;` |
|   406253 | 1809 | `	 }` |
|        - | 1810 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2957232 | 1811 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2365786 | 1812 | `		 iLeft = -1;` |
| 15232522 | 1813 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12866738 | 1814 | `			 if( apNode[iCur] == 0 ){` |
| 10500548 | 1815 | `				 continue;` |
|        - | 1816 | `			 }` |
|  2366192 | 1817 | `			 pNode = apNode[iCur];` |
|  2366192 | 1818 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1819 | `				 /* Get the right node */` |
|       72 | 1820 | `				 iRight = iCur + 1;` |
|      110 | 1821 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1822 | `					 iRight++;` |
|        2 | 1823 | `				 }` |
|       72 | 1824 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1825 | `					 /* Syntax error */` |
|      ! 0 | 1826 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1827 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1828 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1829 | `					 }` |
|      ! 0 | 1830 | `					 return rc;` |
|        - | 1831 | `				 }` |
|        - | 1832 | `				 /* Link the node to the tree */` |
|       72 | 1833 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1834 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1835 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1836 | `			 }` |
|  2366192 | 1837 | `			 iLeft = iCur;` |
|  1183097 | 1838 | `		 }` |
|  1182894 | 1839 | `	 }` |
|        - | 1840 | `	 /* Point to the root of the expression tree */` |
|  3216658 | 1841 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2625230 | 1842 | `		 if( apNode[iCur] ){` |
|   533888 | 1843 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1844 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1845 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1846 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1847 | `				  }` |
|       20 | 1848 | `				  return rc;` |
|        - | 1849 | `			 }` |
|   533870 | 1850 | `			 apNode[0] = apNode[iCur];` |
|   533870 | 1851 | `			 apNode[iCur] = 0;` |
|   266934 | 1852 | `		 }` |
|  1312607 | 1853 | `	 }` |
|   591430 | 1854 | `	 return SXRET_OK;` |
|   550392 | 1855 | ` }` |
|        - | 1856 | ` /*` |
|        - | 1857 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1858 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1859 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1860 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1861 | `  */` |
|   690126 | 1862 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1863 |  |
|        - | 1864 | `	ph7_expr_node **apNode;` |
|        - | 1865 | `	ph7_expr_node *pNode;` |
|        - | 1866 | `	sxi32 rc;` |
|        - | 1867 | `	/* Reset node container */` |
|   690128 | 1868 | `	SySetReset(pExprNode);` |
|   690128 | 1869 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1870 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1871 | `	{` |
|   690128 | 1872 | `		int iLastWasTerm = 0;` |
|  3730736 | 1873 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3040644 | 1874 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3040644 | 1875 | `			if( rc != SXRET_OK ){` |
|       35 | 1876 | `				return rc;` |
|        - | 1877 | `			}` |
|        - | 1878 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3040610 | 1879 | `			if( pNode->xCode ){` |
|        - | 1880 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1583294 | 1881 | `				iLastWasTerm = 1;` |
|  2248964 | 1882 | `			}else if( pNode->pOp ){` |
|        - | 1883 | `				/* Operator node */` |
|   684018 | 1884 | `				iLastWasTerm = 0;` |
|   342010 | 1885 | `			}else{` |
|        - | 1886 | `				/* Delimiter: ')' and ']' end terms */` |
|   773302 | 1887 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1888 | `			}` |
|        - | 1889 | `			/* Save the extracted node */` |
|  3040610 | 1890 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1891 | `		}` |
|        - | 1892 | `	}` |
|   690094 | 1893 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1894 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1895 | `		*ppRoot = 0;` |
|      ! 0 | 1896 | `		return SXRET_OK;` |
|        - | 1897 | `	}` |
|   690094 | 1898 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1899 | `	/* Make sure we are dealing with valid nodes */` |
|   690094 | 1900 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   690094 | 1901 | `	if( rc != SXRET_OK ){` |
|        - | 1902 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1903 | `		 * cleanup the mess left behind.` |
|        - | 1904 | `		 */` |
|       51 | 1905 | `		*ppRoot = 0;` |
|       51 | 1906 | `		return rc;` |
|        - | 1907 | `	}` |
|        - | 1908 | `	/* Build the tree */` |
|   690044 | 1909 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   690044 | 1910 | `	if( rc != SXRET_OK ){` |
|        - | 1911 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       90 | 1912 | `		*ppRoot = 0;` |
|       90 | 1913 | `		return rc;` |
|        - | 1914 | `	}` |
|        - | 1915 | `	/* Point to the root of the tree */` |
|   689956 | 1916 | `	*ppRoot = apNode[0];` |
|   689956 | 1917 | `	return SXRET_OK;` |
|   345065 | 1918 |  |
|        - | 1919 |  |
