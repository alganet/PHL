# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 944/1118 lines (84.44%)

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
|   778968 |  264 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  265 |  |
|   778970 |  266 | `	sxu32 n = 0;` |
|        - |  267 | `	sxi32 rc;` |
|        - |  268 | `	/* Do a linear lookup on the operators table */` |
| 12763118 |  269 | `	for(;;){` |
| 25526238 |  270 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  271 | `			break;` |
|        - |  272 | `		}` |
| 25526238 |  273 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  274 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3105876 |  275 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1552939 |  276 | `		}else{` |
| 22420364 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  278 | `		}` |
| 25526238 |  279 | `		if( rc == 0 ){` |
|   782398 |  280 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  281 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   778640 |  282 | `				return &aOpTable[n];` |
|        - |  283 | `			}` |
|        - |  284 | `			/* Handle ambiguity */` |
|     3760 |  285 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  286 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  287 | `				return &aOpTable[n];` |
|        - |  288 | `			}` |
|     3540 |  289 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  290 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  291 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  292 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  293 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  294 | `					return &aOpTable[n];` |
|        - |  295 | `				}` |
|        - |  296 |  |
|        4 |  297 | `			}` |
|     1714 |  298 | `		}` |
| 24747270 |  299 | `		++n; /* Next operator in the table */` |
|        2 |  300 | `	}` |
|        - |  301 | `	/* No such operator */` |
|      ! 0 |  302 | `	return 0;` |
|   389486 |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * Delimit a set of token stream.` |
|        - |  306 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  307 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  308 | ` */` |
|   400590 |  309 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  310 |  |
|   400592 |  311 | `	SyToken *pCur = pIn;` |
|   400592 |  312 | `	sxi32 iNest = 1;` |
|  2276481 |  313 | `	for(;;){` |
|  4552964 |  314 | `		if( pCur >= pEnd ){` |
|      130 |  315 | `			break;` |
|        - |  316 | `		}` |
|  4552836 |  317 | `		if( pCur->nType & nTokStart ){` |
|        - |  318 | `			/* Increment nesting level */` |
|   251710 |  319 | `			iNest++;` |
|  4426982 |  320 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  321 | `			/* Decrement nesting level */` |
|   652172 |  322 | `			iNest--;` |
|   652172 |  323 | `			if( iNest <= 0 ){` |
|   400464 |  324 | `				break;` |
|        - |  325 | `			}` |
|   125854 |  326 | `		}` |
|        - |  327 | `		/* Advance cursor */` |
|  4152374 |  328 | `		pCur++;` |
|        2 |  329 | `	}` |
|        - |  330 | `	/* Point to the end of the chunk */` |
|   400592 |  331 | `	*ppEnd = pCur;` |
|   400592 |  332 |  |
|        - |  333 | `/*` |
|        - |  334 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  335 | ` * Note on reserved keywords.` |
|        - |  336 | ` *  According to the PHP language reference manual:` |
|        - |  337 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  338 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  339 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  340 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  341 | ` */` |
|    12034 |  342 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  343 |  |
|    17985 |  344 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11941 |  345 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  346 | `		){` |
|      146 |  347 | `			return TRUE;` |
|        - |  348 | `	}` |
|    11892 |  349 | `	if( bCheckFunc ){` |
|       95 |  350 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       70 |  351 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       55 |  352 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  353 | `				return TRUE;` |
|        - |  354 | `		}` |
|       21 |  355 | `	}` |
|        - |  356 | `	/* Not a language construct */` |
|    11860 |  357 | `	return FALSE;` |
|     6019 |  358 |  |
|        - |  359 | `/*` |
|        - |  360 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  361 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  362 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  363 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  364 | ` */` |
|   685702 |  365 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  366 |  |
|        - |  367 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  368 | `	sxi32 i,rc;` |
|        - |  369 |  |
|   685704 |  370 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  371 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  372 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  373 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  374 | `	}` |
|   685704 |  375 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3708750 |  376 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3023082 |  377 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  378 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      322 |  379 | `			continue;` |
|        - |  380 | `		}` |
|  3022762 |  381 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   347188 |  382 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    17860 |  383 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  384 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   323304 |  385 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  386 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  387 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  388 | `						 */` |
|   323304 |  389 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   323304 |  390 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   323304 |  391 | `						apNode[i]->pOp = &sFCallOp;` |
|   161651 |  392 | `					}` |
|   161651 |  393 | `			}` |
|   347188 |  394 | `			iParen++;` |
|  2849169 |  395 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   347188 |  396 | `			if( iParen <= 0 ){` |
|       13 |  397 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  398 | `				if( rc != SXERR_ABORT ){` |
|       13 |  399 | `					rc = SXERR_SYNTAX;` |
|        6 |  400 | `				}` |
|       13 |  401 | `				return rc;` |
|        - |  402 | `			}` |
|   347176 |  403 | `			iParen--;` |
|  2501977 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    72422 |  405 | `			iSquare++;` |
|  2292180 |  406 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    72436 |  407 | `			if( iSquare <= 0 ){` |
|        7 |  408 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  409 | `				if( rc != SXERR_ABORT ){` |
|        7 |  410 | `					rc = SXERR_SYNTAX;` |
|        3 |  411 | `				}` |
|        7 |  412 | `				return rc;` |
|        - |  413 | `			}` |
|    72430 |  414 | `			iSquare--;` |
|  2219750 |  415 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2183531 |  462 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  463 | `			if( iBraces <= 0 ){` |
|       13 |  464 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  465 | `				if( rc != SXERR_ABORT ){` |
|       13 |  466 | `					rc = SXERR_SYNTAX;` |
|        6 |  467 | `				}` |
|       13 |  468 | `				return rc;` |
|        - |  469 | `			}` |
|      ! 0 |  470 | `			iBraces--;` |
|  2183514 |  471 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2156 |  472 | `			if( iQuesty > 0 ){` |
|     1976 |  473 | `				iQuesty--;` |
|     1169 |  474 | `			}else if( iParen <= 0 ){` |
|        - |  475 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  476 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  477 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  478 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  479 | `				if( rc != SXERR_ABORT ){` |
|        5 |  480 | `					rc = SXERR_SYNTAX;` |
|        2 |  481 | `				}` |
|        5 |  482 | `				return rc;` |
|        2 |  483 | `			}` |
|  2182435 |  484 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   607780 |  485 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   607780 |  486 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1978 |  487 | `				iQuesty++;` |
|   606792 |  488 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      318 |  489 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      158 |  507 | `			}` |
|   303889 |  508 | `		}` |
|  1511365 |  509 | `	}` |
|   685670 |  510 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  511 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  512 | `		if( rc != SXERR_ABORT ){` |
|       17 |  513 | `			rc = SXERR_SYNTAX;` |
|        8 |  514 | `		}` |
|       17 |  515 | `		return rc;` |
|        - |  516 | `	}` |
|   685654 |  517 | `	return SXRET_OK;` |
|   342853 |  518 |  |
|        - |  519 | `/*` |
|        - |  520 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  521 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  522 | ` */` |
|   553482 |  523 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  524 |  |
|   553484 |  525 | `	SyToken *pIn = *ppCur;` |
|        - |  526 | `	/* Jump the first literal seen */` |
|   553484 |  527 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   553460 |  528 | `		pIn++;` |
|   276729 |  529 | `	}` |
|   276773 |  530 | `	for(;;){` |
|   553548 |  531 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       66 |  532 | `			pIn++;` |
|       66 |  533 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       66 |  534 | `				pIn++;` |
|       32 |  535 | `			}` |
|       34 |  536 | `		}else{` |
|   276743 |  537 | `			break;` |
|        - |  538 | `		}` |
|        2 |  539 | `	}` |
|        - |  540 | `	/* Synchronize pointers */` |
|   553484 |  541 | `	*ppCur = pIn;` |
|   553484 |  542 |  |
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
|      196 |  576 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  577 |  |
|      198 |  578 | `	SyToken *pIn = *ppCur;` |
|        - |  579 | `	sxu32 nLine;` |
|        - |  580 | `	sxi32 rc;` |
|        - |  581 | `	/* Jump the 'function' keyword */` |
|      198 |  582 | `	nLine = pIn->nLine;` |
|      198 |  583 | `	pIn++;` |
|      198 |  584 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  585 | `		pIn++;` |
|        1 |  586 | `	}` |
|      198 |  587 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  588 | `		/* Syntax error */` |
|        5 |  589 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  590 | `		if( rc != SXERR_ABORT ){` |
|        5 |  591 | `			rc = SXERR_SYNTAX;` |
|        2 |  592 | `		}` |
|        5 |  593 | `		goto Synchronize;` |
|        - |  594 | `	}` |
|      194 |  595 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      194 |  596 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      194 |  597 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  598 | `		/* Syntax error */` |
|        5 |  599 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  600 | `		if( rc != SXERR_ABORT ){` |
|        5 |  601 | `			rc = SXERR_SYNTAX;` |
|        2 |  602 | `		}` |
|        5 |  603 | `		goto Synchronize;` |
|        - |  604 | `	}` |
|      190 |  605 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  606 | `	/* Skip optional return type declaration ': [?] type ( \| type )*' */` |
|      190 |  607 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      190 |  634 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
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
|      174 |  667 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      174 |  668 | `		pIn++; /* Jump the leading curly '{' */` |
|      174 |  669 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      174 |  670 | `		if( pIn < pEnd ){` |
|      174 |  671 | `			pIn++;` |
|       86 |  672 | `		}` |
|       88 |  673 | `	}else{` |
|        - |  674 | `		/* Syntax error */` |
|      ! 0 |  675 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  676 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  677 | `			return SXERR_ABORT;` |
|        - |  678 | `		}` |
|        - |  679 | `	}` |
|      174 |  680 | `	rc = SXRET_OK;` |
|       98 |  681 | `Synchronize:` |
|        - |  682 | `	/* Synchronize pointers */` |
|      198 |  683 | `	*ppCur = pIn;` |
|      198 |  684 | `	return rc;` |
|      100 |  685 |  |
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
|        - |  779 | ` * Extract a single expression node from the input.` |
|        - |  780 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  781 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  782 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  783 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  784 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  785 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  786 | ` */` |
|  3023248 |  787 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  788 |  |
|        - |  789 | `	ph7_expr_node *pNode;` |
|        - |  790 | `	SyToken *pCur;` |
|        - |  791 | `	sxi32 rc;` |
|        - |  792 | `	/* Allocate a new node */` |
|  3023250 |  793 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3023250 |  794 | `	if( pNode == 0 ){` |
|        - |  795 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  796 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  797 | `		 */` |
|      ! 0 |  798 | `		return SXERR_MEM;` |
|        - |  799 | `	}` |
|        - |  800 | `	/* Zero the structure */` |
|  3023250 |  801 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3023250 |  802 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  803 | `	/* Point to the head of the token stream */` |
|  3023250 |  804 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  805 | `	/* Start collecting tokens */` |
|  3023250 |  806 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  807 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  808 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       20 |  809 | `		pCur++;` |
|       20 |  810 | `		pGen->pIn = pCur;` |
|       20 |  811 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       20 |  812 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       20 |  813 | `		if( rc == SXRET_OK && *ppNode ){` |
|       20 |  814 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        9 |  815 | `		}` |
|       20 |  816 | `		return rc;` |
|        - |  817 | `	}` |
|  3023232 |  818 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  819 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  820 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  821 | `		 */` |
|      324 |  822 | `		pCur++; /* Skip the opening '[' */` |
|      324 |  823 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      324 |  824 | `		if( pCur < pGen->pEnd ){` |
|      324 |  825 | `			pCur++; /* Skip past the closing ']' */` |
|      163 |  826 | `		}else{` |
|      ! 0 |  827 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  828 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  829 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  830 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  831 | `			}` |
|      ! 0 |  832 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  833 | `			return rc;` |
|        - |  834 | `		}` |
|        - |  835 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  836 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  837 | `		 */` |
|      347 |  838 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  839 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  840 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  841 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  842 | `			}else{` |
|       19 |  843 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  844 | `			}` |
|       25 |  845 | `		}else{` |
|      278 |  846 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  847 | `		}` |
|  3023071 |  848 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  849 | `		/* Point to the instance that describe this operator */` |
|   680234 |  850 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  851 | `		/* Advance the stream cursor */` |
|   680234 |  852 | `		pCur++;` |
|  2682794 |  853 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  854 | `		/* Isolate variable */` |
|  1649662 |  855 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   824838 |  856 | `			pCur++; /* Variable variable */` |
|        2 |  857 | `		}` |
|   824826 |  858 | `		if( pCur < pGen->pEnd ){` |
|   824826 |  859 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  860 | `				/* Variable name */` |
|   824798 |  861 | `				pCur++;` |
|   412428 |  862 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  863 | `				pCur++;` |
|        - |  864 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  865 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  866 | `				if( pCur < pGen->pEnd ){` |
|       18 |  867 | `					pCur++;` |
|       10 |  868 | `				}else{` |
|        5 |  869 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  870 | `					if( rc != SXERR_ABORT ){` |
|        5 |  871 | `						rc = SXERR_SYNTAX;` |
|        2 |  872 | `					}` |
|        5 |  873 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  874 | `					return rc;` |
|        - |  875 | `				}` |
|        8 |  876 | `			}` |
|   412410 |  877 | `		}` |
|   824822 |  878 | `		pNode->xCode = PH7_CompileVariable;` |
|  1930264 |  879 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    36590 |  880 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    36590 |  881 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  882 | `			 /* List/Array node */` |
|    24378 |  883 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  884 | `				 /* Assume a literal */` |
|       17 |  885 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  886 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  887 | `			 }else{` |
|    24362 |  888 | `				 pCur += 2;` |
|        - |  889 | `				 /* Collect array/list tokens */` |
|    24362 |  890 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    24362 |  891 | `				 if( pCur < pGen->pEnd ){` |
|    24360 |  892 | `					 pCur++;` |
|    12181 |  893 | `				 }else{` |
|        - |  894 | `					 /* Syntax error */` |
|        4 |  895 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  896 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  897 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  898 | `						 rc = SXERR_SYNTAX;` |
|        1 |  899 | `					 }` |
|        3 |  900 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  901 | `					 return rc;` |
|        - |  902 | `				 }` |
|    24360 |  903 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    24360 |  904 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  905 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  906 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  907 | `						 /* Syntax error */` |
|        3 |  908 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  909 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  910 | `							 rc = SXERR_SYNTAX;` |
|        1 |  911 | `						 }` |
|        3 |  912 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  913 | `						 return rc;` |
|        - |  914 | `					 }` |
|       12 |  915 | `				 }` |
|        2 |  916 | `			 }` |
|    24400 |  917 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  918 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  919 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  920 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  921 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  922 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  923 | `			 pNode->xCode = PH7_CompileYield;` |
|    12197 |  924 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  925 | `			 /* Annonymous function */` |
|      198 |  926 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  927 | `				 /* Assume a literal */` |
|      ! 0 |  928 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  929 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  930 | `			 }else{` |
|        - |  931 | `				 /* Assemble annonymous functions body */` |
|      198 |  932 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      198 |  933 | `				 if( rc != SXRET_OK ){` |
|       25 |  934 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  935 | `					 return rc;` |
|        - |  936 | `				 }` |
|      174 |  937 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  938 | `			  }` |
|    12071 |  939 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    11943 |  940 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  941 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  942 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  943 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       86 |  944 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       86 |  945 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  946 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  947 | `				 return rc;` |
|        - |  948 | `			 }` |
|       86 |  949 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    11942 |  950 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  951 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 |  952 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 |  953 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 |  954 | `		 }else{` |
|        - |  955 | `			 /* Assume a literal */` |
|    11822 |  956 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11822 |  957 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  958 | `		 }` |
|  1499546 |  959 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  960 | `		 /* Constants,function name,namespace path,class name... */` |
|   541648 |  961 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   541648 |  962 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   270825 |  963 | `	 }else{` |
|   939620 |  964 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  965 | `			 /* Point to the code generator routine */` |
|   170616 |  966 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   170616 |  967 | `			 if( pNode->xCode == 0 ){` |
|        3 |  968 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  969 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  970 | `					 rc = SXERR_SYNTAX;` |
|        1 |  971 | `				 }` |
|        3 |  972 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  973 | `				 return rc;` |
|        - |  974 | `			 }` |
|    85306 |  975 | `		 }` |
|        - |  976 | `		/* Advance the stream cursor */` |
|   939618 |  977 | `		pCur++;` |
|        - |  978 | `	 }` |
|        - |  979 | `	/* Point to the end of the token stream */` |
|  3023198 |  980 | `	pNode->pEnd = pCur;` |
|        - |  981 | `	/* Save the node for later processing */` |
|  3023198 |  982 | `	*ppNode = pNode;` |
|        - |  983 | `	/* Synchronize cursors */` |
|  3023198 |  984 | `	pGen->pIn = pCur;` |
|  3023198 |  985 | `	return SXRET_OK;` |
|  1511626 |  986 |  |
|        - |  987 | `/*` |
|        - |  988 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  989 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  990 | ` * level is zero.` |
|        - |  991 | ` */` |
|    72958 |  992 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  993 |  |
|    72960 |  994 | `	SyToken *pCur = pStart;` |
|    72960 |  995 | `	sxi32 iNest = 0;` |
|    72960 |  996 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  997 | `		/* Last expression */` |
|    38916 |  998 | `		return SXERR_EOF;` |
|        - |  999 | `	}` |
|   137408 | 1000 | `	while( pCur < pEnd ){` |
|   124434 | 1001 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    21072 | 1002 | `			break;` |
|        - | 1003 | `		}` |
|   103364 | 1004 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5896 | 1005 | `			iNest++;` |
|   100417 | 1006 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5898 | 1007 | `			iNest--;` |
|     2948 | 1008 | `		}` |
|   103364 | 1009 | `		pCur++;` |
|        2 | 1010 | `	}` |
|    34046 | 1011 | `	*ppNext = pCur;` |
|    34046 | 1012 | `	return SXRET_OK;` |
|    36481 | 1013 |  |
|        - | 1014 | `/*` |
|        - | 1015 | ` * Free an expression tree.` |
|        - | 1016 | ` */` |
|  2587392 | 1017 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1018 |  |
|  2587394 | 1019 | `	if( pNode->pLeft ){` |
|        - | 1020 | `		/* Release the left tree */` |
|   965204 | 1021 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   482601 | 1022 | `	}` |
|  2587394 | 1023 | `	if( pNode->pRight ){` |
|        - | 1024 | `		/* Release the right tree */` |
|   505338 | 1025 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   252668 | 1026 | `	}` |
|  2587394 | 1027 | `	if( pNode->pCond ){` |
|        - | 1028 | `		/* Release the conditional tree used by the ternary operator */` |
|     1974 | 1029 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      986 | 1030 | `	}` |
|  2587394 | 1031 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1032 | `		ph7_expr_node **apArg;` |
|        - | 1033 | `		sxu32 n;` |
|        - | 1034 | `		/* Release node arguments */` |
|   342640 | 1035 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   723376 | 1036 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   380738 | 1037 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   190370 | 1038 | `		}` |
|   342640 | 1039 | `		SySetRelease(&pNode->aNodeArgs);` |
|   171319 | 1040 | `	}` |
|        - | 1041 | `	/* Finally,release this node */` |
|  2587394 | 1042 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2587394 | 1043 |  |
|        - | 1044 | `/*` |
|        - | 1045 | ` * Free an expression tree.` |
|        - | 1046 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1047 | ` */` |
|   685736 | 1048 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1049 |  |
|        - | 1050 | `	ph7_expr_node **apNode;` |
|        - | 1051 | `	sxu32 n;` |
|   685738 | 1052 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3708934 | 1053 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3023198 | 1054 | `		if( apNode[n] ){` |
|   686052 | 1055 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   343025 | 1056 | `		}` |
|  1511600 | 1057 | `	}` |
|   685738 | 1058 | `	return SXRET_OK;` |
|        2 | 1059 |  |
|        - | 1060 | `/*` |
|        - | 1061 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1062 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1063 | ` */` |
|   219706 | 1064 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1065 |  |
|        - | 1066 | `	sxi32 iExprOp;` |
|   219708 | 1067 | `	if( pNode->pOp == 0 ){` |
|   142892 | 1068 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1069 | `	}` |
|    76818 | 1070 | `	iExprOp = pNode->pOp->iOp;` |
|    76818 | 1071 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    48222 | 1072 | `			return TRUE;` |
|        - | 1073 | `	}` |
|    28598 | 1074 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    28594 | 1075 | `		if( pNode->pLeft->pOp ) {` |
|       18 | 1076 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        5 | 1077 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1078 | `				return FALSE;` |
|        1 | 1079 | `			}` |
|    28585 | 1080 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1081 | `			return FALSE;` |
|        - | 1082 | `		}` |
|    28594 | 1083 | `		return TRUE;` |
|        - | 1084 | `	}` |
|        5 | 1085 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1086 | `		return TRUE;` |
|        - | 1087 | `	}` |
|        - | 1088 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1089 | `	return FALSE;` |
|   109855 | 1090 |  |
|        - | 1091 | `/* Forward declaration */` |
|        - | 1092 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1093 | `/* Macro to check if the given node is a terminal.` |
|        - | 1094 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1095 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1096 | ` * linked ternary/elvis node). */` |
|        - | 1097 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1098 | `/*` |
|        - | 1099 | ` * Buid an expression tree for each given function argument.` |
|        - | 1100 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1101 | ` */` |
|   284396 | 1102 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1103 |  |
|        - | 1104 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1105 | `	sxi32 rc;` |
|        - | 1106 | `	/* Process function arguments from left to right */` |
|   284398 | 1107 | `	iCur = 0;` |
|   303433 | 1108 | `	for(;;){` |
|   606868 | 1109 | `		if( iCur >= nToken ){` |
|        - | 1110 | `			/* No more arguments to process */` |
|   284372 | 1111 | `			break;` |
|        - | 1112 | `		}` |
|   322498 | 1113 | `		iNode = iCur;` |
|   322498 | 1114 | `		iNest = 0;` |
|   807128 | 1115 | `		while( iCur < nToken ){` |
|   522756 | 1116 | `			if( apNode[iCur] ){` |
|   511468 | 1117 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    19064 | 1118 | `					break;` |
|   473344 | 1119 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    26228 | 1120 | `					iNest++;` |
|   460231 | 1121 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    26208 | 1122 | `					iNest--;` |
|    13103 | 1123 | `				}` |
|   236671 | 1124 | `			}` |
|   484632 | 1125 | `			iCur++;` |
|        2 | 1126 | `		}` |
|   322498 | 1127 | `		if( iCur > iNode ){` |
|   322492 | 1128 | `			SyString sArgName = {0, 0};` |
|        - | 1129 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1130 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1131 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   324220 | 1132 | `			if( (iCur - iNode) >= 2` |
|   178931 | 1133 | `				&& apNode[iNode]` |
|    35372 | 1134 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    19441 | 1135 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3485 | 1136 | `				&& apNode[iNode+1]` |
|     3462 | 1137 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1138 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      178 | 1139 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      178 | 1140 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      178 | 1141 | `				apNode[iNode] = 0;` |
|      178 | 1142 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      178 | 1143 | `				apNode[iNode+1] = 0;` |
|      178 | 1144 | `				iNode += 2;` |
|        - | 1145 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1146 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      178 | 1147 | `				if( iNode >= iCur ){` |
|        4 | 1148 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1149 | `						pOp->pStart->nLine,` |
|        - | 1150 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1151 | `						&sArgName);` |
|        3 | 1152 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1153 | `						rc = SXERR_SYNTAX;` |
|        1 | 1154 | `					}` |
|        3 | 1155 | `					return rc;` |
|        - | 1156 | `				}` |
|       87 | 1157 | `			}` |
|   322488 | 1158 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1159 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1160 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1161 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1162 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1163 | `					apNode[iNode] = 0;` |
|      ! 0 | 1164 | `			}` |
|   322490 | 1165 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   322490 | 1166 | `			if( apNode[iNode] ){` |
|   322490 | 1167 | `				if( sArgName.nByte > 0 ){` |
|      176 | 1168 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      176 | 1169 | `					apNode[iNode]->sArgName = sArgName;` |
|       87 | 1170 | `				}` |
|        - | 1171 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   322490 | 1172 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   161246 | 1173 | `			}else{` |
|        - | 1174 | `				/* No expression before comma */` |
|      ! 0 | 1175 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1176 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1177 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1178 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1179 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1180 | `				}` |
|      ! 0 | 1181 | `				return rc;` |
|        - | 1182 | `			}` |
|   161246 | 1183 | `		}else{` |
|        - | 1184 | `			/* Comma with no preceding argument */` |
|        7 | 1185 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1186 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1187 | `				rc = SXERR_SYNTAX;` |
|        3 | 1188 | `			}` |
|        7 | 1189 | `			return rc;` |
|        - | 1190 | `		}` |
|        - | 1191 | `		/* Jump trailing comma */` |
|   322490 | 1192 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    38120 | 1193 | `			iCur++;` |
|    38120 | 1194 | `			if( iCur >= nToken ){` |
|        - | 1195 | `				/* Trailing comma after last argument */` |
|       19 | 1196 | `				break;` |
|        - | 1197 | `			}` |
|    19050 | 1198 | `		}` |
|        2 | 1199 | `	}` |
|   284390 | 1200 | `	return SXRET_OK;` |
|   142200 | 1201 |  |
|        - | 1202 | ` /*` |
|        - | 1203 | `  * Create an expression tree from an array of tokens.` |
|        - | 1204 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1205 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1206 | `  */` |
|  1097192 | 1207 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1208 | ` {` |
|        - | 1209 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1210 | `	 ph7_expr_node *pNode;` |
|        - | 1211 | `	 sxi32 iCur;` |
|        - | 1212 | `	 sxi32 rc;` |
|  1097194 | 1213 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1214 | `		 /* TICKET 1433-17: self evaluating node */` |
|   505962 | 1215 | `		 return SXRET_OK;` |
|        - | 1216 | `	 }` |
|        - | 1217 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3631570 | 1218 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1219 | `		 sxi32 iNest;` |
|        - | 1220 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1221 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1222 | `		  */` |
|  3040340 | 1223 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3016466 | 1224 | `			 continue;` |
|        - | 1225 | `		 }` |
|    23876 | 1226 | `		 iNest = 1;` |
|    23876 | 1227 | `		 iLeft = iCur;` |
|        - | 1228 | `		 /* Find the closing parenthesis */` |
|    23876 | 1229 | `		 iCur++;` |
|   158706 | 1230 | `		 while( iCur < nToken ){` |
|   158706 | 1231 | `			 if( apNode[iCur] ){` |
|   158706 | 1232 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1233 | `					 /* Decrement nesting level */` |
|    41382 | 1234 | `					 iNest--;` |
|    41382 | 1235 | `					 if( iNest <= 0 ){` |
|    23876 | 1236 | `						 break;` |
|        2 | 1237 | `					 }` |
|   126079 | 1238 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1239 | `					 /* Increment nesting level */` |
|    17508 | 1240 | `					 iNest++;` |
|     8753 | 1241 | `				 }` |
|    67415 | 1242 | `			 }` |
|   134832 | 1243 | `			 iCur++;` |
|        2 | 1244 | `		 }` |
|    23876 | 1245 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1246 | `			 /* Recurse and process this expression */` |
|    23876 | 1247 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    23876 | 1248 | `			 if( rc != SXRET_OK ){` |
|        3 | 1249 | `				 return rc;` |
|        - | 1250 | `			 }` |
|    11936 | 1251 | `		 }` |
|        - | 1252 | `		 /* Free the left and right nodes */` |
|    23874 | 1253 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    23874 | 1254 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    23874 | 1255 | `		 apNode[iLeft] = 0;` |
|    23874 | 1256 | `		 apNode[iCur] = 0;` |
|    11938 | 1257 | `	 }` |
|        - | 1258 | `	  /* Process expressions enclosed in braces */` |
|  3784154 | 1259 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1260 | `		 sxi32 iNest;` |
|        - | 1261 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1262 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1263 | `		  */` |
|  3199032 | 1264 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3199032 | 1265 | `			 continue;` |
|        - | 1266 | `		 }` |
|      ! 0 | 1267 | `		 iNest = 1;` |
|      ! 0 | 1268 | `		 iLeft = iCur;` |
|        - | 1269 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1270 | `		 iCur++;` |
|      ! 0 | 1271 | `		 while( iCur < nToken ){` |
|      ! 0 | 1272 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1273 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1274 | `					 /* Decrement nesting level */` |
|      ! 0 | 1275 | `					 iNest--;` |
|      ! 0 | 1276 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1277 | `						 break;` |
|      ! 0 | 1278 | `					 }` |
|      ! 0 | 1279 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1280 | `					 /* Increment nesting level */` |
|      ! 0 | 1281 | `					 iNest++;` |
|      ! 0 | 1282 | `				 }` |
|      ! 0 | 1283 | `			 }` |
|      ! 0 | 1284 | `			 iCur++;` |
|      ! 0 | 1285 | `		 }` |
|      ! 0 | 1286 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1287 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1288 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1289 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1290 | `				 return rc;` |
|        - | 1291 | `			 }` |
|      ! 0 | 1292 | `		 }` |
|        - | 1293 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1294 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1295 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1296 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1297 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1298 | `	 }` |
|        - | 1299 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   585124 | 1300 | `	 iLeft = -1;` |
|  3784118 | 1301 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3199008 | 1302 | `		 if( apNode[iCur] == 0 ){` |
|  1244928 | 1303 | `			 continue;` |
|        - | 1304 | `		 }` |
|  1954082 | 1305 | `		 pNode = apNode[iCur];` |
|  1954082 | 1306 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   504456 | 1307 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1308 | `				 /* Collect function arguments */` |
|   323300 | 1309 | `				 sxi32 iPtr = 0;` |
|   323300 | 1310 | `				 sxi32 nFuncTok = 0;` |
|  1169354 | 1311 | `				 while( nFuncTok + iCur < nToken ){` |
|  1169354 | 1312 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1158066 | 1313 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   335088 | 1314 | `							 iPtr++;` |
|   990523 | 1315 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   335088 | 1316 | `							 iPtr--;` |
|   335088 | 1317 | `							 if( iPtr <= 0 ){` |
|   323300 | 1318 | `								 break;` |
|        - | 1319 | `							 }` |
|     5894 | 1320 | `						 }` |
|   417383 | 1321 | `					 }` |
|   846056 | 1322 | `					 nFuncTok++;` |
|        2 | 1323 | `				 }` |
|   323300 | 1324 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1325 | `					 /* Syntax error */` |
|      ! 0 | 1326 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1327 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1328 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1329 | `					 }` |
|      ! 0 | 1330 | `					 return rc;` |
|        - | 1331 | `				 }` |
|   323300 | 1332 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1333 | `					 /* Syntax error */` |
|      ! 0 | 1334 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1335 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1336 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1337 | `					 }` |
|      ! 0 | 1338 | `					 return rc;` |
|        - | 1339 | `				 }` |
|   323300 | 1340 | `				 if( nFuncTok > 1 ){` |
|        - | 1341 | `					 /* Process function arguments */` |
|   284398 | 1342 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   284398 | 1343 | `					 if( rc != SXRET_OK ){` |
|        9 | 1344 | `						 return rc;` |
|        - | 1345 | `					 }` |
|   142194 | 1346 | `				 }` |
|        - | 1347 | `				 /* Link the node to the tree */` |
|   323292 | 1348 | `				 pNode->pLeft = apNode[iLeft];` |
|   323292 | 1349 | `				 apNode[iLeft] = 0;` |
|  1169322 | 1350 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   846032 | 1351 | `					 apNode[iCur+iPtr] = 0;` |
|   423017 | 1352 | `				 }` |
|   342803 | 1353 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1354 | `				 /* Subscripting */` |
|    72430 | 1355 | `				 sxi32 iArrTok = iCur + 1;` |
|    72430 | 1356 | `				 sxi32 iNest = 1;` |
|    72509 | 1357 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1358 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1359 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1360 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    72428 | 1361 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1362 | `						 /* Syntax error */` |
|      ! 0 | 1363 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1364 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1365 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1366 | `						 }` |
|      ! 0 | 1367 | `						 return rc;` |
|        - | 1368 | `				 }` |
|        - | 1369 | `				 /* Collect index tokens */` |
|   130788 | 1370 | `				 while( iArrTok < nToken ){` |
|   130788 | 1371 | `					 if( apNode[iArrTok] ){` |
|   130756 | 1372 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1373 | `							 /* Increment nesting level */` |
|      ! 0 | 1374 | `							 iNest++;` |
|   130756 | 1375 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1376 | `							 /* Decrement nesting level */` |
|    72430 | 1377 | `							 iNest--;` |
|    72430 | 1378 | `							 if( iNest <= 0 ){` |
|    72430 | 1379 | `								 break;` |
|        - | 1380 | `							 }` |
|      ! 0 | 1381 | `						 }` |
|    29163 | 1382 | `					 }` |
|    58360 | 1383 | `					 ++iArrTok;` |
|        2 | 1384 | `				 }` |
|    72430 | 1385 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1386 | `					 /* Recurse and process this expression */` |
|    58250 | 1387 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    58250 | 1388 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1389 | `						 return rc;` |
|        - | 1390 | `					 }` |
|        - | 1391 | `					 /* Link the node to it's index */` |
|    58250 | 1392 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    29124 | 1393 | `				 }` |
|        - | 1394 | `				 /* Link the node to the tree */` |
|    72430 | 1395 | `				 pNode->pLeft = apNode[iLeft];` |
|    72430 | 1396 | `				 pNode->pRight = 0;` |
|    72430 | 1397 | `				 apNode[iLeft] = 0;` |
|   203216 | 1398 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   130788 | 1399 | `					 apNode[iNest] = 0;` |
|    65395 | 1400 | `				 }` |
|    36216 | 1401 | `			 }else{` |
|        - | 1402 | `				 /* Member access operators [i.e: '->','::'] */` |
|   108730 | 1403 | `				  iRight = iCur + 1;` |
|   108730 | 1404 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1405 | `					 iRight++;` |
|      ! 0 | 1406 | `				 }` |
|   108730 | 1407 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1408 | `					 /* Syntax error */` |
|        5 | 1409 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1410 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1411 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1412 | `					 }` |
|        5 | 1413 | `					 return rc;` |
|        - | 1414 | `				 }` |
|        - | 1415 | `				 /* Link the node to the tree */` |
|   108726 | 1416 | `				 pNode->pLeft = apNode[iLeft];` |
|   108726 | 1417 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   108450 | 1418 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1419 | `						 /* Syntax error */` |
|      ! 0 | 1420 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1421 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1422 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1423 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1424 | `						 }` |
|      ! 0 | 1425 | `						 return rc;` |
|        - | 1426 | `				 }` |
|   108726 | 1427 | `				 pNode->pRight = apNode[iRight];` |
|   108726 | 1428 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1429 | `			 }` |
|   252221 | 1430 | `		 }` |
|  1954070 | 1431 | `		 iLeft = iCur;` |
|   977036 | 1432 | `	 }` |
|        - | 1433 | `	 /* Handle left associative (new, clone) operators */` |
|  3784086 | 1434 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3198976 | 1435 | `		 if( apNode[iCur] == 0 ){` |
|  1764074 | 1436 | `			 continue;` |
|        - | 1437 | `		 }` |
|  1434904 | 1438 | `		 pNode = apNode[iCur];` |
|  1434904 | 1439 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1440 | `			 SyToken *pToken;` |
|        - | 1441 | `			 /* Get the left node */` |
|    14706 | 1442 | `			 iLeft = iCur + 1;` |
|    29380 | 1443 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    14676 | 1444 | `				 iLeft++;` |
|        2 | 1445 | `			 }` |
|    14706 | 1446 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1447 | `				  /* Syntax error */` |
|      ! 0 | 1448 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1449 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1450 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1451 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1452 | `				 }` |
|      ! 0 | 1453 | `				 return rc;` |
|        - | 1454 | `			 }` |
|        - | 1455 | `			 /* Make sure the operand are of a valid type */` |
|    14706 | 1456 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1457 | `				 /* Clone:` |
|        - | 1458 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1459 | `				  *  ++ function call (including annonymous)` |
|        - | 1460 | `				  *  ++ array member` |
|        - | 1461 | `				  *  ++ 'new' operator` |
|        - | 1462 | `				  * Example:` |
|        - | 1463 | `				  *   clone $pObj;` |
|        - | 1464 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1465 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1466 | `				  */` |
|       18 | 1467 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1468 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1469 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1470 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1471 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1472 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1473 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1474 | `						 }` |
|      ! 0 | 1475 | `						 return rc;` |
|        - | 1476 | `					 }` |
|        7 | 1477 | `				 }` |
|       10 | 1478 | `			 }else{` |
|        - | 1479 | `				 /* New */` |
|    14690 | 1480 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1481 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1482 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1483 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1484 | `						 /* Syntax error */` |
|      ! 0 | 1485 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1486 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1487 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1488 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1489 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1490 | `						 }` |
|      ! 0 | 1491 | `						 return rc;` |
|        - | 1492 | `					 }` |
|        8 | 1493 | `				 }` |
|        - | 1494 | `			 }` |
|        - | 1495 | `			  /* Link the node to the tree */` |
|    14706 | 1496 | `			 pNode->pLeft = apNode[iLeft];` |
|    14706 | 1497 | `			 apNode[iLeft] = 0;` |
|    14706 | 1498 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7352 | 1499 | `		 }` |
|   717453 | 1500 | `	 }` |
|        - | 1501 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   585112 | 1502 | `	 iLeft = -1;` |
|  3784086 | 1503 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3195922 | 1504 | `		 if( apNode[iCur] == 0 ){` |
|  1764074 | 1505 | `			 continue;` |
|        - | 1506 | `		 }` |
|  1431850 | 1507 | `		 pNode = apNode[iCur];` |
|  1431850 | 1508 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8494 | 1509 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3072 | 1510 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1511 | `					 /* Link the node to the tree */` |
|     3074 | 1512 | `					 pNode->pLeft = apNode[iLeft];` |
|     3074 | 1513 | `					 apNode[iLeft] = 0;` |
|     1536 | 1514 | `			 }` |
|     5773 | 1515 | `		  }` |
|  1434904 | 1516 | `		 iLeft = iCur;` |
|   717453 | 1517 | `	  }` |
|   588166 | 1518 | `	 iLeft = -1;` |
|  3787140 | 1519 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3198976 | 1520 | `		 if( apNode[iCur] == 0 ){` |
|  1767146 | 1521 | `			 continue;` |
|        - | 1522 | `		 }` |
|  1431832 | 1523 | `		 pNode = apNode[iCur];` |
|  1431832 | 1524 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8475 | 1525 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8476 | 1526 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1527 | `					 /* Syntax error */` |
|      ! 0 | 1528 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1529 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1530 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1531 | `					 }` |
|      ! 0 | 1532 | `					 return rc;` |
|        - | 1533 | `			 }` |
|        - | 1534 | `			 /* Link the node to the tree */` |
|     8476 | 1535 | `			 pNode->pLeft = apNode[iLeft];` |
|     8476 | 1536 | `			 apNode[iLeft] = 0;` |
|        - | 1537 | `			 /* Mark as pre-increment/decrement node */` |
|     8476 | 1538 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4237 | 1539 | `		  }` |
|  1431832 | 1540 | `		 iLeft = iCur;` |
|   715917 | 1541 | `	 }` |
|        - | 1542 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   588166 | 1543 | `	  iLeft = 0;` |
|  3787134 | 1544 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3198972 | 1545 | `		  if( apNode[iCur] ){` |
|  1423354 | 1546 | `			  pNode = apNode[iCur];` |
|  1423354 | 1547 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    37970 | 1548 | `				  if( iLeft > 0 ){` |
|        - | 1549 | `					  /* Link the node to the tree */` |
|    37968 | 1550 | `					  pNode->pLeft = apNode[iLeft];` |
|    37968 | 1551 | `					  apNode[iLeft] = 0;` |
|    37968 | 1552 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1553 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1554 | `							   /* Syntax error */` |
|      ! 0 | 1555 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1556 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1557 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1558 | `							  }` |
|      ! 0 | 1559 | `							  return rc;` |
|        - | 1560 | `						  }` |
|       36 | 1561 | `					  }` |
|    18985 | 1562 | `				  }else{` |
|        - | 1563 | `					  /* Syntax error */` |
|        3 | 1564 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1565 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1566 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1567 | `					  }` |
|        3 | 1568 | `					  return rc;` |
|        - | 1569 | `				  }` |
|    18983 | 1570 | `			  }` |
|        - | 1571 | `			  /* Save terminal position */` |
|  1423352 | 1572 | `			  iLeft = iCur;` |
|   711675 | 1573 | `		  }` |
|  1599486 | 1574 | `	  }` |
|        - | 1575 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6469708 | 1576 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5881554 | 1577 | `		 iLeft = -1;` |
| 37870988 | 1578 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 31989444 | 1579 | `			 if( apNode[iCur] == 0 ){` |
| 20416776 | 1580 | `				 continue;` |
|        - | 1581 | `			 }` |
| 11572670 | 1582 | `			 pNode = apNode[iCur];` |
| 11572670 | 1583 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1584 | `				 /* Get the right node */` |
|   174956 | 1585 | `				 iRight = iCur + 1;` |
|   248460 | 1586 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    73506 | 1587 | `					 iRight++;` |
|        2 | 1588 | `				 }` |
|   174956 | 1589 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1590 | `					 /* Syntax error */` |
|        9 | 1591 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1592 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1593 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1594 | `					 }` |
|        9 | 1595 | `					 return rc;` |
|        - | 1596 | `				 }` |
|   174948 | 1597 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1598 | `					 sxi32  iTmp;` |
|        - | 1599 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       48 | 1600 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1601 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1602 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1603 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1604 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1605 | `						 }` |
|      ! 0 | 1606 | `						 return rc;` |
|        - | 1607 | `					 }` |
|       48 | 1608 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1609 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1610 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1611 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1612 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1613 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1614 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1615 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1616 | `									 }` |
|      ! 0 | 1617 | `									 return rc;` |
|        - | 1618 | `							 }` |
|      ! 0 | 1619 | `						 }` |
|       16 | 1620 | `					 }` |
|        - | 1621 | `					 /* Swap operands */` |
|       48 | 1622 | `					 iTmp = iRight;` |
|       48 | 1623 | `					 iRight = iLeft;` |
|       48 | 1624 | `					 iLeft = iTmp;` |
|       23 | 1625 | `				 }` |
|        - | 1626 | `				 /* Link the node to the tree */` |
|   174948 | 1627 | `				 pNode->pLeft = apNode[iLeft];` |
|   174948 | 1628 | `				 pNode->pRight = apNode[iRight];` |
|   174948 | 1629 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    87473 | 1630 | `			 }` |
| 11572662 | 1631 | `			 iLeft = iCur;` |
|  5786332 | 1632 | `		 }` |
|  2940774 | 1633 | `	 }` |
|        - | 1634 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1635 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1636 | `	  * we are dealing with a single operator.` |
|        - | 1637 | `	  */` |
|   588156 | 1638 | `	  iLeft = -1;` |
|  3778562 | 1639 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3192382 | 1640 | `		  if( apNode[iCur] == 0 ){` |
|  2162790 | 1641 | `			  continue;` |
|        - | 1642 | `		  }` |
|  1029594 | 1643 | `		  pNode = apNode[iCur];` |
|  1029594 | 1644 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1976 | 1645 | `			  sxi32 iNest = 1;` |
|     1976 | 1646 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1647 | `				  /* Missing condition */` |
|        3 | 1648 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1649 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1650 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1651 | `				  }` |
|        3 | 1652 | `				  return rc;` |
|        - | 1653 | `			  }` |
|        - | 1654 | `			  /* Get the right node */` |
|     1974 | 1655 | `			  iRight = iCur + 1;` |
|     4182 | 1656 | `			  while( iRight < nToken  ){` |
|     4182 | 1657 | `				  if( apNode[iRight] ){` |
|     3878 | 1658 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1659 | `						  /* Increment nesting level */` |
|      ! 0 | 1660 | `						  ++iNest;` |
|     3878 | 1661 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1662 | `						  /* Decrement nesting level */` |
|     1974 | 1663 | `						  --iNest;` |
|     1974 | 1664 | `						  if( iNest <= 0 ){` |
|     1974 | 1665 | `							  break;` |
|        - | 1666 | `						  }` |
|      ! 0 | 1667 | `					  }` |
|      952 | 1668 | `				  }` |
|     2210 | 1669 | `				  iRight++;` |
|        2 | 1670 | `			  }` |
|     1974 | 1671 | `			  if( iRight > iCur + 1 ){` |
|        - | 1672 | `				  /* Recurse and process the then expression */` |
|     1906 | 1673 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1906 | 1674 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1675 | `					  return rc;` |
|        - | 1676 | `				  }` |
|        - | 1677 | `				  /* Link the node to the tree */` |
|     1906 | 1678 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      952 | 1679 | `			  }else{` |
|        - | 1680 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1681 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1682 | `			  }` |
|     1974 | 1683 | `			  apNode[iCur + 1] = 0;` |
|     1974 | 1684 | `			  if( iRight + 1 < nToken ){` |
|        - | 1685 | `				  /* Recurse and process the else expression */` |
|     1974 | 1686 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1974 | 1687 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1688 | `					  return rc;` |
|        - | 1689 | `				  }` |
|        - | 1690 | `				  /* Link the node to the tree */` |
|     1974 | 1691 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1974 | 1692 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      988 | 1693 | `			  }else{` |
|      ! 0 | 1694 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1695 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1696 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1697 | `				 }` |
|      ! 0 | 1698 | `				 return rc;` |
|        - | 1699 | `			  }` |
|        - | 1700 | `			  /* Point to the condition */` |
|     1974 | 1701 | `			  pNode->pCond  = apNode[iLeft];` |
|     1974 | 1702 | `			  apNode[iLeft] = 0;` |
|     1974 | 1703 | `			  break;` |
|        - | 1704 | `		  }` |
|  1027620 | 1705 | `		  iLeft = iCur;` |
|   513811 | 1706 | `	  }` |
|        - | 1707 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1708 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1709 | `	  * so there is no need for a precedence loop here.` |
|        - | 1710 | `	  */` |
|   588154 | 1711 | `	 iRight = -1;` |
|  3786990 | 1712 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3198884 | 1713 | `		 if( apNode[iCur] == 0 ){` |
|  2390940 | 1714 | `			 continue;` |
|        - | 1715 | `		 }` |
|   807946 | 1716 | `		 pNode = apNode[iCur];` |
|   807946 | 1717 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1718 | `			 /* Get the left node */` |
|   219672 | 1719 | `			 iLeft = iCur - 1;` |
|   310956 | 1720 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    91286 | 1721 | `				 iLeft--;` |
|        2 | 1722 | `			 }` |
|   219672 | 1723 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1724 | `				 /* Syntax error */` |
|       43 | 1725 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1726 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1727 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1728 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1729 | `				 }else{` |
|       39 | 1730 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1731 | `				 }` |
|       43 | 1732 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1733 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1734 | `				 }` |
|       43 | 1735 | `				 return rc;` |
|        - | 1736 | `			 }` |
|   219630 | 1737 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1738 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1739 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1740 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1741 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1742 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1743 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1744 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1745 | `					 }else{` |
|        4 | 1746 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1747 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1748 | `					 }` |
|        5 | 1749 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1750 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1751 | `					 }` |
|        5 | 1752 | `					 return rc;` |
|        - | 1753 | `				 }` |
|       26 | 1754 | `			 }` |
|        - | 1755 | `			 /* Link the node to the tree (Reverse) */` |
|   219626 | 1756 | `			 pNode->pLeft = apNode[iRight];` |
|   219626 | 1757 | `			 pNode->pRight = apNode[iLeft];` |
|   219626 | 1758 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   109812 | 1759 | `		 }` |
|   807900 | 1760 | `		 iRight = iCur;` |
|   403951 | 1761 | `	 }` |
|        - | 1762 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2940532 | 1763 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2352426 | 1764 | `		 iLeft = -1;` |
| 15147746 | 1765 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12795322 | 1766 | `			 if( apNode[iCur] == 0 ){` |
| 10442492 | 1767 | `				 continue;` |
|        - | 1768 | `			 }` |
|  2352832 | 1769 | `			 pNode = apNode[iCur];` |
|  2352832 | 1770 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1771 | `				 /* Get the right node */` |
|       72 | 1772 | `				 iRight = iCur + 1;` |
|      110 | 1773 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1774 | `					 iRight++;` |
|        2 | 1775 | `				 }` |
|       72 | 1776 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1777 | `					 /* Syntax error */` |
|      ! 0 | 1778 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1779 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1780 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1781 | `					 }` |
|      ! 0 | 1782 | `					 return rc;` |
|        - | 1783 | `				 }` |
|        - | 1784 | `				 /* Link the node to the tree */` |
|       72 | 1785 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1786 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1787 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1788 | `			 }` |
|  2352832 | 1789 | `			 iLeft = iCur;` |
|  1176417 | 1790 | `		 }` |
|  1176214 | 1791 | `	 }` |
|        - | 1792 | `	 /* Point to the root of the expression tree */` |
|  3198804 | 1793 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2610716 | 1794 | `		 if( apNode[iCur] ){` |
|   530872 | 1795 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1796 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1797 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1798 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1799 | `				  }` |
|       20 | 1800 | `				  return rc;` |
|        - | 1801 | `			 }` |
|   530854 | 1802 | `			 apNode[0] = apNode[iCur];` |
|   530854 | 1803 | `			 apNode[iCur] = 0;` |
|   265426 | 1804 | `		 }` |
|  1305350 | 1805 | `	 }` |
|   588090 | 1806 | `	 return SXRET_OK;` |
|   547071 | 1807 | ` }` |
|        - | 1808 | ` /*` |
|        - | 1809 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1810 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1811 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1812 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1813 | `  */` |
|   685736 | 1814 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1815 |  |
|        - | 1816 | `	ph7_expr_node **apNode;` |
|        - | 1817 | `	ph7_expr_node *pNode;` |
|        - | 1818 | `	sxi32 rc;` |
|        - | 1819 | `	/* Reset node container */` |
|   685738 | 1820 | `	SySetReset(pExprNode);` |
|   685738 | 1821 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1822 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1823 | `	{` |
|   685738 | 1824 | `		int iLastWasTerm = 0;` |
|  3708934 | 1825 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3023232 | 1826 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3023232 | 1827 | `			if( rc != SXRET_OK ){` |
|       35 | 1828 | `				return rc;` |
|        - | 1829 | `			}` |
|        - | 1830 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3023198 | 1831 | `			if( pNode->xCode ){` |
|        - | 1832 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1573962 | 1833 | `				iLastWasTerm = 1;` |
|  2236218 | 1834 | `			}else if( pNode->pOp ){` |
|        - | 1835 | `				/* Operator node */` |
|   680234 | 1836 | `				iLastWasTerm = 0;` |
|   340118 | 1837 | `			}else{` |
|        - | 1838 | `				/* Delimiter: ')' and ']' end terms */` |
|   769006 | 1839 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1840 | `			}` |
|        - | 1841 | `			/* Save the extracted node */` |
|  3023198 | 1842 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1843 | `		}` |
|        - | 1844 | `	}` |
|   685704 | 1845 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1846 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1847 | `		*ppRoot = 0;` |
|      ! 0 | 1848 | `		return SXRET_OK;` |
|        - | 1849 | `	}` |
|   685704 | 1850 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1851 | `	/* Make sure we are dealing with valid nodes */` |
|   685704 | 1852 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   685704 | 1853 | `	if( rc != SXRET_OK ){` |
|        - | 1854 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1855 | `		 * cleanup the mess left behind.` |
|        - | 1856 | `		 */` |
|       51 | 1857 | `		*ppRoot = 0;` |
|       51 | 1858 | `		return rc;` |
|        - | 1859 | `	}` |
|        - | 1860 | `	/* Build the tree */` |
|   685654 | 1861 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   685654 | 1862 | `	if( rc != SXRET_OK ){` |
|        - | 1863 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       90 | 1864 | `		*ppRoot = 0;` |
|       90 | 1865 | `		return rc;` |
|        - | 1866 | `	}` |
|        - | 1867 | `	/* Point to the root of the tree */` |
|   685566 | 1868 | `	*ppRoot = apNode[0];` |
|   685566 | 1869 | `	return SXRET_OK;` |
|   342870 | 1870 |  |
|        - | 1871 |  |
