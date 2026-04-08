# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 834/984 lines (84.76%)

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
|   714782 |  258 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  259 |  |
|   714784 |  260 | `	sxu32 n = 0;` |
|        - |  261 | `	sxi32 rc;` |
|        - |  262 | `	/* Do a linear lookup on the operators table */` |
| 11683857 |  263 | `	for(;;){` |
| 23367716 |  264 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  265 | `			break;` |
|        - |  266 | `		}` |
| 23367716 |  267 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  268 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2850922 |  269 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1425462 |  270 | `		}else{` |
| 20516796 |  271 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  272 | `		}` |
| 23367716 |  273 | `		if( rc == 0 ){` |
|   717920 |  274 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  275 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   714454 |  276 | `				return &aOpTable[n];` |
|        - |  277 | `			}` |
|        - |  278 | `			/* Handle ambiguity */` |
|     3468 |  279 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  280 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  281 | `				return &aOpTable[n];` |
|        - |  282 | `			}` |
|     3248 |  283 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  284 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  285 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  286 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  287 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  288 | `					return &aOpTable[n];` |
|        - |  289 | `				}` |
|        - |  290 |  |
|        4 |  291 | `			}` |
|     1568 |  292 | `		}` |
| 22652934 |  293 | `		++n; /* Next operator in the table */` |
|        2 |  294 | `	}` |
|        - |  295 | `	/* No such operator */` |
|      ! 0 |  296 | `	return 0;` |
|   357393 |  297 |  |
|        - |  298 | `/*` |
|        - |  299 | ` * Delimit a set of token stream.` |
|        - |  300 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  301 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  302 | ` */` |
|   367870 |  303 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  304 |  |
|   367872 |  305 | `	SyToken *pCur = pIn;` |
|   367872 |  306 | `	sxi32 iNest = 1;` |
|  2090692 |  307 | `	for(;;){` |
|  4181386 |  308 | `		if( pCur >= pEnd ){` |
|      124 |  309 | `			break;` |
|        - |  310 | `		}` |
|  4181264 |  311 | `		if( pCur->nType & nTokStart ){` |
|        - |  312 | `			/* Increment nesting level */` |
|   231364 |  313 | `			iNest++;` |
|  4065583 |  314 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  315 | `			/* Decrement nesting level */` |
|   599112 |  316 | `			iNest--;` |
|   599112 |  317 | `			if( iNest <= 0 ){` |
|   367750 |  318 | `				break;` |
|        - |  319 | `			}` |
|   115681 |  320 | `		}` |
|        - |  321 | `		/* Advance cursor */` |
|  3813516 |  322 | `		pCur++;` |
|        2 |  323 | `	}` |
|        - |  324 | `	/* Point to the end of the chunk */` |
|   367872 |  325 | `	*ppEnd = pCur;` |
|   367872 |  326 |  |
|        - |  327 | `/*` |
|        - |  328 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  329 | ` * Note on reserved keywords.` |
|        - |  330 | ` *  According to the PHP language reference manual:` |
|        - |  331 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  332 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  333 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  334 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  335 | ` */` |
|    11054 |  336 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  337 |  |
|    16517 |  338 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    10965 |  339 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  340 | `		){` |
|      142 |  341 | `			return TRUE;` |
|        - |  342 | `	}` |
|    10916 |  343 | `	if( bCheckFunc ){` |
|       92 |  344 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  345 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  346 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  347 | `				return TRUE;` |
|        - |  348 | `		}` |
|       20 |  349 | `	}` |
|        - |  350 | `	/* Not a language construct */` |
|    10884 |  351 | `	return FALSE;` |
|     5529 |  352 |  |
|        - |  353 | `/*` |
|        - |  354 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  355 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  356 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  357 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  358 | ` */` |
|   629802 |  359 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  360 |  |
|        - |  361 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  362 | `	sxi32 i,rc;` |
|        - |  363 |  |
|   629804 |  364 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  365 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       14 |  366 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       14 |  367 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        6 |  368 | `	}` |
|   629804 |  369 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3406856 |  370 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2777084 |  371 | `		if( apNode[i]->xCode == PH7_CompileShortArray ){` |
|        - |  372 | `			/* Short array literal: brackets are self-contained, skip */` |
|      190 |  373 | `			continue;` |
|        - |  374 | `		}` |
|  2776896 |  375 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   319078 |  376 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    16376 |  377 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  378 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   297134 |  379 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  380 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  381 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  382 | `						 */` |
|   297134 |  383 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   297134 |  384 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   297134 |  385 | `						apNode[i]->pOp = &sFCallOp;` |
|   148566 |  386 | `					}` |
|   148566 |  387 | `			}` |
|   319078 |  388 | `			iParen++;` |
|  2617358 |  389 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   319074 |  390 | `			if( iParen <= 0 ){` |
|        9 |  391 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  392 | `				if( rc != SXERR_ABORT ){` |
|        9 |  393 | `					rc = SXERR_SYNTAX;` |
|        4 |  394 | `				}` |
|        9 |  395 | `				return rc;` |
|        - |  396 | `			}` |
|   319066 |  397 | `			iParen--;` |
|  2298280 |  398 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    66552 |  399 | `			iSquare++;` |
|  2105473 |  400 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    66566 |  401 | `			if( iSquare <= 0 ){` |
|        7 |  402 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  403 | `				if( rc != SXERR_ABORT ){` |
|        7 |  404 | `					rc = SXERR_SYNTAX;` |
|        3 |  405 | `				}` |
|        7 |  406 | `				return rc;` |
|        - |  407 | `			}` |
|    66560 |  408 | `			iSquare--;` |
|  2038913 |  409 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2005629 |  456 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  457 | `			if( iBraces <= 0 ){` |
|       13 |  458 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  459 | `				if( rc != SXERR_ABORT ){` |
|       13 |  460 | `					rc = SXERR_SYNTAX;` |
|        6 |  461 | `				}` |
|       13 |  462 | `				return rc;` |
|        - |  463 | `			}` |
|      ! 0 |  464 | `			iBraces--;` |
|  2005612 |  465 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1886 |  466 | `			if( iQuesty <= 0 ){` |
|        5 |  467 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  468 | `				if( rc != SXERR_ABORT ){` |
|        5 |  469 | `					rc = SXERR_SYNTAX;` |
|        2 |  470 | `				}` |
|        5 |  471 | `				return rc;` |
|        - |  472 | `			}` |
|     1882 |  473 | `			iQuesty--;` |
|  2004668 |  474 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   558062 |  475 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   558062 |  476 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1884 |  477 | `				iQuesty++;` |
|   557121 |  478 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   279030 |  498 | `		}` |
|  1388434 |  499 | `	}` |
|   629774 |  500 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  501 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  502 | `		if( rc != SXERR_ABORT ){` |
|       17 |  503 | `			rc = SXERR_SYNTAX;` |
|        8 |  504 | `		}` |
|       17 |  505 | `		return rc;` |
|        - |  506 | `	}` |
|   629758 |  507 | `	return SXRET_OK;` |
|   314903 |  508 |  |
|        - |  509 | `/*` |
|        - |  510 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  511 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  512 | ` */` |
|   508210 |  513 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  514 |  |
|   508212 |  515 | `	SyToken *pIn = *ppCur;` |
|        - |  516 | `	/* Jump the first literal seen */` |
|   508212 |  517 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   508190 |  518 | `		pIn++;` |
|   254094 |  519 | `	}` |
|   254135 |  520 | `	for(;;){` |
|   508272 |  521 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       62 |  522 | `			pIn++;` |
|       62 |  523 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       62 |  524 | `				pIn++;` |
|       30 |  525 | `			}` |
|       32 |  526 | `		}else{` |
|   254107 |  527 | `			break;` |
|        - |  528 | `		}` |
|        2 |  529 | `	}` |
|        - |  530 | `	/* Synchronize pointers */` |
|   508212 |  531 | `	*ppCur = pIn;` |
|   508212 |  532 |  |
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
|  2777234 |  669 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  670 |  |
|        - |  671 | `	ph7_expr_node *pNode;` |
|        - |  672 | `	SyToken *pCur;` |
|        - |  673 | `	sxi32 rc;` |
|        - |  674 | `	/* Allocate a new node */` |
|  2777236 |  675 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2777236 |  676 | `	if( pNode == 0 ){` |
|        - |  677 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  678 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  679 | `		 */` |
|      ! 0 |  680 | `		return SXERR_MEM;` |
|        - |  681 | `	}` |
|        - |  682 | `	/* Zero the structure */` |
|  2777236 |  683 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2777236 |  684 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  685 | `	/* Point to the head of the token stream */` |
|  2777236 |  686 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  687 | `	/* Start collecting tokens */` |
|  2777236 |  688 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
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
|  2777222 |  700 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  701 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  702 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  703 | `		 */` |
|      192 |  704 | `		pCur++; /* Skip the opening '[' */` |
|      192 |  705 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      192 |  706 | `		if( pCur < pGen->pEnd ){` |
|      192 |  707 | `			pCur++; /* Skip past the closing ']' */` |
|       97 |  708 | `		}else{` |
|      ! 0 |  709 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  710 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  711 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  712 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  713 | `			}` |
|      ! 0 |  714 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  715 | `			return rc;` |
|        - |  716 | `		}` |
|      192 |  717 | `		pNode->xCode = PH7_CompileShortArray;` |
|  2777127 |  718 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  719 | `		/* Point to the instance that describe this operator */` |
|   624646 |  720 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  721 | `		/* Advance the stream cursor */` |
|   624646 |  722 | `		pCur++;` |
|  2464710 |  723 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  724 | `		/* Isolate variable */` |
|  1515498 |  725 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   757756 |  726 | `			pCur++; /* Variable variable */` |
|        2 |  727 | `		}` |
|   757744 |  728 | `		if( pCur < pGen->pEnd ){` |
|   757744 |  729 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  730 | `				/* Variable name */` |
|   757716 |  731 | `				pCur++;` |
|   378887 |  732 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  733 | `				pCur++;` |
|        - |  734 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  735 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  736 | `				if( pCur < pGen->pEnd ){` |
|       18 |  737 | `					pCur++;` |
|       10 |  738 | `				}else{` |
|        5 |  739 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  740 | `					if( rc != SXERR_ABORT ){` |
|        5 |  741 | `						rc = SXERR_SYNTAX;` |
|        2 |  742 | `					}` |
|        5 |  743 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  744 | `					return rc;` |
|        - |  745 | `				}` |
|        8 |  746 | `			}` |
|   378869 |  747 | `		}` |
|   757740 |  748 | `		pNode->xCode = PH7_CompileVariable;` |
|  1773515 |  749 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    33680 |  750 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    33680 |  751 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  752 | `			 /* List/Array node */` |
|    22536 |  753 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  754 | `				 /* Assume a literal */` |
|       17 |  755 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  756 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  757 | `			 }else{` |
|    22520 |  758 | `				 pCur += 2;` |
|        - |  759 | `				 /* Collect array/list tokens */` |
|    22520 |  760 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    22520 |  761 | `				 if( pCur < pGen->pEnd ){` |
|    22518 |  762 | `					 pCur++;` |
|    11260 |  763 | `				 }else{` |
|        - |  764 | `					 /* Syntax error */` |
|        4 |  765 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  766 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  767 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  768 | `						 rc = SXERR_SYNTAX;` |
|        1 |  769 | `					 }` |
|        3 |  770 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  771 | `					 return rc;` |
|        - |  772 | `				 }` |
|    22518 |  773 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    22518 |  774 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  775 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  776 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  777 | `						 /* Syntax error */` |
|        3 |  778 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  779 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  780 | `							 rc = SXERR_SYNTAX;` |
|        1 |  781 | `						 }` |
|        3 |  782 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  783 | `						 return rc;` |
|        - |  784 | `					 }` |
|       12 |  785 | `				 }` |
|        2 |  786 | `			 }` |
|    22411 |  787 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  788 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  789 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  790 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  791 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  792 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  793 | `			 pNode->xCode = PH7_CompileYield;` |
|    11130 |  794 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  795 | `			 /* Annonymous function */` |
|      194 |  796 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  797 | `				 /* Assume a literal */` |
|      ! 0 |  798 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  799 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  800 | `			 }else{` |
|        - |  801 | `				 /* Assemble annonymous functions body */` |
|      194 |  802 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      194 |  803 | `				 if( rc != SXRET_OK ){` |
|       25 |  804 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  805 | `					 return rc;` |
|        - |  806 | `				 }` |
|      170 |  807 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  808 | `			  }` |
|    11006 |  809 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  810 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       76 |  811 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       76 |  812 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       39 |  813 | `		 }else{` |
|        - |  814 | `			 /* Assume a literal */` |
|    10848 |  815 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    10848 |  816 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  817 | `		 }` |
|  1377793 |  818 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  819 | `		 /* Constants,function name,namespace path,class name... */` |
|   497350 |  820 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   497350 |  821 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   248676 |  822 | `	 }else{` |
|   863620 |  823 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  824 | `			 /* Point to the code generator routine */` |
|   156988 |  825 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   156988 |  826 | `			 if( pNode->xCode == 0 ){` |
|        3 |  827 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  828 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  829 | `					 rc = SXERR_SYNTAX;` |
|        1 |  830 | `				 }` |
|        3 |  831 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  832 | `				 return rc;` |
|        - |  833 | `			 }` |
|    78492 |  834 | `		 }` |
|        - |  835 | `		/* Advance the stream cursor */` |
|   863618 |  836 | `		pCur++;` |
|        - |  837 | `	 }` |
|        - |  838 | `	/* Point to the end of the token stream */` |
|  2777188 |  839 | `	pNode->pEnd = pCur;` |
|        - |  840 | `	/* Save the node for later processing */` |
|  2777188 |  841 | `	*ppNode = pNode;` |
|        - |  842 | `	/* Synchronize cursors */` |
|  2777188 |  843 | `	pGen->pIn = pCur;` |
|  2777188 |  844 | `	return SXRET_OK;` |
|  1388619 |  845 |  |
|        - |  846 | `/*` |
|        - |  847 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  848 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  849 | ` * level is zero.` |
|        - |  850 | ` */` |
|    67190 |  851 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  852 |  |
|    67192 |  853 | `	SyToken *pCur = pStart;` |
|    67192 |  854 | `	sxi32 iNest = 0;` |
|    67192 |  855 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  856 | `		/* Last expression */` |
|    35912 |  857 | `		return SXERR_EOF;` |
|        - |  858 | `	}` |
|   126230 |  859 | `	while( pCur < pEnd ){` |
|   114238 |  860 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    19290 |  861 | `			break;` |
|        - |  862 | `		}` |
|    94950 |  863 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5316 |  864 | `			iNest++;` |
|    92293 |  865 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5318 |  866 | `			iNest--;` |
|     2658 |  867 | `		}` |
|    94950 |  868 | `		pCur++;` |
|        2 |  869 | `	}` |
|    31282 |  870 | `	*ppNext = pCur;` |
|    31282 |  871 | `	return SXRET_OK;` |
|    33597 |  872 |  |
|        - |  873 | `/*` |
|        - |  874 | ` * Free an expression tree.` |
|        - |  875 | ` */` |
|  2376490 |  876 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  877 |  |
|  2376492 |  878 | `	if( pNode->pLeft ){` |
|        - |  879 | `		/* Release the left tree */` |
|   886446 |  880 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   443222 |  881 | `	}` |
|  2376492 |  882 | `	if( pNode->pRight ){` |
|        - |  883 | `		/* Release the right tree */` |
|   463982 |  884 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   231990 |  885 | `	}` |
|  2376492 |  886 | `	if( pNode->pCond ){` |
|        - |  887 | `		/* Release the conditional tree used by the ternary operator */` |
|     1880 |  888 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      939 |  889 | `	}` |
|  2376492 |  890 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  891 | `		ph7_expr_node **apArg;` |
|        - |  892 | `		sxu32 n;` |
|        - |  893 | `		/* Release node arguments */` |
|   315084 |  894 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   665300 |  895 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   350218 |  896 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   175110 |  897 | `		}` |
|   315084 |  898 | `		SySetRelease(&pNode->aNodeArgs);` |
|   157541 |  899 | `	}` |
|        - |  900 | `	/* Finally,release this node */` |
|  2376492 |  901 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2376492 |  902 |  |
|        - |  903 | `/*` |
|        - |  904 | ` * Free an expression tree.` |
|        - |  905 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  906 | ` */` |
|   629836 |  907 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  908 |  |
|        - |  909 | `	ph7_expr_node **apNode;` |
|        - |  910 | `	sxu32 n;` |
|   629838 |  911 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3407024 |  912 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2777188 |  913 | `		if( apNode[n] ){` |
|   630110 |  914 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   315054 |  915 | `		}` |
|  1388595 |  916 | `	}` |
|   629838 |  917 | `	return SXRET_OK;` |
|        2 |  918 |  |
|        - |  919 | `/*` |
|        - |  920 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  921 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  922 | ` */` |
|   201792 |  923 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  924 |  |
|        - |  925 | `	sxi32 iExprOp;` |
|   201794 |  926 | `	if( pNode->pOp == 0 ){` |
|   131376 |  927 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  928 | `	}` |
|    70420 |  929 | `	iExprOp = pNode->pOp->iOp;` |
|    70420 |  930 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    44158 |  931 | `			return TRUE;` |
|        - |  932 | `	}` |
|    26264 |  933 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    26260 |  934 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  935 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  936 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  937 | `				return FALSE;` |
|        1 |  938 | `			}` |
|    26257 |  939 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  940 | `			return FALSE;` |
|        - |  941 | `		}` |
|    26260 |  942 | `		return TRUE;` |
|        - |  943 | `	}` |
|        5 |  944 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  945 | `		return TRUE;` |
|        - |  946 | `	}` |
|        - |  947 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  948 | `	return FALSE;` |
|   100898 |  949 |  |
|        - |  950 | `/* Forward declaration */` |
|        - |  951 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  952 | `/* Macro to check if the given node is a terminal.` |
|        - |  953 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  954 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  955 | ` * linked ternary/elvis node). */` |
|        - |  956 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  957 | `/*` |
|        - |  958 | ` * Buid an expression tree for each given function argument.` |
|        - |  959 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  960 | ` */` |
|   261544 |  961 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  962 |  |
|        - |  963 | `	sxi32 iNest,iCur,iNode;` |
|        - |  964 | `	sxi32 rc;` |
|        - |  965 | `	/* Process function arguments from left to right */` |
|   261546 |  966 | `	iCur = 0;` |
|   279110 |  967 | `	for(;;){` |
|   558222 |  968 | `		if( iCur >= nToken ){` |
|        - |  969 | `			/* No more arguments to process */` |
|   261544 |  970 | `			break;` |
|        - |  971 | `		}` |
|   296680 |  972 | `		iNode = iCur;` |
|   296680 |  973 | `		iNest = 0;` |
|   741924 |  974 | `		while( iCur < nToken ){` |
|   480382 |  975 | `			if( apNode[iCur] ){` |
|   470014 |  976 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    17570 |  977 | `					break;` |
|   434878 |  978 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    24092 |  979 | `					iNest++;` |
|   422833 |  980 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    24080 |  981 | `					iNest--;` |
|    12039 |  982 | `				}` |
|   217438 |  983 | `			}` |
|   445246 |  984 | `			iCur++;` |
|        2 |  985 | `		}` |
|   296680 |  986 | `		if( iCur > iNode ){` |
|   296676 |  987 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  988 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  989 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  990 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  991 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  992 | `					apNode[iNode] = 0;` |
|      ! 0 |  993 | `			}` |
|   296678 |  994 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   296678 |  995 | `			if( apNode[iNode] ){` |
|        - |  996 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   296678 |  997 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   148340 |  998 | `			}else{` |
|        - |  999 | `				/* Empty function argument */` |
|      ! 0 | 1000 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 | 1001 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1002 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1003 | `				}` |
|      ! 0 | 1004 | `				return rc;` |
|        - | 1005 | `			}` |
|   148340 | 1006 | `		}else{` |
|        3 | 1007 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 | 1008 | `			if( rc != SXERR_ABORT ){` |
|        3 | 1009 | `				rc = SXERR_SYNTAX;` |
|        1 | 1010 | `			}` |
|        3 | 1011 | `			return rc;` |
|        - | 1012 | `		}` |
|        - | 1013 | `		/* Jump trailing comma */` |
|   296678 | 1014 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    35136 | 1015 | `			iCur++;` |
|    35136 | 1016 | `			if( iCur >= nToken ){` |
|        - | 1017 | `				/* missing function argument */` |
|      ! 0 | 1018 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 | 1019 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1020 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1021 | `				}` |
|      ! 0 | 1022 | `				return rc;` |
|        - | 1023 | `			}` |
|    17567 | 1024 | `		}` |
|        2 | 1025 | `	}` |
|   261544 | 1026 | `	return SXRET_OK;` |
|   130774 | 1027 |  |
|        - | 1028 | ` /*` |
|        - | 1029 | `  * Create an expression tree from an array of tokens.` |
|        - | 1030 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1031 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1032 | `  */` |
|  1008396 | 1033 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1034 | ` {` |
|        - | 1035 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1036 | `	 ph7_expr_node *pNode;` |
|        - | 1037 | `	 sxi32 iCur;` |
|        - | 1038 | `	 sxi32 rc;` |
|  1008398 | 1039 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1040 | `		 /* TICKET 1433-17: self evaluating node */` |
|   465444 | 1041 | `		 return SXRET_OK;` |
|        - | 1042 | `	 }` |
|        - | 1043 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3335704 | 1044 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1045 | `		 sxi32 iNest;` |
|        - | 1046 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1047 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1048 | `		  */` |
|  2792752 | 1049 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2770818 | 1050 | `			 continue;` |
|        - | 1051 | `		 }` |
|    21936 | 1052 | `		 iNest = 1;` |
|    21936 | 1053 | `		 iLeft = iCur;` |
|        - | 1054 | `		 /* Find the closing parenthesis */` |
|    21936 | 1055 | `		 iCur++;` |
|   145978 | 1056 | `		 while( iCur < nToken ){` |
|   145978 | 1057 | `			 if( apNode[iCur] ){` |
|   145978 | 1058 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1059 | `					 /* Decrement nesting level */` |
|    38044 | 1060 | `					 iNest--;` |
|    38044 | 1061 | `					 if( iNest <= 0 ){` |
|    21936 | 1062 | `						 break;` |
|        2 | 1063 | `					 }` |
|   115990 | 1064 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1065 | `					 /* Increment nesting level */` |
|    16110 | 1066 | `					 iNest++;` |
|     8054 | 1067 | `				 }` |
|    62021 | 1068 | `			 }` |
|   124044 | 1069 | `			 iCur++;` |
|        2 | 1070 | `		 }` |
|    21936 | 1071 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1072 | `			 /* Recurse and process this expression */` |
|    21936 | 1073 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    21936 | 1074 | `			 if( rc != SXRET_OK ){` |
|        3 | 1075 | `				 return rc;` |
|        - | 1076 | `			 }` |
|    10966 | 1077 | `		 }` |
|        - | 1078 | `		 /* Free the left and right nodes */` |
|    21934 | 1079 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    21934 | 1080 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    21934 | 1081 | `		 apNode[iLeft] = 0;` |
|    21934 | 1082 | `		 apNode[iCur] = 0;` |
|    10968 | 1083 | `	 }` |
|        - | 1084 | `	  /* Process expressions enclosed in braces */` |
|  3476064 | 1085 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1086 | `		 sxi32 iNest;` |
|        - | 1087 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1088 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1089 | `		  */` |
|  2938716 | 1090 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  2938716 | 1091 | `			 continue;` |
|        - | 1092 | `		 }` |
|      ! 0 | 1093 | `		 iNest = 1;` |
|      ! 0 | 1094 | `		 iLeft = iCur;` |
|        - | 1095 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1096 | `		 iCur++;` |
|      ! 0 | 1097 | `		 while( iCur < nToken ){` |
|      ! 0 | 1098 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1099 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1100 | `					 /* Decrement nesting level */` |
|      ! 0 | 1101 | `					 iNest--;` |
|      ! 0 | 1102 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1103 | `						 break;` |
|      ! 0 | 1104 | `					 }` |
|      ! 0 | 1105 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1106 | `					 /* Increment nesting level */` |
|      ! 0 | 1107 | `					 iNest++;` |
|      ! 0 | 1108 | `				 }` |
|      ! 0 | 1109 | `			 }` |
|      ! 0 | 1110 | `			 iCur++;` |
|      ! 0 | 1111 | `		 }` |
|      ! 0 | 1112 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1113 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1114 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1115 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1116 | `				 return rc;` |
|        - | 1117 | `			 }` |
|      ! 0 | 1118 | `		 }` |
|        - | 1119 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1120 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1121 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1122 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1123 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1124 | `	 }` |
|        - | 1125 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   537350 | 1126 | `	 iLeft = -1;` |
|  3476052 | 1127 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2938710 | 1128 | `		 if( apNode[iCur] == 0 ){` |
|  1143650 | 1129 | `			 continue;` |
|        - | 1130 | `		 }` |
|  1795062 | 1131 | `		 pNode = apNode[iCur];` |
|  1795062 | 1132 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   463094 | 1133 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1134 | `				 /* Collect function arguments */` |
|   297130 | 1135 | `				 sxi32 iPtr = 0;` |
|   297130 | 1136 | `				 sxi32 nFuncTok = 0;` |
|  1074640 | 1137 | `				 while( nFuncTok + iCur < nToken ){` |
|  1074640 | 1138 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1064272 | 1139 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   307942 | 1140 | `							 iPtr++;` |
|   910302 | 1141 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   307942 | 1142 | `							 iPtr--;` |
|   307942 | 1143 | `							 if( iPtr <= 0 ){` |
|   297130 | 1144 | `								 break;` |
|        - | 1145 | `							 }` |
|     5406 | 1146 | `						 }` |
|   383571 | 1147 | `					 }` |
|   777512 | 1148 | `					 nFuncTok++;` |
|        2 | 1149 | `				 }` |
|   297130 | 1150 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1151 | `					 /* Syntax error */` |
|      ! 0 | 1152 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1153 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1154 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1155 | `					 }` |
|      ! 0 | 1156 | `					 return rc;` |
|        - | 1157 | `				 }` |
|   297130 | 1158 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1159 | `					 /* Syntax error */` |
|      ! 0 | 1160 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1161 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1162 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1163 | `					 }` |
|      ! 0 | 1164 | `					 return rc;` |
|        - | 1165 | `				 }` |
|   297130 | 1166 | `				 if( nFuncTok > 1 ){` |
|        - | 1167 | `					 /* Process function arguments */` |
|   261546 | 1168 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   261546 | 1169 | `					 if( rc != SXRET_OK ){` |
|        3 | 1170 | `						 return rc;` |
|        - | 1171 | `					 }` |
|   130771 | 1172 | `				 }` |
|        - | 1173 | `				 /* Link the node to the tree */` |
|   297128 | 1174 | `				 pNode->pLeft = apNode[iLeft];` |
|   297128 | 1175 | `				 apNode[iLeft] = 0;` |
|  1074632 | 1176 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   777506 | 1177 | `					 apNode[iCur+iPtr] = 0;` |
|   388754 | 1178 | `				 }` |
|   314529 | 1179 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1180 | `				 /* Subscripting */` |
|    66560 | 1181 | `				 sxi32 iArrTok = iCur + 1;` |
|    66560 | 1182 | `				 sxi32 iNest = 1;` |
|    66627 | 1183 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1184 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1185 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    66558 | 1186 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1187 | `						 /* Syntax error */` |
|      ! 0 | 1188 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1189 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1190 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1191 | `						 }` |
|      ! 0 | 1192 | `						 return rc;` |
|        - | 1193 | `				 }` |
|        - | 1194 | `				 /* Collect index tokens */` |
|   120210 | 1195 | `				 while( iArrTok < nToken ){` |
|   120210 | 1196 | `					 if( apNode[iArrTok] ){` |
|   120178 | 1197 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1198 | `							 /* Increment nesting level */` |
|      ! 0 | 1199 | `							 iNest++;` |
|   120178 | 1200 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1201 | `							 /* Decrement nesting level */` |
|    66560 | 1202 | `							 iNest--;` |
|    66560 | 1203 | `							 if( iNest <= 0 ){` |
|    66560 | 1204 | `								 break;` |
|        - | 1205 | `							 }` |
|      ! 0 | 1206 | `						 }` |
|    26809 | 1207 | `					 }` |
|    53652 | 1208 | `					 ++iArrTok;` |
|        2 | 1209 | `				 }` |
|    66560 | 1210 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1211 | `					 /* Recurse and process this expression */` |
|    53542 | 1212 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    53542 | 1213 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1214 | `						 return rc;` |
|        - | 1215 | `					 }` |
|        - | 1216 | `					 /* Link the node to it's index */` |
|    53542 | 1217 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    26770 | 1218 | `				 }` |
|        - | 1219 | `				 /* Link the node to the tree */` |
|    66560 | 1220 | `				 pNode->pLeft = apNode[iLeft];` |
|    66560 | 1221 | `				 pNode->pRight = 0;` |
|    66560 | 1222 | `				 apNode[iLeft] = 0;` |
|   186768 | 1223 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   120210 | 1224 | `					 apNode[iNest] = 0;` |
|    60106 | 1225 | `				 }` |
|    33281 | 1226 | `			 }else{` |
|        - | 1227 | `				 /* Member access operators [i.e: '->','::'] */` |
|    99408 | 1228 | `				  iRight = iCur + 1;` |
|    99408 | 1229 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1230 | `					 iRight++;` |
|      ! 0 | 1231 | `				 }` |
|    99408 | 1232 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1233 | `					 /* Syntax error */` |
|        5 | 1234 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1235 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1236 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1237 | `					 }` |
|        5 | 1238 | `					 return rc;` |
|        - | 1239 | `				 }` |
|        - | 1240 | `				 /* Link the node to the tree */` |
|    99404 | 1241 | `				 pNode->pLeft = apNode[iLeft];` |
|    99404 | 1242 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|    99232 | 1243 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1244 | `						 /* Syntax error */` |
|      ! 0 | 1245 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1246 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1247 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1248 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1249 | `						 }` |
|      ! 0 | 1250 | `						 return rc;` |
|        - | 1251 | `				 }` |
|    99404 | 1252 | `				 pNode->pRight = apNode[iRight];` |
|    99404 | 1253 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1254 | `			 }` |
|   231543 | 1255 | `		 }` |
|  1795056 | 1256 | `		 iLeft = iCur;` |
|   897529 | 1257 | `	 }` |
|        - | 1258 | `	 /* Handle left associative (new, clone) operators */` |
|  3476032 | 1259 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2938690 | 1260 | `		 if( apNode[iCur] == 0 ){` |
|  1620096 | 1261 | `			 continue;` |
|        - | 1262 | `		 }` |
|  1318596 | 1263 | `		 pNode = apNode[iCur];` |
|  1318596 | 1264 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1265 | `			 SyToken *pToken;` |
|        - | 1266 | `			 /* Get the left node */` |
|    13362 | 1267 | `			 iLeft = iCur + 1;` |
|    26692 | 1268 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13332 | 1269 | `				 iLeft++;` |
|        2 | 1270 | `			 }` |
|    13362 | 1271 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1272 | `				  /* Syntax error */` |
|      ! 0 | 1273 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1274 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1275 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1276 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1277 | `				 }` |
|      ! 0 | 1278 | `				 return rc;` |
|        - | 1279 | `			 }` |
|        - | 1280 | `			 /* Make sure the operand are of a valid type */` |
|    13362 | 1281 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1282 | `				 /* Clone:` |
|        - | 1283 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1284 | `				  *  ++ function call (including annonymous)` |
|        - | 1285 | `				  *  ++ array member` |
|        - | 1286 | `				  *  ++ 'new' operator` |
|        - | 1287 | `				  * Example:` |
|        - | 1288 | `				  *   clone $pObj;` |
|        - | 1289 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1290 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1291 | `				  */` |
|       18 | 1292 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1293 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1294 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1295 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1296 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1297 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1298 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1299 | `						 }` |
|      ! 0 | 1300 | `						 return rc;` |
|        - | 1301 | `					 }` |
|        7 | 1302 | `				 }` |
|       10 | 1303 | `			 }else{` |
|        - | 1304 | `				 /* New */` |
|    13346 | 1305 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1306 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1307 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1308 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1309 | `						 /* Syntax error */` |
|      ! 0 | 1310 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1311 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1312 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1313 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1314 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1315 | `						 }` |
|      ! 0 | 1316 | `						 return rc;` |
|        - | 1317 | `					 }` |
|        8 | 1318 | `				 }` |
|        - | 1319 | `			 }` |
|        - | 1320 | `			  /* Link the node to the tree */` |
|    13362 | 1321 | `			 pNode->pLeft = apNode[iLeft];` |
|    13362 | 1322 | `			 apNode[iLeft] = 0;` |
|    13362 | 1323 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6680 | 1324 | `		 }` |
|   659299 | 1325 | `	 }` |
|        - | 1326 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   537344 | 1327 | `	 iLeft = -1;` |
|  3478834 | 1328 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2938690 | 1329 | `		 if( apNode[iCur] == 0 ){` |
|  1620096 | 1330 | `			 continue;` |
|        - | 1331 | `		 }` |
|  1318596 | 1332 | `		 pNode = apNode[iCur];` |
|  1318596 | 1333 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7802 | 1334 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2820 | 1335 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1336 | `					 /* Link the node to the tree */` |
|     2822 | 1337 | `					 pNode->pLeft = apNode[iLeft];` |
|     2822 | 1338 | `					 apNode[iLeft] = 0;` |
|     1410 | 1339 | `			 }` |
|     5301 | 1340 | `		  }` |
|  1321398 | 1341 | `		 iLeft = iCur;` |
|   662101 | 1342 | `	  }` |
|   540146 | 1343 | `	 iLeft = -1;` |
|  3478834 | 1344 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2938690 | 1345 | `		 if( apNode[iCur] == 0 ){` |
|  1622916 | 1346 | `			 continue;` |
|        - | 1347 | `		 }` |
|  1315776 | 1348 | `		 pNode = apNode[iCur];` |
|  1315776 | 1349 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7782 | 1350 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     7784 | 1351 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1352 | `					 /* Syntax error */` |
|      ! 0 | 1353 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1354 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1355 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1356 | `					 }` |
|      ! 0 | 1357 | `					 return rc;` |
|        - | 1358 | `			 }` |
|        - | 1359 | `			 /* Link the node to the tree */` |
|     7784 | 1360 | `			 pNode->pLeft = apNode[iLeft];` |
|     7784 | 1361 | `			 apNode[iLeft] = 0;` |
|        - | 1362 | `			 /* Mark as pre-increment/decrement node */` |
|     7784 | 1363 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3891 | 1364 | `		  }` |
|  1315776 | 1365 | `		 iLeft = iCur;` |
|   657889 | 1366 | `	 }` |
|        - | 1367 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   540146 | 1368 | `	  iLeft = 0;` |
|  3478828 | 1369 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2938686 | 1370 | `		  if( apNode[iCur] ){` |
|  1307990 | 1371 | `			  pNode = apNode[iCur];` |
|  1307990 | 1372 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    34890 | 1373 | `				  if( iLeft > 0 ){` |
|        - | 1374 | `					  /* Link the node to the tree */` |
|    34888 | 1375 | `					  pNode->pLeft = apNode[iLeft];` |
|    34888 | 1376 | `					  apNode[iLeft] = 0;` |
|    34888 | 1377 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|        9 | 1378 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1379 | `							   /* Syntax error */` |
|      ! 0 | 1380 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1381 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1382 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1383 | `							  }` |
|      ! 0 | 1384 | `							  return rc;` |
|        - | 1385 | `						  }` |
|        4 | 1386 | `					  }` |
|    17445 | 1387 | `				  }else{` |
|        - | 1388 | `					  /* Syntax error */` |
|        3 | 1389 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1390 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1391 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1392 | `					  }` |
|        3 | 1393 | `					  return rc;` |
|        - | 1394 | `				  }` |
|    17443 | 1395 | `			  }` |
|        - | 1396 | `			  /* Save terminal position */` |
|  1307988 | 1397 | `			  iLeft = iCur;` |
|   653993 | 1398 | `		  }` |
|  1469343 | 1399 | `	  }` |
|        - | 1400 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  5941488 | 1401 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5401354 | 1402 | `		 iLeft = -1;` |
| 34787928 | 1403 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 29386584 | 1404 | `			 if( apNode[iCur] == 0 ){` |
| 18753592 | 1405 | `				 continue;` |
|        - | 1406 | `			 }` |
| 10632994 | 1407 | `			 pNode = apNode[iCur];` |
| 10632994 | 1408 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1409 | `				 /* Get the right node */` |
|   160924 | 1410 | `				 iRight = iCur + 1;` |
|   228576 | 1411 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    67654 | 1412 | `					 iRight++;` |
|        2 | 1413 | `				 }` |
|   160924 | 1414 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1415 | `					 /* Syntax error */` |
|        9 | 1416 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1417 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1418 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1419 | `					 }` |
|        9 | 1420 | `					 return rc;` |
|        - | 1421 | `				 }` |
|   160916 | 1422 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1423 | `					 sxi32  iTmp;` |
|        - | 1424 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1425 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1426 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1427 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1428 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1429 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1430 | `						 }` |
|      ! 0 | 1431 | `						 return rc;` |
|        - | 1432 | `					 }` |
|       46 | 1433 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1434 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1435 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1436 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1437 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1438 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1439 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1440 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1441 | `									 }` |
|      ! 0 | 1442 | `									 return rc;` |
|        - | 1443 | `							 }` |
|      ! 0 | 1444 | `						 }` |
|       15 | 1445 | `					 }` |
|        - | 1446 | `					 /* Swap operands */` |
|       46 | 1447 | `					 iTmp = iRight;` |
|       46 | 1448 | `					 iRight = iLeft;` |
|       46 | 1449 | `					 iLeft = iTmp;` |
|       22 | 1450 | `				 }` |
|        - | 1451 | `				 /* Link the node to the tree */` |
|   160916 | 1452 | `				 pNode->pLeft = apNode[iLeft];` |
|   160916 | 1453 | `				 pNode->pRight = apNode[iRight];` |
|   160916 | 1454 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    80457 | 1455 | `			 }` |
| 10632986 | 1456 | `			 iLeft = iCur;` |
|  5316494 | 1457 | `		 }` |
|  2700674 | 1458 | `	 }` |
|        - | 1459 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1460 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1461 | `	  * we are dealing with a single operator.` |
|        - | 1462 | `	  */` |
|   540136 | 1463 | `	  iLeft = -1;` |
|  3470756 | 1464 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2932502 | 1465 | `		  if( apNode[iCur] == 0 ){` |
|  1986848 | 1466 | `			  continue;` |
|        - | 1467 | `		  }` |
|   945656 | 1468 | `		  pNode = apNode[iCur];` |
|   945656 | 1469 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1882 | 1470 | `			  sxi32 iNest = 1;` |
|     1882 | 1471 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1472 | `				  /* Missing condition */` |
|        3 | 1473 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1474 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1475 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1476 | `				  }` |
|        3 | 1477 | `				  return rc;` |
|        - | 1478 | `			  }` |
|        - | 1479 | `			  /* Get the right node */` |
|     1880 | 1480 | `			  iRight = iCur + 1;` |
|     3982 | 1481 | `			  while( iRight < nToken  ){` |
|     3982 | 1482 | `				  if( apNode[iRight] ){` |
|     3690 | 1483 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1484 | `						  /* Increment nesting level */` |
|      ! 0 | 1485 | `						  ++iNest;` |
|     3690 | 1486 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1487 | `						  /* Decrement nesting level */` |
|     1880 | 1488 | `						  --iNest;` |
|     1880 | 1489 | `						  if( iNest <= 0 ){` |
|     1880 | 1490 | `							  break;` |
|        - | 1491 | `						  }` |
|      ! 0 | 1492 | `					  }` |
|      905 | 1493 | `				  }` |
|     2104 | 1494 | `				  iRight++;` |
|        2 | 1495 | `			  }` |
|     1880 | 1496 | `			  if( iRight > iCur + 1 ){` |
|        - | 1497 | `				  /* Recurse and process the then expression */` |
|     1812 | 1498 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1812 | 1499 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1500 | `					  return rc;` |
|        - | 1501 | `				  }` |
|        - | 1502 | `				  /* Link the node to the tree */` |
|     1812 | 1503 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      905 | 1504 | `			  }else{` |
|        - | 1505 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1506 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1507 | `			  }` |
|     1880 | 1508 | `			  apNode[iCur + 1] = 0;` |
|     1880 | 1509 | `			  if( iRight + 1 < nToken ){` |
|        - | 1510 | `				  /* Recurse and process the else expression */` |
|     1880 | 1511 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1880 | 1512 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1513 | `					  return rc;` |
|        - | 1514 | `				  }` |
|        - | 1515 | `				  /* Link the node to the tree */` |
|     1880 | 1516 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1880 | 1517 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      941 | 1518 | `			  }else{` |
|      ! 0 | 1519 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1520 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1521 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1522 | `				 }` |
|      ! 0 | 1523 | `				 return rc;` |
|        - | 1524 | `			  }` |
|        - | 1525 | `			  /* Point to the condition */` |
|     1880 | 1526 | `			  pNode->pCond  = apNode[iLeft];` |
|     1880 | 1527 | `			  apNode[iLeft] = 0;` |
|     1880 | 1528 | `			  break;` |
|        - | 1529 | `		  }` |
|   943776 | 1530 | `		  iLeft = iCur;` |
|   471889 | 1531 | `	  }` |
|        - | 1532 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1533 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1534 | `	  * so there is no need for a precedence loop here.` |
|        - | 1535 | `	  */` |
|   540134 | 1536 | `	 iRight = -1;` |
|  3478694 | 1537 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  2938602 | 1538 | `		 if( apNode[iCur] == 0 ){` |
|  2196590 | 1539 | `			 continue;` |
|        - | 1540 | `		 }` |
|   742014 | 1541 | `		 pNode = apNode[iCur];` |
|   742014 | 1542 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1543 | `			 /* Get the left node */` |
|   201758 | 1544 | `			 iLeft = iCur - 1;` |
|   285472 | 1545 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    83716 | 1546 | `				 iLeft--;` |
|        2 | 1547 | `			 }` |
|   201758 | 1548 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1549 | `				 /* Syntax error */` |
|       39 | 1550 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1551 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1552 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1553 | `				 }` |
|       39 | 1554 | `				 return rc;` |
|        - | 1555 | `			 }` |
|   201720 | 1556 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       28 | 1557 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
|        - | 1558 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1559 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1560 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1561 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1562 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1563 | `					 }` |
|        3 | 1564 | `					 return rc;` |
|        - | 1565 | `				 }` |
|       12 | 1566 | `			 }` |
|        - | 1567 | `			 /* Link the node to the tree (Reverse) */` |
|   201718 | 1568 | `			 pNode->pLeft = apNode[iRight];` |
|   201718 | 1569 | `			 pNode->pRight = apNode[iLeft];` |
|   201718 | 1570 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   100858 | 1571 | `		 }` |
|   741974 | 1572 | `		 iRight = iCur;` |
|   370988 | 1573 | `	 }` |
|        - | 1574 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2700462 | 1575 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2160370 | 1576 | `		 iLeft = -1;` |
| 13914602 | 1577 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 11754234 | 1578 | `			 if( apNode[iCur] == 0 ){` |
|  9593460 | 1579 | `				 continue;` |
|        - | 1580 | `			 }` |
|  2160776 | 1581 | `			 pNode = apNode[iCur];` |
|  2160776 | 1582 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1583 | `				 /* Get the right node */` |
|       72 | 1584 | `				 iRight = iCur + 1;` |
|      110 | 1585 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1586 | `					 iRight++;` |
|        2 | 1587 | `				 }` |
|       72 | 1588 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1589 | `					 /* Syntax error */` |
|      ! 0 | 1590 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1591 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1592 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1593 | `					 }` |
|      ! 0 | 1594 | `					 return rc;` |
|        - | 1595 | `				 }` |
|        - | 1596 | `				 /* Link the node to the tree */` |
|       72 | 1597 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1598 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1599 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1600 | `			 }` |
|  2160776 | 1601 | `			 iLeft = iCur;` |
|  1080389 | 1602 | `		 }` |
|  1080186 | 1603 | `	 }` |
|        - | 1604 | `	 /* Point to the root of the expression tree */` |
|  2938532 | 1605 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2398458 | 1606 | `		 if( apNode[iCur] ){` |
|   487522 | 1607 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1608 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1609 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1610 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1611 | `				  }` |
|       20 | 1612 | `				  return rc;` |
|        - | 1613 | `			 }` |
|   487504 | 1614 | `			 apNode[0] = apNode[iCur];` |
|   487504 | 1615 | `			 apNode[iCur] = 0;` |
|   243751 | 1616 | `		 }` |
|  1199221 | 1617 | `	 }` |
|   540076 | 1618 | `	 return SXRET_OK;` |
|   502799 | 1619 | ` }` |
|        - | 1620 | ` /*` |
|        - | 1621 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1622 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1623 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1624 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1625 | `  */` |
|   629836 | 1626 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1627 |  |
|        - | 1628 | `	ph7_expr_node **apNode;` |
|        - | 1629 | `	ph7_expr_node *pNode;` |
|        - | 1630 | `	sxi32 rc;` |
|        - | 1631 | `	/* Reset node container */` |
|   629838 | 1632 | `	SySetReset(pExprNode);` |
|   629838 | 1633 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1634 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1635 | `	{` |
|   629838 | 1636 | `		int iLastWasTerm = 0;` |
|  3407024 | 1637 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2777222 | 1638 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2777222 | 1639 | `			if( rc != SXRET_OK ){` |
|       35 | 1640 | `				return rc;` |
|        - | 1641 | `			}` |
|        - | 1642 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2777188 | 1643 | `			if( pNode->xCode ){` |
|        - | 1644 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1445912 | 1645 | `				iLastWasTerm = 1;` |
|  2054233 | 1646 | `			}else if( pNode->pOp ){` |
|        - | 1647 | `				/* Operator node */` |
|   624646 | 1648 | `				iLastWasTerm = 0;` |
|   312324 | 1649 | `			}else{` |
|        - | 1650 | `				/* Delimiter: ')' and ']' end terms */` |
|   706634 | 1651 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1652 | `			}` |
|        - | 1653 | `			/* Save the extracted node */` |
|  2777188 | 1654 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1655 | `		}` |
|        - | 1656 | `	}` |
|   629804 | 1657 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1658 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1659 | `		*ppRoot = 0;` |
|      ! 0 | 1660 | `		return SXRET_OK;` |
|        - | 1661 | `	}` |
|   629804 | 1662 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1663 | `	/* Make sure we are dealing with valid nodes */` |
|   629804 | 1664 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   629804 | 1665 | `	if( rc != SXRET_OK ){` |
|        - | 1666 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1667 | `		 * cleanup the mess left behind.` |
|        - | 1668 | `		 */` |
|       47 | 1669 | `		*ppRoot = 0;` |
|       47 | 1670 | `		return rc;` |
|        - | 1671 | `	}` |
|        - | 1672 | `	/* Build the tree */` |
|   629758 | 1673 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   629758 | 1674 | `	if( rc != SXRET_OK ){` |
|        - | 1675 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       78 | 1676 | `		*ppRoot = 0;` |
|       78 | 1677 | `		return rc;` |
|        - | 1678 | `	}` |
|        - | 1679 | `	/* Point to the root of the tree */` |
|   629682 | 1680 | `	*ppRoot = apNode[0];` |
|   629682 | 1681 | `	return SXRET_OK;` |
|   314920 | 1682 |  |
|        - | 1683 |  |
