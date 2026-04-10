# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 852/998 lines (85.37%)

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
|   736330 |  264 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  265 |  |
|   736332 |  266 | `	sxu32 n = 0;` |
|        - |  267 | `	sxi32 rc;` |
|        - |  268 | `	/* Do a linear lookup on the operators table */` |
| 12067276 |  269 | `	for(;;){` |
| 24134554 |  270 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  271 | `			break;` |
|        - |  272 | `		}` |
| 24134554 |  273 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  274 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2935956 |  275 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1467979 |  276 | `		}else{` |
| 21198600 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  278 | `		}` |
| 24134554 |  279 | `		if( rc == 0 ){` |
|   739570 |  280 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  281 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   736002 |  282 | `				return &aOpTable[n];` |
|        - |  283 | `			}` |
|        - |  284 | `			/* Handle ambiguity */` |
|     3570 |  285 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  286 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  287 | `				return &aOpTable[n];` |
|        - |  288 | `			}` |
|     3350 |  289 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  290 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  291 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  292 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  293 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  294 | `					return &aOpTable[n];` |
|        - |  295 | `				}` |
|        - |  296 |  |
|        4 |  297 | `			}` |
|     1619 |  298 | `		}` |
| 23398224 |  299 | `		++n; /* Next operator in the table */` |
|        2 |  300 | `	}` |
|        - |  301 | `	/* No such operator */` |
|      ! 0 |  302 | `	return 0;` |
|   368167 |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * Delimit a set of token stream.` |
|        - |  306 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  307 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  308 | ` */` |
|   378682 |  309 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  310 |  |
|   378684 |  311 | `	SyToken *pCur = pIn;` |
|   378684 |  312 | `	sxi32 iNest = 1;` |
|  2152274 |  313 | `	for(;;){` |
|  4304550 |  314 | `		if( pCur >= pEnd ){` |
|      124 |  315 | `			break;` |
|        - |  316 | `		}` |
|  4304428 |  317 | `		if( pCur->nType & nTokStart ){` |
|        - |  318 | `			/* Increment nesting level */` |
|   238122 |  319 | `			iNest++;` |
|  4185368 |  320 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  321 | `			/* Decrement nesting level */` |
|   616682 |  322 | `			iNest--;` |
|   616682 |  323 | `			if( iNest <= 0 ){` |
|   378562 |  324 | `				break;` |
|        - |  325 | `			}` |
|   119060 |  326 | `		}` |
|        - |  327 | `		/* Advance cursor */` |
|  3925868 |  328 | `		pCur++;` |
|        2 |  329 | `	}` |
|        - |  330 | `	/* Point to the end of the chunk */` |
|   378684 |  331 | `	*ppEnd = pCur;` |
|   378684 |  332 |  |
|        - |  333 | `/*` |
|        - |  334 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  335 | ` * Note on reserved keywords.` |
|        - |  336 | ` *  According to the PHP language reference manual:` |
|        - |  337 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  338 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  339 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  340 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  341 | ` */` |
|    11406 |  342 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  343 |  |
|    17043 |  344 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11313 |  345 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  346 | `		){` |
|      146 |  347 | `			return TRUE;` |
|        - |  348 | `	}` |
|    11264 |  349 | `	if( bCheckFunc ){` |
|       92 |  350 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  351 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  352 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  353 | `				return TRUE;` |
|        - |  354 | `		}` |
|       20 |  355 | `	}` |
|        - |  356 | `	/* Not a language construct */` |
|    11232 |  357 | `	return FALSE;` |
|     5705 |  358 |  |
|        - |  359 | `/*` |
|        - |  360 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  361 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  362 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  363 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  364 | ` */` |
|   648482 |  365 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  366 |  |
|        - |  367 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  368 | `	sxi32 i,rc;` |
|        - |  369 |  |
|   648484 |  370 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  371 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  372 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  373 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  374 | `	}` |
|   648484 |  375 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3507488 |  376 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2859040 |  377 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  378 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      290 |  379 | `			continue;` |
|        - |  380 | `		}` |
|  2858752 |  381 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   328372 |  382 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    16904 |  383 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  384 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   305744 |  385 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  386 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  387 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  388 | `						 */` |
|   305744 |  389 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   305744 |  390 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   305744 |  391 | `						apNode[i]->pOp = &sFCallOp;` |
|   152871 |  392 | `					}` |
|   152871 |  393 | `			}` |
|   328372 |  394 | `			iParen++;` |
|  2694567 |  395 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   328372 |  396 | `			if( iParen <= 0 ){` |
|       13 |  397 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  398 | `				if( rc != SXERR_ABORT ){` |
|       13 |  399 | `					rc = SXERR_SYNTAX;` |
|        6 |  400 | `				}` |
|       13 |  401 | `				return rc;` |
|        - |  402 | `			}` |
|   328360 |  403 | `			iParen--;` |
|  2366191 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    68546 |  405 | `			iSquare++;` |
|  2167740 |  406 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    68560 |  407 | `			if( iSquare <= 0 ){` |
|        7 |  408 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  409 | `				if( rc != SXERR_ABORT ){` |
|        7 |  410 | `					rc = SXERR_SYNTAX;` |
|        3 |  411 | `				}` |
|        7 |  412 | `				return rc;` |
|        - |  413 | `			}` |
|    68554 |  414 | `			iSquare--;` |
|  2099186 |  415 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2064905 |  462 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  463 | `			if( iBraces <= 0 ){` |
|       13 |  464 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  465 | `				if( rc != SXERR_ABORT ){` |
|       13 |  466 | `					rc = SXERR_SYNTAX;` |
|        6 |  467 | `				}` |
|       13 |  468 | `				return rc;` |
|        - |  469 | `			}` |
|      ! 0 |  470 | `			iBraces--;` |
|  2064888 |  471 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1902 |  472 | `			if( iQuesty <= 0 ){` |
|        5 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  474 | `				if( rc != SXERR_ABORT ){` |
|        5 |  475 | `					rc = SXERR_SYNTAX;` |
|        2 |  476 | `				}` |
|        5 |  477 | `				return rc;` |
|        - |  478 | `			}` |
|     1898 |  479 | `			iQuesty--;` |
|  2063936 |  480 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   574756 |  481 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   574756 |  482 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1900 |  483 | `				iQuesty++;` |
|   573807 |  484 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   287377 |  504 | `		}` |
|  1429360 |  505 | `	}` |
|   648450 |  506 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  507 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  508 | `		if( rc != SXERR_ABORT ){` |
|       17 |  509 | `			rc = SXERR_SYNTAX;` |
|        8 |  510 | `		}` |
|       17 |  511 | `		return rc;` |
|        - |  512 | `	}` |
|   648434 |  513 | `	return SXRET_OK;` |
|   324243 |  514 |  |
|        - |  515 | `/*` |
|        - |  516 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  517 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  518 | ` */` |
|   523048 |  519 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  520 |  |
|   523050 |  521 | `	SyToken *pIn = *ppCur;` |
|        - |  522 | `	/* Jump the first literal seen */` |
|   523050 |  523 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   523028 |  524 | `		pIn++;` |
|   261513 |  525 | `	}` |
|   261554 |  526 | `	for(;;){` |
|   523110 |  527 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       62 |  528 | `			pIn++;` |
|       62 |  529 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       62 |  530 | `				pIn++;` |
|       30 |  531 | `			}` |
|       32 |  532 | `		}else{` |
|   261526 |  533 | `			break;` |
|        - |  534 | `		}` |
|        2 |  535 | `	}` |
|        - |  536 | `	/* Synchronize pointers */` |
|   523050 |  537 | `	*ppCur = pIn;` |
|   523050 |  538 |  |
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
|        - |  602 | `	/* Skip optional return type declaration ': [?] type' */` |
|      188 |  603 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  604 | `		pIn++; /* Skip ':' */` |
|        - |  605 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  606 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  607 | `			pIn++;` |
|      ! 0 |  608 | `		}` |
|        - |  609 | `		/* Skip the type name (keyword or identifier) */` |
|        5 |  610 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  611 | `			pIn++;` |
|        2 |  612 | `		}` |
|        2 |  613 | `	}` |
|      188 |  614 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       32 |  615 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  616 | `		/* Check if we are dealing with a closure */` |
|       32 |  617 | `		if( nKey == PH7_TKWRD_USE ){` |
|       24 |  618 | `			pIn++; /* Jump the 'use' keyword */` |
|       24 |  619 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  620 | `				/* Syntax error */` |
|        5 |  621 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  622 | `				if( rc != SXERR_ABORT ){` |
|        5 |  623 | `					rc = SXERR_SYNTAX;` |
|        2 |  624 | `				}` |
|        5 |  625 | `				goto Synchronize;` |
|        - |  626 | `			}` |
|       20 |  627 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       20 |  628 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       20 |  629 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  630 | `				/* Syntax error */` |
|        5 |  631 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  632 | `				if( rc != SXERR_ABORT ){` |
|        5 |  633 | `					rc = SXERR_SYNTAX;` |
|        2 |  634 | `				}` |
|        5 |  635 | `				goto Synchronize;` |
|        - |  636 | `			}` |
|       16 |  637 | `			pIn++;` |
|        9 |  638 | `		}else{` |
|        - |  639 | `			/* Syntax error */` |
|        9 |  640 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  641 | `			if( rc != SXERR_ABORT ){` |
|        9 |  642 | `				rc = SXERR_SYNTAX;` |
|        4 |  643 | `			}` |
|        9 |  644 | `			goto Synchronize;` |
|        - |  645 | `		}` |
|        7 |  646 | `	}` |
|      172 |  647 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      172 |  648 | `		pIn++; /* Jump the leading curly '{' */` |
|      172 |  649 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      172 |  650 | `		if( pIn < pEnd ){` |
|      172 |  651 | `			pIn++;` |
|       85 |  652 | `		}` |
|       87 |  653 | `	}else{` |
|        - |  654 | `		/* Syntax error */` |
|      ! 0 |  655 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  656 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  657 | `			return SXERR_ABORT;` |
|        - |  658 | `		}` |
|        - |  659 | `	}` |
|      172 |  660 | `	rc = SXRET_OK;` |
|       97 |  661 | `Synchronize:` |
|        - |  662 | `	/* Synchronize pointers */` |
|      196 |  663 | `	*ppCur = pIn;` |
|      196 |  664 | `	return rc;` |
|       99 |  665 |  |
|        - |  666 | `/*` |
|        - |  667 | ` * Extract a single expression node from the input.` |
|        - |  668 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  669 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  670 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  671 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  672 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  673 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  674 | ` */` |
|  2859202 |  675 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  676 |  |
|        - |  677 | `	ph7_expr_node *pNode;` |
|        - |  678 | `	SyToken *pCur;` |
|        - |  679 | `	sxi32 rc;` |
|        - |  680 | `	/* Allocate a new node */` |
|  2859204 |  681 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2859204 |  682 | `	if( pNode == 0 ){` |
|        - |  683 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  684 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  685 | `		 */` |
|      ! 0 |  686 | `		return SXERR_MEM;` |
|        - |  687 | `	}` |
|        - |  688 | `	/* Zero the structure */` |
|  2859204 |  689 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2859204 |  690 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  691 | `	/* Point to the head of the token stream */` |
|  2859204 |  692 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  693 | `	/* Start collecting tokens */` |
|  2859204 |  694 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  695 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  696 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       15 |  697 | `		pCur++;` |
|       15 |  698 | `		pGen->pIn = pCur;` |
|       15 |  699 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       15 |  700 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       15 |  701 | `		if( rc == SXRET_OK && *ppNode ){` |
|       15 |  702 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        7 |  703 | `		}` |
|       15 |  704 | `		return rc;` |
|        - |  705 | `	}` |
|  2859190 |  706 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  707 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  708 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  709 | `		 */` |
|      292 |  710 | `		pCur++; /* Skip the opening '[' */` |
|      292 |  711 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      292 |  712 | `		if( pCur < pGen->pEnd ){` |
|      292 |  713 | `			pCur++; /* Skip past the closing ']' */` |
|      147 |  714 | `		}else{` |
|      ! 0 |  715 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  716 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  717 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  718 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  719 | `			}` |
|      ! 0 |  720 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  721 | `			return rc;` |
|        - |  722 | `		}` |
|        - |  723 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  724 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  725 | `		 */` |
|      315 |  726 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  727 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  728 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  729 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  730 | `			}else{` |
|       19 |  731 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  732 | `			}` |
|       25 |  733 | `		}else{` |
|      246 |  734 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  735 | `		}` |
|  2859045 |  736 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  737 | `		/* Point to the instance that describe this operator */` |
|   643334 |  738 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  739 | `		/* Advance the stream cursor */` |
|   643334 |  740 | `		pCur++;` |
|  2537234 |  741 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  742 | `		/* Isolate variable */` |
|  1559882 |  743 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   779948 |  744 | `			pCur++; /* Variable variable */` |
|        2 |  745 | `		}` |
|   779936 |  746 | `		if( pCur < pGen->pEnd ){` |
|   779936 |  747 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  748 | `				/* Variable name */` |
|   779908 |  749 | `				pCur++;` |
|   389983 |  750 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  751 | `				pCur++;` |
|        - |  752 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  753 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  754 | `				if( pCur < pGen->pEnd ){` |
|       18 |  755 | `					pCur++;` |
|       10 |  756 | `				}else{` |
|        5 |  757 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  758 | `					if( rc != SXERR_ABORT ){` |
|        5 |  759 | `						rc = SXERR_SYNTAX;` |
|        2 |  760 | `					}` |
|        5 |  761 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  762 | `					return rc;` |
|        - |  763 | `				}` |
|        8 |  764 | `			}` |
|   389965 |  765 | `		}` |
|   779932 |  766 | `		pNode->xCode = PH7_CompileVariable;` |
|  1825599 |  767 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    34644 |  768 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    34644 |  769 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  770 | `			 /* List/Array node */` |
|    23146 |  771 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  772 | `				 /* Assume a literal */` |
|       17 |  773 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  774 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  775 | `			 }else{` |
|    23130 |  776 | `				 pCur += 2;` |
|        - |  777 | `				 /* Collect array/list tokens */` |
|    23130 |  778 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    23130 |  779 | `				 if( pCur < pGen->pEnd ){` |
|    23128 |  780 | `					 pCur++;` |
|    11565 |  781 | `				 }else{` |
|        - |  782 | `					 /* Syntax error */` |
|        4 |  783 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  784 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  785 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  786 | `						 rc = SXERR_SYNTAX;` |
|        1 |  787 | `					 }` |
|        3 |  788 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  789 | `					 return rc;` |
|        - |  790 | `				 }` |
|    23128 |  791 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    23128 |  792 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  793 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  794 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  795 | `						 /* Syntax error */` |
|        3 |  796 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  797 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  798 | `							 rc = SXERR_SYNTAX;` |
|        1 |  799 | `						 }` |
|        3 |  800 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  801 | `						 return rc;` |
|        - |  802 | `					 }` |
|       12 |  803 | `				 }` |
|        2 |  804 | `			 }` |
|    23070 |  805 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  806 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  807 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  808 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  809 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  810 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  811 | `			 pNode->xCode = PH7_CompileYield;` |
|    11484 |  812 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  813 | `			 /* Annonymous function */` |
|      196 |  814 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  815 | `				 /* Assume a literal */` |
|      ! 0 |  816 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  817 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  818 | `			 }else{` |
|        - |  819 | `				 /* Assemble annonymous functions body */` |
|      196 |  820 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      196 |  821 | `				 if( rc != SXRET_OK ){` |
|       25 |  822 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  823 | `					 return rc;` |
|        - |  824 | `				 }` |
|      172 |  825 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  826 | `			  }` |
|    11359 |  827 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  828 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 |  829 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 |  830 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 |  831 | `		 }else{` |
|        - |  832 | `			 /* Assume a literal */` |
|    11196 |  833 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11196 |  834 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  835 | `		 }` |
|  1418299 |  836 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  837 | `		 /* Constants,function name,namespace path,class name... */` |
|   511840 |  838 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   511840 |  839 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   255921 |  840 | `	 }else{` |
|   889154 |  841 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  842 | `			 /* Point to the code generator routine */` |
|   161912 |  843 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   161912 |  844 | `			 if( pNode->xCode == 0 ){` |
|        3 |  845 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  846 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  847 | `					 rc = SXERR_SYNTAX;` |
|        1 |  848 | `				 }` |
|        3 |  849 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  850 | `				 return rc;` |
|        - |  851 | `			 }` |
|    80954 |  852 | `		 }` |
|        - |  853 | `		/* Advance the stream cursor */` |
|   889152 |  854 | `		pCur++;` |
|        - |  855 | `	 }` |
|        - |  856 | `	/* Point to the end of the token stream */` |
|  2859156 |  857 | `	pNode->pEnd = pCur;` |
|        - |  858 | `	/* Save the node for later processing */` |
|  2859156 |  859 | `	*ppNode = pNode;` |
|        - |  860 | `	/* Synchronize cursors */` |
|  2859156 |  861 | `	pGen->pIn = pCur;` |
|  2859156 |  862 | `	return SXRET_OK;` |
|  1429603 |  863 |  |
|        - |  864 | `/*` |
|        - |  865 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  866 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  867 | ` * level is zero.` |
|        - |  868 | ` */` |
|    69516 |  869 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  870 |  |
|    69518 |  871 | `	SyToken *pCur = pStart;` |
|    69518 |  872 | `	sxi32 iNest = 0;` |
|    69518 |  873 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  874 | `		/* Last expression */` |
|    37118 |  875 | `		return SXERR_EOF;` |
|        - |  876 | `	}` |
|   131314 |  877 | `	while( pCur < pEnd ){` |
|   118958 |  878 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    20046 |  879 | `			break;` |
|        - |  880 | `		}` |
|    98914 |  881 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5614 |  882 | `			iNest++;` |
|    96108 |  883 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5616 |  884 | `			iNest--;` |
|     2807 |  885 | `		}` |
|    98914 |  886 | `		pCur++;` |
|        2 |  887 | `	}` |
|    32402 |  888 | `	*ppNext = pCur;` |
|    32402 |  889 | `	return SXRET_OK;` |
|    34760 |  890 |  |
|        - |  891 | `/*` |
|        - |  892 | ` * Free an expression tree.` |
|        - |  893 | ` */` |
|  2446878 |  894 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  895 |  |
|  2446880 |  896 | `	if( pNode->pLeft ){` |
|        - |  897 | `		/* Release the left tree */` |
|   912762 |  898 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   456380 |  899 | `	}` |
|  2446880 |  900 | `	if( pNode->pRight ){` |
|        - |  901 | `		/* Release the right tree */` |
|   477864 |  902 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   238931 |  903 | `	}` |
|  2446880 |  904 | `	if( pNode->pCond ){` |
|        - |  905 | `		/* Release the conditional tree used by the ternary operator */` |
|     1896 |  906 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      947 |  907 | `	}` |
|  2446880 |  908 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  909 | `		ph7_expr_node **apArg;` |
|        - |  910 | `		sxu32 n;` |
|        - |  911 | `		/* Release node arguments */` |
|   324220 |  912 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   684524 |  913 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   360306 |  914 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   180154 |  915 | `		}` |
|   324220 |  916 | `		SySetRelease(&pNode->aNodeArgs);` |
|   162109 |  917 | `	}` |
|        - |  918 | `	/* Finally,release this node */` |
|  2446880 |  919 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2446880 |  920 |  |
|        - |  921 | `/*` |
|        - |  922 | ` * Free an expression tree.` |
|        - |  923 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  924 | ` */` |
|   648516 |  925 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  926 |  |
|        - |  927 | `	ph7_expr_node **apNode;` |
|        - |  928 | `	sxu32 n;` |
|   648518 |  929 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3507672 |  930 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2859156 |  931 | `		if( apNode[n] ){` |
|   648828 |  932 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   324413 |  933 | `		}` |
|  1429579 |  934 | `	}` |
|   648518 |  935 | `	return SXRET_OK;` |
|        2 |  936 |  |
|        - |  937 | `/*` |
|        - |  938 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  939 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  940 | ` */` |
|   207718 |  941 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  942 |  |
|        - |  943 | `	sxi32 iExprOp;` |
|   207720 |  944 | `	if( pNode->pOp == 0 ){` |
|   135222 |  945 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  946 | `	}` |
|    72500 |  947 | `	iExprOp = pNode->pOp->iOp;` |
|    72500 |  948 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    45454 |  949 | `			return TRUE;` |
|        - |  950 | `	}` |
|    27048 |  951 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    27044 |  952 | `		if( pNode->pLeft->pOp ) {` |
|       12 |  953 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  954 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  955 | `				return FALSE;` |
|        1 |  956 | `			}` |
|    27038 |  957 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  958 | `			return FALSE;` |
|        - |  959 | `		}` |
|    27044 |  960 | `		return TRUE;` |
|        - |  961 | `	}` |
|        5 |  962 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  963 | `		return TRUE;` |
|        - |  964 | `	}` |
|        - |  965 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  966 | `	return FALSE;` |
|   103861 |  967 |  |
|        - |  968 | `/* Forward declaration */` |
|        - |  969 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  970 | `/* Macro to check if the given node is a terminal.` |
|        - |  971 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  972 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  973 | ` * linked ternary/elvis node). */` |
|        - |  974 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  975 | `/*` |
|        - |  976 | ` * Buid an expression tree for each given function argument.` |
|        - |  977 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  978 | ` */` |
|   269070 |  979 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  980 |  |
|        - |  981 | `	sxi32 iNest,iCur,iNode;` |
|        - |  982 | `	sxi32 rc;` |
|        - |  983 | `	/* Process function arguments from left to right */` |
|   269072 |  984 | `	iCur = 0;` |
|   287104 |  985 | `	for(;;){` |
|   574210 |  986 | `		if( iCur >= nToken ){` |
|        - |  987 | `			/* No more arguments to process */` |
|   269052 |  988 | `			break;` |
|        - |  989 | `		}` |
|   305160 |  990 | `		iNode = iCur;` |
|   305160 |  991 | `		iNest = 0;` |
|   763196 |  992 | `		while( iCur < nToken ){` |
|   494146 |  993 | `			if( apNode[iCur] ){` |
|   483474 |  994 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    18056 |  995 | `					break;` |
|   447366 |  996 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    24798 |  997 | `					iNest++;` |
|   434968 |  998 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    24784 |  999 | `					iNest--;` |
|    12391 | 1000 | `				}` |
|   223682 | 1001 | `			}` |
|   458038 | 1002 | `			iCur++;` |
|        2 | 1003 | `		}` |
|   305160 | 1004 | `		if( iCur > iNode ){` |
|   305152 | 1005 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1006 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1007 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1008 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1009 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1010 | `					apNode[iNode] = 0;` |
|      ! 0 | 1011 | `			}` |
|   305154 | 1012 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   305154 | 1013 | `			if( apNode[iNode] ){` |
|        - | 1014 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   305154 | 1015 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   152578 | 1016 | `			}else{` |
|        - | 1017 | `				/* No expression before comma */` |
|      ! 0 | 1018 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1019 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1020 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1021 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1022 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1023 | `				}` |
|      ! 0 | 1024 | `				return rc;` |
|        - | 1025 | `			}` |
|   152578 | 1026 | `		}else{` |
|        - | 1027 | `			/* Comma with no preceding argument */` |
|        7 | 1028 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1029 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1030 | `				rc = SXERR_SYNTAX;` |
|        3 | 1031 | `			}` |
|        7 | 1032 | `			return rc;` |
|        - | 1033 | `		}` |
|        - | 1034 | `		/* Jump trailing comma */` |
|   305154 | 1035 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    36104 | 1036 | `			iCur++;` |
|    36104 | 1037 | `			if( iCur >= nToken ){` |
|        - | 1038 | `				/* Trailing comma after last argument */` |
|       15 | 1039 | `				break;` |
|        - | 1040 | `			}` |
|    18044 | 1041 | `		}` |
|        2 | 1042 | `	}` |
|   269066 | 1043 | `	return SXRET_OK;` |
|   134537 | 1044 |  |
|        - | 1045 | ` /*` |
|        - | 1046 | `  * Create an expression tree from an array of tokens.` |
|        - | 1047 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1048 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1049 | `  */` |
|  1037964 | 1050 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1051 | ` {` |
|        - | 1052 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1053 | `	 ph7_expr_node *pNode;` |
|        - | 1054 | `	 sxi32 iCur;` |
|        - | 1055 | `	 sxi32 rc;` |
|  1037966 | 1056 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1057 | `		 /* TICKET 1433-17: self evaluating node */` |
|   478896 | 1058 | `		 return SXRET_OK;` |
|        - | 1059 | `	 }` |
|        - | 1060 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3434100 | 1061 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1062 | `		 sxi32 iNest;` |
|        - | 1063 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1064 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1065 | `		  */` |
|  2875032 | 1066 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2852414 | 1067 | `			 continue;` |
|        - | 1068 | `		 }` |
|    22620 | 1069 | `		 iNest = 1;` |
|    22620 | 1070 | `		 iLeft = iCur;` |
|        - | 1071 | `		 /* Find the closing parenthesis */` |
|    22620 | 1072 | `		 iCur++;` |
|   150392 | 1073 | `		 while( iCur < nToken ){` |
|   150392 | 1074 | `			 if( apNode[iCur] ){` |
|   150392 | 1075 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1076 | `					 /* Decrement nesting level */` |
|    39186 | 1077 | `					 iNest--;` |
|    39186 | 1078 | `					 if( iNest <= 0 ){` |
|    22620 | 1079 | `						 break;` |
|        2 | 1080 | `					 }` |
|   119491 | 1081 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1082 | `					 /* Increment nesting level */` |
|    16568 | 1083 | `					 iNest++;` |
|     8283 | 1084 | `				 }` |
|    63886 | 1085 | `			 }` |
|   127774 | 1086 | `			 iCur++;` |
|        2 | 1087 | `		 }` |
|    22620 | 1088 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1089 | `			 /* Recurse and process this expression */` |
|    22620 | 1090 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    22620 | 1091 | `			 if( rc != SXRET_OK ){` |
|        3 | 1092 | `				 return rc;` |
|        - | 1093 | `			 }` |
|    11308 | 1094 | `		 }` |
|        - | 1095 | `		 /* Free the left and right nodes */` |
|    22618 | 1096 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    22618 | 1097 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    22618 | 1098 | `		 apNode[iLeft] = 0;` |
|    22618 | 1099 | `		 apNode[iCur] = 0;` |
|    11310 | 1100 | `	 }` |
|        - | 1101 | `	  /* Process expressions enclosed in braces */` |
|  3578698 | 1102 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1103 | `		 sxi32 iNest;` |
|        - | 1104 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1105 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1106 | `		  */` |
|  3025410 | 1107 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3025410 | 1108 | `			 continue;` |
|        - | 1109 | `		 }` |
|      ! 0 | 1110 | `		 iNest = 1;` |
|      ! 0 | 1111 | `		 iLeft = iCur;` |
|        - | 1112 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1113 | `		 iCur++;` |
|      ! 0 | 1114 | `		 while( iCur < nToken ){` |
|      ! 0 | 1115 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1116 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1117 | `					 /* Decrement nesting level */` |
|      ! 0 | 1118 | `					 iNest--;` |
|      ! 0 | 1119 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1120 | `						 break;` |
|      ! 0 | 1121 | `					 }` |
|      ! 0 | 1122 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1123 | `					 /* Increment nesting level */` |
|      ! 0 | 1124 | `					 iNest++;` |
|      ! 0 | 1125 | `				 }` |
|      ! 0 | 1126 | `			 }` |
|      ! 0 | 1127 | `			 iCur++;` |
|      ! 0 | 1128 | `		 }` |
|      ! 0 | 1129 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1130 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1131 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1132 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1133 | `				 return rc;` |
|        - | 1134 | `			 }` |
|      ! 0 | 1135 | `		 }` |
|        - | 1136 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1137 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1138 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1139 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1140 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1141 | `	 }` |
|        - | 1142 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   553290 | 1143 | `	 iLeft = -1;` |
|  3578670 | 1144 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3025392 | 1145 | `		 if( apNode[iCur] == 0 ){` |
|  1177060 | 1146 | `			 continue;` |
|        - | 1147 | `		 }` |
|  1848334 | 1148 | `		 pNode = apNode[iCur];` |
|  1848334 | 1149 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   476696 | 1150 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1151 | `				 /* Collect function arguments */` |
|   305740 | 1152 | `				 sxi32 iPtr = 0;` |
|   305740 | 1153 | `				 sxi32 nFuncTok = 0;` |
|  1105624 | 1154 | `				 while( nFuncTok + iCur < nToken ){` |
|  1105624 | 1155 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1094952 | 1156 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   316874 | 1157 | `							 iPtr++;` |
|   936516 | 1158 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   316874 | 1159 | `							 iPtr--;` |
|   316874 | 1160 | `							 if( iPtr <= 0 ){` |
|   305740 | 1161 | `								 break;` |
|        - | 1162 | `							 }` |
|     5567 | 1163 | `						 }` |
|   394606 | 1164 | `					 }` |
|   799886 | 1165 | `					 nFuncTok++;` |
|        2 | 1166 | `				 }` |
|   305740 | 1167 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1168 | `					 /* Syntax error */` |
|      ! 0 | 1169 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1170 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1171 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1172 | `					 }` |
|      ! 0 | 1173 | `					 return rc;` |
|        - | 1174 | `				 }` |
|   305740 | 1175 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1176 | `					 /* Syntax error */` |
|      ! 0 | 1177 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1178 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1179 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1180 | `					 }` |
|      ! 0 | 1181 | `					 return rc;` |
|        - | 1182 | `				 }` |
|   305740 | 1183 | `				 if( nFuncTok > 1 ){` |
|        - | 1184 | `					 /* Process function arguments */` |
|   269072 | 1185 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   269072 | 1186 | `					 if( rc != SXRET_OK ){` |
|        7 | 1187 | `						 return rc;` |
|        - | 1188 | `					 }` |
|   134532 | 1189 | `				 }` |
|        - | 1190 | `				 /* Link the node to the tree */` |
|   305734 | 1191 | `				 pNode->pLeft = apNode[iLeft];` |
|   305734 | 1192 | `				 apNode[iLeft] = 0;` |
|  1105600 | 1193 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   799868 | 1194 | `					 apNode[iCur+iPtr] = 0;` |
|   399935 | 1195 | `				 }` |
|   323824 | 1196 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1197 | `				 /* Subscripting */` |
|    68554 | 1198 | `				 sxi32 iArrTok = iCur + 1;` |
|    68554 | 1199 | `				 sxi32 iNest = 1;` |
|    68630 | 1200 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1201 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1202 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    68552 | 1203 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1204 | `						 /* Syntax error */` |
|      ! 0 | 1205 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1206 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1207 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1208 | `						 }` |
|      ! 0 | 1209 | `						 return rc;` |
|        - | 1210 | `				 }` |
|        - | 1211 | `				 /* Collect index tokens */` |
|   123816 | 1212 | `				 while( iArrTok < nToken ){` |
|   123816 | 1213 | `					 if( apNode[iArrTok] ){` |
|   123784 | 1214 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1215 | `							 /* Increment nesting level */` |
|      ! 0 | 1216 | `							 iNest++;` |
|   123784 | 1217 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1218 | `							 /* Decrement nesting level */` |
|    68554 | 1219 | `							 iNest--;` |
|    68554 | 1220 | `							 if( iNest <= 0 ){` |
|    68554 | 1221 | `								 break;` |
|        - | 1222 | `							 }` |
|      ! 0 | 1223 | `						 }` |
|    27615 | 1224 | `					 }` |
|    55264 | 1225 | `					 ++iArrTok;` |
|        2 | 1226 | `				 }` |
|    68554 | 1227 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1228 | `					 /* Recurse and process this expression */` |
|    55154 | 1229 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    55154 | 1230 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1231 | `						 return rc;` |
|        - | 1232 | `					 }` |
|        - | 1233 | `					 /* Link the node to it's index */` |
|    55154 | 1234 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    27576 | 1235 | `				 }` |
|        - | 1236 | `				 /* Link the node to the tree */` |
|    68554 | 1237 | `				 pNode->pLeft = apNode[iLeft];` |
|    68554 | 1238 | `				 pNode->pRight = 0;` |
|    68554 | 1239 | `				 apNode[iLeft] = 0;` |
|   192368 | 1240 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   123816 | 1241 | `					 apNode[iNest] = 0;` |
|    61909 | 1242 | `				 }` |
|    34278 | 1243 | `			 }else{` |
|        - | 1244 | `				 /* Member access operators [i.e: '->','::'] */` |
|   102406 | 1245 | `				  iRight = iCur + 1;` |
|   102406 | 1246 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1247 | `					 iRight++;` |
|      ! 0 | 1248 | `				 }` |
|   102406 | 1249 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1250 | `					 /* Syntax error */` |
|        5 | 1251 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1252 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1253 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1254 | `					 }` |
|        5 | 1255 | `					 return rc;` |
|        - | 1256 | `				 }` |
|        - | 1257 | `				 /* Link the node to the tree */` |
|   102402 | 1258 | `				 pNode->pLeft = apNode[iLeft];` |
|   102402 | 1259 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   102182 | 1260 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1261 | `						 /* Syntax error */` |
|      ! 0 | 1262 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1263 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1264 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1265 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1266 | `						 }` |
|      ! 0 | 1267 | `						 return rc;` |
|        - | 1268 | `				 }` |
|   102402 | 1269 | `				 pNode->pRight = apNode[iRight];` |
|   102402 | 1270 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1271 | `			 }` |
|   238342 | 1272 | `		 }` |
|  1848324 | 1273 | `		 iLeft = iCur;` |
|   924163 | 1274 | `	 }` |
|        - | 1275 | `	 /* Handle left associative (new, clone) operators */` |
|  3578642 | 1276 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3025364 | 1277 | `		 if( apNode[iCur] == 0 ){` |
|  1667546 | 1278 | `			 continue;` |
|        - | 1279 | `		 }` |
|  1357820 | 1280 | `		 pNode = apNode[iCur];` |
|  1357820 | 1281 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1282 | `			 SyToken *pToken;` |
|        - | 1283 | `			 /* Get the left node */` |
|    13804 | 1284 | `			 iLeft = iCur + 1;` |
|    27576 | 1285 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13774 | 1286 | `				 iLeft++;` |
|        2 | 1287 | `			 }` |
|    13804 | 1288 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1289 | `				  /* Syntax error */` |
|      ! 0 | 1290 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1291 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1292 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1293 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1294 | `				 }` |
|      ! 0 | 1295 | `				 return rc;` |
|        - | 1296 | `			 }` |
|        - | 1297 | `			 /* Make sure the operand are of a valid type */` |
|    13804 | 1298 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1299 | `				 /* Clone:` |
|        - | 1300 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1301 | `				  *  ++ function call (including annonymous)` |
|        - | 1302 | `				  *  ++ array member` |
|        - | 1303 | `				  *  ++ 'new' operator` |
|        - | 1304 | `				  * Example:` |
|        - | 1305 | `				  *   clone $pObj;` |
|        - | 1306 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1307 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1308 | `				  */` |
|       18 | 1309 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1310 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1311 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1312 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1313 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1314 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1315 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1316 | `						 }` |
|      ! 0 | 1317 | `						 return rc;` |
|        - | 1318 | `					 }` |
|        7 | 1319 | `				 }` |
|       10 | 1320 | `			 }else{` |
|        - | 1321 | `				 /* New */` |
|    13788 | 1322 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1323 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1324 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1325 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1326 | `						 /* Syntax error */` |
|      ! 0 | 1327 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1328 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1329 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1330 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1331 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1332 | `						 }` |
|      ! 0 | 1333 | `						 return rc;` |
|        - | 1334 | `					 }` |
|        8 | 1335 | `				 }` |
|        - | 1336 | `			 }` |
|        - | 1337 | `			  /* Link the node to the tree */` |
|    13804 | 1338 | `			 pNode->pLeft = apNode[iLeft];` |
|    13804 | 1339 | `			 apNode[iLeft] = 0;` |
|    13804 | 1340 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6901 | 1341 | `		 }` |
|   678911 | 1342 | `	 }` |
|        - | 1343 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   553280 | 1344 | `	 iLeft = -1;` |
|  3581532 | 1345 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3025364 | 1346 | `		 if( apNode[iCur] == 0 ){` |
|  1667546 | 1347 | `			 continue;` |
|        - | 1348 | `		 }` |
|  1357820 | 1349 | `		 pNode = apNode[iCur];` |
|  1357820 | 1350 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8030 | 1351 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2908 | 1352 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1353 | `					 /* Link the node to the tree */` |
|     2910 | 1354 | `					 pNode->pLeft = apNode[iLeft];` |
|     2910 | 1355 | `					 apNode[iLeft] = 0;` |
|     1454 | 1356 | `			 }` |
|     5459 | 1357 | `		  }` |
|  1360710 | 1358 | `		 iLeft = iCur;` |
|   681801 | 1359 | `	  }` |
|   556170 | 1360 | `	 iLeft = -1;` |
|  3581532 | 1361 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3025364 | 1362 | `		 if( apNode[iCur] == 0 ){` |
|  1670454 | 1363 | `			 continue;` |
|        - | 1364 | `		 }` |
|  1354912 | 1365 | `		 pNode = apNode[iCur];` |
|  1354912 | 1366 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8010 | 1367 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8012 | 1368 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1369 | `					 /* Syntax error */` |
|      ! 0 | 1370 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1371 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1372 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1373 | `					 }` |
|      ! 0 | 1374 | `					 return rc;` |
|        - | 1375 | `			 }` |
|        - | 1376 | `			 /* Link the node to the tree */` |
|     8012 | 1377 | `			 pNode->pLeft = apNode[iLeft];` |
|     8012 | 1378 | `			 apNode[iLeft] = 0;` |
|        - | 1379 | `			 /* Mark as pre-increment/decrement node */` |
|     8012 | 1380 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4005 | 1381 | `		  }` |
|  1354912 | 1382 | `		 iLeft = iCur;` |
|   677457 | 1383 | `	 }` |
|        - | 1384 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   556170 | 1385 | `	  iLeft = 0;` |
|  3581526 | 1386 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3025360 | 1387 | `		  if( apNode[iCur] ){` |
|  1346898 | 1388 | `			  pNode = apNode[iCur];` |
|  1346898 | 1389 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    35966 | 1390 | `				  if( iLeft > 0 ){` |
|        - | 1391 | `					  /* Link the node to the tree */` |
|    35964 | 1392 | `					  pNode->pLeft = apNode[iLeft];` |
|    35964 | 1393 | `					  apNode[iLeft] = 0;` |
|    35964 | 1394 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1395 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1396 | `							   /* Syntax error */` |
|      ! 0 | 1397 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1398 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1399 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1400 | `							  }` |
|      ! 0 | 1401 | `							  return rc;` |
|        - | 1402 | `						  }` |
|       36 | 1403 | `					  }` |
|    17983 | 1404 | `				  }else{` |
|        - | 1405 | `					  /* Syntax error */` |
|        3 | 1406 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1407 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1408 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1409 | `					  }` |
|        3 | 1410 | `					  return rc;` |
|        - | 1411 | `				  }` |
|    17981 | 1412 | `			  }` |
|        - | 1413 | `			  /* Save terminal position */` |
|  1346896 | 1414 | `			  iLeft = iCur;` |
|   673447 | 1415 | `		  }` |
|  1512680 | 1416 | `	  }` |
|        - | 1417 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6117752 | 1418 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5561594 | 1419 | `		 iLeft = -1;` |
| 35814908 | 1420 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 30253324 | 1421 | `			 if( apNode[iCur] == 0 ){` |
| 19307468 | 1422 | `				 continue;` |
|        - | 1423 | `			 }` |
| 10945858 | 1424 | `			 pNode = apNode[iCur];` |
| 10945858 | 1425 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1426 | `				 /* Get the right node */` |
|   165872 | 1427 | `				 iRight = iCur + 1;` |
|   235508 | 1428 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    69638 | 1429 | `					 iRight++;` |
|        2 | 1430 | `				 }` |
|   165872 | 1431 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1432 | `					 /* Syntax error */` |
|        9 | 1433 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1434 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1435 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1436 | `					 }` |
|        9 | 1437 | `					 return rc;` |
|        - | 1438 | `				 }` |
|   165864 | 1439 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1440 | `					 sxi32  iTmp;` |
|        - | 1441 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       48 | 1442 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1443 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1444 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1445 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1446 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1447 | `						 }` |
|      ! 0 | 1448 | `						 return rc;` |
|        - | 1449 | `					 }` |
|       48 | 1450 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1451 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1452 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1453 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1454 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1455 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1456 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1457 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1458 | `									 }` |
|      ! 0 | 1459 | `									 return rc;` |
|        - | 1460 | `							 }` |
|      ! 0 | 1461 | `						 }` |
|       16 | 1462 | `					 }` |
|        - | 1463 | `					 /* Swap operands */` |
|       48 | 1464 | `					 iTmp = iRight;` |
|       48 | 1465 | `					 iRight = iLeft;` |
|       48 | 1466 | `					 iLeft = iTmp;` |
|       23 | 1467 | `				 }` |
|        - | 1468 | `				 /* Link the node to the tree */` |
|   165864 | 1469 | `				 pNode->pLeft = apNode[iLeft];` |
|   165864 | 1470 | `				 pNode->pRight = apNode[iRight];` |
|   165864 | 1471 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    82931 | 1472 | `			 }` |
| 10945850 | 1473 | `			 iLeft = iCur;` |
|  5472926 | 1474 | `		 }` |
|  2780794 | 1475 | `	 }` |
|        - | 1476 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1477 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1478 | `	  * we are dealing with a single operator.` |
|        - | 1479 | `	  */` |
|   556160 | 1480 | `	  iLeft = -1;` |
|  3573360 | 1481 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3019098 | 1482 | `		  if( apNode[iCur] == 0 ){` |
|  2045556 | 1483 | `			  continue;` |
|        - | 1484 | `		  }` |
|   973544 | 1485 | `		  pNode = apNode[iCur];` |
|   973544 | 1486 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1898 | 1487 | `			  sxi32 iNest = 1;` |
|     1898 | 1488 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1489 | `				  /* Missing condition */` |
|        3 | 1490 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1491 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1492 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1493 | `				  }` |
|        3 | 1494 | `				  return rc;` |
|        - | 1495 | `			  }` |
|        - | 1496 | `			  /* Get the right node */` |
|     1896 | 1497 | `			  iRight = iCur + 1;` |
|     4020 | 1498 | `			  while( iRight < nToken  ){` |
|     4020 | 1499 | `				  if( apNode[iRight] ){` |
|     3722 | 1500 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1501 | `						  /* Increment nesting level */` |
|      ! 0 | 1502 | `						  ++iNest;` |
|     3722 | 1503 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1504 | `						  /* Decrement nesting level */` |
|     1896 | 1505 | `						  --iNest;` |
|     1896 | 1506 | `						  if( iNest <= 0 ){` |
|     1896 | 1507 | `							  break;` |
|        - | 1508 | `						  }` |
|      ! 0 | 1509 | `					  }` |
|      913 | 1510 | `				  }` |
|     2126 | 1511 | `				  iRight++;` |
|        2 | 1512 | `			  }` |
|     1896 | 1513 | `			  if( iRight > iCur + 1 ){` |
|        - | 1514 | `				  /* Recurse and process the then expression */` |
|     1828 | 1515 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1828 | 1516 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1517 | `					  return rc;` |
|        - | 1518 | `				  }` |
|        - | 1519 | `				  /* Link the node to the tree */` |
|     1828 | 1520 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      913 | 1521 | `			  }else{` |
|        - | 1522 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1523 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1524 | `			  }` |
|     1896 | 1525 | `			  apNode[iCur + 1] = 0;` |
|     1896 | 1526 | `			  if( iRight + 1 < nToken ){` |
|        - | 1527 | `				  /* Recurse and process the else expression */` |
|     1896 | 1528 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1896 | 1529 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1530 | `					  return rc;` |
|        - | 1531 | `				  }` |
|        - | 1532 | `				  /* Link the node to the tree */` |
|     1896 | 1533 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1896 | 1534 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      949 | 1535 | `			  }else{` |
|      ! 0 | 1536 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1537 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1538 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1539 | `				 }` |
|      ! 0 | 1540 | `				 return rc;` |
|        - | 1541 | `			  }` |
|        - | 1542 | `			  /* Point to the condition */` |
|     1896 | 1543 | `			  pNode->pCond  = apNode[iLeft];` |
|     1896 | 1544 | `			  apNode[iLeft] = 0;` |
|     1896 | 1545 | `			  break;` |
|        - | 1546 | `		  }` |
|   971648 | 1547 | `		  iLeft = iCur;` |
|   485825 | 1548 | `	  }` |
|        - | 1549 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1550 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1551 | `	  * so there is no need for a precedence loop here.` |
|        - | 1552 | `	  */` |
|   556158 | 1553 | `	 iRight = -1;` |
|  3581382 | 1554 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3025272 | 1555 | `		 if( apNode[iCur] == 0 ){` |
|  2261312 | 1556 | `			 continue;` |
|        - | 1557 | `		 }` |
|   763962 | 1558 | `		 pNode = apNode[iCur];` |
|   763962 | 1559 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1560 | `			 /* Get the left node */` |
|   207684 | 1561 | `			 iLeft = iCur - 1;` |
|   293880 | 1562 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    86198 | 1563 | `				 iLeft--;` |
|        2 | 1564 | `			 }` |
|   207684 | 1565 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1566 | `				 /* Syntax error */` |
|       43 | 1567 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1568 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1569 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1570 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1571 | `				 }else{` |
|       39 | 1572 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1573 | `				 }` |
|       43 | 1574 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1575 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1576 | `				 }` |
|       43 | 1577 | `				 return rc;` |
|        - | 1578 | `			 }` |
|   207642 | 1579 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1580 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1581 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1582 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1583 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1584 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1585 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1586 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1587 | `					 }else{` |
|        4 | 1588 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1589 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1590 | `					 }` |
|        5 | 1591 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1592 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1593 | `					 }` |
|        5 | 1594 | `					 return rc;` |
|        - | 1595 | `				 }` |
|       26 | 1596 | `			 }` |
|        - | 1597 | `			 /* Link the node to the tree (Reverse) */` |
|   207638 | 1598 | `			 pNode->pLeft = apNode[iRight];` |
|   207638 | 1599 | `			 pNode->pRight = apNode[iLeft];` |
|   207638 | 1600 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   103818 | 1601 | `		 }` |
|   763916 | 1602 | `		 iRight = iCur;` |
|   381959 | 1603 | `	 }` |
|        - | 1604 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2780552 | 1605 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2224442 | 1606 | `		 iLeft = -1;` |
| 14325314 | 1607 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12100874 | 1608 | `			 if( apNode[iCur] == 0 ){` |
|  9876028 | 1609 | `				 continue;` |
|        - | 1610 | `			 }` |
|  2224848 | 1611 | `			 pNode = apNode[iCur];` |
|  2224848 | 1612 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1613 | `				 /* Get the right node */` |
|       72 | 1614 | `				 iRight = iCur + 1;` |
|      110 | 1615 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1616 | `					 iRight++;` |
|        2 | 1617 | `				 }` |
|       72 | 1618 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1619 | `					 /* Syntax error */` |
|      ! 0 | 1620 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1621 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1622 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1623 | `					 }` |
|      ! 0 | 1624 | `					 return rc;` |
|        - | 1625 | `				 }` |
|        - | 1626 | `				 /* Link the node to the tree */` |
|       72 | 1627 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1628 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1629 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1630 | `			 }` |
|  2224848 | 1631 | `			 iLeft = iCur;` |
|  1112425 | 1632 | `		 }` |
|  1112222 | 1633 | `	 }` |
|        - | 1634 | `	 /* Point to the root of the expression tree */` |
|  3025192 | 1635 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2469100 | 1636 | `		 if( apNode[iCur] ){` |
|   501976 | 1637 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1638 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1639 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1640 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1641 | `				  }` |
|       20 | 1642 | `				  return rc;` |
|        - | 1643 | `			 }` |
|   501958 | 1644 | `			 apNode[0] = apNode[iCur];` |
|   501958 | 1645 | `			 apNode[iCur] = 0;` |
|   250978 | 1646 | `		 }` |
|  1234542 | 1647 | `	 }` |
|   556094 | 1648 | `	 return SXRET_OK;` |
|   517539 | 1649 | ` }` |
|        - | 1650 | ` /*` |
|        - | 1651 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1652 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1653 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1654 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1655 | `  */` |
|   648516 | 1656 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1657 |  |
|        - | 1658 | `	ph7_expr_node **apNode;` |
|        - | 1659 | `	ph7_expr_node *pNode;` |
|        - | 1660 | `	sxi32 rc;` |
|        - | 1661 | `	/* Reset node container */` |
|   648518 | 1662 | `	SySetReset(pExprNode);` |
|   648518 | 1663 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1664 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1665 | `	{` |
|   648518 | 1666 | `		int iLastWasTerm = 0;` |
|  3507672 | 1667 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2859190 | 1668 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2859190 | 1669 | `			if( rc != SXRET_OK ){` |
|       35 | 1670 | `				return rc;` |
|        - | 1671 | `			}` |
|        - | 1672 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2859156 | 1673 | `			if( pNode->xCode ){` |
|        - | 1674 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1488582 | 1675 | `				iLastWasTerm = 1;` |
|  2114866 | 1676 | `			}else if( pNode->pOp ){` |
|        - | 1677 | `				/* Operator node */` |
|   643334 | 1678 | `				iLastWasTerm = 0;` |
|   321668 | 1679 | `			}else{` |
|        - | 1680 | `				/* Delimiter: ')' and ']' end terms */` |
|   727244 | 1681 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1682 | `			}` |
|        - | 1683 | `			/* Save the extracted node */` |
|  2859156 | 1684 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1685 | `		}` |
|        - | 1686 | `	}` |
|   648484 | 1687 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1688 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1689 | `		*ppRoot = 0;` |
|      ! 0 | 1690 | `		return SXRET_OK;` |
|        - | 1691 | `	}` |
|   648484 | 1692 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1693 | `	/* Make sure we are dealing with valid nodes */` |
|   648484 | 1694 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   648484 | 1695 | `	if( rc != SXRET_OK ){` |
|        - | 1696 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1697 | `		 * cleanup the mess left behind.` |
|        - | 1698 | `		 */` |
|       51 | 1699 | `		*ppRoot = 0;` |
|       51 | 1700 | `		return rc;` |
|        - | 1701 | `	}` |
|        - | 1702 | `	/* Build the tree */` |
|   648434 | 1703 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   648434 | 1704 | `	if( rc != SXRET_OK ){` |
|        - | 1705 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       88 | 1706 | `		*ppRoot = 0;` |
|       88 | 1707 | `		return rc;` |
|        - | 1708 | `	}` |
|        - | 1709 | `	/* Point to the root of the tree */` |
|   648348 | 1710 | `	*ppRoot = apNode[0];` |
|   648348 | 1711 | `	return SXRET_OK;` |
|   324260 | 1712 |  |
|        - | 1713 |  |
