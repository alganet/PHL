# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 843/993 lines (84.89%)

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
|        - |  238 | `	/* Precedence 19,left-associative */` |
|        - |  239 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  240 | `	/* Precedence 20,left-associative */` |
|        - |  241 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  242 | `	/* Precedence 21,left-associative */` |
|        - |  243 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  244 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  245 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  246 | `};` |
|        - |  247 | `/* Function call operator need special handling */` |
|        - |  248 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  249 | `/*` |
|        - |  250 | ` * Check if the given token is a potential operator or not.` |
|        - |  251 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  252 | ` * look like an operator.` |
|        - |  253 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  254 | ` * Otherwise NULL.` |
|        - |  255 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  256 | ` * a binary minus or unary minus.]` |
|        - |  257 | ` */` |
|   724356 |  258 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  259 |  |
|   724358 |  260 | `	sxu32 n = 0;` |
|        - |  261 | `	sxi32 rc;` |
|        - |  262 | `	/* Do a linear lookup on the operators table */` |
| 11837934 |  263 | `	for(;;){` |
| 23675870 |  264 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  265 | `			break;` |
|        - |  266 | `		}` |
| 23675870 |  267 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  268 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2888810 |  269 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1444406 |  270 | `		}else{` |
| 20787062 |  271 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  272 | `		}` |
| 23675870 |  273 | `		if( rc == 0 ){` |
|   727528 |  274 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  275 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   724028 |  276 | `				return &aOpTable[n];` |
|        - |  277 | `			}` |
|        - |  278 | `			/* Handle ambiguity */` |
|     3502 |  279 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  280 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  281 | `				return &aOpTable[n];` |
|        - |  282 | `			}` |
|     3282 |  283 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  284 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  285 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  286 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  287 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  288 | `					return &aOpTable[n];` |
|        - |  289 | `				}` |
|        - |  290 |  |
|        4 |  291 | `			}` |
|     1585 |  292 | `		}` |
| 22951514 |  293 | `		++n; /* Next operator in the table */` |
|        2 |  294 | `	}` |
|        - |  295 | `	/* No such operator */` |
|      ! 0 |  296 | `	return 0;` |
|   362180 |  297 |  |
|        - |  298 | `/*` |
|        - |  299 | ` * Delimit a set of token stream.` |
|        - |  300 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  301 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  302 | ` */` |
|   372802 |  303 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  304 |  |
|   372804 |  305 | `	SyToken *pCur = pIn;` |
|   372804 |  306 | `	sxi32 iNest = 1;` |
|  2118539 |  307 | `	for(;;){` |
|  4237080 |  308 | `		if( pCur >= pEnd ){` |
|      124 |  309 | `			break;` |
|        - |  310 | `		}` |
|  4236958 |  311 | `		if( pCur->nType & nTokStart ){` |
|        - |  312 | `			/* Increment nesting level */` |
|   234418 |  313 | `			iNest++;` |
|  4119750 |  314 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  315 | `			/* Decrement nesting level */` |
|   607098 |  316 | `			iNest--;` |
|   607098 |  317 | `			if( iNest <= 0 ){` |
|   372682 |  318 | `				break;` |
|        - |  319 | `			}` |
|   117208 |  320 | `		}` |
|        - |  321 | `		/* Advance cursor */` |
|  3864278 |  322 | `		pCur++;` |
|        2 |  323 | `	}` |
|        - |  324 | `	/* Point to the end of the chunk */` |
|   372804 |  325 | `	*ppEnd = pCur;` |
|   372804 |  326 |  |
|        - |  327 | `/*` |
|        - |  328 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  329 | ` * Note on reserved keywords.` |
|        - |  330 | ` *  According to the PHP language reference manual:` |
|        - |  331 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  332 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  333 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  334 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  335 | ` */` |
|    11232 |  336 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  337 |  |
|    16782 |  338 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11139 |  339 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  340 | `		){` |
|      146 |  341 | `			return TRUE;` |
|        - |  342 | `	}` |
|    11090 |  343 | `	if( bCheckFunc ){` |
|       92 |  344 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  345 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  346 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  347 | `				return TRUE;` |
|        - |  348 | `		}` |
|       20 |  349 | `	}` |
|        - |  350 | `	/* Not a language construct */` |
|    11058 |  351 | `	return FALSE;` |
|     5618 |  352 |  |
|        - |  353 | `/*` |
|        - |  354 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  355 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  356 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  357 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  358 | ` */` |
|   638208 |  359 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  360 |  |
|        - |  361 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  362 | `	sxi32 i,rc;` |
|        - |  363 |  |
|   638210 |  364 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  365 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       14 |  366 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       14 |  367 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        6 |  368 | `	}` |
|   638210 |  369 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3451678 |  370 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2813504 |  371 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  372 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      266 |  373 | `			continue;` |
|        - |  374 | `		}` |
|  2813240 |  375 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   323266 |  376 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    16580 |  377 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  378 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   301050 |  379 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  380 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  381 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  382 | `						 */` |
|   301050 |  383 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   301050 |  384 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   301050 |  385 | `						apNode[i]->pOp = &sFCallOp;` |
|   150524 |  386 | `					}` |
|   150524 |  387 | `			}` |
|   323266 |  388 | `			iParen++;` |
|  2651608 |  389 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   323266 |  390 | `			if( iParen <= 0 ){` |
|       13 |  391 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  392 | `				if( rc != SXERR_ABORT ){` |
|       13 |  393 | `					rc = SXERR_SYNTAX;` |
|        6 |  394 | `				}` |
|       13 |  395 | `				return rc;` |
|        - |  396 | `			}` |
|   323254 |  397 | `			iParen--;` |
|  2328338 |  398 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    67404 |  399 | `			iSquare++;` |
|  2133011 |  400 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    67418 |  401 | `			if( iSquare <= 0 ){` |
|        7 |  402 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  403 | `				if( rc != SXERR_ABORT ){` |
|        7 |  404 | `					rc = SXERR_SYNTAX;` |
|        3 |  405 | `				}` |
|        7 |  406 | `				return rc;` |
|        - |  407 | `			}` |
|    67412 |  408 | `			iSquare--;` |
|  2065599 |  409 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       11 |  410 | `			iBraces++;` |
|       11 |  411 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  412 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  413 | `				int iNest = 1;` |
|       11 |  414 | `				sxi32 j=i+1;` |
|        - |  415 | `				/*` |
|        - |  416 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  417 | `				 */` |
|       11 |  418 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  419 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  420 | `				pOp = aOpTable;` |
|       11 |  421 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       51 |  422 | `				while( pOp < pEnd ){` |
|       51 |  423 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  424 | `						break;` |
|        - |  425 | `					}` |
|       41 |  426 | `					pOp++;` |
|        1 |  427 | `				}` |
|       11 |  428 | `				if( pOp >= pEnd ){` |
|      ! 0 |  429 | `					pOp = 0;` |
|      ! 0 |  430 | `				}` |
|       11 |  431 | `				if( pOp ){` |
|       11 |  432 | `					apNode[i]->pOp = pOp;` |
|       11 |  433 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  434 | `				}` |
|       11 |  435 | `				iBraces--;` |
|       11 |  436 | `				iSquare++;` |
|       21 |  437 | `				while( j < nNode ){` |
|       21 |  438 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  439 | `						/* Increment nesting level */` |
|      ! 0 |  440 | `						iNest++;` |
|       21 |  441 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  442 | `						/* Decrement nesting level */` |
|       11 |  443 | `						iNest--;` |
|       11 |  444 | `						if( iNest < 1 ){` |
|       11 |  445 | `							break;` |
|        - |  446 | `						}` |
|      ! 0 |  447 | `					}` |
|       11 |  448 | `					j++;` |
|        1 |  449 | `				}` |
|       11 |  450 | `				if( j < nNode ){` |
|       11 |  451 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  452 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  453 | `				}` |
|        - |  454 |  |
|        6 |  455 | `			}` |
|  2031889 |  456 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  457 | `			if( iBraces <= 0 ){` |
|       13 |  458 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  459 | `				if( rc != SXERR_ABORT ){` |
|       13 |  460 | `					rc = SXERR_SYNTAX;` |
|        6 |  461 | `				}` |
|       13 |  462 | `				return rc;` |
|        - |  463 | `			}` |
|      ! 0 |  464 | `			iBraces--;` |
|  2031872 |  465 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1890 |  466 | `			if( iQuesty <= 0 ){` |
|        5 |  467 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  468 | `				if( rc != SXERR_ABORT ){` |
|        5 |  469 | `					rc = SXERR_SYNTAX;` |
|        2 |  470 | `				}` |
|        5 |  471 | `				return rc;` |
|        - |  472 | `			}` |
|     1886 |  473 | `			iQuesty--;` |
|  2030926 |  474 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   565392 |  475 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   565392 |  476 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1888 |  477 | `				iQuesty++;` |
|   564449 |  478 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      318 |  479 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  480 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  481 | `					sxu32 n = 0;` |
|       11 |  482 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  483 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  484 | `					}` |
|        - |  485 | `					/*` |
|        - |  486 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  487 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  488 | `					 */` |
|      245 |  489 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      235 |  490 | `						++n;` |
|        1 |  491 | `					}` |
|       11 |  492 | `					pOp = &aOpTable[n];` |
|        - |  493 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  494 | `					apNode[i]->pOp = pOp;` |
|       11 |  495 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  496 | `				}` |
|      158 |  497 | `			}` |
|   282695 |  498 | `		}` |
|  1406604 |  499 | `	}` |
|   638176 |  500 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  501 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  502 | `		if( rc != SXERR_ABORT ){` |
|       17 |  503 | `			rc = SXERR_SYNTAX;` |
|        8 |  504 | `		}` |
|       17 |  505 | `		return rc;` |
|        - |  506 | `	}` |
|   638160 |  507 | `	return SXRET_OK;` |
|   319106 |  508 |  |
|        - |  509 | `/*` |
|        - |  510 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  511 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  512 | ` */` |
|   514954 |  513 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  514 |  |
|   514956 |  515 | `	SyToken *pIn = *ppCur;` |
|        - |  516 | `	/* Jump the first literal seen */` |
|   514956 |  517 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   514934 |  518 | `		pIn++;` |
|   257466 |  519 | `	}` |
|   257507 |  520 | `	for(;;){` |
|   515016 |  521 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       62 |  522 | `			pIn++;` |
|       62 |  523 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       62 |  524 | `				pIn++;` |
|       30 |  525 | `			}` |
|       32 |  526 | `		}else{` |
|   257479 |  527 | `			break;` |
|        - |  528 | `		}` |
|        2 |  529 | `	}` |
|        - |  530 | `	/* Synchronize pointers */` |
|   514956 |  531 | `	*ppCur = pIn;` |
|   514956 |  532 |  |
|        - |  533 | `/*` |
|        - |  534 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  535 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  536 | ` * Note on annonymous functions.` |
|        - |  537 | ` *  According to the PHP language reference manual:` |
|        - |  538 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  539 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  540 | ` *  parameters, but they have many other uses.` |
|        - |  541 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  542 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  543 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  544 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  545 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  546 | ` *` |
|        - |  547 | ` * Some example:` |
|        - |  548 | ` *  $greet = function($name)` |
|        - |  549 | ` * {` |
|        - |  550 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  551 | ` * };` |
|        - |  552 | ` *  $greet('World');` |
|        - |  553 | ` *  $greet('PHP');` |
|        - |  554 | ` *` |
|        - |  555 | ` * $double = function($a) {` |
|        - |  556 | ` *   return $a * 2;` |
|        - |  557 | ` * };` |
|        - |  558 | ` * // This is our range of numbers` |
|        - |  559 | ` * $numbers = range(1, 5);` |
|        - |  560 | ` * // Use the Annonymous function as a callback here to` |
|        - |  561 | ` * // double the size of each element in our` |
|        - |  562 | ` * // range` |
|        - |  563 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  564 | ` * print implode(' ', $new_numbers);` |
|        - |  565 | ` */` |
|      192 |  566 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  567 |  |
|      194 |  568 | `	SyToken *pIn = *ppCur;` |
|        - |  569 | `	sxu32 nLine;` |
|        - |  570 | `	sxi32 rc;` |
|        - |  571 | `	/* Jump the 'function' keyword */` |
|      194 |  572 | `	nLine = pIn->nLine;` |
|      194 |  573 | `	pIn++;` |
|      194 |  574 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  575 | `		pIn++;` |
|        1 |  576 | `	}` |
|      194 |  577 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  578 | `		/* Syntax error */` |
|        5 |  579 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  580 | `		if( rc != SXERR_ABORT ){` |
|        5 |  581 | `			rc = SXERR_SYNTAX;` |
|        2 |  582 | `		}` |
|        5 |  583 | `		goto Synchronize;` |
|        - |  584 | `	}` |
|      190 |  585 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      190 |  586 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      190 |  587 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  588 | `		/* Syntax error */` |
|        5 |  589 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  590 | `		if( rc != SXERR_ABORT ){` |
|        5 |  591 | `			rc = SXERR_SYNTAX;` |
|        2 |  592 | `		}` |
|        5 |  593 | `		goto Synchronize;` |
|        - |  594 | `	}` |
|      186 |  595 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  596 | `	/* Skip optional return type declaration ': [?] type' */` |
|      186 |  597 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  598 | `		pIn++; /* Skip ':' */` |
|        - |  599 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  600 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  601 | `			pIn++;` |
|      ! 0 |  602 | `		}` |
|        - |  603 | `		/* Skip the type name (keyword or identifier) */` |
|        5 |  604 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  605 | `			pIn++;` |
|        2 |  606 | `		}` |
|        2 |  607 | `	}` |
|      186 |  608 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       30 |  609 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  610 | `		/* Check if we are dealing with a closure */` |
|       30 |  611 | `		if( nKey == PH7_TKWRD_USE ){` |
|       22 |  612 | `			pIn++; /* Jump the 'use' keyword */` |
|       22 |  613 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  614 | `				/* Syntax error */` |
|        5 |  615 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  616 | `				if( rc != SXERR_ABORT ){` |
|        5 |  617 | `					rc = SXERR_SYNTAX;` |
|        2 |  618 | `				}` |
|        5 |  619 | `				goto Synchronize;` |
|        - |  620 | `			}` |
|       18 |  621 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       18 |  622 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       18 |  623 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  624 | `				/* Syntax error */` |
|        5 |  625 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  626 | `				if( rc != SXERR_ABORT ){` |
|        5 |  627 | `					rc = SXERR_SYNTAX;` |
|        2 |  628 | `				}` |
|        5 |  629 | `				goto Synchronize;` |
|        - |  630 | `			}` |
|       14 |  631 | `			pIn++;` |
|        8 |  632 | `		}else{` |
|        - |  633 | `			/* Syntax error */` |
|        9 |  634 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  635 | `			if( rc != SXERR_ABORT ){` |
|        9 |  636 | `				rc = SXERR_SYNTAX;` |
|        4 |  637 | `			}` |
|        9 |  638 | `			goto Synchronize;` |
|        - |  639 | `		}` |
|        6 |  640 | `	}` |
|      170 |  641 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      170 |  642 | `		pIn++; /* Jump the leading curly '{' */` |
|      170 |  643 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      170 |  644 | `		if( pIn < pEnd ){` |
|      170 |  645 | `			pIn++;` |
|       84 |  646 | `		}` |
|       86 |  647 | `	}else{` |
|        - |  648 | `		/* Syntax error */` |
|      ! 0 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  650 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  651 | `			return SXERR_ABORT;` |
|        - |  652 | `		}` |
|        - |  653 | `	}` |
|      170 |  654 | `	rc = SXRET_OK;` |
|       96 |  655 | `Synchronize:` |
|        - |  656 | `	/* Synchronize pointers */` |
|      194 |  657 | `	*ppCur = pIn;` |
|      194 |  658 | `	return rc;` |
|       98 |  659 |  |
|        - |  660 | `/*` |
|        - |  661 | ` * Extract a single expression node from the input.` |
|        - |  662 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  663 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  664 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  665 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  666 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  667 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  668 | ` */` |
|  2813666 |  669 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  670 |  |
|        - |  671 | `	ph7_expr_node *pNode;` |
|        - |  672 | `	SyToken *pCur;` |
|        - |  673 | `	sxi32 rc;` |
|        - |  674 | `	/* Allocate a new node */` |
|  2813668 |  675 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2813668 |  676 | `	if( pNode == 0 ){` |
|        - |  677 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  678 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  679 | `		 */` |
|      ! 0 |  680 | `		return SXERR_MEM;` |
|        - |  681 | `	}` |
|        - |  682 | `	/* Zero the structure */` |
|  2813668 |  683 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2813668 |  684 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  685 | `	/* Point to the head of the token stream */` |
|  2813668 |  686 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  687 | `	/* Start collecting tokens */` |
|  2813668 |  688 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  689 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  690 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       15 |  691 | `		pCur++;` |
|       15 |  692 | `		pGen->pIn = pCur;` |
|       15 |  693 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       15 |  694 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       15 |  695 | `		if( rc == SXRET_OK && *ppNode ){` |
|       15 |  696 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        7 |  697 | `		}` |
|       15 |  698 | `		return rc;` |
|        - |  699 | `	}` |
|  2813654 |  700 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  701 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  702 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  703 | `		 */` |
|      268 |  704 | `		pCur++; /* Skip the opening '[' */` |
|      268 |  705 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      268 |  706 | `		if( pCur < pGen->pEnd ){` |
|      268 |  707 | `			pCur++; /* Skip past the closing ']' */` |
|      135 |  708 | `		}else{` |
|      ! 0 |  709 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  710 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  711 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  712 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  713 | `			}` |
|      ! 0 |  714 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  715 | `			return rc;` |
|        - |  716 | `		}` |
|        - |  717 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  718 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  719 | `		 */` |
|      291 |  720 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  721 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  722 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  723 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  724 | `			}else{` |
|       19 |  725 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  726 | `			}` |
|       25 |  727 | `		}else{` |
|      222 |  728 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  729 | `		}` |
|  2813521 |  730 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  731 | `		/* Point to the instance that describe this operator */` |
|   632828 |  732 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  733 | `		/* Advance the stream cursor */` |
|   632828 |  734 | `		pCur++;` |
|  2496975 |  735 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  736 | `		/* Isolate variable */` |
|  1535350 |  737 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   767682 |  738 | `			pCur++; /* Variable variable */` |
|        2 |  739 | `		}` |
|   767670 |  740 | `		if( pCur < pGen->pEnd ){` |
|   767670 |  741 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  742 | `				/* Variable name */` |
|   767642 |  743 | `				pCur++;` |
|   383850 |  744 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  745 | `				pCur++;` |
|        - |  746 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  747 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  748 | `				if( pCur < pGen->pEnd ){` |
|       18 |  749 | `					pCur++;` |
|       10 |  750 | `				}else{` |
|        5 |  751 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  752 | `					if( rc != SXERR_ABORT ){` |
|        5 |  753 | `						rc = SXERR_SYNTAX;` |
|        2 |  754 | `					}` |
|        5 |  755 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  756 | `					return rc;` |
|        - |  757 | `				}` |
|        8 |  758 | `			}` |
|   383832 |  759 | `		}` |
|   767666 |  760 | `		pNode->xCode = PH7_CompileVariable;` |
|  1796726 |  761 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    34132 |  762 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    34132 |  763 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  764 | `			 /* List/Array node */` |
|    22810 |  765 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  766 | `				 /* Assume a literal */` |
|       17 |  767 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  768 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  769 | `			 }else{` |
|    22794 |  770 | `				 pCur += 2;` |
|        - |  771 | `				 /* Collect array/list tokens */` |
|    22794 |  772 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    22794 |  773 | `				 if( pCur < pGen->pEnd ){` |
|    22792 |  774 | `					 pCur++;` |
|    11397 |  775 | `				 }else{` |
|        - |  776 | `					 /* Syntax error */` |
|        4 |  777 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  778 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  779 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  780 | `						 rc = SXERR_SYNTAX;` |
|        1 |  781 | `					 }` |
|        3 |  782 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  783 | `					 return rc;` |
|        - |  784 | `				 }` |
|    22792 |  785 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    22792 |  786 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  787 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  788 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  789 | `						 /* Syntax error */` |
|        3 |  790 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  791 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  792 | `							 rc = SXERR_SYNTAX;` |
|        1 |  793 | `						 }` |
|        3 |  794 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  795 | `						 return rc;` |
|        - |  796 | `					 }` |
|       12 |  797 | `				 }` |
|        2 |  798 | `			 }` |
|    22726 |  799 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  800 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  801 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  802 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  803 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  804 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  805 | `			 pNode->xCode = PH7_CompileYield;` |
|    11308 |  806 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  807 | `			 /* Annonymous function */` |
|      194 |  808 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  809 | `				 /* Assume a literal */` |
|      ! 0 |  810 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  811 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  812 | `			 }else{` |
|        - |  813 | `				 /* Assemble annonymous functions body */` |
|      194 |  814 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      194 |  815 | `				 if( rc != SXRET_OK ){` |
|       25 |  816 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  817 | `					 return rc;` |
|        - |  818 | `				 }` |
|      170 |  819 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  820 | `			  }` |
|    11184 |  821 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  822 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 |  823 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 |  824 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 |  825 | `		 }else{` |
|        - |  826 | `			 /* Assume a literal */` |
|    11022 |  827 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11022 |  828 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  829 | `		 }` |
|  1395815 |  830 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  831 | `		 /* Constants,function name,namespace path,class name... */` |
|   503920 |  832 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   503920 |  833 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   251961 |  834 | `	 }else{` |
|   874846 |  835 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  836 | `			 /* Point to the code generator routine */` |
|   158970 |  837 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   158970 |  838 | `			 if( pNode->xCode == 0 ){` |
|        3 |  839 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  840 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  841 | `					 rc = SXERR_SYNTAX;` |
|        1 |  842 | `				 }` |
|        3 |  843 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  844 | `				 return rc;` |
|        - |  845 | `			 }` |
|    79483 |  846 | `		 }` |
|        - |  847 | `		/* Advance the stream cursor */` |
|   874844 |  848 | `		pCur++;` |
|        - |  849 | `	 }` |
|        - |  850 | `	/* Point to the end of the token stream */` |
|  2813620 |  851 | `	pNode->pEnd = pCur;` |
|        - |  852 | `	/* Save the node for later processing */` |
|  2813620 |  853 | `	*ppNode = pNode;` |
|        - |  854 | `	/* Synchronize cursors */` |
|  2813620 |  855 | `	pGen->pIn = pCur;` |
|  2813620 |  856 | `	return SXRET_OK;` |
|  1406835 |  857 |  |
|        - |  858 | `/*` |
|        - |  859 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  860 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  861 | ` * level is zero.` |
|        - |  862 | ` */` |
|    68290 |  863 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  864 |  |
|    68292 |  865 | `	SyToken *pCur = pStart;` |
|    68292 |  866 | `	sxi32 iNest = 0;` |
|    68292 |  867 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  868 | `		/* Last expression */` |
|    36452 |  869 | `		return SXERR_EOF;` |
|        - |  870 | `	}` |
|   128390 |  871 | `	while( pCur < pEnd ){` |
|   116180 |  872 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    19632 |  873 | `			break;` |
|        - |  874 | `		}` |
|    96550 |  875 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5422 |  876 | `			iNest++;` |
|    93840 |  877 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5424 |  878 | `			iNest--;` |
|     2711 |  879 | `		}` |
|    96550 |  880 | `		pCur++;` |
|        2 |  881 | `	}` |
|    31842 |  882 | `	*ppNext = pCur;` |
|    31842 |  883 | `	return SXRET_OK;` |
|    34147 |  884 |  |
|        - |  885 | `/*` |
|        - |  886 | ` * Free an expression tree.` |
|        - |  887 | ` */` |
|  2407722 |  888 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  889 |  |
|  2407724 |  890 | `	if( pNode->pLeft ){` |
|        - |  891 | `		/* Release the left tree */` |
|   898116 |  892 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   449057 |  893 | `	}` |
|  2407724 |  894 | `	if( pNode->pRight ){` |
|        - |  895 | `		/* Release the right tree */` |
|   470078 |  896 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   235038 |  897 | `	}` |
|  2407724 |  898 | `	if( pNode->pCond ){` |
|        - |  899 | `		/* Release the conditional tree used by the ternary operator */` |
|     1884 |  900 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      941 |  901 | `	}` |
|  2407724 |  902 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  903 | `		ph7_expr_node **apArg;` |
|        - |  904 | `		sxu32 n;` |
|        - |  905 | `		/* Release node arguments */` |
|   319154 |  906 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   673868 |  907 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   354716 |  908 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   177359 |  909 | `		}` |
|   319154 |  910 | `		SySetRelease(&pNode->aNodeArgs);` |
|   159576 |  911 | `	}` |
|        - |  912 | `	/* Finally,release this node */` |
|  2407724 |  913 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2407724 |  914 |  |
|        - |  915 | `/*` |
|        - |  916 | ` * Free an expression tree.` |
|        - |  917 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  918 | ` */` |
|   638242 |  919 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  920 |  |
|        - |  921 | `	ph7_expr_node **apNode;` |
|        - |  922 | `	sxu32 n;` |
|   638244 |  923 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3451862 |  924 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2813620 |  925 | `		if( apNode[n] ){` |
|   638530 |  926 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   319264 |  927 | `		}` |
|  1406811 |  928 | `	}` |
|   638244 |  929 | `	return SXRET_OK;` |
|        2 |  930 |  |
|        - |  931 | `/*` |
|        - |  932 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  933 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  934 | ` */` |
|   204400 |  935 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  936 |  |
|        - |  937 | `	sxi32 iExprOp;` |
|   204402 |  938 | `	if( pNode->pOp == 0 ){` |
|   133064 |  939 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  940 | `	}` |
|    71340 |  941 | `	iExprOp = pNode->pOp->iOp;` |
|    71340 |  942 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    44736 |  943 | `			return TRUE;` |
|        - |  944 | `	}` |
|    26606 |  945 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    26602 |  946 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  947 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  948 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  949 | `				return FALSE;` |
|        1 |  950 | `			}` |
|    26599 |  951 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  952 | `			return FALSE;` |
|        - |  953 | `		}` |
|    26602 |  954 | `		return TRUE;` |
|        - |  955 | `	}` |
|        5 |  956 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  957 | `		return TRUE;` |
|        - |  958 | `	}` |
|        - |  959 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  960 | `	return FALSE;` |
|   102202 |  961 |  |
|        - |  962 | `/* Forward declaration */` |
|        - |  963 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  964 | `/* Macro to check if the given node is a terminal.` |
|        - |  965 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  966 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  967 | ` * linked ternary/elvis node). */` |
|        - |  968 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  969 | `/*` |
|        - |  970 | ` * Buid an expression tree for each given function argument.` |
|        - |  971 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  972 | ` */` |
|   264934 |  973 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  974 |  |
|        - |  975 | `	sxi32 iNest,iCur,iNode;` |
|        - |  976 | `	sxi32 rc;` |
|        - |  977 | `	/* Process function arguments from left to right */` |
|   264936 |  978 | `	iCur = 0;` |
|   282714 |  979 | `	for(;;){` |
|   565430 |  980 | `		if( iCur >= nToken ){` |
|        - |  981 | `			/* No more arguments to process */` |
|   264934 |  982 | `			break;` |
|        - |  983 | `		}` |
|   300498 |  984 | `		iNode = iCur;` |
|   300498 |  985 | `		iNest = 0;` |
|   751518 |  986 | `		while( iCur < nToken ){` |
|   486586 |  987 | `			if( apNode[iCur] ){` |
|   476082 |  988 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    17784 |  989 | `					break;` |
|   440518 |  990 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    24418 |  991 | `					iNest++;` |
|   428310 |  992 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    24404 |  993 | `					iNest--;` |
|    12201 |  994 | `				}` |
|   220258 |  995 | `			}` |
|   451022 |  996 | `			iCur++;` |
|        2 |  997 | `		}` |
|   300498 |  998 | `		if( iCur > iNode ){` |
|   300494 |  999 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1000 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1001 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1002 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1003 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1004 | `					apNode[iNode] = 0;` |
|      ! 0 | 1005 | `			}` |
|   300496 | 1006 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   300496 | 1007 | `			if( apNode[iNode] ){` |
|        - | 1008 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   300496 | 1009 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   150249 | 1010 | `			}else{` |
|        - | 1011 | `				/* Empty function argument */` |
|      ! 0 | 1012 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 | 1013 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1014 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1015 | `				}` |
|      ! 0 | 1016 | `				return rc;` |
|        - | 1017 | `			}` |
|   150249 | 1018 | `		}else{` |
|        3 | 1019 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 | 1020 | `			if( rc != SXERR_ABORT ){` |
|        3 | 1021 | `				rc = SXERR_SYNTAX;` |
|        1 | 1022 | `			}` |
|        3 | 1023 | `			return rc;` |
|        - | 1024 | `		}` |
|        - | 1025 | `		/* Jump trailing comma */` |
|   300496 | 1026 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    35564 | 1027 | `			iCur++;` |
|    35564 | 1028 | `			if( iCur >= nToken ){` |
|        - | 1029 | `				/* missing function argument */` |
|      ! 0 | 1030 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 | 1031 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1032 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1033 | `				}` |
|      ! 0 | 1034 | `				return rc;` |
|        - | 1035 | `			}` |
|    17781 | 1036 | `		}` |
|        2 | 1037 | `	}` |
|   264934 | 1038 | `	return SXRET_OK;` |
|   132469 | 1039 |  |
|        - | 1040 | ` /*` |
|        - | 1041 | `  * Create an expression tree from an array of tokens.` |
|        - | 1042 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1043 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1044 | `  */` |
|  1021610 | 1045 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1046 | ` {` |
|        - | 1047 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1048 | `	 ph7_expr_node *pNode;` |
|        - | 1049 | `	 sxi32 iCur;` |
|        - | 1050 | `	 sxi32 rc;` |
|  1021612 | 1051 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1052 | `		 /* TICKET 1433-17: self evaluating node */` |
|   471564 | 1053 | `		 return SXRET_OK;` |
|        - | 1054 | `	 }` |
|        - | 1055 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3379290 | 1056 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1057 | `		 sxi32 iNest;` |
|        - | 1058 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1059 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1060 | `		  */` |
|  2829244 | 1061 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2807038 | 1062 | `			 continue;` |
|        - | 1063 | `		 }` |
|    22208 | 1064 | `		 iNest = 1;` |
|    22208 | 1065 | `		 iLeft = iCur;` |
|        - | 1066 | `		 /* Find the closing parenthesis */` |
|    22208 | 1067 | `		 iCur++;` |
|   147780 | 1068 | `		 while( iCur < nToken ){` |
|   147780 | 1069 | `			 if( apNode[iCur] ){` |
|   147780 | 1070 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1071 | `					 /* Decrement nesting level */` |
|    38520 | 1072 | `					 iNest--;` |
|    38520 | 1073 | `					 if( iNest <= 0 ){` |
|    22208 | 1074 | `						 break;` |
|        2 | 1075 | `					 }` |
|   117418 | 1076 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1077 | `					 /* Increment nesting level */` |
|    16314 | 1078 | `					 iNest++;` |
|     8156 | 1079 | `				 }` |
|    62786 | 1080 | `			 }` |
|   125574 | 1081 | `			 iCur++;` |
|        2 | 1082 | `		 }` |
|    22208 | 1083 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1084 | `			 /* Recurse and process this expression */` |
|    22208 | 1085 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    22208 | 1086 | `			 if( rc != SXRET_OK ){` |
|        3 | 1087 | `				 return rc;` |
|        - | 1088 | `			 }` |
|    11102 | 1089 | `		 }` |
|        - | 1090 | `		 /* Free the left and right nodes */` |
|    22206 | 1091 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    22206 | 1092 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    22206 | 1093 | `		 apNode[iLeft] = 0;` |
|    22206 | 1094 | `		 apNode[iCur] = 0;` |
|    11104 | 1095 | `	 }` |
|        - | 1096 | `	  /* Process expressions enclosed in braces */` |
|  3521384 | 1097 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1098 | `		 sxi32 iNest;` |
|        - | 1099 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1100 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1101 | `		  */` |
|  2977010 | 1102 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  2977010 | 1103 | `			 continue;` |
|        - | 1104 | `		 }` |
|      ! 0 | 1105 | `		 iNest = 1;` |
|      ! 0 | 1106 | `		 iLeft = iCur;` |
|        - | 1107 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1108 | `		 iCur++;` |
|      ! 0 | 1109 | `		 while( iCur < nToken ){` |
|      ! 0 | 1110 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1111 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1112 | `					 /* Decrement nesting level */` |
|      ! 0 | 1113 | `					 iNest--;` |
|      ! 0 | 1114 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1115 | `						 break;` |
|      ! 0 | 1116 | `					 }` |
|      ! 0 | 1117 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1118 | `					 /* Increment nesting level */` |
|      ! 0 | 1119 | `					 iNest++;` |
|      ! 0 | 1120 | `				 }` |
|      ! 0 | 1121 | `			 }` |
|      ! 0 | 1122 | `			 iCur++;` |
|      ! 0 | 1123 | `		 }` |
|      ! 0 | 1124 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1125 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1126 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1127 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1128 | `				 return rc;` |
|        - | 1129 | `			 }` |
|      ! 0 | 1130 | `		 }` |
|        - | 1131 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1132 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1133 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1134 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1135 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1136 | `	 }` |
|        - | 1137 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   544376 | 1138 | `	 iLeft = -1;` |
|  3521372 | 1139 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2977004 | 1140 | `		 if( apNode[iCur] == 0 ){` |
|  1158500 | 1141 | `			 continue;` |
|        - | 1142 | `		 }` |
|  1818506 | 1143 | `		 pNode = apNode[iCur];` |
|  1818506 | 1144 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   469252 | 1145 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1146 | `				 /* Collect function arguments */` |
|   301046 | 1147 | `				 sxi32 iPtr = 0;` |
|   301046 | 1148 | `				 sxi32 nFuncTok = 0;` |
|  1088676 | 1149 | `				 while( nFuncTok + iCur < nToken ){` |
|  1088676 | 1150 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1078172 | 1151 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   312012 | 1152 | `							 iPtr++;` |
|   922167 | 1153 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   312012 | 1154 | `							 iPtr--;` |
|   312012 | 1155 | `							 if( iPtr <= 0 ){` |
|   301046 | 1156 | `								 break;` |
|        - | 1157 | `							 }` |
|     5483 | 1158 | `						 }` |
|   388563 | 1159 | `					 }` |
|   787632 | 1160 | `					 nFuncTok++;` |
|        2 | 1161 | `				 }` |
|   301046 | 1162 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1163 | `					 /* Syntax error */` |
|      ! 0 | 1164 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1165 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1166 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1167 | `					 }` |
|      ! 0 | 1168 | `					 return rc;` |
|        - | 1169 | `				 }` |
|   301046 | 1170 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1171 | `					 /* Syntax error */` |
|      ! 0 | 1172 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1173 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1174 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1175 | `					 }` |
|      ! 0 | 1176 | `					 return rc;` |
|        - | 1177 | `				 }` |
|   301046 | 1178 | `				 if( nFuncTok > 1 ){` |
|        - | 1179 | `					 /* Process function arguments */` |
|   264936 | 1180 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   264936 | 1181 | `					 if( rc != SXRET_OK ){` |
|        3 | 1182 | `						 return rc;` |
|        - | 1183 | `					 }` |
|   132466 | 1184 | `				 }` |
|        - | 1185 | `				 /* Link the node to the tree */` |
|   301044 | 1186 | `				 pNode->pLeft = apNode[iLeft];` |
|   301044 | 1187 | `				 apNode[iLeft] = 0;` |
|  1088668 | 1188 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   787626 | 1189 | `					 apNode[iCur+iPtr] = 0;` |
|   393814 | 1190 | `				 }` |
|   318729 | 1191 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1192 | `				 /* Subscripting */` |
|    67412 | 1193 | `				 sxi32 iArrTok = iCur + 1;` |
|    67412 | 1194 | `				 sxi32 iNest = 1;` |
|    67479 | 1195 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1196 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1197 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    67410 | 1198 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1199 | `						 /* Syntax error */` |
|      ! 0 | 1200 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1201 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1202 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1203 | `						 }` |
|      ! 0 | 1204 | `						 return rc;` |
|        - | 1205 | `				 }` |
|        - | 1206 | `				 /* Collect index tokens */` |
|   121742 | 1207 | `				 while( iArrTok < nToken ){` |
|   121742 | 1208 | `					 if( apNode[iArrTok] ){` |
|   121710 | 1209 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1210 | `							 /* Increment nesting level */` |
|      ! 0 | 1211 | `							 iNest++;` |
|   121710 | 1212 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1213 | `							 /* Decrement nesting level */` |
|    67412 | 1214 | `							 iNest--;` |
|    67412 | 1215 | `							 if( iNest <= 0 ){` |
|    67412 | 1216 | `								 break;` |
|        - | 1217 | `							 }` |
|      ! 0 | 1218 | `						 }` |
|    27149 | 1219 | `					 }` |
|    54332 | 1220 | `					 ++iArrTok;` |
|        2 | 1221 | `				 }` |
|    67412 | 1222 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1223 | `					 /* Recurse and process this expression */` |
|    54222 | 1224 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    54222 | 1225 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1226 | `						 return rc;` |
|        - | 1227 | `					 }` |
|        - | 1228 | `					 /* Link the node to it's index */` |
|    54222 | 1229 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    27110 | 1230 | `				 }` |
|        - | 1231 | `				 /* Link the node to the tree */` |
|    67412 | 1232 | `				 pNode->pLeft = apNode[iLeft];` |
|    67412 | 1233 | `				 pNode->pRight = 0;` |
|    67412 | 1234 | `				 apNode[iLeft] = 0;` |
|   189152 | 1235 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   121742 | 1236 | `					 apNode[iNest] = 0;` |
|    60872 | 1237 | `				 }` |
|    33707 | 1238 | `			 }else{` |
|        - | 1239 | `				 /* Member access operators [i.e: '->','::'] */` |
|   100798 | 1240 | `				  iRight = iCur + 1;` |
|   100798 | 1241 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1242 | `					 iRight++;` |
|      ! 0 | 1243 | `				 }` |
|   100798 | 1244 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1245 | `					 /* Syntax error */` |
|        5 | 1246 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1247 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1248 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1249 | `					 }` |
|        5 | 1250 | `					 return rc;` |
|        - | 1251 | `				 }` |
|        - | 1252 | `				 /* Link the node to the tree */` |
|   100794 | 1253 | `				 pNode->pLeft = apNode[iLeft];` |
|   100794 | 1254 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   100576 | 1255 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1256 | `						 /* Syntax error */` |
|      ! 0 | 1257 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1258 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1259 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1260 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1261 | `						 }` |
|      ! 0 | 1262 | `						 return rc;` |
|        - | 1263 | `				 }` |
|   100794 | 1264 | `				 pNode->pRight = apNode[iRight];` |
|   100794 | 1265 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1266 | `			 }` |
|   234622 | 1267 | `		 }` |
|  1818500 | 1268 | `		 iLeft = iCur;` |
|   909251 | 1269 | `	 }` |
|        - | 1270 | `	 /* Handle left associative (new, clone) operators */` |
|  3521352 | 1271 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2976984 | 1272 | `		 if( apNode[iCur] == 0 ){` |
|  1641332 | 1273 | `			 continue;` |
|        - | 1274 | `		 }` |
|  1335654 | 1275 | `		 pNode = apNode[iCur];` |
|  1335654 | 1276 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1277 | `			 SyToken *pToken;` |
|        - | 1278 | `			 /* Get the left node */` |
|    13590 | 1279 | `			 iLeft = iCur + 1;` |
|    27148 | 1280 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13560 | 1281 | `				 iLeft++;` |
|        2 | 1282 | `			 }` |
|    13590 | 1283 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1284 | `				  /* Syntax error */` |
|      ! 0 | 1285 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1286 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1287 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1288 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1289 | `				 }` |
|      ! 0 | 1290 | `				 return rc;` |
|        - | 1291 | `			 }` |
|        - | 1292 | `			 /* Make sure the operand are of a valid type */` |
|    13590 | 1293 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1294 | `				 /* Clone:` |
|        - | 1295 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1296 | `				  *  ++ function call (including annonymous)` |
|        - | 1297 | `				  *  ++ array member` |
|        - | 1298 | `				  *  ++ 'new' operator` |
|        - | 1299 | `				  * Example:` |
|        - | 1300 | `				  *   clone $pObj;` |
|        - | 1301 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1302 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1303 | `				  */` |
|       18 | 1304 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1305 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1306 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1307 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1308 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1309 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1310 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1311 | `						 }` |
|      ! 0 | 1312 | `						 return rc;` |
|        - | 1313 | `					 }` |
|        7 | 1314 | `				 }` |
|       10 | 1315 | `			 }else{` |
|        - | 1316 | `				 /* New */` |
|    13574 | 1317 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1318 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1319 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1320 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1321 | `						 /* Syntax error */` |
|      ! 0 | 1322 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1323 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1324 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1325 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1326 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1327 | `						 }` |
|      ! 0 | 1328 | `						 return rc;` |
|        - | 1329 | `					 }` |
|        8 | 1330 | `				 }` |
|        - | 1331 | `			 }` |
|        - | 1332 | `			  /* Link the node to the tree */` |
|    13590 | 1333 | `			 pNode->pLeft = apNode[iLeft];` |
|    13590 | 1334 | `			 apNode[iLeft] = 0;` |
|    13590 | 1335 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6794 | 1336 | `		 }` |
|   667828 | 1337 | `	 }` |
|        - | 1338 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   544370 | 1339 | `	 iLeft = -1;` |
|  3524188 | 1340 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2976984 | 1341 | `		 if( apNode[iCur] == 0 ){` |
|  1641332 | 1342 | `			 continue;` |
|        - | 1343 | `		 }` |
|  1335654 | 1344 | `		 pNode = apNode[iCur];` |
|  1335654 | 1345 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7904 | 1346 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2854 | 1347 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1348 | `					 /* Link the node to the tree */` |
|     2856 | 1349 | `					 pNode->pLeft = apNode[iLeft];` |
|     2856 | 1350 | `					 apNode[iLeft] = 0;` |
|     1427 | 1351 | `			 }` |
|     5369 | 1352 | `		  }` |
|  1338490 | 1353 | `		 iLeft = iCur;` |
|   670664 | 1354 | `	  }` |
|   547206 | 1355 | `	 iLeft = -1;` |
|  3524188 | 1356 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2976984 | 1357 | `		 if( apNode[iCur] == 0 ){` |
|  1644186 | 1358 | `			 continue;` |
|        - | 1359 | `		 }` |
|  1332800 | 1360 | `		 pNode = apNode[iCur];` |
|  1332800 | 1361 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7884 | 1362 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     7886 | 1363 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1364 | `					 /* Syntax error */` |
|      ! 0 | 1365 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1366 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1367 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1368 | `					 }` |
|      ! 0 | 1369 | `					 return rc;` |
|        - | 1370 | `			 }` |
|        - | 1371 | `			 /* Link the node to the tree */` |
|     7886 | 1372 | `			 pNode->pLeft = apNode[iLeft];` |
|     7886 | 1373 | `			 apNode[iLeft] = 0;` |
|        - | 1374 | `			 /* Mark as pre-increment/decrement node */` |
|     7886 | 1375 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3942 | 1376 | `		  }` |
|  1332800 | 1377 | `		 iLeft = iCur;` |
|   666401 | 1378 | `	 }` |
|        - | 1379 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   547206 | 1380 | `	  iLeft = 0;` |
|  3524182 | 1381 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2976980 | 1382 | `		  if( apNode[iCur] ){` |
|  1324912 | 1383 | `			  pNode = apNode[iCur];` |
|  1324912 | 1384 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    35332 | 1385 | `				  if( iLeft > 0 ){` |
|        - | 1386 | `					  /* Link the node to the tree */` |
|    35330 | 1387 | `					  pNode->pLeft = apNode[iLeft];` |
|    35330 | 1388 | `					  apNode[iLeft] = 0;` |
|    35330 | 1389 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|        9 | 1390 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1391 | `							   /* Syntax error */` |
|      ! 0 | 1392 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1393 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1394 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1395 | `							  }` |
|      ! 0 | 1396 | `							  return rc;` |
|        - | 1397 | `						  }` |
|        4 | 1398 | `					  }` |
|    17666 | 1399 | `				  }else{` |
|        - | 1400 | `					  /* Syntax error */` |
|        3 | 1401 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1402 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1403 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1404 | `					  }` |
|        3 | 1405 | `					  return rc;` |
|        - | 1406 | `				  }` |
|    17664 | 1407 | `			  }` |
|        - | 1408 | `			  /* Save terminal position */` |
|  1324910 | 1409 | `			  iLeft = iCur;` |
|   662454 | 1410 | `		  }` |
|  1488490 | 1411 | `	  }` |
|        - | 1412 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6019148 | 1413 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5471954 | 1414 | `		 iLeft = -1;` |
| 35241468 | 1415 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 29769524 | 1416 | `			 if( apNode[iCur] == 0 ){` |
| 18999174 | 1417 | `				 continue;` |
|        - | 1418 | `			 }` |
| 10770352 | 1419 | `			 pNode = apNode[iCur];` |
| 10770352 | 1420 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1421 | `				 /* Get the right node */` |
|   163018 | 1422 | `				 iRight = iCur + 1;` |
|   231564 | 1423 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    68548 | 1424 | `					 iRight++;` |
|        2 | 1425 | `				 }` |
|   163018 | 1426 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1427 | `					 /* Syntax error */` |
|        9 | 1428 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1429 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1430 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1431 | `					 }` |
|        9 | 1432 | `					 return rc;` |
|        - | 1433 | `				 }` |
|   163010 | 1434 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1435 | `					 sxi32  iTmp;` |
|        - | 1436 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1437 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1438 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1439 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1440 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1441 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1442 | `						 }` |
|      ! 0 | 1443 | `						 return rc;` |
|        - | 1444 | `					 }` |
|       46 | 1445 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1446 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1447 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1448 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1449 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1450 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1451 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1452 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1453 | `									 }` |
|      ! 0 | 1454 | `									 return rc;` |
|        - | 1455 | `							 }` |
|      ! 0 | 1456 | `						 }` |
|       15 | 1457 | `					 }` |
|        - | 1458 | `					 /* Swap operands */` |
|       46 | 1459 | `					 iTmp = iRight;` |
|       46 | 1460 | `					 iRight = iLeft;` |
|       46 | 1461 | `					 iLeft = iTmp;` |
|       22 | 1462 | `				 }` |
|        - | 1463 | `				 /* Link the node to the tree */` |
|   163010 | 1464 | `				 pNode->pLeft = apNode[iLeft];` |
|   163010 | 1465 | `				 pNode->pRight = apNode[iRight];` |
|   163010 | 1466 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    81504 | 1467 | `			 }` |
| 10770344 | 1468 | `			 iLeft = iCur;` |
|  5385173 | 1469 | `		 }` |
|  2735974 | 1470 | `	 }` |
|        - | 1471 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1472 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1473 | `	  * we are dealing with a single operator.` |
|        - | 1474 | `	  */` |
|   547196 | 1475 | `	  iLeft = -1;` |
|  3516088 | 1476 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2970778 | 1477 | `		  if( apNode[iCur] == 0 ){` |
|  2012844 | 1478 | `			  continue;` |
|        - | 1479 | `		  }` |
|   957936 | 1480 | `		  pNode = apNode[iCur];` |
|   957936 | 1481 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1886 | 1482 | `			  sxi32 iNest = 1;` |
|     1886 | 1483 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1484 | `				  /* Missing condition */` |
|        3 | 1485 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1486 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1487 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1488 | `				  }` |
|        3 | 1489 | `				  return rc;` |
|        - | 1490 | `			  }` |
|        - | 1491 | `			  /* Get the right node */` |
|     1884 | 1492 | `			  iRight = iCur + 1;` |
|     3990 | 1493 | `			  while( iRight < nToken  ){` |
|     3990 | 1494 | `				  if( apNode[iRight] ){` |
|     3698 | 1495 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1496 | `						  /* Increment nesting level */` |
|      ! 0 | 1497 | `						  ++iNest;` |
|     3698 | 1498 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1499 | `						  /* Decrement nesting level */` |
|     1884 | 1500 | `						  --iNest;` |
|     1884 | 1501 | `						  if( iNest <= 0 ){` |
|     1884 | 1502 | `							  break;` |
|        - | 1503 | `						  }` |
|      ! 0 | 1504 | `					  }` |
|      907 | 1505 | `				  }` |
|     2108 | 1506 | `				  iRight++;` |
|        2 | 1507 | `			  }` |
|     1884 | 1508 | `			  if( iRight > iCur + 1 ){` |
|        - | 1509 | `				  /* Recurse and process the then expression */` |
|     1816 | 1510 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1816 | 1511 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1512 | `					  return rc;` |
|        - | 1513 | `				  }` |
|        - | 1514 | `				  /* Link the node to the tree */` |
|     1816 | 1515 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      907 | 1516 | `			  }else{` |
|        - | 1517 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1518 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1519 | `			  }` |
|     1884 | 1520 | `			  apNode[iCur + 1] = 0;` |
|     1884 | 1521 | `			  if( iRight + 1 < nToken ){` |
|        - | 1522 | `				  /* Recurse and process the else expression */` |
|     1884 | 1523 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1884 | 1524 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1525 | `					  return rc;` |
|        - | 1526 | `				  }` |
|        - | 1527 | `				  /* Link the node to the tree */` |
|     1884 | 1528 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1884 | 1529 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      943 | 1530 | `			  }else{` |
|      ! 0 | 1531 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1532 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1533 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1534 | `				 }` |
|      ! 0 | 1535 | `				 return rc;` |
|        - | 1536 | `			  }` |
|        - | 1537 | `			  /* Point to the condition */` |
|     1884 | 1538 | `			  pNode->pCond  = apNode[iLeft];` |
|     1884 | 1539 | `			  apNode[iLeft] = 0;` |
|     1884 | 1540 | `			  break;` |
|        - | 1541 | `		  }` |
|   956052 | 1542 | `		  iLeft = iCur;` |
|   478027 | 1543 | `	  }` |
|        - | 1544 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1545 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1546 | `	  * so there is no need for a precedence loop here.` |
|        - | 1547 | `	  */` |
|   547194 | 1548 | `	 iRight = -1;` |
|  3524048 | 1549 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  2976896 | 1550 | `		 if( apNode[iCur] == 0 ){` |
|  2225216 | 1551 | `			 continue;` |
|        - | 1552 | `		 }` |
|   751682 | 1553 | `		 pNode = apNode[iCur];` |
|   751682 | 1554 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1555 | `			 /* Get the left node */` |
|   204366 | 1556 | `			 iLeft = iCur - 1;` |
|   289170 | 1557 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    84806 | 1558 | `				 iLeft--;` |
|        2 | 1559 | `			 }` |
|   204366 | 1560 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1561 | `				 /* Syntax error */` |
|       39 | 1562 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1563 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1564 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1565 | `				 }` |
|       39 | 1566 | `				 return rc;` |
|        - | 1567 | `			 }` |
|   204328 | 1568 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       71 | 1569 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1570 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1571 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1572 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1573 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1574 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1575 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1576 | `					 }` |
|        3 | 1577 | `					 return rc;` |
|        - | 1578 | `				 }` |
|       26 | 1579 | `			 }` |
|        - | 1580 | `			 /* Link the node to the tree (Reverse) */` |
|   204326 | 1581 | `			 pNode->pLeft = apNode[iRight];` |
|   204326 | 1582 | `			 pNode->pRight = apNode[iLeft];` |
|   204326 | 1583 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   102162 | 1584 | `		 }` |
|   751642 | 1585 | `		 iRight = iCur;` |
|   375822 | 1586 | `	 }` |
|        - | 1587 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2735762 | 1588 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2188610 | 1589 | `		 iLeft = -1;` |
| 14096018 | 1590 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 11907410 | 1591 | `			 if( apNode[iCur] == 0 ){` |
|  9718396 | 1592 | `				 continue;` |
|        - | 1593 | `			 }` |
|  2189016 | 1594 | `			 pNode = apNode[iCur];` |
|  2189016 | 1595 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1596 | `				 /* Get the right node */` |
|       72 | 1597 | `				 iRight = iCur + 1;` |
|      110 | 1598 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1599 | `					 iRight++;` |
|        2 | 1600 | `				 }` |
|       72 | 1601 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1602 | `					 /* Syntax error */` |
|      ! 0 | 1603 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1604 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1605 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1606 | `					 }` |
|      ! 0 | 1607 | `					 return rc;` |
|        - | 1608 | `				 }` |
|        - | 1609 | `				 /* Link the node to the tree */` |
|       72 | 1610 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1611 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1612 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1613 | `			 }` |
|  2189016 | 1614 | `			 iLeft = iCur;` |
|  1094509 | 1615 | `		 }` |
|  1094306 | 1616 | `	 }` |
|        - | 1617 | `	 /* Point to the root of the expression tree */` |
|  2976826 | 1618 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2429692 | 1619 | `		 if( apNode[iCur] ){` |
|   493858 | 1620 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1621 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1622 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1623 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1624 | `				  }` |
|       20 | 1625 | `				  return rc;` |
|        - | 1626 | `			 }` |
|   493840 | 1627 | `			 apNode[0] = apNode[iCur];` |
|   493840 | 1628 | `			 apNode[iCur] = 0;` |
|   246919 | 1629 | `		 }` |
|  1214838 | 1630 | `	 }` |
|   547136 | 1631 | `	 return SXRET_OK;` |
|   509389 | 1632 | ` }` |
|        - | 1633 | ` /*` |
|        - | 1634 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1635 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1636 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1637 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1638 | `  */` |
|   638242 | 1639 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1640 |  |
|        - | 1641 | `	ph7_expr_node **apNode;` |
|        - | 1642 | `	ph7_expr_node *pNode;` |
|        - | 1643 | `	sxi32 rc;` |
|        - | 1644 | `	/* Reset node container */` |
|   638244 | 1645 | `	SySetReset(pExprNode);` |
|   638244 | 1646 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1647 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1648 | `	{` |
|   638244 | 1649 | `		int iLastWasTerm = 0;` |
|  3451862 | 1650 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2813654 | 1651 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2813654 | 1652 | `			if( rc != SXRET_OK ){` |
|       35 | 1653 | `				return rc;` |
|        - | 1654 | `			}` |
|        - | 1655 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2813620 | 1656 | `			if( pNode->xCode ){` |
|        - | 1657 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1464918 | 1658 | `				iLastWasTerm = 1;` |
|  2081162 | 1659 | `			}else if( pNode->pOp ){` |
|        - | 1660 | `				/* Operator node */` |
|   632828 | 1661 | `				iLastWasTerm = 0;` |
|   316415 | 1662 | `			}else{` |
|        - | 1663 | `				/* Delimiter: ')' and ']' end terms */` |
|   715878 | 1664 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1665 | `			}` |
|        - | 1666 | `			/* Save the extracted node */` |
|  2813620 | 1667 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1668 | `		}` |
|        - | 1669 | `	}` |
|   638210 | 1670 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1671 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1672 | `		*ppRoot = 0;` |
|      ! 0 | 1673 | `		return SXRET_OK;` |
|        - | 1674 | `	}` |
|   638210 | 1675 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1676 | `	/* Make sure we are dealing with valid nodes */` |
|   638210 | 1677 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   638210 | 1678 | `	if( rc != SXRET_OK ){` |
|        - | 1679 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1680 | `		 * cleanup the mess left behind.` |
|        - | 1681 | `		 */` |
|       51 | 1682 | `		*ppRoot = 0;` |
|       51 | 1683 | `		return rc;` |
|        - | 1684 | `	}` |
|        - | 1685 | `	/* Build the tree */` |
|   638160 | 1686 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   638160 | 1687 | `	if( rc != SXRET_OK ){` |
|        - | 1688 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       78 | 1689 | `		*ppRoot = 0;` |
|       78 | 1690 | `		return rc;` |
|        - | 1691 | `	}` |
|        - | 1692 | `	/* Point to the root of the tree */` |
|   638084 | 1693 | `	*ppRoot = apNode[0];` |
|   638084 | 1694 | `	return SXRET_OK;` |
|   319123 | 1695 |  |
|        - | 1696 |  |
