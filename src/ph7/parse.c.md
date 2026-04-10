# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 909/1061 lines (85.67%)

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
|   744214 |  264 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  265 |  |
|   744216 |  266 | `	sxu32 n = 0;` |
|        - |  267 | `	sxi32 rc;` |
|        - |  268 | `	/* Do a linear lookup on the operators table */` |
| 12198711 |  269 | `	for(;;){` |
| 24397424 |  270 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  271 | `			break;` |
|        - |  272 | `		}` |
| 24397424 |  273 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  274 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2967874 |  275 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1483938 |  276 | `		}else{` |
| 21429552 |  277 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  278 | `		}` |
| 24397424 |  279 | `		if( rc == 0 ){` |
|   747508 |  280 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  281 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   743886 |  282 | `				return &aOpTable[n];` |
|        - |  283 | `			}` |
|        - |  284 | `			/* Handle ambiguity */` |
|     3624 |  285 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  286 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  287 | `				return &aOpTable[n];` |
|        - |  288 | `			}` |
|     3404 |  289 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  290 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  291 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  292 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  293 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  294 | `					return &aOpTable[n];` |
|        - |  295 | `				}` |
|        - |  296 |  |
|        4 |  297 | `			}` |
|     1646 |  298 | `		}` |
| 23653210 |  299 | `		++n; /* Next operator in the table */` |
|        2 |  300 | `	}` |
|        - |  301 | `	/* No such operator */` |
|      ! 0 |  302 | `	return 0;` |
|   372109 |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * Delimit a set of token stream.` |
|        - |  306 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  307 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  308 | ` */` |
|   382830 |  309 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  310 |  |
|   382832 |  311 | `	SyToken *pCur = pIn;` |
|   382832 |  312 | `	sxi32 iNest = 1;` |
|  2175166 |  313 | `	for(;;){` |
|  4350334 |  314 | `		if( pCur >= pEnd ){` |
|      128 |  315 | `			break;` |
|        - |  316 | `		}` |
|  4350208 |  317 | `		if( pCur->nType & nTokStart ){` |
|        - |  318 | `			/* Increment nesting level */` |
|   240596 |  319 | `			iNest++;` |
|  4229911 |  320 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  321 | `			/* Decrement nesting level */` |
|   623300 |  322 | `			iNest--;` |
|   623300 |  323 | `			if( iNest <= 0 ){` |
|   382706 |  324 | `				break;` |
|        - |  325 | `			}` |
|   120297 |  326 | `		}` |
|        - |  327 | `		/* Advance cursor */` |
|  3967504 |  328 | `		pCur++;` |
|        2 |  329 | `	}` |
|        - |  330 | `	/* Point to the end of the chunk */` |
|   382832 |  331 | `	*ppEnd = pCur;` |
|   382832 |  332 |  |
|        - |  333 | `/*` |
|        - |  334 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  335 | ` * Note on reserved keywords.` |
|        - |  336 | ` *  According to the PHP language reference manual:` |
|        - |  337 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  338 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  339 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  340 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  341 | ` */` |
|    11518 |  342 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  343 |  |
|    17211 |  344 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11425 |  345 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  346 | `		){` |
|      146 |  347 | `			return TRUE;` |
|        - |  348 | `	}` |
|    11376 |  349 | `	if( bCheckFunc ){` |
|       92 |  350 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  351 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  352 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  353 | `				return TRUE;` |
|        - |  354 | `		}` |
|       20 |  355 | `	}` |
|        - |  356 | `	/* Not a language construct */` |
|    11344 |  357 | `	return FALSE;` |
|     5761 |  358 |  |
|        - |  359 | `/*` |
|        - |  360 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  361 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  362 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  363 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  364 | ` */` |
|   655592 |  365 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  366 |  |
|        - |  367 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  368 | `	sxi32 i,rc;` |
|        - |  369 |  |
|   655594 |  370 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  371 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  372 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  373 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  374 | `	}` |
|   655594 |  375 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3544948 |  376 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2889390 |  377 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  378 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      296 |  379 | `			continue;` |
|        - |  380 | `		}` |
|  2889096 |  381 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   331824 |  382 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    17096 |  383 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  384 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   308960 |  385 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  386 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  387 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  388 | `						 */` |
|   308960 |  389 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   308960 |  390 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   308960 |  391 | `						apNode[i]->pOp = &sFCallOp;` |
|   154479 |  392 | `					}` |
|   154479 |  393 | `			}` |
|   331824 |  394 | `			iParen++;` |
|  2723185 |  395 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   331824 |  396 | `			if( iParen <= 0 ){` |
|       13 |  397 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  398 | `				if( rc != SXERR_ABORT ){` |
|       13 |  399 | `					rc = SXERR_SYNTAX;` |
|        6 |  400 | `				}` |
|       13 |  401 | `				return rc;` |
|        - |  402 | `			}` |
|   331812 |  403 | `			iParen--;` |
|  2391357 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    69262 |  405 | `			iSquare++;` |
|  2190822 |  406 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    69276 |  407 | `			if( iSquare <= 0 ){` |
|        7 |  408 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  409 | `				if( rc != SXERR_ABORT ){` |
|        7 |  410 | `					rc = SXERR_SYNTAX;` |
|        3 |  411 | `				}` |
|        7 |  412 | `				return rc;` |
|        - |  413 | `			}` |
|    69270 |  414 | `			iSquare--;` |
|  2121552 |  415 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2086913 |  462 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  463 | `			if( iBraces <= 0 ){` |
|       13 |  464 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  465 | `				if( rc != SXERR_ABORT ){` |
|       13 |  466 | `					rc = SXERR_SYNTAX;` |
|        6 |  467 | `				}` |
|       13 |  468 | `				return rc;` |
|        - |  469 | `			}` |
|      ! 0 |  470 | `			iBraces--;` |
|  2086896 |  471 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1906 |  472 | `			if( iQuesty <= 0 ){` |
|        5 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  474 | `				if( rc != SXERR_ABORT ){` |
|        5 |  475 | `					rc = SXERR_SYNTAX;` |
|        2 |  476 | `				}` |
|        5 |  477 | `				return rc;` |
|        - |  478 | `			}` |
|     1902 |  479 | `			iQuesty--;` |
|  2085942 |  480 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   580828 |  481 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   580828 |  482 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1904 |  483 | `				iQuesty++;` |
|   579877 |  484 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   290413 |  504 | `		}` |
|  1444532 |  505 | `	}` |
|   655560 |  506 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  507 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  508 | `		if( rc != SXERR_ABORT ){` |
|       17 |  509 | `			rc = SXERR_SYNTAX;` |
|        8 |  510 | `		}` |
|       17 |  511 | `		return rc;` |
|        - |  512 | `	}` |
|   655544 |  513 | `	return SXRET_OK;` |
|   327798 |  514 |  |
|        - |  515 | `/*` |
|        - |  516 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  517 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  518 | ` */` |
|   528428 |  519 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  520 |  |
|   528430 |  521 | `	SyToken *pIn = *ppCur;` |
|        - |  522 | `	/* Jump the first literal seen */` |
|   528430 |  523 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   528408 |  524 | `		pIn++;` |
|   264203 |  525 | `	}` |
|   264244 |  526 | `	for(;;){` |
|   528490 |  527 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       62 |  528 | `			pIn++;` |
|       62 |  529 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       62 |  530 | `				pIn++;` |
|       30 |  531 | `			}` |
|       32 |  532 | `		}else{` |
|   264216 |  533 | `			break;` |
|        - |  534 | `		}` |
|        2 |  535 | `	}` |
|        - |  536 | `	/* Synchronize pointers */` |
|   528430 |  537 | `	*ppCur = pIn;` |
|   528430 |  538 |  |
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
|        - |  667 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  668 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  669 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  670 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  671 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  672 | ` */` |
|       84 |  673 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  674 |  |
|       86 |  675 | `	SyToken *pIn = *ppCur;` |
|        - |  676 | `	sxu32 nLine;` |
|        - |  677 | `	sxi32 rc;` |
|        - |  678 | `	int iNest;` |
|       86 |  679 | `	nLine = pIn->nLine;` |
|        - |  680 | `	/* Optional 'static' prefix */` |
|       84 |  681 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       86 |  682 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  683 | `		pIn++;` |
|        1 |  684 | `	}` |
|        - |  685 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       84 |  686 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       86 |  687 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  688 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  689 | `		goto Synchronize;` |
|        - |  690 | `	}` |
|       86 |  691 | `	pIn++; /* Jump 'fn' */` |
|       42 |  692 | `	SXUNUSED(nLine);` |
|       42 |  693 | `	SXUNUSED(pGen);` |
|        - |  694 | `	/* Optional '&' for return-by-reference */` |
|       86 |  695 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  696 | `		pIn++;` |
|      ! 0 |  697 | `	}` |
|        - |  698 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  699 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  700 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  701 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       86 |  702 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       84 |  703 | `		pIn++; /* '(' */` |
|       84 |  704 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       84 |  705 | `		if( pIn < pEnd ){` |
|       82 |  706 | `			pIn++; /* ')' */` |
|       40 |  707 | `		}` |
|       41 |  708 | `	}` |
|        - |  709 | `	/* Optional return type ': [?] type' */` |
|       86 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        7 |  711 | `		pIn++;` |
|        7 |  712 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP)` |
|        5 |  713 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        3 |  714 | `			pIn++;` |
|        1 |  715 | `		}` |
|        7 |  716 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        7 |  717 | `			pIn++;` |
|        3 |  718 | `		}` |
|        3 |  719 | `	}` |
|        - |  720 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       86 |  721 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       79 |  722 | `		pIn++;` |
|       39 |  723 | `	}` |
|        - |  724 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       86 |  725 | `	iNest = 0;` |
|      566 |  726 | `	while( pIn < pEnd ){` |
|      488 |  727 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  728 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  729 | `			break;` |
|        - |  730 | `		}` |
|      482 |  731 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       21 |  732 | `			iNest++;` |
|      472 |  733 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       21 |  734 | `			iNest--;` |
|       10 |  735 | `		}` |
|      482 |  736 | `		pIn++;` |
|        2 |  737 | `	}` |
|       86 |  738 | `	rc = SXRET_OK;` |
|       42 |  739 | `Synchronize:` |
|       86 |  740 | `	*ppCur = pIn;` |
|       86 |  741 | `	return rc;` |
|        2 |  742 |  |
|        - |  743 | `/*` |
|        - |  744 | ` * Extract a single expression node from the input.` |
|        - |  745 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  746 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  747 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  748 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  749 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  750 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  751 | ` */` |
|  2889552 |  752 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  753 |  |
|        - |  754 | `	ph7_expr_node *pNode;` |
|        - |  755 | `	SyToken *pCur;` |
|        - |  756 | `	sxi32 rc;` |
|        - |  757 | `	/* Allocate a new node */` |
|  2889554 |  758 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2889554 |  759 | `	if( pNode == 0 ){` |
|        - |  760 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  761 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  762 | `		 */` |
|      ! 0 |  763 | `		return SXERR_MEM;` |
|        - |  764 | `	}` |
|        - |  765 | `	/* Zero the structure */` |
|  2889554 |  766 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2889554 |  767 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  768 | `	/* Point to the head of the token stream */` |
|  2889554 |  769 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  770 | `	/* Start collecting tokens */` |
|  2889554 |  771 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  772 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  773 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       15 |  774 | `		pCur++;` |
|       15 |  775 | `		pGen->pIn = pCur;` |
|       15 |  776 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       15 |  777 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       15 |  778 | `		if( rc == SXRET_OK && *ppNode ){` |
|       15 |  779 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        7 |  780 | `		}` |
|       15 |  781 | `		return rc;` |
|        - |  782 | `	}` |
|  2889540 |  783 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  784 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  785 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  786 | `		 */` |
|      298 |  787 | `		pCur++; /* Skip the opening '[' */` |
|      298 |  788 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      298 |  789 | `		if( pCur < pGen->pEnd ){` |
|      298 |  790 | `			pCur++; /* Skip past the closing ']' */` |
|      150 |  791 | `		}else{` |
|      ! 0 |  792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  793 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  794 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  795 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  796 | `			}` |
|      ! 0 |  797 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  798 | `			return rc;` |
|        - |  799 | `		}` |
|        - |  800 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  801 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  802 | `		 */` |
|      321 |  803 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  804 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  805 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  806 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  807 | `			}else{` |
|       19 |  808 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  809 | `			}` |
|       25 |  810 | `		}else{` |
|      252 |  811 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  812 | `		}` |
|  2889392 |  813 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  814 | `		/* Point to the instance that describe this operator */` |
|   650122 |  815 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  816 | `		/* Advance the stream cursor */` |
|   650122 |  817 | `		pCur++;` |
|  2564184 |  818 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  819 | `		/* Isolate variable */` |
|  1576874 |  820 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   788444 |  821 | `			pCur++; /* Variable variable */` |
|        2 |  822 | `		}` |
|   788432 |  823 | `		if( pCur < pGen->pEnd ){` |
|   788432 |  824 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  825 | `				/* Variable name */` |
|   788404 |  826 | `				pCur++;` |
|   394231 |  827 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  828 | `				pCur++;` |
|        - |  829 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  830 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  831 | `				if( pCur < pGen->pEnd ){` |
|       18 |  832 | `					pCur++;` |
|       10 |  833 | `				}else{` |
|        5 |  834 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  835 | `					if( rc != SXERR_ABORT ){` |
|        5 |  836 | `						rc = SXERR_SYNTAX;` |
|        2 |  837 | `					}` |
|        5 |  838 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  839 | `					return rc;` |
|        - |  840 | `				}` |
|        8 |  841 | `			}` |
|   394213 |  842 | `		}` |
|   788428 |  843 | `		pNode->xCode = PH7_CompileVariable;` |
|  1844907 |  844 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    35064 |  845 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    35064 |  846 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  847 | `			 /* List/Array node */` |
|    23370 |  848 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  849 | `				 /* Assume a literal */` |
|       17 |  850 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  851 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  852 | `			 }else{` |
|    23354 |  853 | `				 pCur += 2;` |
|        - |  854 | `				 /* Collect array/list tokens */` |
|    23354 |  855 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    23354 |  856 | `				 if( pCur < pGen->pEnd ){` |
|    23352 |  857 | `					 pCur++;` |
|    11677 |  858 | `				 }else{` |
|        - |  859 | `					 /* Syntax error */` |
|        4 |  860 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  861 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  862 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  863 | `						 rc = SXERR_SYNTAX;` |
|        1 |  864 | `					 }` |
|        3 |  865 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  866 | `					 return rc;` |
|        - |  867 | `				 }` |
|    23352 |  868 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    23352 |  869 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  870 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  871 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  872 | `						 /* Syntax error */` |
|        3 |  873 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  874 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  875 | `							 rc = SXERR_SYNTAX;` |
|        1 |  876 | `						 }` |
|        3 |  877 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  878 | `						 return rc;` |
|        - |  879 | `					 }` |
|       12 |  880 | `				 }` |
|        2 |  881 | `			 }` |
|    23378 |  882 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  883 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  884 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  885 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  886 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  887 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  888 | `			 pNode->xCode = PH7_CompileYield;` |
|    11680 |  889 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  890 | `			 /* Annonymous function */` |
|      196 |  891 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  892 | `				 /* Assume a literal */` |
|      ! 0 |  893 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  894 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  895 | `			 }else{` |
|        - |  896 | `				 /* Assemble annonymous functions body */` |
|      196 |  897 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      196 |  898 | `				 if( rc != SXRET_OK ){` |
|       25 |  899 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  900 | `					 return rc;` |
|        - |  901 | `				 }` |
|      172 |  902 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - |  903 | `			  }` |
|    11556 |  904 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    11429 |  905 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       28 |  906 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       14 |  907 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  908 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       86 |  909 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       86 |  910 | `			 if( rc != SXRET_OK ){` |
|      ! 0 |  911 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  912 | `				 return rc;` |
|        - |  913 | `			 }` |
|       86 |  914 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    11428 |  915 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  916 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 |  917 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 |  918 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 |  919 | `		 }else{` |
|        - |  920 | `			 /* Assume a literal */` |
|    11308 |  921 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11308 |  922 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  923 | `		 }` |
|  1433149 |  924 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  925 | `		 /* Constants,function name,namespace path,class name... */` |
|   517108 |  926 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   517108 |  927 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   258555 |  928 | `	 }else{` |
|   898526 |  929 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  930 | `			 /* Point to the code generator routine */` |
|   163660 |  931 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   163660 |  932 | `			 if( pNode->xCode == 0 ){` |
|        3 |  933 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  934 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  935 | `					 rc = SXERR_SYNTAX;` |
|        1 |  936 | `				 }` |
|        3 |  937 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  938 | `				 return rc;` |
|        - |  939 | `			 }` |
|    81828 |  940 | `		 }` |
|        - |  941 | `		/* Advance the stream cursor */` |
|   898524 |  942 | `		pCur++;` |
|        - |  943 | `	 }` |
|        - |  944 | `	/* Point to the end of the token stream */` |
|  2889506 |  945 | `	pNode->pEnd = pCur;` |
|        - |  946 | `	/* Save the node for later processing */` |
|  2889506 |  947 | `	*ppNode = pNode;` |
|        - |  948 | `	/* Synchronize cursors */` |
|  2889506 |  949 | `	pGen->pIn = pCur;` |
|  2889506 |  950 | `	return SXRET_OK;` |
|  1444778 |  951 |  |
|        - |  952 | `/*` |
|        - |  953 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  954 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  955 | ` * level is zero.` |
|        - |  956 | ` */` |
|    70434 |  957 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  958 |  |
|    70436 |  959 | `	SyToken *pCur = pStart;` |
|    70436 |  960 | `	sxi32 iNest = 0;` |
|    70436 |  961 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  962 | `		/* Last expression */` |
|    37552 |  963 | `		return SXERR_EOF;` |
|        - |  964 | `	}` |
|   133108 |  965 | `	while( pCur < pEnd ){` |
|   120594 |  966 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    20372 |  967 | `			break;` |
|        - |  968 | `		}` |
|   100224 |  969 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5740 |  970 | `			iNest++;` |
|    97355 |  971 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5742 |  972 | `			iNest--;` |
|     2870 |  973 | `		}` |
|   100224 |  974 | `		pCur++;` |
|        2 |  975 | `	}` |
|    32886 |  976 | `	*ppNext = pCur;` |
|    32886 |  977 | `	return SXRET_OK;` |
|    35219 |  978 |  |
|        - |  979 | `/*` |
|        - |  980 | ` * Free an expression tree.` |
|        - |  981 | ` */` |
|  2472918 |  982 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  983 |  |
|  2472920 |  984 | `	if( pNode->pLeft ){` |
|        - |  985 | `		/* Release the left tree */` |
|   922392 |  986 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   461195 |  987 | `	}` |
|  2472920 |  988 | `	if( pNode->pRight ){` |
|        - |  989 | `		/* Release the right tree */` |
|   482932 |  990 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   241465 |  991 | `	}` |
|  2472920 |  992 | `	if( pNode->pCond ){` |
|        - |  993 | `		/* Release the conditional tree used by the ternary operator */` |
|     1900 |  994 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      949 |  995 | `	}` |
|  2472920 |  996 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  997 | `		ph7_expr_node **apArg;` |
|        - |  998 | `		sxu32 n;` |
|        - |  999 | `		/* Release node arguments */` |
|   327602 | 1000 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   691662 | 1001 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   364062 | 1002 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   182032 | 1003 | `		}` |
|   327602 | 1004 | `		SySetRelease(&pNode->aNodeArgs);` |
|   163800 | 1005 | `	}` |
|        - | 1006 | `	/* Finally,release this node */` |
|  2472920 | 1007 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2472920 | 1008 |  |
|        - | 1009 | `/*` |
|        - | 1010 | ` * Free an expression tree.` |
|        - | 1011 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1012 | ` */` |
|   655626 | 1013 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 | 1014 |  |
|        - | 1015 | `	ph7_expr_node **apNode;` |
|        - | 1016 | `	sxu32 n;` |
|   655628 | 1017 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3545132 | 1018 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2889506 | 1019 | `		if( apNode[n] ){` |
|   655938 | 1020 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   327968 | 1021 | `		}` |
|  1444754 | 1022 | `	}` |
|   655628 | 1023 | `	return SXRET_OK;` |
|        2 | 1024 |  |
|        - | 1025 | `/*` |
|        - | 1026 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1027 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1028 | ` */` |
|   209988 | 1029 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 | 1030 |  |
|        - | 1031 | `	sxi32 iExprOp;` |
|   209990 | 1032 | `	if( pNode->pOp == 0 ){` |
|   136736 | 1033 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1034 | `	}` |
|    73256 | 1035 | `	iExprOp = pNode->pOp->iOp;` |
|    73256 | 1036 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    45930 | 1037 | `			return TRUE;` |
|        - | 1038 | `	}` |
|    27328 | 1039 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    27324 | 1040 | `		if( pNode->pLeft->pOp ) {` |
|       12 | 1041 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 | 1042 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1043 | `				return FALSE;` |
|        1 | 1044 | `			}` |
|    27318 | 1045 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1046 | `			return FALSE;` |
|        - | 1047 | `		}` |
|    27324 | 1048 | `		return TRUE;` |
|        - | 1049 | `	}` |
|        5 | 1050 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 | 1051 | `		return TRUE;` |
|        - | 1052 | `	}` |
|        - | 1053 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1054 | `	return FALSE;` |
|   104996 | 1055 |  |
|        - | 1056 | `/* Forward declaration */` |
|        - | 1057 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1058 | `/* Macro to check if the given node is a terminal.` |
|        - | 1059 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1060 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1061 | ` * linked ternary/elvis node). */` |
|        - | 1062 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1063 | `/*` |
|        - | 1064 | ` * Buid an expression tree for each given function argument.` |
|        - | 1065 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1066 | ` */` |
|   271876 | 1067 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1068 |  |
|        - | 1069 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1070 | `	sxi32 rc;` |
|        - | 1071 | `	/* Process function arguments from left to right */` |
|   271878 | 1072 | `	iCur = 0;` |
|   290097 | 1073 | `	for(;;){` |
|   580196 | 1074 | `		if( iCur >= nToken ){` |
|        - | 1075 | `			/* No more arguments to process */` |
|   271858 | 1076 | `			break;` |
|        - | 1077 | `		}` |
|   308340 | 1078 | `		iNode = iCur;` |
|   308340 | 1079 | `		iNest = 0;` |
|   771150 | 1080 | `		while( iCur < nToken ){` |
|   499294 | 1081 | `			if( apNode[iCur] ){` |
|   488510 | 1082 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    18243 | 1083 | `					break;` |
|   452028 | 1084 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    25058 | 1085 | `					iNest++;` |
|   439500 | 1086 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    25042 | 1087 | `					iNest--;` |
|    12520 | 1088 | `				}` |
|   226013 | 1089 | `			}` |
|   462812 | 1090 | `			iCur++;` |
|        2 | 1091 | `		}` |
|   308340 | 1092 | `		if( iCur > iNode ){` |
|   308332 | 1093 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1094 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1095 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1096 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1097 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1098 | `					apNode[iNode] = 0;` |
|      ! 0 | 1099 | `			}` |
|   308334 | 1100 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   308334 | 1101 | `			if( apNode[iNode] ){` |
|        - | 1102 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   308334 | 1103 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   154168 | 1104 | `			}else{` |
|        - | 1105 | `				/* No expression before comma */` |
|      ! 0 | 1106 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1107 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1108 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1109 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1110 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1111 | `				}` |
|      ! 0 | 1112 | `				return rc;` |
|        - | 1113 | `			}` |
|   154168 | 1114 | `		}else{` |
|        - | 1115 | `			/* Comma with no preceding argument */` |
|        7 | 1116 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1117 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1118 | `				rc = SXERR_SYNTAX;` |
|        3 | 1119 | `			}` |
|        7 | 1120 | `			return rc;` |
|        - | 1121 | `		}` |
|        - | 1122 | `		/* Jump trailing comma */` |
|   308334 | 1123 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    36478 | 1124 | `			iCur++;` |
|    36478 | 1125 | `			if( iCur >= nToken ){` |
|        - | 1126 | `				/* Trailing comma after last argument */` |
|       15 | 1127 | `				break;` |
|        - | 1128 | `			}` |
|    18231 | 1129 | `		}` |
|        2 | 1130 | `	}` |
|   271872 | 1131 | `	return SXRET_OK;` |
|   135940 | 1132 |  |
|        - | 1133 | ` /*` |
|        - | 1134 | `  * Create an expression tree from an array of tokens.` |
|        - | 1135 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1136 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1137 | `  */` |
|  1049110 | 1138 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1139 | ` {` |
|        - | 1140 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1141 | `	 ph7_expr_node *pNode;` |
|        - | 1142 | `	 sxi32 iCur;` |
|        - | 1143 | `	 sxi32 rc;` |
|  1049112 | 1144 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1145 | `		 /* TICKET 1433-17: self evaluating node */` |
|   484058 | 1146 | `		 return SXRET_OK;` |
|        - | 1147 | `	 }` |
|        - | 1148 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3470398 | 1149 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1150 | `		 sxi32 iNest;` |
|        - | 1151 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1152 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1153 | `		  */` |
|  2905346 | 1154 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2882492 | 1155 | `			 continue;` |
|        - | 1156 | `		 }` |
|    22856 | 1157 | `		 iNest = 1;` |
|    22856 | 1158 | `		 iLeft = iCur;` |
|        - | 1159 | `		 /* Find the closing parenthesis */` |
|    22856 | 1160 | `		 iCur++;` |
|   151920 | 1161 | `		 while( iCur < nToken ){` |
|   151920 | 1162 | `			 if( apNode[iCur] ){` |
|   151920 | 1163 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1164 | `					 /* Decrement nesting level */` |
|    39594 | 1165 | `					 iNest--;` |
|    39594 | 1166 | `					 if( iNest <= 0 ){` |
|    22856 | 1167 | `						 break;` |
|        2 | 1168 | `					 }` |
|   120697 | 1169 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1170 | `					 /* Increment nesting level */` |
|    16740 | 1171 | `					 iNest++;` |
|     8369 | 1172 | `				 }` |
|    64532 | 1173 | `			 }` |
|   129066 | 1174 | `			 iCur++;` |
|        2 | 1175 | `		 }` |
|    22856 | 1176 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1177 | `			 /* Recurse and process this expression */` |
|    22856 | 1178 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    22856 | 1179 | `			 if( rc != SXRET_OK ){` |
|        3 | 1180 | `				 return rc;` |
|        - | 1181 | `			 }` |
|    11426 | 1182 | `		 }` |
|        - | 1183 | `		 /* Free the left and right nodes */` |
|    22854 | 1184 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    22854 | 1185 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    22854 | 1186 | `		 apNode[iLeft] = 0;` |
|    22854 | 1187 | `		 apNode[iCur] = 0;` |
|    11428 | 1188 | `	 }` |
|        - | 1189 | `	  /* Process expressions enclosed in braces */` |
|  3616452 | 1190 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1191 | `		 sxi32 iNest;` |
|        - | 1192 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1193 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1194 | `		  */` |
|  3057252 | 1195 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3057252 | 1196 | `			 continue;` |
|        - | 1197 | `		 }` |
|      ! 0 | 1198 | `		 iNest = 1;` |
|      ! 0 | 1199 | `		 iLeft = iCur;` |
|        - | 1200 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1201 | `		 iCur++;` |
|      ! 0 | 1202 | `		 while( iCur < nToken ){` |
|      ! 0 | 1203 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1204 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1205 | `					 /* Decrement nesting level */` |
|      ! 0 | 1206 | `					 iNest--;` |
|      ! 0 | 1207 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1208 | `						 break;` |
|      ! 0 | 1209 | `					 }` |
|      ! 0 | 1210 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1211 | `					 /* Increment nesting level */` |
|      ! 0 | 1212 | `					 iNest++;` |
|      ! 0 | 1213 | `				 }` |
|      ! 0 | 1214 | `			 }` |
|      ! 0 | 1215 | `			 iCur++;` |
|      ! 0 | 1216 | `		 }` |
|      ! 0 | 1217 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1218 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1219 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1220 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1221 | `				 return rc;` |
|        - | 1222 | `			 }` |
|      ! 0 | 1223 | `		 }` |
|        - | 1224 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1225 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1226 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1227 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1228 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1229 | `	 }` |
|        - | 1230 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   559202 | 1231 | `	 iLeft = -1;` |
|  3616424 | 1232 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3057234 | 1233 | `		 if( apNode[iCur] == 0 ){` |
|  1189320 | 1234 | `			 continue;` |
|        - | 1235 | `		 }` |
|  1867916 | 1236 | `		 pNode = apNode[iCur];` |
|  1867916 | 1237 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   481700 | 1238 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1239 | `				 /* Collect function arguments */` |
|   308956 | 1240 | `				 sxi32 iPtr = 0;` |
|   308956 | 1241 | `				 sxi32 nFuncTok = 0;` |
|  1117204 | 1242 | `				 while( nFuncTok + iCur < nToken ){` |
|  1117204 | 1243 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1106420 | 1244 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   320208 | 1245 | `							 iPtr++;` |
|   946317 | 1246 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   320208 | 1247 | `							 iPtr--;` |
|   320208 | 1248 | `							 if( iPtr <= 0 ){` |
|   308956 | 1249 | `								 break;` |
|        - | 1250 | `							 }` |
|     5626 | 1251 | `						 }` |
|   398732 | 1252 | `					 }` |
|   808250 | 1253 | `					 nFuncTok++;` |
|        2 | 1254 | `				 }` |
|   308956 | 1255 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1256 | `					 /* Syntax error */` |
|      ! 0 | 1257 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1258 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1259 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1260 | `					 }` |
|      ! 0 | 1261 | `					 return rc;` |
|        - | 1262 | `				 }` |
|   308956 | 1263 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1264 | `					 /* Syntax error */` |
|      ! 0 | 1265 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1266 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1267 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1268 | `					 }` |
|      ! 0 | 1269 | `					 return rc;` |
|        - | 1270 | `				 }` |
|   308956 | 1271 | `				 if( nFuncTok > 1 ){` |
|        - | 1272 | `					 /* Process function arguments */` |
|   271878 | 1273 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   271878 | 1274 | `					 if( rc != SXRET_OK ){` |
|        7 | 1275 | `						 return rc;` |
|        - | 1276 | `					 }` |
|   135935 | 1277 | `				 }` |
|        - | 1278 | `				 /* Link the node to the tree */` |
|   308950 | 1279 | `				 pNode->pLeft = apNode[iLeft];` |
|   308950 | 1280 | `				 apNode[iLeft] = 0;` |
|  1117180 | 1281 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   808232 | 1282 | `					 apNode[iCur+iPtr] = 0;` |
|   404117 | 1283 | `				 }` |
|   327220 | 1284 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1285 | `				 /* Subscripting */` |
|    69270 | 1286 | `				 sxi32 iArrTok = iCur + 1;` |
|    69270 | 1287 | `				 sxi32 iNest = 1;` |
|    69346 | 1288 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1289 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1290 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       10 | 1291 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    69268 | 1292 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1293 | `						 /* Syntax error */` |
|      ! 0 | 1294 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1295 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1296 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1297 | `						 }` |
|      ! 0 | 1298 | `						 return rc;` |
|        - | 1299 | `				 }` |
|        - | 1300 | `				 /* Collect index tokens */` |
|   125108 | 1301 | `				 while( iArrTok < nToken ){` |
|   125108 | 1302 | `					 if( apNode[iArrTok] ){` |
|   125076 | 1303 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1304 | `							 /* Increment nesting level */` |
|      ! 0 | 1305 | `							 iNest++;` |
|   125076 | 1306 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1307 | `							 /* Decrement nesting level */` |
|    69270 | 1308 | `							 iNest--;` |
|    69270 | 1309 | `							 if( iNest <= 0 ){` |
|    69270 | 1310 | `								 break;` |
|        - | 1311 | `							 }` |
|      ! 0 | 1312 | `						 }` |
|    27903 | 1313 | `					 }` |
|    55840 | 1314 | `					 ++iArrTok;` |
|        2 | 1315 | `				 }` |
|    69270 | 1316 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1317 | `					 /* Recurse and process this expression */` |
|    55730 | 1318 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    55730 | 1319 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1320 | `						 return rc;` |
|        - | 1321 | `					 }` |
|        - | 1322 | `					 /* Link the node to it's index */` |
|    55730 | 1323 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    27864 | 1324 | `				 }` |
|        - | 1325 | `				 /* Link the node to the tree */` |
|    69270 | 1326 | `				 pNode->pLeft = apNode[iLeft];` |
|    69270 | 1327 | `				 pNode->pRight = 0;` |
|    69270 | 1328 | `				 apNode[iLeft] = 0;` |
|   194376 | 1329 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   125108 | 1330 | `					 apNode[iNest] = 0;` |
|    62555 | 1331 | `				 }` |
|    34636 | 1332 | `			 }else{` |
|        - | 1333 | `				 /* Member access operators [i.e: '->','::'] */` |
|   103478 | 1334 | `				  iRight = iCur + 1;` |
|   103478 | 1335 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1336 | `					 iRight++;` |
|      ! 0 | 1337 | `				 }` |
|   103478 | 1338 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1339 | `					 /* Syntax error */` |
|        5 | 1340 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1341 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1342 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1343 | `					 }` |
|        5 | 1344 | `					 return rc;` |
|        - | 1345 | `				 }` |
|        - | 1346 | `				 /* Link the node to the tree */` |
|   103474 | 1347 | `				 pNode->pLeft = apNode[iLeft];` |
|   103474 | 1348 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   103254 | 1349 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1350 | `						 /* Syntax error */` |
|      ! 0 | 1351 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1352 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1353 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1354 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1355 | `						 }` |
|      ! 0 | 1356 | `						 return rc;` |
|        - | 1357 | `				 }` |
|   103474 | 1358 | `				 pNode->pRight = apNode[iRight];` |
|   103474 | 1359 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1360 | `			 }` |
|   240844 | 1361 | `		 }` |
|  1867906 | 1362 | `		 iLeft = iCur;` |
|   933954 | 1363 | `	 }` |
|        - | 1364 | `	 /* Handle left associative (new, clone) operators */` |
|  3616396 | 1365 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3057206 | 1366 | `		 if( apNode[iCur] == 0 ){` |
|  1684956 | 1367 | `			 continue;` |
|        - | 1368 | `		 }` |
|  1372252 | 1369 | `		 pNode = apNode[iCur];` |
|  1372252 | 1370 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1371 | `			 SyToken *pToken;` |
|        - | 1372 | `			 /* Get the left node */` |
|    13950 | 1373 | `			 iLeft = iCur + 1;` |
|    27868 | 1374 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13920 | 1375 | `				 iLeft++;` |
|        2 | 1376 | `			 }` |
|    13950 | 1377 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1378 | `				  /* Syntax error */` |
|      ! 0 | 1379 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1380 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1381 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1382 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1383 | `				 }` |
|      ! 0 | 1384 | `				 return rc;` |
|        - | 1385 | `			 }` |
|        - | 1386 | `			 /* Make sure the operand are of a valid type */` |
|    13950 | 1387 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1388 | `				 /* Clone:` |
|        - | 1389 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1390 | `				  *  ++ function call (including annonymous)` |
|        - | 1391 | `				  *  ++ array member` |
|        - | 1392 | `				  *  ++ 'new' operator` |
|        - | 1393 | `				  * Example:` |
|        - | 1394 | `				  *   clone $pObj;` |
|        - | 1395 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1396 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1397 | `				  */` |
|       18 | 1398 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1399 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1400 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1401 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1402 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1403 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1404 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1405 | `						 }` |
|      ! 0 | 1406 | `						 return rc;` |
|        - | 1407 | `					 }` |
|        7 | 1408 | `				 }` |
|       10 | 1409 | `			 }else{` |
|        - | 1410 | `				 /* New */` |
|    13934 | 1411 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1412 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1413 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1414 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1415 | `						 /* Syntax error */` |
|      ! 0 | 1416 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1417 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1418 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1419 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1420 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1421 | `						 }` |
|      ! 0 | 1422 | `						 return rc;` |
|        - | 1423 | `					 }` |
|        8 | 1424 | `				 }` |
|        - | 1425 | `			 }` |
|        - | 1426 | `			  /* Link the node to the tree */` |
|    13950 | 1427 | `			 pNode->pLeft = apNode[iLeft];` |
|    13950 | 1428 | `			 apNode[iLeft] = 0;` |
|    13950 | 1429 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6974 | 1430 | `		 }` |
|   686127 | 1431 | `	 }` |
|        - | 1432 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   559192 | 1433 | `	 iLeft = -1;` |
|  3616396 | 1434 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3054280 | 1435 | `		 if( apNode[iCur] == 0 ){` |
|  1684956 | 1436 | `			 continue;` |
|        - | 1437 | `		 }` |
|  1369326 | 1438 | `		 pNode = apNode[iCur];` |
|  1369326 | 1439 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8114 | 1440 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2944 | 1441 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1442 | `					 /* Link the node to the tree */` |
|     2946 | 1443 | `					 pNode->pLeft = apNode[iLeft];` |
|     2946 | 1444 | `					 apNode[iLeft] = 0;` |
|     1472 | 1445 | `			 }` |
|     5519 | 1446 | `		  }` |
|  1372252 | 1447 | `		 iLeft = iCur;` |
|   686127 | 1448 | `	  }` |
|   562118 | 1449 | `	 iLeft = -1;` |
|  3619322 | 1450 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3057206 | 1451 | `		 if( apNode[iCur] == 0 ){` |
|  1687900 | 1452 | `			 continue;` |
|        - | 1453 | `		 }` |
|  1369308 | 1454 | `		 pNode = apNode[iCur];` |
|  1369308 | 1455 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8094 | 1456 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8096 | 1457 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1458 | `					 /* Syntax error */` |
|      ! 0 | 1459 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1460 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1461 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1462 | `					 }` |
|      ! 0 | 1463 | `					 return rc;` |
|        - | 1464 | `			 }` |
|        - | 1465 | `			 /* Link the node to the tree */` |
|     8096 | 1466 | `			 pNode->pLeft = apNode[iLeft];` |
|     8096 | 1467 | `			 apNode[iLeft] = 0;` |
|        - | 1468 | `			 /* Mark as pre-increment/decrement node */` |
|     8096 | 1469 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4047 | 1470 | `		  }` |
|  1369308 | 1471 | `		 iLeft = iCur;` |
|   684655 | 1472 | `	 }` |
|        - | 1473 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   562118 | 1474 | `	  iLeft = 0;` |
|  3619316 | 1475 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3057202 | 1476 | `		  if( apNode[iCur] ){` |
|  1361210 | 1477 | `			  pNode = apNode[iCur];` |
|  1361210 | 1478 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    36330 | 1479 | `				  if( iLeft > 0 ){` |
|        - | 1480 | `					  /* Link the node to the tree */` |
|    36328 | 1481 | `					  pNode->pLeft = apNode[iLeft];` |
|    36328 | 1482 | `					  apNode[iLeft] = 0;` |
|    36328 | 1483 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1484 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1485 | `							   /* Syntax error */` |
|      ! 0 | 1486 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1487 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1488 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1489 | `							  }` |
|      ! 0 | 1490 | `							  return rc;` |
|        - | 1491 | `						  }` |
|       36 | 1492 | `					  }` |
|    18165 | 1493 | `				  }else{` |
|        - | 1494 | `					  /* Syntax error */` |
|        3 | 1495 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1496 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1497 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1498 | `					  }` |
|        3 | 1499 | `					  return rc;` |
|        - | 1500 | `				  }` |
|    18163 | 1501 | `			  }` |
|        - | 1502 | `			  /* Save terminal position */` |
|  1361208 | 1503 | `			  iLeft = iCur;` |
|   680603 | 1504 | `		  }` |
|  1528601 | 1505 | `	  }` |
|        - | 1506 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6183180 | 1507 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5621074 | 1508 | `		 iLeft = -1;` |
| 36192808 | 1509 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 30571744 | 1510 | `			 if( apNode[iCur] == 0 ){` |
| 19509006 | 1511 | `				 continue;` |
|        - | 1512 | `			 }` |
| 11062740 | 1513 | `			 pNode = apNode[iCur];` |
| 11062740 | 1514 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1515 | `				 /* Get the right node */` |
|   167594 | 1516 | `				 iRight = iCur + 1;` |
|   237932 | 1517 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    70340 | 1518 | `					 iRight++;` |
|        2 | 1519 | `				 }` |
|   167594 | 1520 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1521 | `					 /* Syntax error */` |
|        9 | 1522 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1523 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1524 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1525 | `					 }` |
|        9 | 1526 | `					 return rc;` |
|        - | 1527 | `				 }` |
|   167586 | 1528 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1529 | `					 sxi32  iTmp;` |
|        - | 1530 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       48 | 1531 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1532 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1533 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1534 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1535 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1536 | `						 }` |
|      ! 0 | 1537 | `						 return rc;` |
|        - | 1538 | `					 }` |
|       48 | 1539 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       34 | 1540 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1541 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1542 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1543 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1544 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1545 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1546 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1547 | `									 }` |
|      ! 0 | 1548 | `									 return rc;` |
|        - | 1549 | `							 }` |
|      ! 0 | 1550 | `						 }` |
|       16 | 1551 | `					 }` |
|        - | 1552 | `					 /* Swap operands */` |
|       48 | 1553 | `					 iTmp = iRight;` |
|       48 | 1554 | `					 iRight = iLeft;` |
|       48 | 1555 | `					 iLeft = iTmp;` |
|       23 | 1556 | `				 }` |
|        - | 1557 | `				 /* Link the node to the tree */` |
|   167586 | 1558 | `				 pNode->pLeft = apNode[iLeft];` |
|   167586 | 1559 | `				 pNode->pRight = apNode[iRight];` |
|   167586 | 1560 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    83792 | 1561 | `			 }` |
| 11062732 | 1562 | `			 iLeft = iCur;` |
|  5531367 | 1563 | `		 }` |
|  2810534 | 1564 | `	 }` |
|        - | 1565 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1566 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1567 | `	  * we are dealing with a single operator.` |
|        - | 1568 | `	  */` |
|   562108 | 1569 | `	  iLeft = -1;` |
|  3611130 | 1570 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3050924 | 1571 | `		  if( apNode[iCur] == 0 ){` |
|  2066890 | 1572 | `			  continue;` |
|        - | 1573 | `		  }` |
|   984036 | 1574 | `		  pNode = apNode[iCur];` |
|   984036 | 1575 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1902 | 1576 | `			  sxi32 iNest = 1;` |
|     1902 | 1577 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1578 | `				  /* Missing condition */` |
|        3 | 1579 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1580 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1581 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1582 | `				  }` |
|        3 | 1583 | `				  return rc;` |
|        - | 1584 | `			  }` |
|        - | 1585 | `			  /* Get the right node */` |
|     1900 | 1586 | `			  iRight = iCur + 1;` |
|     4028 | 1587 | `			  while( iRight < nToken  ){` |
|     4028 | 1588 | `				  if( apNode[iRight] ){` |
|     3730 | 1589 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1590 | `						  /* Increment nesting level */` |
|      ! 0 | 1591 | `						  ++iNest;` |
|     3730 | 1592 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1593 | `						  /* Decrement nesting level */` |
|     1900 | 1594 | `						  --iNest;` |
|     1900 | 1595 | `						  if( iNest <= 0 ){` |
|     1900 | 1596 | `							  break;` |
|        - | 1597 | `						  }` |
|      ! 0 | 1598 | `					  }` |
|      915 | 1599 | `				  }` |
|     2130 | 1600 | `				  iRight++;` |
|        2 | 1601 | `			  }` |
|     1900 | 1602 | `			  if( iRight > iCur + 1 ){` |
|        - | 1603 | `				  /* Recurse and process the then expression */` |
|     1832 | 1604 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1832 | 1605 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1606 | `					  return rc;` |
|        - | 1607 | `				  }` |
|        - | 1608 | `				  /* Link the node to the tree */` |
|     1832 | 1609 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      915 | 1610 | `			  }else{` |
|        - | 1611 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1612 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1613 | `			  }` |
|     1900 | 1614 | `			  apNode[iCur + 1] = 0;` |
|     1900 | 1615 | `			  if( iRight + 1 < nToken ){` |
|        - | 1616 | `				  /* Recurse and process the else expression */` |
|     1900 | 1617 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1900 | 1618 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1619 | `					  return rc;` |
|        - | 1620 | `				  }` |
|        - | 1621 | `				  /* Link the node to the tree */` |
|     1900 | 1622 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1900 | 1623 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      951 | 1624 | `			  }else{` |
|      ! 0 | 1625 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1626 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1627 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1628 | `				 }` |
|      ! 0 | 1629 | `				 return rc;` |
|        - | 1630 | `			  }` |
|        - | 1631 | `			  /* Point to the condition */` |
|     1900 | 1632 | `			  pNode->pCond  = apNode[iLeft];` |
|     1900 | 1633 | `			  apNode[iLeft] = 0;` |
|     1900 | 1634 | `			  break;` |
|        - | 1635 | `		  }` |
|   982136 | 1636 | `		  iLeft = iCur;` |
|   491069 | 1637 | `	  }` |
|        - | 1638 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1639 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1640 | `	  * so there is no need for a precedence loop here.` |
|        - | 1641 | `	  */` |
|   562106 | 1642 | `	 iRight = -1;` |
|  3619172 | 1643 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3057114 | 1644 | `		 if( apNode[iCur] == 0 ){` |
|  2284936 | 1645 | `			 continue;` |
|        - | 1646 | `		 }` |
|   772180 | 1647 | `		 pNode = apNode[iCur];` |
|   772180 | 1648 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1649 | `			 /* Get the left node */` |
|   209954 | 1650 | `			 iLeft = iCur - 1;` |
|   297046 | 1651 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    87094 | 1652 | `				 iLeft--;` |
|        2 | 1653 | `			 }` |
|   209954 | 1654 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1655 | `				 /* Syntax error */` |
|       43 | 1656 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1657 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        7 | 1658 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 1659 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        3 | 1660 | `				 }else{` |
|       39 | 1661 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 1662 | `				 }` |
|       43 | 1663 | `				 if( rc != SXERR_ABORT ){` |
|       41 | 1664 | `					 rc = SXERR_SYNTAX;` |
|       20 | 1665 | `				 }` |
|       43 | 1666 | `				 return rc;` |
|        - | 1667 | `			 }` |
|   209912 | 1668 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       73 | 1669 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1670 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1671 | `					 /* Left operand must be a modifiable l-value */` |
|        5 | 1672 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 1673 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 1674 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 1675 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 1676 | `					 }else{` |
|        4 | 1677 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1678 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 1679 | `					 }` |
|        5 | 1680 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1681 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1682 | `					 }` |
|        5 | 1683 | `					 return rc;` |
|        - | 1684 | `				 }` |
|       26 | 1685 | `			 }` |
|        - | 1686 | `			 /* Link the node to the tree (Reverse) */` |
|   209908 | 1687 | `			 pNode->pLeft = apNode[iRight];` |
|   209908 | 1688 | `			 pNode->pRight = apNode[iLeft];` |
|   209908 | 1689 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   104953 | 1690 | `		 }` |
|   772134 | 1691 | `		 iRight = iCur;` |
|   386068 | 1692 | `	 }` |
|        - | 1693 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2810292 | 1694 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2248234 | 1695 | `		 iLeft = -1;` |
| 14476474 | 1696 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12228242 | 1697 | `			 if( apNode[iCur] == 0 ){` |
|  9979604 | 1698 | `				 continue;` |
|        - | 1699 | `			 }` |
|  2248640 | 1700 | `			 pNode = apNode[iCur];` |
|  2248640 | 1701 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1702 | `				 /* Get the right node */` |
|       72 | 1703 | `				 iRight = iCur + 1;` |
|      110 | 1704 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1705 | `					 iRight++;` |
|        2 | 1706 | `				 }` |
|       72 | 1707 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1708 | `					 /* Syntax error */` |
|      ! 0 | 1709 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1710 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1711 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1712 | `					 }` |
|      ! 0 | 1713 | `					 return rc;` |
|        - | 1714 | `				 }` |
|        - | 1715 | `				 /* Link the node to the tree */` |
|       72 | 1716 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1717 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1718 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1719 | `			 }` |
|  2248640 | 1720 | `			 iLeft = iCur;` |
|  1124321 | 1721 | `		 }` |
|  1124118 | 1722 | `	 }` |
|        - | 1723 | `	 /* Point to the root of the expression tree */` |
|  3057034 | 1724 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2494994 | 1725 | `		 if( apNode[iCur] ){` |
|   507364 | 1726 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1727 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1728 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1729 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1730 | `				  }` |
|       20 | 1731 | `				  return rc;` |
|        - | 1732 | `			 }` |
|   507346 | 1733 | `			 apNode[0] = apNode[iCur];` |
|   507346 | 1734 | `			 apNode[iCur] = 0;` |
|   253672 | 1735 | `		 }` |
|  1247489 | 1736 | `	 }` |
|   562042 | 1737 | `	 return SXRET_OK;` |
|   523094 | 1738 | ` }` |
|        - | 1739 | ` /*` |
|        - | 1740 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1741 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1742 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1743 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1744 | `  */` |
|   655626 | 1745 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1746 |  |
|        - | 1747 | `	ph7_expr_node **apNode;` |
|        - | 1748 | `	ph7_expr_node *pNode;` |
|        - | 1749 | `	sxi32 rc;` |
|        - | 1750 | `	/* Reset node container */` |
|   655628 | 1751 | `	SySetReset(pExprNode);` |
|   655628 | 1752 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1753 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1754 | `	{` |
|   655628 | 1755 | `		int iLastWasTerm = 0;` |
|  3545132 | 1756 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2889540 | 1757 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2889540 | 1758 | `			if( rc != SXRET_OK ){` |
|       35 | 1759 | `				return rc;` |
|        - | 1760 | `			}` |
|        - | 1761 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2889506 | 1762 | `			if( pNode->xCode ){` |
|        - | 1763 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1504520 | 1764 | `				iLastWasTerm = 1;` |
|  2137247 | 1765 | `			}else if( pNode->pOp ){` |
|        - | 1766 | `				/* Operator node */` |
|   650122 | 1767 | `				iLastWasTerm = 0;` |
|   325062 | 1768 | `			}else{` |
|        - | 1769 | `				/* Delimiter: ')' and ']' end terms */` |
|   734868 | 1770 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1771 | `			}` |
|        - | 1772 | `			/* Save the extracted node */` |
|  2889506 | 1773 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1774 | `		}` |
|        - | 1775 | `	}` |
|   655594 | 1776 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1777 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1778 | `		*ppRoot = 0;` |
|      ! 0 | 1779 | `		return SXRET_OK;` |
|        - | 1780 | `	}` |
|   655594 | 1781 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1782 | `	/* Make sure we are dealing with valid nodes */` |
|   655594 | 1783 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   655594 | 1784 | `	if( rc != SXRET_OK ){` |
|        - | 1785 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1786 | `		 * cleanup the mess left behind.` |
|        - | 1787 | `		 */` |
|       51 | 1788 | `		*ppRoot = 0;` |
|       51 | 1789 | `		return rc;` |
|        - | 1790 | `	}` |
|        - | 1791 | `	/* Build the tree */` |
|   655544 | 1792 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   655544 | 1793 | `	if( rc != SXRET_OK ){` |
|        - | 1794 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       88 | 1795 | `		*ppRoot = 0;` |
|       88 | 1796 | `		return rc;` |
|        - | 1797 | `	}` |
|        - | 1798 | `	/* Point to the root of the tree */` |
|   655458 | 1799 | `	*ppRoot = apNode[0];` |
|   655458 | 1800 | `	return SXRET_OK;` |
|   327815 | 1801 |  |
|        - | 1802 |  |
