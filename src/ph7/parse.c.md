# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1018/1181 lines (86.20%)

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
|   913648 |  265 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  266 |  |
|   913650 |  267 | `	sxu32 n = 0;` |
|        - |  268 | `	sxi32 rc;` |
|        - |  269 | `	/* Do a linear lookup on the operators table */` |
| 14936859 |  270 | `	for(;;){` |
| 29873720 |  271 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  272 | `			break;` |
|        - |  273 | `		}` |
| 29873720 |  274 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  275 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3588944 |  276 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1794473 |  277 | `		}else{` |
| 26284778 |  278 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  279 | `		}` |
| 29873720 |  280 | `		if( rc == 0 ){` |
|   917202 |  281 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  282 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   913314 |  283 | `				return &aOpTable[n];` |
|        - |  284 | `			}` |
|        - |  285 | `			/* Handle ambiguity */` |
|     3890 |  286 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  287 | `				/* Unary opertors have prcedence here over binary operators */` |
|      226 |  288 | `				return &aOpTable[n];` |
|        - |  289 | `			}` |
|     3666 |  290 | `			if( pLast->nType & PH7_TK_OP ){` |
|      122 |  291 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  292 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      122 |  293 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  294 | `					/* Unary opertors have prcedence here over binary operators */` |
|      114 |  295 | `					return &aOpTable[n];` |
|        - |  296 | `				}` |
|        - |  297 |  |
|        4 |  298 | `			}` |
|     1776 |  299 | `		}` |
| 28960072 |  300 | `		++n; /* Next operator in the table */` |
|        2 |  301 | `	}` |
|        - |  302 | `	/* No such operator */` |
|      ! 0 |  303 | `	return 0;` |
|   456826 |  304 |  |
|        - |  305 | `/*` |
|        - |  306 | ` * Delimit a set of token stream.` |
|        - |  307 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  308 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  309 | ` */` |
|   475786 |  310 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  311 |  |
|   475788 |  312 | `	SyToken *pCur = pIn;` |
|   475788 |  313 | `	sxi32 iNest = 1;` |
|  2860067 |  314 | `	for(;;){` |
|  5720136 |  315 | `		if( pCur >= pEnd ){` |
|      168 |  316 | `			break;` |
|        - |  317 | `		}` |
|  5719970 |  318 | `		if( pCur->nType & nTokStart ){` |
|        - |  319 | `			/* Increment nesting level */` |
|   300032 |  320 | `			iNest++;` |
|  5569955 |  321 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  322 | `			/* Decrement nesting level */` |
|   775652 |  323 | `			iNest--;` |
|   775652 |  324 | `			if( iNest <= 0 ){` |
|   475622 |  325 | `				break;` |
|        - |  326 | `			}` |
|   150015 |  327 | `		}` |
|        - |  328 | `		/* Advance cursor */` |
|  5244350 |  329 | `		pCur++;` |
|        2 |  330 | `	}` |
|        - |  331 | `	/* Point to the end of the chunk */` |
|   475788 |  332 | `	*ppEnd = pCur;` |
|   475788 |  333 |  |
|        - |  334 | `/*` |
|        - |  335 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  336 | ` * Note on reserved keywords.` |
|        - |  337 | ` *  According to the PHP language reference manual:` |
|        - |  338 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  339 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  340 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  341 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  342 | ` */` |
|    18370 |  343 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  344 |  |
|    27487 |  345 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    18275 |  346 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  347 | `		){` |
|      150 |  348 | `			return TRUE;` |
|        - |  349 | `	}` |
|    18224 |  350 | `	if( bCheckFunc ){` |
|       98 |  351 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       72 |  352 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       57 |  353 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  354 | `				return TRUE;` |
|        - |  355 | `		}` |
|       22 |  356 | `	}` |
|        - |  357 | `	/* Not a language construct */` |
|    18192 |  358 | `	return FALSE;` |
|     9187 |  359 |  |
|        - |  360 | `/*` |
|        - |  361 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  362 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  363 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  364 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  365 | ` */` |
|   775486 |  366 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  367 |  |
|        - |  368 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  369 | `	sxi32 i,rc;` |
|        - |  370 |  |
|   775488 |  371 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  372 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  373 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  374 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  375 | `	}` |
|   775488 |  376 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  4176164 |  377 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  3400712 |  378 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  379 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      328 |  380 | `			continue;` |
|        - |  381 | `		}` |
|  3400386 |  382 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   372982 |  383 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    18562 |  384 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  385 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   348114 |  386 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  387 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  388 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  389 | `						 */` |
|   348114 |  390 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   348114 |  391 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   348114 |  392 | `						apNode[i]->pOp = &sFCallOp;` |
|   174056 |  393 | `					}` |
|   174056 |  394 | `			}` |
|   372982 |  395 | `			iParen++;` |
|  3213896 |  396 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   372982 |  397 | `			if( iParen <= 0 ){` |
|       13 |  398 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  399 | `				if( rc != SXERR_ABORT ){` |
|       13 |  400 | `					rc = SXERR_SYNTAX;` |
|        6 |  401 | `				}` |
|       13 |  402 | `				return rc;` |
|        - |  403 | `			}` |
|   372970 |  404 | `			iParen--;` |
|  2840910 |  405 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    75290 |  406 | `			iSquare++;` |
|  2616782 |  407 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    75304 |  408 | `			if( iSquare <= 0 ){` |
|        7 |  409 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  410 | `				if( rc != SXERR_ABORT ){` |
|        7 |  411 | `					rc = SXERR_SYNTAX;` |
|        3 |  412 | `				}` |
|        7 |  413 | `				return rc;` |
|        - |  414 | `			}` |
|    75298 |  415 | `			iSquare--;` |
|  2541484 |  416 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       14 |  417 | `			iBraces++;` |
|       14 |  418 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|        7 |  462 | `			}` |
|  2503830 |  463 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       15 |  464 | `			if( iBraces <= 0 ){` |
|       13 |  465 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  466 | `				if( rc != SXERR_ABORT ){` |
|       13 |  467 | `					rc = SXERR_SYNTAX;` |
|        6 |  468 | `				}` |
|       13 |  469 | `				return rc;` |
|        - |  470 | `			}` |
|        3 |  471 | `			iBraces--;` |
|  2503811 |  472 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2240 |  473 | `			if( iQuesty > 0 ){` |
|     2060 |  474 | `				iQuesty--;` |
|     1211 |  475 | `			}else if( iParen <= 0 ){` |
|        - |  476 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  477 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  478 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        5 |  479 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  480 | `				if( rc != SXERR_ABORT ){` |
|        5 |  481 | `					rc = SXERR_SYNTAX;` |
|        2 |  482 | `				}` |
|        5 |  483 | `				return rc;` |
|        2 |  484 | `			}` |
|  2502689 |  485 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   714570 |  486 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   714570 |  487 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2062 |  488 | `				iQuesty++;` |
|   713540 |  489 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      324 |  490 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      161 |  508 | `			}` |
|   357284 |  509 | `		}` |
|  1700177 |  510 | `	}` |
|   775454 |  511 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  512 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  513 | `		if( rc != SXERR_ABORT ){` |
|       17 |  514 | `			rc = SXERR_SYNTAX;` |
|        8 |  515 | `		}` |
|       17 |  516 | `		return rc;` |
|        - |  517 | `	}` |
|   775438 |  518 | `	return SXRET_OK;` |
|   387745 |  519 |  |
|        - |  520 | `/*` |
|        - |  521 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  522 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  523 | ` */` |
|   646540 |  524 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  525 |  |
|   646542 |  526 | `	SyToken *pIn = *ppCur;` |
|        - |  527 | `	/* Jump the first literal seen */` |
|   646542 |  528 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   646508 |  529 | `		pIn++;` |
|   323253 |  530 | `	}` |
|   323311 |  531 | `	for(;;){` |
|   646624 |  532 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       84 |  533 | `			pIn++;` |
|       84 |  534 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       82 |  535 | `				pIn++;` |
|       40 |  536 | `			}` |
|       43 |  537 | `		}else{` |
|   323272 |  538 | `			break;` |
|        - |  539 | `		}` |
|        2 |  540 | `	}` |
|        - |  541 | `	/* Synchronize pointers */` |
|   646542 |  542 | `	*ppCur = pIn;` |
|   646542 |  543 |  |
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
|       86 |  694 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  695 |  |
|       88 |  696 | `	SyToken *pIn = *ppCur;` |
|        - |  697 | `	sxu32 nLine;` |
|        - |  698 | `	sxi32 rc;` |
|        - |  699 | `	int iNest;` |
|       88 |  700 | `	nLine = pIn->nLine;` |
|        - |  701 | `	/* Optional 'static' prefix */` |
|       86 |  702 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       88 |  703 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  704 | `		pIn++;` |
|        1 |  705 | `	}` |
|        - |  706 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       86 |  707 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       88 |  708 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  709 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  710 | `		goto Synchronize;` |
|        - |  711 | `	}` |
|       88 |  712 | `	pIn++; /* Jump 'fn' */` |
|       43 |  713 | `	SXUNUSED(nLine);` |
|       43 |  714 | `	SXUNUSED(pGen);` |
|        - |  715 | `	/* Optional '&' for return-by-reference */` |
|       88 |  716 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  717 | `		pIn++;` |
|      ! 0 |  718 | `	}` |
|        - |  719 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  720 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  721 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  722 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       88 |  723 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       86 |  724 | `		pIn++; /* '(' */` |
|       86 |  725 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       86 |  726 | `		if( pIn < pEnd ){` |
|       84 |  727 | `			pIn++; /* ')' */` |
|       41 |  728 | `		}` |
|       42 |  729 | `	}` |
|        - |  730 | `	/* Optional return type ': [?] type ( \| type )*' */` |
|       88 |  731 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       88 |  757 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       81 |  758 | `		pIn++;` |
|       40 |  759 | `	}` |
|        - |  760 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       88 |  761 | `	iNest = 0;` |
|      586 |  762 | `	while( pIn < pEnd ){` |
|      506 |  763 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  764 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  765 | `			break;` |
|        - |  766 | `		}` |
|      500 |  767 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       23 |  768 | `			iNest++;` |
|      489 |  769 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       23 |  770 | `			iNest--;` |
|       11 |  771 | `		}` |
|      500 |  772 | `		pIn++;` |
|        2 |  773 | `	}` |
|       88 |  774 | `	rc = SXRET_OK;` |
|       43 |  775 | `Synchronize:` |
|       88 |  776 | `	*ppCur = pIn;` |
|       88 |  777 | `	return rc;` |
|        2 |  778 |  |
|        - |  779 | `/*` |
|        - |  780 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  781 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  782 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  783 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  784 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  785 | ` */` |
|       70 |  786 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  787 |  |
|       72 |  788 | `	SyToken *pIn = *ppCur;` |
|        - |  789 | `	sxi32 rc;` |
|       35 |  790 | `	SXUNUSED(pGen);` |
|        - |  791 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  792 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       72 |  793 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  794 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  795 | `		goto Synchronize;` |
|        - |  796 | `	}` |
|       72 |  797 | `	pIn++; /* Jump 'match' */` |
|        - |  798 | `	/* Optional '(' subject ')' */` |
|       72 |  799 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       72 |  800 | `		pIn++;` |
|       72 |  801 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       72 |  802 | `		if( pIn < pEnd ){` |
|       72 |  803 | `			pIn++; /* ')' */` |
|       35 |  804 | `		}` |
|       35 |  805 | `	}` |
|        - |  806 | `	/* Optional '{' arms '}' */` |
|       72 |  807 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       72 |  808 | `		pIn++;` |
|       72 |  809 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       72 |  810 | `		if( pIn < pEnd ){` |
|       72 |  811 | `			pIn++; /* '}' */` |
|       35 |  812 | `		}` |
|       35 |  813 | `	}` |
|       72 |  814 | `	rc = SXRET_OK;` |
|       35 |  815 | `Synchronize:` |
|       72 |  816 | `	*ppCur = pIn;` |
|       72 |  817 | `	return rc;` |
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
|  3400878 |  828 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  829 |  |
|        - |  830 | `	ph7_expr_node *pNode;` |
|        - |  831 | `	SyToken *pCur;` |
|        - |  832 | `	sxi32 rc;` |
|        - |  833 | `	/* Allocate a new node */` |
|  3400880 |  834 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  3400880 |  835 | `	if( pNode == 0 ){` |
|        - |  836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  838 | `		 */` |
|      ! 0 |  839 | `		return SXERR_MEM;` |
|        - |  840 | `	}` |
|        - |  841 | `	/* Zero the structure */` |
|  3400880 |  842 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  3400880 |  843 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  844 | `	/* Point to the head of the token stream */` |
|  3400880 |  845 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  846 | `	/* Start collecting tokens */` |
|  3400880 |  847 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
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
|  3400862 |  859 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  860 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  861 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  862 | `		 */` |
|      330 |  863 | `		pCur++; /* Skip the opening '[' */` |
|      330 |  864 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      330 |  865 | `		if( pCur < pGen->pEnd ){` |
|      330 |  866 | `			pCur++; /* Skip past the closing ']' */` |
|      166 |  867 | `		}else{` |
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
|      353 |  879 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  880 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  881 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  882 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  883 | `			}else{` |
|       19 |  884 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  885 | `			}` |
|       25 |  886 | `		}else{` |
|      284 |  887 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  888 | `		}` |
|  3400698 |  889 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  890 | `		/* Point to the instance that describe this operator */` |
|   789892 |  891 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  892 | `		/* Advance the stream cursor */` |
|   789892 |  893 | `		pCur++;` |
|  3005589 |  894 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  895 | `		/* Isolate variable */` |
|  1839502 |  896 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   919758 |  897 | `			pCur++; /* Variable variable */` |
|        2 |  898 | `		}` |
|   919746 |  899 | `		if( pCur < pGen->pEnd ){` |
|   919746 |  900 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  901 | `				/* Variable name */` |
|   919718 |  902 | `				pCur++;` |
|   459888 |  903 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   459870 |  918 | `		}` |
|   919742 |  919 | `		pNode->xCode = PH7_CompileVariable;` |
|  2150770 |  920 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    43956 |  921 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    43956 |  922 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  923 | `			 /* List/Array node */` |
|    25298 |  924 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  925 | `				 /* Assume a literal */` |
|       17 |  926 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  927 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  928 | `			 }else{` |
|    25282 |  929 | `				 pCur += 2;` |
|        - |  930 | `				 /* Collect array/list tokens */` |
|    25282 |  931 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    25282 |  932 | `				 if( pCur < pGen->pEnd ){` |
|    25280 |  933 | `					 pCur++;` |
|    12641 |  934 | `				 }else{` |
|        - |  935 | `					 /* Syntax error */` |
|        4 |  936 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  937 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  938 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  939 | `						 rc = SXERR_SYNTAX;` |
|        1 |  940 | `					 }` |
|        3 |  941 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  942 | `					 return rc;` |
|        - |  943 | `				 }` |
|    25280 |  944 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    25280 |  945 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    31306 |  958 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  959 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       36 |  960 | `			 pCur++; /* Skip 'yield' keyword */` |
|       36 |  961 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  962 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  963 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       36 |  964 | `			 pNode->xCode = PH7_CompileYield;` |
|    18643 |  965 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
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
|    18514 |  980 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    18382 |  981 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  982 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  983 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  984 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       88 |  985 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       88 |  986 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  987 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  988 | `				 return rc;` |
|        - |  989 | `			 }` |
|       88 |  990 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    18381 |  991 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - |  992 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       72 |  993 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       72 |  994 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  995 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  996 | `				 return rc;` |
|        - |  997 | `			 }` |
|       72 |  998 | `			 pNode->xCode = PH7_CompileMatch;` |
|    18303 |  999 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1000 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1001 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1002 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1003 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1004 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1005 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1006 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1007 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    18250 | 1008 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1009 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       82 | 1010 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       82 | 1011 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       42 | 1012 | `		 }else{` |
|        - | 1013 | `			 /* Assume a literal */` |
|    18152 | 1014 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    18152 | 1015 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 | 1016 | `		 }` |
|  1668909 | 1017 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1018 | `		 /* Constants,function name,namespace path,class name... */` |
|   628376 | 1019 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   628376 | 1020 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   314189 | 1021 | `	 }else{` |
|  1018572 | 1022 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1023 | `			 /* Point to the code generator routine */` |
|   195024 | 1024 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   195024 | 1025 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1026 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1027 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1028 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1029 | `				 }` |
|        3 | 1030 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1031 | `				 return rc;` |
|        - | 1032 | `			 }` |
|    97510 | 1033 | `		 }` |
|        - | 1034 | `		/* Advance the stream cursor */` |
|  1018570 | 1035 | `		pCur++;` |
|        - | 1036 | `	 }` |
|        - | 1037 | `	/* Point to the end of the token stream */` |
|  3400828 | 1038 | `	pNode->pEnd = pCur;` |
|        - | 1039 | `	/* Save the node for later processing */` |
|  3400828 | 1040 | `	*ppNode = pNode;` |
|        - | 1041 | `	/* Synchronize cursors */` |
|  3400828 | 1042 | `	pGen->pIn = pCur;` |
|  3400828 | 1043 | `	return SXRET_OK;` |
|  1700441 | 1044 |  |
|        - | 1045 | `/*` |
|        - | 1046 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1047 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1048 | ` * level is zero.` |
|        - | 1049 | ` */` |
|    75910 | 1050 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 | 1051 |  |
|    75912 | 1052 | `	SyToken *pCur = pStart;` |
|    75912 | 1053 | `	sxi32 iNest = 0;` |
|    75912 | 1054 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1055 | `		/* Last expression */` |
|    40352 | 1056 | `		return SXERR_EOF;` |
|        - | 1057 | `	}` |
|   143398 | 1058 | `	while( pCur < pEnd ){` |
|   129982 | 1059 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    22146 | 1060 | `			break;` |
|        - | 1061 | `		}` |
|   107838 | 1062 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     6254 | 1063 | `			iNest++;` |
|   104712 | 1064 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     6256 | 1065 | `			iNest--;` |
|     3127 | 1066 | `		}` |
|   107838 | 1067 | `		pCur++;` |
|        2 | 1068 | `	}` |
|    35562 | 1069 | `	*ppNext = pCur;` |
|    35562 | 1070 | `	return SXRET_OK;` |
|    37957 | 1071 |  |
|        - | 1072 | `/*` |
|        - | 1073 | ` * Free an expression tree.` |
|        - | 1074 | ` */` |
|  2935862 | 1075 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 | 1076 |  |
|  2935864 | 1077 | `	if( pNode->pLeft ){` |
|        - | 1078 | `		/* Release the left tree */` |
|  1098264 | 1079 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   549131 | 1080 | `	}` |
|  2935864 | 1081 | `	if( pNode->pRight ){` |
|        - | 1082 | `		/* Release the right tree */` |
|   607978 | 1083 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   303988 | 1084 | `	}` |
|  2935864 | 1085 | `	if( pNode->pCond ){` |
|        - | 1086 | `		/* Release the conditional tree used by the ternary operator */` |
|     2058 | 1087 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1028 | 1088 | `	}` |
|  2935864 | 1089 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1090 | `		ph7_expr_node **apArg;` |
|        - | 1091 | `		sxu32 n;` |
|        - | 1092 | `		/* Release node arguments */` |
|   362152 | 1093 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   763798 | 1094 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   401648 | 1095 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   200825 | 1096 | `		}` |
|   362152 | 1097 | `		SySetRelease(&pNode->aNodeArgs);` |
|   181075 | 1098 | `	}` |
|        - | 1099 | `	/* Finally,release this node */` |
|  2935864 | 1100 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2935864 | 1101 |  |
|        - | 1102 | `/*` |
|        - | 1103 | ` * Free an expression tree.` |
|        - | 1104 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1105 | ` */` |
|   775520 | 1106 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1107 |  |
|        - | 1108 | `	ph7_expr_node **apNode;` |
|        - | 1109 | `	sxu32 n;` |
|   775522 | 1110 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  4176348 | 1111 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  3400828 | 1112 | `		if( apNode[n] ){` |
|   775856 | 1113 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   387927 | 1114 | `		}` |
|  1700415 | 1115 | `	}` |
|   775522 | 1116 | `	return SXRET_OK;` |
|        2 | 1117 |  |
|        - | 1118 | `/*` |
|        - | 1119 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1120 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1121 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1122 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1123 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1124 | ` */` |
|  1088918 | 1125 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        2 | 1126 |  |
|  1088920 | 1127 | `	if( pNode == 0 ){` |
|   670770 | 1128 | `		return 0;` |
|        - | 1129 | `	}` |
|   418152 | 1130 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       13 | 1131 | `		return 1;` |
|        - | 1132 | `	}` |
|   418140 | 1133 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        5 | 1134 | `		return 1;` |
|        - | 1135 | `	}` |
|   418136 | 1136 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1137 | `		return 1;` |
|        - | 1138 | `	}` |
|   418136 | 1139 | `	return 0;` |
|   544461 | 1140 |  |
|        - | 1141 | `/*` |
|        - | 1142 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1143 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1144 | ` */` |
|   246214 | 1145 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1146 |  |
|        - | 1147 | `	sxi32 iExprOp;` |
|   246216 | 1148 | `	if( pNode->pOp == 0 ){` |
|   148642 | 1149 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1150 | `	}` |
|    97576 | 1151 | `	iExprOp = pNode->pOp->iOp;` |
|    97576 | 1152 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    67840 | 1153 | `			return TRUE;` |
|        - | 1154 | `	}` |
|    29738 | 1155 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    29734 | 1156 | `		if( pNode->pLeft->pOp ) {` |
|       18 | 1157 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        5 | 1158 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1159 | `				return FALSE;` |
|        1 | 1160 | `			}` |
|    29725 | 1161 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1162 | `			return FALSE;` |
|        - | 1163 | `		}` |
|    29734 | 1164 | `		return TRUE;` |
|        - | 1165 | `	}` |
|        5 | 1166 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1167 | `		return TRUE;` |
|        - | 1168 | `	}` |
|        - | 1169 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1170 | `	return FALSE;` |
|   123109 | 1171 |  |
|        - | 1172 | `/* Forward declaration */` |
|        - | 1173 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1174 | `/* Macro to check if the given node is a terminal.` |
|        - | 1175 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1176 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1177 | ` * linked ternary/elvis node). */` |
|        - | 1178 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1179 | `/*` |
|        - | 1180 | ` * Buid an expression tree for each given function argument.` |
|        - | 1181 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1182 | ` */` |
|   301610 | 1183 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1184 |  |
|        - | 1185 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1186 | `	sxi32 rc;` |
|        - | 1187 | `	/* Process function arguments from left to right */` |
|   301612 | 1188 | `	iCur = 0;` |
|   321346 | 1189 | `	for(;;){` |
|   642694 | 1190 | `		if( iCur >= nToken ){` |
|        - | 1191 | `			/* No more arguments to process */` |
|   301586 | 1192 | `			break;` |
|        - | 1193 | `		}` |
|   341110 | 1194 | `		iNode = iCur;` |
|   341110 | 1195 | `		iNest = 0;` |
|   850954 | 1196 | `		while( iCur < nToken ){` |
|   549368 | 1197 | `			if( apNode[iCur] ){` |
|   537624 | 1198 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    19763 | 1199 | `					break;` |
|   498102 | 1200 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    27296 | 1201 | `					iNest++;` |
|   484455 | 1202 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    27276 | 1203 | `					iNest--;` |
|    13637 | 1204 | `				}` |
|   249050 | 1205 | `			}` |
|   509846 | 1206 | `			iCur++;` |
|        2 | 1207 | `		}` |
|   341110 | 1208 | `		if( iCur > iNode ){` |
|   341104 | 1209 | `			SyString sArgName = {0, 0};` |
|        - | 1210 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1211 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1212 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   342889 | 1213 | `			if( (iCur - iNode) >= 2` |
|   188951 | 1214 | `				&& apNode[iNode]` |
|    36800 | 1215 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    20229 | 1216 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     3616 | 1217 | `				&& apNode[iNode+1]` |
|     3576 | 1218 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1219 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      178 | 1220 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      178 | 1221 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      178 | 1222 | `				apNode[iNode] = 0;` |
|      178 | 1223 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      178 | 1224 | `				apNode[iNode+1] = 0;` |
|      178 | 1225 | `				iNode += 2;` |
|        - | 1226 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1227 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      178 | 1228 | `				if( iNode >= iCur ){` |
|        4 | 1229 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1230 | `						pOp->pStart->nLine,` |
|        - | 1231 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1232 | `						&sArgName);` |
|        3 | 1233 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1234 | `						rc = SXERR_SYNTAX;` |
|        1 | 1235 | `					}` |
|        3 | 1236 | `					return rc;` |
|        - | 1237 | `				}` |
|       87 | 1238 | `			}` |
|   341100 | 1239 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1240 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1241 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1242 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1243 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1244 | `					apNode[iNode] = 0;` |
|      ! 0 | 1245 | `			}` |
|   341102 | 1246 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   341102 | 1247 | `			if( apNode[iNode] ){` |
|   341102 | 1248 | `				if( sArgName.nByte > 0 ){` |
|      176 | 1249 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      176 | 1250 | `					apNode[iNode]->sArgName = sArgName;` |
|       87 | 1251 | `				}` |
|        - | 1252 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   341102 | 1253 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   170552 | 1254 | `			}else{` |
|        - | 1255 | `				/* No expression before comma */` |
|      ! 0 | 1256 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1257 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1258 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1259 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1260 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1261 | `				}` |
|      ! 0 | 1262 | `				return rc;` |
|        - | 1263 | `			}` |
|   170552 | 1264 | `		}else{` |
|        - | 1265 | `			/* Comma with no preceding argument */` |
|        7 | 1266 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1267 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1268 | `				rc = SXERR_SYNTAX;` |
|        3 | 1269 | `			}` |
|        7 | 1270 | `			return rc;` |
|        - | 1271 | `		}` |
|        - | 1272 | `		/* Jump trailing comma */` |
|   341102 | 1273 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    39518 | 1274 | `			iCur++;` |
|    39518 | 1275 | `			if( iCur >= nToken ){` |
|        - | 1276 | `				/* Trailing comma after last argument */` |
|       19 | 1277 | `				break;` |
|        - | 1278 | `			}` |
|    19749 | 1279 | `		}` |
|        2 | 1280 | `	}` |
|   301604 | 1281 | `	return SXRET_OK;` |
|   150807 | 1282 |  |
|        - | 1283 | ` /*` |
|        - | 1284 | `  * Create an expression tree from an array of tokens.` |
|        - | 1285 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1286 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1287 | `  */` |
|  1209156 | 1288 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1289 | ` {` |
|        - | 1290 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1291 | `	 ph7_expr_node *pNode;` |
|        - | 1292 | `	 sxi32 iCur;` |
|        - | 1293 | `	 sxi32 rc;` |
|  1209158 | 1294 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1295 | `		 /* TICKET 1433-17: self evaluating node */` |
|   547008 | 1296 | `		 return SXRET_OK;` |
|        - | 1297 | `	 }` |
|        - | 1298 | `	 /* Process expressions enclosed in parenthesis first */` |
|  4065792 | 1299 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1300 | `		 sxi32 iNest;` |
|        - | 1301 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1302 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1303 | `		  */` |
|  3403644 | 1304 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  3378786 | 1305 | `			 continue;` |
|        - | 1306 | `		 }` |
|    24860 | 1307 | `		 iNest = 1;` |
|    24860 | 1308 | `		 iLeft = iCur;` |
|        - | 1309 | `		 /* Find the closing parenthesis */` |
|    24860 | 1310 | `		 iCur++;` |
|   165278 | 1311 | `		 while( iCur < nToken ){` |
|   165278 | 1312 | `			 if( apNode[iCur] ){` |
|   165278 | 1313 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1314 | `					 /* Decrement nesting level */` |
|    43070 | 1315 | `					 iNest--;` |
|    43070 | 1316 | `					 if( iNest <= 0 ){` |
|    24860 | 1317 | `						 break;` |
|        2 | 1318 | `					 }` |
|   131315 | 1319 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1320 | `					 /* Increment nesting level */` |
|    18212 | 1321 | `					 iNest++;` |
|     9105 | 1322 | `				 }` |
|    70209 | 1323 | `			 }` |
|   140420 | 1324 | `			 iCur++;` |
|        2 | 1325 | `		 }` |
|    24860 | 1326 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1327 | `			 /* Recurse and process this expression */` |
|    24860 | 1328 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    24860 | 1329 | `			 if( rc != SXRET_OK ){` |
|        3 | 1330 | `				 return rc;` |
|        - | 1331 | `			 }` |
|    12428 | 1332 | `		 }` |
|        - | 1333 | `		 /* Free the left and right nodes */` |
|    24858 | 1334 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    24858 | 1335 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    24858 | 1336 | `		 apNode[iLeft] = 0;` |
|    24858 | 1337 | `		 apNode[iCur] = 0;` |
|    12430 | 1338 | `	 }` |
|        - | 1339 | `	  /* Process expressions enclosed in braces */` |
|  4224710 | 1340 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1341 | `		 sxi32 iNest;` |
|        - | 1342 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1343 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1344 | `		  */` |
|  3568906 | 1345 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3568904 | 1346 | `			 continue;` |
|        - | 1347 | `		 }` |
|        3 | 1348 | `		 iNest = 1;` |
|        3 | 1349 | `		 iLeft = iCur;` |
|        - | 1350 | `		 /* Find the closing parenthesis */` |
|        3 | 1351 | `		 iCur++;` |
|        3 | 1352 | `		 while( iCur < nToken ){` |
|        3 | 1353 | `			 if( apNode[iCur] ){` |
|        3 | 1354 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1355 | `					 /* Decrement nesting level */` |
|        3 | 1356 | `					 iNest--;` |
|        3 | 1357 | `					 if( iNest <= 0 ){` |
|        3 | 1358 | `						 break;` |
|      ! 0 | 1359 | `					 }` |
|      ! 0 | 1360 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1361 | `					 /* Increment nesting level */` |
|      ! 0 | 1362 | `					 iNest++;` |
|      ! 0 | 1363 | `				 }` |
|      ! 0 | 1364 | `			 }` |
|      ! 0 | 1365 | `			 iCur++;` |
|      ! 0 | 1366 | `		 }` |
|        3 | 1367 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1368 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1369 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1370 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1371 | `				 return rc;` |
|        - | 1372 | `			 }` |
|      ! 0 | 1373 | `		 }` |
|        - | 1374 | `		 /* Free the left and right nodes */` |
|        3 | 1375 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        3 | 1376 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        3 | 1377 | `		 apNode[iLeft] = 0;` |
|        3 | 1378 | `		 apNode[iCur] = 0;` |
|        2 | 1379 | `	 }` |
|        - | 1380 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   655806 | 1381 | `	 iLeft = -1;` |
|  4224676 | 1382 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3568884 | 1383 | `		 if( apNode[iCur] == 0 ){` |
|  1359776 | 1384 | `			 continue;` |
|        - | 1385 | `		 }` |
|  2209110 | 1386 | `		 pNode = apNode[iCur];` |
|  2209110 | 1387 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   583794 | 1388 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1389 | `				 /* Collect function arguments */` |
|   348110 | 1390 | `				 sxi32 iPtr = 0;` |
|   348110 | 1391 | `				 sxi32 nFuncTok = 0;` |
|  1245586 | 1392 | `				 while( nFuncTok + iCur < nToken ){` |
|  1245586 | 1393 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1233842 | 1394 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   360396 | 1395 | `							 iPtr++;` |
|  1053645 | 1396 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   360396 | 1397 | `							 iPtr--;` |
|   360396 | 1398 | `							 if( iPtr <= 0 ){` |
|   348110 | 1399 | `								 break;` |
|        - | 1400 | `							 }` |
|     6143 | 1401 | `						 }` |
|   442866 | 1402 | `					 }` |
|   897478 | 1403 | `					 nFuncTok++;` |
|        2 | 1404 | `				 }` |
|   348110 | 1405 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1406 | `					 /* Syntax error */` |
|      ! 0 | 1407 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1408 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1409 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1410 | `					 }` |
|      ! 0 | 1411 | `					 return rc;` |
|        - | 1412 | `				 }` |
|   348110 | 1413 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1414 | `					 /* Syntax error */` |
|      ! 0 | 1415 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1416 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1417 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1418 | `					 }` |
|      ! 0 | 1419 | `					 return rc;` |
|        - | 1420 | `				 }` |
|   348110 | 1421 | `				 if( nFuncTok > 1 ){` |
|        - | 1422 | `					 /* Process function arguments */` |
|   301612 | 1423 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   301612 | 1424 | `					 if( rc != SXRET_OK ){` |
|        9 | 1425 | `						 return rc;` |
|        - | 1426 | `					 }` |
|   150801 | 1427 | `				 }` |
|        - | 1428 | `				 /* Link the node to the tree */` |
|   348102 | 1429 | `				 pNode->pLeft = apNode[iLeft];` |
|   348102 | 1430 | `				 apNode[iLeft] = 0;` |
|  1245554 | 1431 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   897454 | 1432 | `					 apNode[iCur+iPtr] = 0;` |
|   448728 | 1433 | `				 }` |
|   409736 | 1434 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1435 | `				 /* Subscripting */` |
|    75298 | 1436 | `				 sxi32 iArrTok = iCur + 1;` |
|    75298 | 1437 | `				 sxi32 iNest = 1;` |
|    75380 | 1438 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1439 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1440 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1441 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    75296 | 1442 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1443 | `						 /* Syntax error */` |
|      ! 0 | 1444 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1445 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1446 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1447 | `						 }` |
|      ! 0 | 1448 | `						 return rc;` |
|        - | 1449 | `				 }` |
|        - | 1450 | `				 /* Collect index tokens */` |
|   135954 | 1451 | `				 while( iArrTok < nToken ){` |
|   135954 | 1452 | `					 if( apNode[iArrTok] ){` |
|   135922 | 1453 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1454 | `							 /* Increment nesting level */` |
|      ! 0 | 1455 | `							 iNest++;` |
|   135922 | 1456 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1457 | `							 /* Decrement nesting level */` |
|    75298 | 1458 | `							 iNest--;` |
|    75298 | 1459 | `							 if( iNest <= 0 ){` |
|    75298 | 1460 | `								 break;` |
|        - | 1461 | `							 }` |
|      ! 0 | 1462 | `						 }` |
|    30312 | 1463 | `					 }` |
|    60658 | 1464 | `					 ++iArrTok;` |
|        2 | 1465 | `				 }` |
|    75298 | 1466 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1467 | `					 /* Recurse and process this expression */` |
|    60548 | 1468 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    60548 | 1469 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1470 | `						 return rc;` |
|        - | 1471 | `					 }` |
|        - | 1472 | `					 /* Link the node to it's index */` |
|    60548 | 1473 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    30273 | 1474 | `				 }` |
|        - | 1475 | `				 /* Link the node to the tree */` |
|    75298 | 1476 | `				 pNode->pLeft = apNode[iLeft];` |
|    75298 | 1477 | `				 pNode->pRight = 0;` |
|    75298 | 1478 | `				 apNode[iLeft] = 0;` |
|   211250 | 1479 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   135954 | 1480 | `					 apNode[iNest] = 0;` |
|    67978 | 1481 | `				 }` |
|    37650 | 1482 | `			 }else{` |
|        - | 1483 | `				 /* Member access operators [i.e: '->','::'] */` |
|   160390 | 1484 | `				  iRight = iCur + 1;` |
|   160390 | 1485 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1486 | `					 iRight++;` |
|      ! 0 | 1487 | `				 }` |
|   160390 | 1488 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1489 | `					 /* Syntax error */` |
|        5 | 1490 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1491 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1492 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1493 | `					 }` |
|        5 | 1494 | `					 return rc;` |
|        - | 1495 | `				 }` |
|        - | 1496 | `				 /* Link the node to the tree */` |
|   160386 | 1497 | `				 pNode->pLeft = apNode[iLeft];` |
|   240422 | 1498 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   160270 | 1499 | `					 && pNode->pLeft->pOp == 0 &&` |
|   160076 | 1500 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1501 | `						 /* Syntax error */` |
|      ! 0 | 1502 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1503 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1504 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1505 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1506 | `						 }` |
|      ! 0 | 1507 | `						 return rc;` |
|        - | 1508 | `				 }` |
|   160386 | 1509 | `				 pNode->pRight = apNode[iRight];` |
|   160386 | 1510 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1511 | `			 }` |
|   291890 | 1512 | `		 }` |
|  2209098 | 1513 | `		 iLeft = iCur;` |
|  1104550 | 1514 | `	 }` |
|        - | 1515 | `	 /* Handle left associative (new, clone) operators */` |
|  4224644 | 1516 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3568852 | 1517 | `		 if( apNode[iCur] == 0 ){` |
|  1959054 | 1518 | `			 continue;` |
|        - | 1519 | `		 }` |
|  1609800 | 1520 | `		 pNode = apNode[iCur];` |
|  1609800 | 1521 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1522 | `			 SyToken *pToken;` |
|        - | 1523 | `			 /* Get the left node */` |
|    15500 | 1524 | `			 iLeft = iCur + 1;` |
|    30966 | 1525 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    15468 | 1526 | `				 iLeft++;` |
|        2 | 1527 | `			 }` |
|    15500 | 1528 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1529 | `				  /* Syntax error */` |
|      ! 0 | 1530 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1531 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1532 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1533 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1534 | `				 }` |
|      ! 0 | 1535 | `				 return rc;` |
|        - | 1536 | `			 }` |
|        - | 1537 | `			 /* Make sure the operand are of a valid type */` |
|    15500 | 1538 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1539 | `				 /* Clone:` |
|        - | 1540 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1541 | `				  *  ++ function call (including annonymous)` |
|        - | 1542 | `				  *  ++ array member` |
|        - | 1543 | `				  *  ++ 'new' operator` |
|        - | 1544 | `				  * Example:` |
|        - | 1545 | `				  *   clone $pObj;` |
|        - | 1546 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1547 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1548 | `				  */` |
|       20 | 1549 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1550 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1551 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1552 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1553 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1554 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1555 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1556 | `						 }` |
|      ! 0 | 1557 | `						 return rc;` |
|        - | 1558 | `					 }` |
|        8 | 1559 | `				 }` |
|       11 | 1560 | `			 }else{` |
|        - | 1561 | `				 /* New */` |
|    15482 | 1562 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1563 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1564 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1565 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1566 | `						 /* Syntax error */` |
|      ! 0 | 1567 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1568 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1569 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1570 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1571 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1572 | `						 }` |
|      ! 0 | 1573 | `						 return rc;` |
|        - | 1574 | `					 }` |
|        8 | 1575 | `				 }` |
|        - | 1576 | `			 }` |
|        - | 1577 | `			  /* Link the node to the tree */` |
|    15500 | 1578 | `			 pNode->pLeft = apNode[iLeft];` |
|    15500 | 1579 | `			 apNode[iLeft] = 0;` |
|    15500 | 1580 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7749 | 1581 | `		 }` |
|   804901 | 1582 | `	 }` |
|        - | 1583 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   655794 | 1584 | `	 iLeft = -1;` |
|  4227816 | 1585 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3568852 | 1586 | `		 if( apNode[iCur] == 0 ){` |
|  1959054 | 1587 | `			 continue;` |
|        - | 1588 | `		 }` |
|  1609800 | 1589 | `		 pNode = apNode[iCur];` |
|  1609800 | 1590 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8836 | 1591 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3190 | 1592 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1593 | `					 /* Link the node to the tree */` |
|     3192 | 1594 | `					 pNode->pLeft = apNode[iLeft];` |
|     3192 | 1595 | `					 apNode[iLeft] = 0;` |
|     1595 | 1596 | `			 }` |
|     6003 | 1597 | `		  }` |
|  1612972 | 1598 | `		 iLeft = iCur;` |
|   808073 | 1599 | `	  }` |
|   658966 | 1600 | `	 iLeft = -1;` |
|  4227816 | 1601 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3568852 | 1602 | `		 if( apNode[iCur] == 0 ){` |
|  1962244 | 1603 | `			 continue;` |
|        - | 1604 | `		 }` |
|  1606610 | 1605 | `		 pNode = apNode[iCur];` |
|  1606610 | 1606 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8817 | 1607 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8818 | 1608 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1609 | `					 /* Syntax error */` |
|      ! 0 | 1610 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1611 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1612 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1613 | `					 }` |
|      ! 0 | 1614 | `					 return rc;` |
|        - | 1615 | `			 }` |
|        - | 1616 | `			 /* Link the node to the tree */` |
|     8818 | 1617 | `			 pNode->pLeft = apNode[iLeft];` |
|     8818 | 1618 | `			 apNode[iLeft] = 0;` |
|        - | 1619 | `			 /* Mark as pre-increment/decrement node */` |
|     8818 | 1620 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4408 | 1621 | `		  }` |
|  1606610 | 1622 | `		 iLeft = iCur;` |
|   803306 | 1623 | `	 }` |
|        - | 1624 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   658966 | 1625 | `	  iLeft = 0;` |
|  4227810 | 1626 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3568848 | 1627 | `		  if( apNode[iCur] ){` |
|  1597790 | 1628 | `			  pNode = apNode[iCur];` |
|  1597790 | 1629 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    39458 | 1630 | `				  if( iLeft > 0 ){` |
|        - | 1631 | `					  /* Link the node to the tree */` |
|    39456 | 1632 | `					  pNode->pLeft = apNode[iLeft];` |
|    39456 | 1633 | `					  apNode[iLeft] = 0;` |
|    39456 | 1634 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1635 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1636 | `							   /* Syntax error */` |
|      ! 0 | 1637 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1638 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1639 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1640 | `							  }` |
|      ! 0 | 1641 | `							  return rc;` |
|        - | 1642 | `						  }` |
|       36 | 1643 | `					  }` |
|    19729 | 1644 | `				  }else{` |
|        - | 1645 | `					  /* Syntax error */` |
|        3 | 1646 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1647 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1648 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1649 | `					  }` |
|        3 | 1650 | `					  return rc;` |
|        - | 1651 | `				  }` |
|    19727 | 1652 | `			  }` |
|        - | 1653 | `			  /* Save terminal position */` |
|  1597788 | 1654 | `			  iLeft = iCur;` |
|   798893 | 1655 | `		  }` |
|  1784424 | 1656 | `	  }` |
|        - | 1657 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  7248498 | 1658 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  6589546 | 1659 | `		 iLeft = -1;` |
| 42277692 | 1660 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 35688158 | 1661 | `			 if( apNode[iCur] == 0 ){` |
| 22772440 | 1662 | `				 continue;` |
|        - | 1663 | `			 }` |
| 12915720 | 1664 | `			 pNode = apNode[iCur];` |
| 12915720 | 1665 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1666 | `				 /* Get the right node */` |
|   199346 | 1667 | `				 iRight = iCur + 1;` |
|   284518 | 1668 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    85174 | 1669 | `					 iRight++;` |
|        2 | 1670 | `				 }` |
|   199346 | 1671 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1672 | `					 /* Syntax error */` |
|        9 | 1673 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1674 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1675 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1676 | `					 }` |
|        9 | 1677 | `					 return rc;` |
|        - | 1678 | `				 }` |
|   199338 | 1679 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1680 | `					 sxi32  iTmp;` |
|        - | 1681 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1682 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1683 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1684 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1685 | `					  * is swapped below. */` |
|       50 | 1686 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1687 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1688 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1689 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1690 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1691 | `						 }` |
|        3 | 1692 | `						 return rc;` |
|        - | 1693 | `					 }` |
|       48 | 1694 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1695 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1696 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1697 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1698 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1699 | `						 }` |
|      ! 0 | 1700 | `						 return rc;` |
|        - | 1701 | `					 }` |
|       48 | 1702 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1703 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1704 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1705 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1706 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1707 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1708 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1709 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1710 | `									 }` |
|      ! 0 | 1711 | `									 return rc;` |
|        - | 1712 | `							 }` |
|      ! 0 | 1713 | `						 }` |
|       16 | 1714 | `					 }` |
|        - | 1715 | `					 /* Swap operands */` |
|       48 | 1716 | `					 iTmp = iRight;` |
|       48 | 1717 | `					 iRight = iLeft;` |
|       48 | 1718 | `					 iLeft = iTmp;` |
|       23 | 1719 | `				 }` |
|        - | 1720 | `				 /* Link the node to the tree */` |
|   199336 | 1721 | `				 pNode->pLeft = apNode[iLeft];` |
|   199336 | 1722 | `				 pNode->pRight = apNode[iRight];` |
|   199336 | 1723 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    99667 | 1724 | `			 }` |
| 12915710 | 1725 | `			 iLeft = iCur;` |
|  6457856 | 1726 | `		 }` |
|  3294769 | 1727 | `	 }` |
|        - | 1728 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1729 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1730 | `	  * we are dealing with a single operator.` |
|        - | 1731 | `	  */` |
|   658954 | 1732 | `	  iLeft = -1;` |
|  4218866 | 1733 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3561972 | 1734 | `		  if( apNode[iCur] == 0 ){` |
|  2408466 | 1735 | `			  continue;` |
|        - | 1736 | `		  }` |
|  1153508 | 1737 | `		  pNode = apNode[iCur];` |
|  1153508 | 1738 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2060 | 1739 | `			  sxi32 iNest = 1;` |
|     2060 | 1740 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1741 | `				  /* Missing condition */` |
|        3 | 1742 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1743 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1744 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1745 | `				  }` |
|        3 | 1746 | `				  return rc;` |
|        - | 1747 | `			  }` |
|        - | 1748 | `			  /* Get the right node */` |
|     2058 | 1749 | `			  iRight = iCur + 1;` |
|     4358 | 1750 | `			  while( iRight < nToken  ){` |
|     4358 | 1751 | `				  if( apNode[iRight] ){` |
|     4046 | 1752 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1753 | `						  /* Increment nesting level */` |
|      ! 0 | 1754 | `						  ++iNest;` |
|     4046 | 1755 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1756 | `						  /* Decrement nesting level */` |
|     2058 | 1757 | `						  --iNest;` |
|     2058 | 1758 | `						  if( iNest <= 0 ){` |
|     2058 | 1759 | `							  break;` |
|        - | 1760 | `						  }` |
|      ! 0 | 1761 | `					  }` |
|      994 | 1762 | `				  }` |
|     2302 | 1763 | `				  iRight++;` |
|        2 | 1764 | `			  }` |
|     2058 | 1765 | `			  if( iRight > iCur + 1 ){` |
|        - | 1766 | `				  /* Recurse and process the then expression */` |
|     1990 | 1767 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1990 | 1768 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1769 | `					  return rc;` |
|        - | 1770 | `				  }` |
|        - | 1771 | `				  /* Link the node to the tree */` |
|     1990 | 1772 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      994 | 1773 | `			  }else{` |
|        - | 1774 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1775 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1776 | `			  }` |
|     2058 | 1777 | `			  apNode[iCur + 1] = 0;` |
|     2058 | 1778 | `			  if( iRight + 1 < nToken ){` |
|        - | 1779 | `				  /* Recurse and process the else expression */` |
|     2058 | 1780 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2058 | 1781 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1782 | `					  return rc;` |
|        - | 1783 | `				  }` |
|        - | 1784 | `				  /* Link the node to the tree */` |
|     2058 | 1785 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2058 | 1786 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1030 | 1787 | `			  }else{` |
|      ! 0 | 1788 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1789 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1790 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1791 | `				 }` |
|      ! 0 | 1792 | `				 return rc;` |
|        - | 1793 | `			  }` |
|        - | 1794 | `			  /* Point to the condition */` |
|     2058 | 1795 | `			  pNode->pCond  = apNode[iLeft];` |
|     2058 | 1796 | `			  apNode[iLeft] = 0;` |
|     2058 | 1797 | `			  break;` |
|        - | 1798 | `		  }` |
|  1151450 | 1799 | `		  iLeft = iCur;` |
|   575726 | 1800 | `	  }` |
|        - | 1801 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1802 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1803 | `	  * so there is no need for a precedence loop here.` |
|        - | 1804 | `	  */` |
|   658952 | 1805 | `	 iRight = -1;` |
|  4227612 | 1806 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3568716 | 1807 | `		 if( apNode[iCur] == 0 ){` |
|  2663458 | 1808 | `			 continue;` |
|        - | 1809 | `		 }` |
|   905260 | 1810 | `		 pNode = apNode[iCur];` |
|   905260 | 1811 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1812 | `			 /* Get the left node */` |
|   246188 | 1813 | `			 iLeft = iCur - 1;` |
|   358810 | 1814 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   112624 | 1815 | `				 iLeft--;` |
|        2 | 1816 | `			 }` |
|   246188 | 1817 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1818 | `				 /* Syntax error */` |
|       43 | 1819 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1820 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1821 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1822 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1823 | `				 }else{` |
|       39 | 1824 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1825 | `				 }` |
|       43 | 1826 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1827 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1828 | `				 }` |
|       43 | 1829 | `				 return rc;` |
|        - | 1830 | `			 }` |
|        - | 1831 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 1832 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 1833 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 1834 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 1835 | `			  * a write. */` |
|   246146 | 1836 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        9 | 1837 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1838 | `					 "Can't use nullsafe operator in write context");` |
|        9 | 1839 | `				 if( rc != SXERR_ABORT ){` |
|        9 | 1840 | `					 rc = SXERR_SYNTAX;` |
|        4 | 1841 | `				 }` |
|        9 | 1842 | `				 return rc;` |
|        - | 1843 | `			 }` |
|   246138 | 1844 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1845 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1846 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1847 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1848 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1849 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1850 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1851 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1852 | `					 }else{` |
|        4 | 1853 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1854 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1855 | `					 }` |
|        5 | 1856 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1857 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1858 | `					 }` |
|        5 | 1859 | `					 return rc;` |
|        - | 1860 | `				 }` |
|       26 | 1861 | `			 }` |
|        - | 1862 | `			 /* Link the node to the tree (Reverse) */` |
|   246134 | 1863 | `			 pNode->pLeft = apNode[iRight];` |
|   246134 | 1864 | `			 pNode->pRight = apNode[iLeft];` |
|   246134 | 1865 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   123066 | 1866 | `		 }` |
|   905206 | 1867 | `		 iRight = iCur;` |
|   452604 | 1868 | `	 }` |
|        - | 1869 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  3294482 | 1870 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2635586 | 1871 | `		 iLeft = -1;` |
| 16910170 | 1872 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 14274586 | 1873 | `			 if( apNode[iCur] == 0 ){` |
| 11638596 | 1874 | `				 continue;` |
|        - | 1875 | `			 }` |
|  2635992 | 1876 | `			 pNode = apNode[iCur];` |
|  2635992 | 1877 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1878 | `				 /* Get the right node */` |
|       72 | 1879 | `				 iRight = iCur + 1;` |
|      110 | 1880 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1881 | `					 iRight++;` |
|        2 | 1882 | `				 }` |
|       72 | 1883 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1884 | `					 /* Syntax error */` |
|      ! 0 | 1885 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1886 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1887 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1888 | `					 }` |
|      ! 0 | 1889 | `					 return rc;` |
|        - | 1890 | `				 }` |
|        - | 1891 | `				 /* Link the node to the tree */` |
|       72 | 1892 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1893 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1894 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1895 | `			 }` |
|  2635992 | 1896 | `			 iLeft = iCur;` |
|  1317997 | 1897 | `		 }` |
|  1317794 | 1898 | `	 }` |
|        - | 1899 | `	 /* Point to the root of the expression tree */` |
|  3568620 | 1900 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2909742 | 1901 | `		 if( apNode[iCur] ){` |
|   599308 | 1902 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1903 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1904 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1905 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1906 | `				  }` |
|       20 | 1907 | `				  return rc;` |
|        - | 1908 | `			 }` |
|   599290 | 1909 | `			 apNode[0] = apNode[iCur];` |
|   599290 | 1910 | `			 apNode[iCur] = 0;` |
|   299644 | 1911 | `		 }` |
|  1454863 | 1912 | `	 }` |
|   658880 | 1913 | `	 return SXRET_OK;` |
|   602994 | 1914 | ` }` |
|        - | 1915 | ` /*` |
|        - | 1916 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1917 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1918 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1919 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1920 | `  */` |
|   775520 | 1921 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1922 |  |
|        - | 1923 | `	ph7_expr_node **apNode;` |
|        - | 1924 | `	ph7_expr_node *pNode;` |
|        - | 1925 | `	sxi32 rc;` |
|        - | 1926 | `	/* Reset node container */` |
|   775522 | 1927 | `	SySetReset(pExprNode);` |
|   775522 | 1928 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1929 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1930 | `	{` |
|   775522 | 1931 | `		int iLastWasTerm = 0;` |
|  4176348 | 1932 | `		while( pGen->pIn < pGen->pEnd ){` |
|  3400862 | 1933 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  3400862 | 1934 | `			if( rc != SXRET_OK ){` |
|       35 | 1935 | `				return rc;` |
|        - | 1936 | `			}` |
|        - | 1937 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  3400828 | 1938 | `			if( pNode->xCode ){` |
|        - | 1939 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1787390 | 1940 | `				iLastWasTerm = 1;` |
|  2507134 | 1941 | `			}else if( pNode->pOp ){` |
|        - | 1942 | `				/* Operator node */` |
|   789892 | 1943 | `				iLastWasTerm = 0;` |
|   394947 | 1944 | `			}else{` |
|        - | 1945 | `				/* Delimiter: ')' and ']' end terms */` |
|   823550 | 1946 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1947 | `			}` |
|        - | 1948 | `			/* Save the extracted node */` |
|  3400828 | 1949 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1950 | `		}` |
|        - | 1951 | `	}` |
|   775488 | 1952 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1953 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1954 | `		*ppRoot = 0;` |
|      ! 0 | 1955 | `		return SXRET_OK;` |
|        - | 1956 | `	}` |
|   775488 | 1957 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1958 | `	/* Make sure we are dealing with valid nodes */` |
|   775488 | 1959 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   775488 | 1960 | `	if( rc != SXRET_OK ){` |
|        - | 1961 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1962 | `		 * cleanup the mess left behind.` |
|        - | 1963 | `		 */` |
|       51 | 1964 | `		*ppRoot = 0;` |
|       51 | 1965 | `		return rc;` |
|        - | 1966 | `	}` |
|        - | 1967 | `	/* Build the tree */` |
|   775438 | 1968 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   775438 | 1969 | `	if( rc != SXRET_OK ){` |
|        - | 1970 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      100 | 1971 | `		*ppRoot = 0;` |
|      100 | 1972 | `		return rc;` |
|        - | 1973 | `	}` |
|        - | 1974 | `	/* Point to the root of the tree */` |
|   775340 | 1975 | `	*ppRoot = apNode[0];` |
|   775340 | 1976 | `	return SXRET_OK;` |
|   387762 | 1977 |  |
|        - | 1978 |  |
