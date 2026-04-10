# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 844/990 lines (85.25%)

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
|   732874 |  258 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  259 |  |
|   732876 |  260 | `	sxu32 n = 0;` |
|        - |  261 | `	sxi32 rc;` |
|        - |  262 | `	/* Do a linear lookup on the operators table */` |
| 11974597 |  263 | `	for(;;){` |
| 23949196 |  264 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  265 | `			break;` |
|        - |  266 | `		}` |
| 23949196 |  267 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  268 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2922212 |  269 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1461107 |  270 | `		}else{` |
| 21026986 |  271 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  272 | `		}` |
| 23949196 |  273 | `		if( rc == 0 ){` |
|   736102 |  274 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  275 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   732546 |  276 | `				return &aOpTable[n];` |
|        - |  277 | `			}` |
|        - |  278 | `			/* Handle ambiguity */` |
|     3558 |  279 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  280 | `				/* Unary opertors have prcedence here over binary operators */` |
|      222 |  281 | `				return &aOpTable[n];` |
|        - |  282 | `			}` |
|     3338 |  283 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  284 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  285 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  286 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  287 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  288 | `					return &aOpTable[n];` |
|        - |  289 | `				}` |
|        - |  290 |  |
|        4 |  291 | `			}` |
|     1613 |  292 | `		}` |
| 23216322 |  293 | `		++n; /* Next operator in the table */` |
|        2 |  294 | `	}` |
|        - |  295 | `	/* No such operator */` |
|      ! 0 |  296 | `	return 0;` |
|   366439 |  297 |  |
|        - |  298 | `/*` |
|        - |  299 | ` * Delimit a set of token stream.` |
|        - |  300 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  301 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  302 | ` */` |
|   377004 |  303 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  304 |  |
|   377006 |  305 | `	SyToken *pCur = pIn;` |
|   377006 |  306 | `	sxi32 iNest = 1;` |
|  2142635 |  307 | `	for(;;){` |
|  4285272 |  308 | `		if( pCur >= pEnd ){` |
|      124 |  309 | `			break;` |
|        - |  310 | `		}` |
|  4285150 |  311 | `		if( pCur->nType & nTokStart ){` |
|        - |  312 | `			/* Increment nesting level */` |
|   237062 |  313 | `			iNest++;` |
|  4166620 |  314 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  315 | `			/* Decrement nesting level */` |
|   613944 |  316 | `			iNest--;` |
|   613944 |  317 | `			if( iNest <= 0 ){` |
|   376884 |  318 | `				break;` |
|        - |  319 | `			}` |
|   118530 |  320 | `		}` |
|        - |  321 | `		/* Advance cursor */` |
|  3908268 |  322 | `		pCur++;` |
|        2 |  323 | `	}` |
|        - |  324 | `	/* Point to the end of the chunk */` |
|   377006 |  325 | `	*ppEnd = pCur;` |
|   377006 |  326 |  |
|        - |  327 | `/*` |
|        - |  328 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  329 | ` * Note on reserved keywords.` |
|        - |  330 | ` *  According to the PHP language reference manual:` |
|        - |  331 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  332 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  333 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  334 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  335 | ` */` |
|    11356 |  336 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  337 |  |
|    16968 |  338 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11263 |  339 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  340 | `		){` |
|      146 |  341 | `			return TRUE;` |
|        - |  342 | `	}` |
|    11214 |  343 | `	if( bCheckFunc ){` |
|       92 |  344 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  345 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  346 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  347 | `				return TRUE;` |
|        - |  348 | `		}` |
|       20 |  349 | `	}` |
|        - |  350 | `	/* Not a language construct */` |
|    11182 |  351 | `	return FALSE;` |
|     5680 |  352 |  |
|        - |  353 | `/*` |
|        - |  354 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  355 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  356 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  357 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  358 | ` */` |
|   645476 |  359 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  360 |  |
|        - |  361 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  362 | `	sxi32 i,rc;` |
|        - |  363 |  |
|   645478 |  364 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  365 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       18 |  366 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       18 |  367 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        8 |  368 | `	}` |
|   645478 |  369 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3491388 |  370 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2845946 |  371 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  372 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      272 |  373 | `			continue;` |
|        - |  374 | `		}` |
|  2845676 |  375 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   326940 |  376 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    16824 |  377 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  378 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   304418 |  379 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  380 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  381 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  382 | `						 */` |
|   304418 |  383 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   304418 |  384 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   304418 |  385 | `						apNode[i]->pOp = &sFCallOp;` |
|   152208 |  386 | `					}` |
|   152208 |  387 | `			}` |
|   326940 |  388 | `			iParen++;` |
|  2682207 |  389 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   326940 |  390 | `			if( iParen <= 0 ){` |
|       13 |  391 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       13 |  392 | `				if( rc != SXERR_ABORT ){` |
|       13 |  393 | `					rc = SXERR_SYNTAX;` |
|        6 |  394 | `				}` |
|       13 |  395 | `				return rc;` |
|        - |  396 | `			}` |
|   326928 |  397 | `			iParen--;` |
|  2355263 |  398 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    68172 |  399 | `			iSquare++;` |
|  2157715 |  400 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    68186 |  401 | `			if( iSquare <= 0 ){` |
|        7 |  402 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  403 | `				if( rc != SXERR_ABORT ){` |
|        7 |  404 | `					rc = SXERR_SYNTAX;` |
|        3 |  405 | `				}` |
|        7 |  406 | `				return rc;` |
|        - |  407 | `			}` |
|    68180 |  408 | `			iSquare--;` |
|  2089535 |  409 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2055441 |  456 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  457 | `			if( iBraces <= 0 ){` |
|       13 |  458 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  459 | `				if( rc != SXERR_ABORT ){` |
|       13 |  460 | `					rc = SXERR_SYNTAX;` |
|        6 |  461 | `				}` |
|       13 |  462 | `				return rc;` |
|        - |  463 | `			}` |
|      ! 0 |  464 | `			iBraces--;` |
|  2055424 |  465 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1894 |  466 | `			if( iQuesty <= 0 ){` |
|        5 |  467 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  468 | `				if( rc != SXERR_ABORT ){` |
|        5 |  469 | `					rc = SXERR_SYNTAX;` |
|        2 |  470 | `				}` |
|        5 |  471 | `				return rc;` |
|        - |  472 | `			}` |
|     1890 |  473 | `			iQuesty--;` |
|  2054476 |  474 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   572132 |  475 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   572132 |  476 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1892 |  477 | `				iQuesty++;` |
|   571187 |  478 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   286065 |  498 | `		}` |
|  1422822 |  499 | `	}` |
|   645444 |  500 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  501 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  502 | `		if( rc != SXERR_ABORT ){` |
|       17 |  503 | `			rc = SXERR_SYNTAX;` |
|        8 |  504 | `		}` |
|       17 |  505 | `		return rc;` |
|        - |  506 | `	}` |
|   645428 |  507 | `	return SXRET_OK;` |
|   322740 |  508 |  |
|        - |  509 | `/*` |
|        - |  510 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  511 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  512 | ` */` |
|   520734 |  513 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  514 |  |
|   520736 |  515 | `	SyToken *pIn = *ppCur;` |
|        - |  516 | `	/* Jump the first literal seen */` |
|   520736 |  517 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   520714 |  518 | `		pIn++;` |
|   260356 |  519 | `	}` |
|   260397 |  520 | `	for(;;){` |
|   520796 |  521 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       62 |  522 | `			pIn++;` |
|       62 |  523 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       62 |  524 | `				pIn++;` |
|       30 |  525 | `			}` |
|       32 |  526 | `		}else{` |
|   260369 |  527 | `			break;` |
|        - |  528 | `		}` |
|        2 |  529 | `	}` |
|        - |  530 | `	/* Synchronize pointers */` |
|   520736 |  531 | `	*ppCur = pIn;` |
|   520736 |  532 |  |
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
|      194 |  566 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  567 |  |
|      196 |  568 | `	SyToken *pIn = *ppCur;` |
|        - |  569 | `	sxu32 nLine;` |
|        - |  570 | `	sxi32 rc;` |
|        - |  571 | `	/* Jump the 'function' keyword */` |
|      196 |  572 | `	nLine = pIn->nLine;` |
|      196 |  573 | `	pIn++;` |
|      196 |  574 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  575 | `		pIn++;` |
|        1 |  576 | `	}` |
|      196 |  577 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  578 | `		/* Syntax error */` |
|        5 |  579 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  580 | `		if( rc != SXERR_ABORT ){` |
|        5 |  581 | `			rc = SXERR_SYNTAX;` |
|        2 |  582 | `		}` |
|        5 |  583 | `		goto Synchronize;` |
|        - |  584 | `	}` |
|      192 |  585 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      192 |  586 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      192 |  587 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  588 | `		/* Syntax error */` |
|        5 |  589 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  590 | `		if( rc != SXERR_ABORT ){` |
|        5 |  591 | `			rc = SXERR_SYNTAX;` |
|        2 |  592 | `		}` |
|        5 |  593 | `		goto Synchronize;` |
|        - |  594 | `	}` |
|      188 |  595 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  596 | `	/* Skip optional return type declaration ': [?] type' */` |
|      188 |  597 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|      188 |  608 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       32 |  609 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  610 | `		/* Check if we are dealing with a closure */` |
|       32 |  611 | `		if( nKey == PH7_TKWRD_USE ){` |
|       24 |  612 | `			pIn++; /* Jump the 'use' keyword */` |
|       24 |  613 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  614 | `				/* Syntax error */` |
|        5 |  615 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  616 | `				if( rc != SXERR_ABORT ){` |
|        5 |  617 | `					rc = SXERR_SYNTAX;` |
|        2 |  618 | `				}` |
|        5 |  619 | `				goto Synchronize;` |
|        - |  620 | `			}` |
|       20 |  621 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       20 |  622 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       20 |  623 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  624 | `				/* Syntax error */` |
|        5 |  625 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  626 | `				if( rc != SXERR_ABORT ){` |
|        5 |  627 | `					rc = SXERR_SYNTAX;` |
|        2 |  628 | `				}` |
|        5 |  629 | `				goto Synchronize;` |
|        - |  630 | `			}` |
|       16 |  631 | `			pIn++;` |
|        9 |  632 | `		}else{` |
|        - |  633 | `			/* Syntax error */` |
|        9 |  634 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  635 | `			if( rc != SXERR_ABORT ){` |
|        9 |  636 | `				rc = SXERR_SYNTAX;` |
|        4 |  637 | `			}` |
|        9 |  638 | `			goto Synchronize;` |
|        - |  639 | `		}` |
|        7 |  640 | `	}` |
|      172 |  641 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      172 |  642 | `		pIn++; /* Jump the leading curly '{' */` |
|      172 |  643 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      172 |  644 | `		if( pIn < pEnd ){` |
|      172 |  645 | `			pIn++;` |
|       85 |  646 | `		}` |
|       87 |  647 | `	}else{` |
|        - |  648 | `		/* Syntax error */` |
|      ! 0 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  650 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  651 | `			return SXERR_ABORT;` |
|        - |  652 | `		}` |
|        - |  653 | `	}` |
|      172 |  654 | `	rc = SXRET_OK;` |
|       97 |  655 | `Synchronize:` |
|        - |  656 | `	/* Synchronize pointers */` |
|      196 |  657 | `	*ppCur = pIn;` |
|      196 |  658 | `	return rc;` |
|       99 |  659 |  |
|        - |  660 | `/*` |
|        - |  661 | ` * Extract a single expression node from the input.` |
|        - |  662 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  663 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  664 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  665 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  666 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  667 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  668 | ` */` |
|  2846108 |  669 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  670 |  |
|        - |  671 | `	ph7_expr_node *pNode;` |
|        - |  672 | `	SyToken *pCur;` |
|        - |  673 | `	sxi32 rc;` |
|        - |  674 | `	/* Allocate a new node */` |
|  2846110 |  675 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2846110 |  676 | `	if( pNode == 0 ){` |
|        - |  677 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  678 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  679 | `		 */` |
|      ! 0 |  680 | `		return SXERR_MEM;` |
|        - |  681 | `	}` |
|        - |  682 | `	/* Zero the structure */` |
|  2846110 |  683 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2846110 |  684 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  685 | `	/* Point to the head of the token stream */` |
|  2846110 |  686 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  687 | `	/* Start collecting tokens */` |
|  2846110 |  688 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
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
|  2846096 |  700 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  701 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  702 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  703 | `		 */` |
|      274 |  704 | `		pCur++; /* Skip the opening '[' */` |
|      274 |  705 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      274 |  706 | `		if( pCur < pGen->pEnd ){` |
|      274 |  707 | `			pCur++; /* Skip past the closing ']' */` |
|      138 |  708 | `		}else{` |
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
|      297 |  720 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       48 |  721 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       48 |  722 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       30 |  723 | `				pNode->xCode = PH7_CompileShortList;` |
|       16 |  724 | `			}else{` |
|       19 |  725 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  726 | `			}` |
|       25 |  727 | `		}else{` |
|      228 |  728 | `			pNode->xCode = PH7_CompileShortArray;` |
|        2 |  729 | `		}` |
|  2845960 |  730 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  731 | `		/* Point to the instance that describe this operator */` |
|   640336 |  732 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  733 | `		/* Advance the stream cursor */` |
|   640336 |  734 | `		pCur++;` |
|  2525657 |  735 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  736 | `		/* Isolate variable */` |
|  1552638 |  737 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   776326 |  738 | `			pCur++; /* Variable variable */` |
|        2 |  739 | `		}` |
|   776314 |  740 | `		if( pCur < pGen->pEnd ){` |
|   776314 |  741 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  742 | `				/* Variable name */` |
|   776286 |  743 | `				pCur++;` |
|   388172 |  744 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   388154 |  759 | `		}` |
|   776310 |  760 | `		pNode->xCode = PH7_CompileVariable;` |
|  1817332 |  761 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    34498 |  762 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    34498 |  763 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  764 | `			 /* List/Array node */` |
|    23050 |  765 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  766 | `				 /* Assume a literal */` |
|       17 |  767 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  768 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  769 | `			 }else{` |
|    23034 |  770 | `				 pCur += 2;` |
|        - |  771 | `				 /* Collect array/list tokens */` |
|    23034 |  772 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    23034 |  773 | `				 if( pCur < pGen->pEnd ){` |
|    23032 |  774 | `					 pCur++;` |
|    11517 |  775 | `				 }else{` |
|        - |  776 | `					 /* Syntax error */` |
|        4 |  777 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  778 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  779 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  780 | `						 rc = SXERR_SYNTAX;` |
|        1 |  781 | `					 }` |
|        3 |  782 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  783 | `					 return rc;` |
|        - |  784 | `				 }` |
|    23032 |  785 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    23032 |  786 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    22972 |  799 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  800 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  801 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  802 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  803 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  804 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  805 | `			 pNode->xCode = PH7_CompileYield;` |
|    11434 |  806 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  807 | `			 /* Annonymous function */` |
|      196 |  808 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  809 | `				 /* Assume a literal */` |
|      ! 0 |  810 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  811 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  812 | `			 }else{` |
|        - |  813 | `				 /* Assemble annonymous functions body */` |
|      196 |  814 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      196 |  815 | `				 if( rc != SXRET_OK ){` |
|       25 |  816 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  817 | `					 return rc;` |
|        - |  818 | `				 }` |
|      172 |  819 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  820 | `			  }` |
|    11309 |  821 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  822 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       80 |  823 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       80 |  824 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       41 |  825 | `		 }else{` |
|        - |  826 | `			 /* Assume a literal */` |
|    11146 |  827 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11146 |  828 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  829 | `		 }` |
|  1411916 |  830 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  831 | `		 /* Constants,function name,namespace path,class name... */` |
|   509576 |  832 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   509576 |  833 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   254789 |  834 | `	 }else{` |
|   885108 |  835 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  836 | `			 /* Point to the code generator routine */` |
|   161112 |  837 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   161112 |  838 | `			 if( pNode->xCode == 0 ){` |
|        3 |  839 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  840 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  841 | `					 rc = SXERR_SYNTAX;` |
|        1 |  842 | `				 }` |
|        3 |  843 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  844 | `				 return rc;` |
|        - |  845 | `			 }` |
|    80554 |  846 | `		 }` |
|        - |  847 | `		/* Advance the stream cursor */` |
|   885106 |  848 | `		pCur++;` |
|        - |  849 | `	 }` |
|        - |  850 | `	/* Point to the end of the token stream */` |
|  2846062 |  851 | `	pNode->pEnd = pCur;` |
|        - |  852 | `	/* Save the node for later processing */` |
|  2846062 |  853 | `	*ppNode = pNode;` |
|        - |  854 | `	/* Synchronize cursors */` |
|  2846062 |  855 | `	pGen->pIn = pCur;` |
|  2846062 |  856 | `	return SXRET_OK;` |
|  1423056 |  857 |  |
|        - |  858 | `/*` |
|        - |  859 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  860 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  861 | ` * level is zero.` |
|        - |  862 | ` */` |
|    69194 |  863 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  864 |  |
|    69196 |  865 | `	SyToken *pCur = pStart;` |
|    69196 |  866 | `	sxi32 iNest = 0;` |
|    69196 |  867 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  868 | `		/* Last expression */` |
|    36958 |  869 | `		return SXERR_EOF;` |
|        - |  870 | `	}` |
|   130566 |  871 | `	while( pCur < pEnd ){` |
|   118260 |  872 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    19934 |  873 | `			break;` |
|        - |  874 | `		}` |
|    98328 |  875 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5564 |  876 | `			iNest++;` |
|    95547 |  877 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5566 |  878 | `			iNest--;` |
|     2782 |  879 | `		}` |
|    98328 |  880 | `		pCur++;` |
|        2 |  881 | `	}` |
|    32240 |  882 | `	*ppNext = pCur;` |
|    32240 |  883 | `	return SXRET_OK;` |
|    34599 |  884 |  |
|        - |  885 | `/*` |
|        - |  886 | ` * Free an expression tree.` |
|        - |  887 | ` */` |
|  2435636 |  888 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  889 |  |
|  2435638 |  890 | `	if( pNode->pLeft ){` |
|        - |  891 | `		/* Release the left tree */` |
|   908588 |  892 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   454293 |  893 | `	}` |
|  2435638 |  894 | `	if( pNode->pRight ){` |
|        - |  895 | `		/* Release the right tree */` |
|   475654 |  896 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   237826 |  897 | `	}` |
|  2435638 |  898 | `	if( pNode->pCond ){` |
|        - |  899 | `		/* Release the conditional tree used by the ternary operator */` |
|     1888 |  900 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      943 |  901 | `	}` |
|  2435638 |  902 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  903 | `		ph7_expr_node **apArg;` |
|        - |  904 | `		sxu32 n;` |
|        - |  905 | `		/* Release node arguments */` |
|   322740 |  906 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   681420 |  907 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   358682 |  908 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   179342 |  909 | `		}` |
|   322740 |  910 | `		SySetRelease(&pNode->aNodeArgs);` |
|   161369 |  911 | `	}` |
|        - |  912 | `	/* Finally,release this node */` |
|  2435638 |  913 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2435638 |  914 |  |
|        - |  915 | `/*` |
|        - |  916 | ` * Free an expression tree.` |
|        - |  917 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  918 | ` */` |
|   645510 |  919 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  920 |  |
|        - |  921 | `	ph7_expr_node **apNode;` |
|        - |  922 | `	sxu32 n;` |
|   645512 |  923 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3491572 |  924 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2846062 |  925 | `		if( apNode[n] ){` |
|   645814 |  926 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   322906 |  927 | `		}` |
|  1423032 |  928 | `	}` |
|   645512 |  929 | `	return SXRET_OK;` |
|        2 |  930 |  |
|        - |  931 | `/*` |
|        - |  932 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  933 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  934 | ` */` |
|   206710 |  935 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  936 |  |
|        - |  937 | `	sxi32 iExprOp;` |
|   206712 |  938 | `	if( pNode->pOp == 0 ){` |
|   134560 |  939 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  940 | `	}` |
|    72154 |  941 | `	iExprOp = pNode->pOp->iOp;` |
|    72154 |  942 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    45250 |  943 | `			return TRUE;` |
|        - |  944 | `	}` |
|    26906 |  945 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    26902 |  946 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  947 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  948 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  949 | `				return FALSE;` |
|        1 |  950 | `			}` |
|    26899 |  951 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  952 | `			return FALSE;` |
|        - |  953 | `		}` |
|    26902 |  954 | `		return TRUE;` |
|        - |  955 | `	}` |
|        5 |  956 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  957 | `		return TRUE;` |
|        - |  958 | `	}` |
|        - |  959 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  960 | `	return FALSE;` |
|   103357 |  961 |  |
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
|   267904 |  973 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  974 |  |
|        - |  975 | `	sxi32 iNest,iCur,iNode;` |
|        - |  976 | `	sxi32 rc;` |
|        - |  977 | `	/* Process function arguments from left to right */` |
|   267906 |  978 | `	iCur = 0;` |
|   285866 |  979 | `	for(;;){` |
|   571734 |  980 | `		if( iCur >= nToken ){` |
|        - |  981 | `			/* No more arguments to process */` |
|   267886 |  982 | `			break;` |
|        - |  983 | `		}` |
|   303850 |  984 | `		iNode = iCur;` |
|   303850 |  985 | `		iNest = 0;` |
|   759898 |  986 | `		while( iCur < nToken ){` |
|   492014 |  987 | `			if( apNode[iCur] ){` |
|   481390 |  988 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    17984 |  989 | `					break;` |
|   445426 |  990 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    24688 |  991 | `					iNest++;` |
|   433083 |  992 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    24674 |  993 | `					iNest--;` |
|    12336 |  994 | `				}` |
|   222712 |  995 | `			}` |
|   456050 |  996 | `			iCur++;` |
|        2 |  997 | `		}` |
|   303850 |  998 | `		if( iCur > iNode ){` |
|   303842 |  999 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 | 1000 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1001 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1002 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1003 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1004 | `					apNode[iNode] = 0;` |
|      ! 0 | 1005 | `			}` |
|   303844 | 1006 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   303844 | 1007 | `			if( apNode[iNode] ){` |
|        - | 1008 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   303844 | 1009 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   151923 | 1010 | `			}else{` |
|        - | 1011 | `				/* No expression before comma */` |
|      ! 0 | 1012 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1013 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1014 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1015 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1016 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1017 | `				}` |
|      ! 0 | 1018 | `				return rc;` |
|        - | 1019 | `			}` |
|   151923 | 1020 | `		}else{` |
|        - | 1021 | `			/* Comma with no preceding argument */` |
|        7 | 1022 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        7 | 1023 | `			if( rc != SXERR_ABORT ){` |
|        7 | 1024 | `				rc = SXERR_SYNTAX;` |
|        3 | 1025 | `			}` |
|        7 | 1026 | `			return rc;` |
|        - | 1027 | `		}` |
|        - | 1028 | `		/* Jump trailing comma */` |
|   303844 | 1029 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    35960 | 1030 | `			iCur++;` |
|    35960 | 1031 | `			if( iCur >= nToken ){` |
|        - | 1032 | `				/* Trailing comma after last argument */` |
|       15 | 1033 | `				break;` |
|        - | 1034 | `			}` |
|    17972 | 1035 | `		}` |
|        2 | 1036 | `	}` |
|   267900 | 1037 | `	return SXRET_OK;` |
|   133954 | 1038 |  |
|        - | 1039 | ` /*` |
|        - | 1040 | `  * Create an expression tree from an array of tokens.` |
|        - | 1041 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1042 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1043 | `  */` |
|  1033200 | 1044 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1045 | ` {` |
|        - | 1046 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1047 | `	 ph7_expr_node *pNode;` |
|        - | 1048 | `	 sxi32 iCur;` |
|        - | 1049 | `	 sxi32 rc;` |
|  1033202 | 1050 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1051 | `		 /* TICKET 1433-17: self evaluating node */` |
|   476686 | 1052 | `		 return SXRET_OK;` |
|        - | 1053 | `	 }` |
|        - | 1054 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3418426 | 1055 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1056 | `		 sxi32 iNest;` |
|        - | 1057 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1058 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1059 | `		  */` |
|  2861912 | 1060 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2839400 | 1061 | `			 continue;` |
|        - | 1062 | `		 }` |
|    22514 | 1063 | `		 iNest = 1;` |
|    22514 | 1064 | `		 iLeft = iCur;` |
|        - | 1065 | `		 /* Find the closing parenthesis */` |
|    22514 | 1066 | `		 iCur++;` |
|   149634 | 1067 | `		 while( iCur < nToken ){` |
|   149634 | 1068 | `			 if( apNode[iCur] ){` |
|   149634 | 1069 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1070 | `					 /* Decrement nesting level */` |
|    39006 | 1071 | `					 iNest--;` |
|    39006 | 1072 | `					 if( iNest <= 0 ){` |
|    22514 | 1073 | `						 break;` |
|        2 | 1074 | `					 }` |
|   118876 | 1075 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1076 | `					 /* Increment nesting level */` |
|    16494 | 1077 | `					 iNest++;` |
|     8246 | 1078 | `				 }` |
|    63560 | 1079 | `			 }` |
|   127122 | 1080 | `			 iCur++;` |
|        2 | 1081 | `		 }` |
|    22514 | 1082 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1083 | `			 /* Recurse and process this expression */` |
|    22514 | 1084 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    22514 | 1085 | `			 if( rc != SXRET_OK ){` |
|        3 | 1086 | `				 return rc;` |
|        - | 1087 | `			 }` |
|    11255 | 1088 | `		 }` |
|        - | 1089 | `		 /* Free the left and right nodes */` |
|    22512 | 1090 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    22512 | 1091 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    22512 | 1092 | `		 apNode[iLeft] = 0;` |
|    22512 | 1093 | `		 apNode[iCur] = 0;` |
|    11257 | 1094 | `	 }` |
|        - | 1095 | `	  /* Process expressions enclosed in braces */` |
|  3562290 | 1096 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1097 | `		 sxi32 iNest;` |
|        - | 1098 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1099 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1100 | `		  */` |
|  3011532 | 1101 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3011532 | 1102 | `			 continue;` |
|        - | 1103 | `		 }` |
|      ! 0 | 1104 | `		 iNest = 1;` |
|      ! 0 | 1105 | `		 iLeft = iCur;` |
|        - | 1106 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1107 | `		 iCur++;` |
|      ! 0 | 1108 | `		 while( iCur < nToken ){` |
|      ! 0 | 1109 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1110 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1111 | `					 /* Decrement nesting level */` |
|      ! 0 | 1112 | `					 iNest--;` |
|      ! 0 | 1113 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1114 | `						 break;` |
|      ! 0 | 1115 | `					 }` |
|      ! 0 | 1116 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1117 | `					 /* Increment nesting level */` |
|      ! 0 | 1118 | `					 iNest++;` |
|      ! 0 | 1119 | `				 }` |
|      ! 0 | 1120 | `			 }` |
|      ! 0 | 1121 | `			 iCur++;` |
|      ! 0 | 1122 | `		 }` |
|      ! 0 | 1123 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1124 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1125 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1126 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1127 | `				 return rc;` |
|        - | 1128 | `			 }` |
|      ! 0 | 1129 | `		 }` |
|        - | 1130 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1131 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1132 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1133 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1134 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1135 | `	 }` |
|        - | 1136 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   550760 | 1137 | `	 iLeft = -1;` |
|  3562262 | 1138 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3011514 | 1139 | `		 if( apNode[iCur] == 0 ){` |
|  1171676 | 1140 | `			 continue;` |
|        - | 1141 | `		 }` |
|  1839840 | 1142 | `		 pNode = apNode[iCur];` |
|  1839840 | 1143 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   474540 | 1144 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1145 | `				 /* Collect function arguments */` |
|   304414 | 1146 | `				 sxi32 iPtr = 0;` |
|   304414 | 1147 | `				 sxi32 nFuncTok = 0;` |
|  1100840 | 1148 | `				 while( nFuncTok + iCur < nToken ){` |
|  1100840 | 1149 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1090216 | 1150 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   315500 | 1151 | `							 iPtr++;` |
|   932467 | 1152 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   315500 | 1153 | `							 iPtr--;` |
|   315500 | 1154 | `							 if( iPtr <= 0 ){` |
|   304414 | 1155 | `								 break;` |
|        - | 1156 | `							 }` |
|     5543 | 1157 | `						 }` |
|   392901 | 1158 | `					 }` |
|   796428 | 1159 | `					 nFuncTok++;` |
|        2 | 1160 | `				 }` |
|   304414 | 1161 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1162 | `					 /* Syntax error */` |
|      ! 0 | 1163 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1164 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1165 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1166 | `					 }` |
|      ! 0 | 1167 | `					 return rc;` |
|        - | 1168 | `				 }` |
|   304414 | 1169 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1170 | `					 /* Syntax error */` |
|      ! 0 | 1171 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1172 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1173 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1174 | `					 }` |
|      ! 0 | 1175 | `					 return rc;` |
|        - | 1176 | `				 }` |
|   304414 | 1177 | `				 if( nFuncTok > 1 ){` |
|        - | 1178 | `					 /* Process function arguments */` |
|   267906 | 1179 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   267906 | 1180 | `					 if( rc != SXRET_OK ){` |
|        7 | 1181 | `						 return rc;` |
|        - | 1182 | `					 }` |
|   133949 | 1183 | `				 }` |
|        - | 1184 | `				 /* Link the node to the tree */` |
|   304408 | 1185 | `				 pNode->pLeft = apNode[iLeft];` |
|   304408 | 1186 | `				 apNode[iLeft] = 0;` |
|  1100816 | 1187 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   796410 | 1188 | `					 apNode[iCur+iPtr] = 0;` |
|   398206 | 1189 | `				 }` |
|   322331 | 1190 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1191 | `				 /* Subscripting */` |
|    68180 | 1192 | `				 sxi32 iArrTok = iCur + 1;` |
|    68180 | 1193 | `				 sxi32 iNest = 1;` |
|    68247 | 1194 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1195 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1196 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    68178 | 1197 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1198 | `						 /* Syntax error */` |
|      ! 0 | 1199 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1200 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1201 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1202 | `						 }` |
|      ! 0 | 1203 | `						 return rc;` |
|        - | 1204 | `				 }` |
|        - | 1205 | `				 /* Collect index tokens */` |
|   123128 | 1206 | `				 while( iArrTok < nToken ){` |
|   123128 | 1207 | `					 if( apNode[iArrTok] ){` |
|   123096 | 1208 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1209 | `							 /* Increment nesting level */` |
|      ! 0 | 1210 | `							 iNest++;` |
|   123096 | 1211 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1212 | `							 /* Decrement nesting level */` |
|    68180 | 1213 | `							 iNest--;` |
|    68180 | 1214 | `							 if( iNest <= 0 ){` |
|    68180 | 1215 | `								 break;` |
|        - | 1216 | `							 }` |
|      ! 0 | 1217 | `						 }` |
|    27458 | 1218 | `					 }` |
|    54950 | 1219 | `					 ++iArrTok;` |
|        2 | 1220 | `				 }` |
|    68180 | 1221 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1222 | `					 /* Recurse and process this expression */` |
|    54840 | 1223 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    54840 | 1224 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1225 | `						 return rc;` |
|        - | 1226 | `					 }` |
|        - | 1227 | `					 /* Link the node to it's index */` |
|    54840 | 1228 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    27419 | 1229 | `				 }` |
|        - | 1230 | `				 /* Link the node to the tree */` |
|    68180 | 1231 | `				 pNode->pLeft = apNode[iLeft];` |
|    68180 | 1232 | `				 pNode->pRight = 0;` |
|    68180 | 1233 | `				 apNode[iLeft] = 0;` |
|   191306 | 1234 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   123128 | 1235 | `					 apNode[iNest] = 0;` |
|    61565 | 1236 | `				 }` |
|    34091 | 1237 | `			 }else{` |
|        - | 1238 | `				 /* Member access operators [i.e: '->','::'] */` |
|   101950 | 1239 | `				  iRight = iCur + 1;` |
|   101950 | 1240 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1241 | `					 iRight++;` |
|      ! 0 | 1242 | `				 }` |
|   101950 | 1243 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1244 | `					 /* Syntax error */` |
|        5 | 1245 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1246 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1247 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1248 | `					 }` |
|        5 | 1249 | `					 return rc;` |
|        - | 1250 | `				 }` |
|        - | 1251 | `				 /* Link the node to the tree */` |
|   101946 | 1252 | `				 pNode->pLeft = apNode[iLeft];` |
|   101946 | 1253 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   101726 | 1254 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1255 | `						 /* Syntax error */` |
|      ! 0 | 1256 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1257 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1258 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1259 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1260 | `						 }` |
|      ! 0 | 1261 | `						 return rc;` |
|        - | 1262 | `				 }` |
|   101946 | 1263 | `				 pNode->pRight = apNode[iRight];` |
|   101946 | 1264 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1265 | `			 }` |
|   237264 | 1266 | `		 }` |
|  1839830 | 1267 | `		 iLeft = iCur;` |
|   919916 | 1268 | `	 }` |
|        - | 1269 | `	 /* Handle left associative (new, clone) operators */` |
|  3562234 | 1270 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3011486 | 1271 | `		 if( apNode[iCur] == 0 ){` |
|  1659946 | 1272 | `			 continue;` |
|        - | 1273 | `		 }` |
|  1351542 | 1274 | `		 pNode = apNode[iCur];` |
|  1351542 | 1275 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1276 | `			 SyToken *pToken;` |
|        - | 1277 | `			 /* Get the left node */` |
|    13744 | 1278 | `			 iLeft = iCur + 1;` |
|    27456 | 1279 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13714 | 1280 | `				 iLeft++;` |
|        2 | 1281 | `			 }` |
|    13744 | 1282 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1283 | `				  /* Syntax error */` |
|      ! 0 | 1284 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1285 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1286 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1287 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1288 | `				 }` |
|      ! 0 | 1289 | `				 return rc;` |
|        - | 1290 | `			 }` |
|        - | 1291 | `			 /* Make sure the operand are of a valid type */` |
|    13744 | 1292 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1293 | `				 /* Clone:` |
|        - | 1294 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1295 | `				  *  ++ function call (including annonymous)` |
|        - | 1296 | `				  *  ++ array member` |
|        - | 1297 | `				  *  ++ 'new' operator` |
|        - | 1298 | `				  * Example:` |
|        - | 1299 | `				  *   clone $pObj;` |
|        - | 1300 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1301 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1302 | `				  */` |
|       18 | 1303 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1304 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1305 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1306 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1307 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1308 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1309 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1310 | `						 }` |
|      ! 0 | 1311 | `						 return rc;` |
|        - | 1312 | `					 }` |
|        7 | 1313 | `				 }` |
|       10 | 1314 | `			 }else{` |
|        - | 1315 | `				 /* New */` |
|    13728 | 1316 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1317 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1318 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1319 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1320 | `						 /* Syntax error */` |
|      ! 0 | 1321 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1322 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1323 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1324 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1325 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1326 | `						 }` |
|      ! 0 | 1327 | `						 return rc;` |
|        - | 1328 | `					 }` |
|        8 | 1329 | `				 }` |
|        - | 1330 | `			 }` |
|        - | 1331 | `			  /* Link the node to the tree */` |
|    13744 | 1332 | `			 pNode->pLeft = apNode[iLeft];` |
|    13744 | 1333 | `			 apNode[iLeft] = 0;` |
|    13744 | 1334 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6871 | 1335 | `		 }` |
|   675772 | 1336 | `	 }` |
|        - | 1337 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   550750 | 1338 | `	 iLeft = -1;` |
|  3565112 | 1339 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3011486 | 1340 | `		 if( apNode[iCur] == 0 ){` |
|  1659946 | 1341 | `			 continue;` |
|        - | 1342 | `		 }` |
|  1351542 | 1343 | `		 pNode = apNode[iCur];` |
|  1351542 | 1344 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7994 | 1345 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2896 | 1346 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1347 | `					 /* Link the node to the tree */` |
|     2898 | 1348 | `					 pNode->pLeft = apNode[iLeft];` |
|     2898 | 1349 | `					 apNode[iLeft] = 0;` |
|     1448 | 1350 | `			 }` |
|     5435 | 1351 | `		  }` |
|  1354420 | 1352 | `		 iLeft = iCur;` |
|   678650 | 1353 | `	  }` |
|   553628 | 1354 | `	 iLeft = -1;` |
|  3565112 | 1355 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3011486 | 1356 | `		 if( apNode[iCur] == 0 ){` |
|  1662842 | 1357 | `			 continue;` |
|        - | 1358 | `		 }` |
|  1348646 | 1359 | `		 pNode = apNode[iCur];` |
|  1348646 | 1360 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7974 | 1361 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     7976 | 1362 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1363 | `					 /* Syntax error */` |
|      ! 0 | 1364 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1365 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1366 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1367 | `					 }` |
|      ! 0 | 1368 | `					 return rc;` |
|        - | 1369 | `			 }` |
|        - | 1370 | `			 /* Link the node to the tree */` |
|     7976 | 1371 | `			 pNode->pLeft = apNode[iLeft];` |
|     7976 | 1372 | `			 apNode[iLeft] = 0;` |
|        - | 1373 | `			 /* Mark as pre-increment/decrement node */` |
|     7976 | 1374 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3987 | 1375 | `		  }` |
|  1348646 | 1376 | `		 iLeft = iCur;` |
|   674324 | 1377 | `	 }` |
|        - | 1378 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   553628 | 1379 | `	  iLeft = 0;` |
|  3565106 | 1380 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3011482 | 1381 | `		  if( apNode[iCur] ){` |
|  1340668 | 1382 | `			  pNode = apNode[iCur];` |
|  1340668 | 1383 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    35810 | 1384 | `				  if( iLeft > 0 ){` |
|        - | 1385 | `					  /* Link the node to the tree */` |
|    35808 | 1386 | `					  pNode->pLeft = apNode[iLeft];` |
|    35808 | 1387 | `					  apNode[iLeft] = 0;` |
|    35808 | 1388 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1389 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1390 | `							   /* Syntax error */` |
|      ! 0 | 1391 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1392 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1393 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1394 | `							  }` |
|      ! 0 | 1395 | `							  return rc;` |
|        - | 1396 | `						  }` |
|       36 | 1397 | `					  }` |
|    17905 | 1398 | `				  }else{` |
|        - | 1399 | `					  /* Syntax error */` |
|        3 | 1400 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1401 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1402 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1403 | `					  }` |
|        3 | 1404 | `					  return rc;` |
|        - | 1405 | `				  }` |
|    17903 | 1406 | `			  }` |
|        - | 1407 | `			  /* Save terminal position */` |
|  1340666 | 1408 | `			  iLeft = iCur;` |
|   670332 | 1409 | `		  }` |
|  1505741 | 1410 | `	  }` |
|        - | 1411 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6089790 | 1412 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5536174 | 1413 | `		 iLeft = -1;` |
| 35650708 | 1414 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 30114544 | 1415 | `			 if( apNode[iCur] == 0 ){` |
| 19219688 | 1416 | `				 continue;` |
|        - | 1417 | `			 }` |
| 10894858 | 1418 | `			 pNode = apNode[iCur];` |
| 10894858 | 1419 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1420 | `				 /* Get the right node */` |
|   165128 | 1421 | `				 iRight = iCur + 1;` |
|   234426 | 1422 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    69300 | 1423 | `					 iRight++;` |
|        2 | 1424 | `				 }` |
|   165128 | 1425 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1426 | `					 /* Syntax error */` |
|        9 | 1427 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1428 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1429 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1430 | `					 }` |
|        9 | 1431 | `					 return rc;` |
|        - | 1432 | `				 }` |
|   165120 | 1433 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1434 | `					 sxi32  iTmp;` |
|        - | 1435 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1436 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1437 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1438 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1439 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1440 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1441 | `						 }` |
|      ! 0 | 1442 | `						 return rc;` |
|        - | 1443 | `					 }` |
|       46 | 1444 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1445 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1446 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1447 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1448 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1449 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1450 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1451 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1452 | `									 }` |
|      ! 0 | 1453 | `									 return rc;` |
|        - | 1454 | `							 }` |
|      ! 0 | 1455 | `						 }` |
|       15 | 1456 | `					 }` |
|        - | 1457 | `					 /* Swap operands */` |
|       46 | 1458 | `					 iTmp = iRight;` |
|       46 | 1459 | `					 iRight = iLeft;` |
|       46 | 1460 | `					 iLeft = iTmp;` |
|       22 | 1461 | `				 }` |
|        - | 1462 | `				 /* Link the node to the tree */` |
|   165120 | 1463 | `				 pNode->pLeft = apNode[iLeft];` |
|   165120 | 1464 | `				 pNode->pRight = apNode[iRight];` |
|   165120 | 1465 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    82559 | 1466 | `			 }` |
| 10894850 | 1467 | `			 iLeft = iCur;` |
|  5447426 | 1468 | `		 }` |
|  2768084 | 1469 | `	 }` |
|        - | 1470 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1471 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1472 | `	  * we are dealing with a single operator.` |
|        - | 1473 | `	  */` |
|   553618 | 1474 | `	  iLeft = -1;` |
|  3556996 | 1475 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3005268 | 1476 | `		  if( apNode[iCur] == 0 ){` |
|  2036288 | 1477 | `			  continue;` |
|        - | 1478 | `		  }` |
|   968982 | 1479 | `		  pNode = apNode[iCur];` |
|   968982 | 1480 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1890 | 1481 | `			  sxi32 iNest = 1;` |
|     1890 | 1482 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1483 | `				  /* Missing condition */` |
|        3 | 1484 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1485 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1486 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1487 | `				  }` |
|        3 | 1488 | `				  return rc;` |
|        - | 1489 | `			  }` |
|        - | 1490 | `			  /* Get the right node */` |
|     1888 | 1491 | `			  iRight = iCur + 1;` |
|     3998 | 1492 | `			  while( iRight < nToken  ){` |
|     3998 | 1493 | `				  if( apNode[iRight] ){` |
|     3706 | 1494 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1495 | `						  /* Increment nesting level */` |
|      ! 0 | 1496 | `						  ++iNest;` |
|     3706 | 1497 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1498 | `						  /* Decrement nesting level */` |
|     1888 | 1499 | `						  --iNest;` |
|     1888 | 1500 | `						  if( iNest <= 0 ){` |
|     1888 | 1501 | `							  break;` |
|        - | 1502 | `						  }` |
|      ! 0 | 1503 | `					  }` |
|      909 | 1504 | `				  }` |
|     2112 | 1505 | `				  iRight++;` |
|        2 | 1506 | `			  }` |
|     1888 | 1507 | `			  if( iRight > iCur + 1 ){` |
|        - | 1508 | `				  /* Recurse and process the then expression */` |
|     1820 | 1509 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1820 | 1510 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1511 | `					  return rc;` |
|        - | 1512 | `				  }` |
|        - | 1513 | `				  /* Link the node to the tree */` |
|     1820 | 1514 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      909 | 1515 | `			  }else{` |
|        - | 1516 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1517 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1518 | `			  }` |
|     1888 | 1519 | `			  apNode[iCur + 1] = 0;` |
|     1888 | 1520 | `			  if( iRight + 1 < nToken ){` |
|        - | 1521 | `				  /* Recurse and process the else expression */` |
|     1888 | 1522 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1888 | 1523 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1524 | `					  return rc;` |
|        - | 1525 | `				  }` |
|        - | 1526 | `				  /* Link the node to the tree */` |
|     1888 | 1527 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1888 | 1528 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      945 | 1529 | `			  }else{` |
|      ! 0 | 1530 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1531 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1532 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1533 | `				 }` |
|      ! 0 | 1534 | `				 return rc;` |
|        - | 1535 | `			  }` |
|        - | 1536 | `			  /* Point to the condition */` |
|     1888 | 1537 | `			  pNode->pCond  = apNode[iLeft];` |
|     1888 | 1538 | `			  apNode[iLeft] = 0;` |
|     1888 | 1539 | `			  break;` |
|        - | 1540 | `		  }` |
|   967094 | 1541 | `		  iLeft = iCur;` |
|   483548 | 1542 | `	  }` |
|        - | 1543 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1544 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1545 | `	  * so there is no need for a precedence loop here.` |
|        - | 1546 | `	  */` |
|   553616 | 1547 | `	 iRight = -1;` |
|  3564972 | 1548 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3011398 | 1549 | `		 if( apNode[iCur] == 0 ){` |
|  2250986 | 1550 | `			 continue;` |
|        - | 1551 | `		 }` |
|   760414 | 1552 | `		 pNode = apNode[iCur];` |
|   760414 | 1553 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1554 | `			 /* Get the left node */` |
|   206676 | 1555 | `			 iLeft = iCur - 1;` |
|   292444 | 1556 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    85770 | 1557 | `				 iLeft--;` |
|        2 | 1558 | `			 }` |
|   206676 | 1559 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1560 | `				 /* Syntax error */` |
|       39 | 1561 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1562 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1563 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1564 | `				 }` |
|       39 | 1565 | `				 return rc;` |
|        - | 1566 | `			 }` |
|   206638 | 1567 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       71 | 1568 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       54 | 1569 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 1570 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1571 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1572 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1573 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1574 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1575 | `					 }` |
|        3 | 1576 | `					 return rc;` |
|        - | 1577 | `				 }` |
|       26 | 1578 | `			 }` |
|        - | 1579 | `			 /* Link the node to the tree (Reverse) */` |
|   206636 | 1580 | `			 pNode->pLeft = apNode[iRight];` |
|   206636 | 1581 | `			 pNode->pRight = apNode[iLeft];` |
|   206636 | 1582 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   103317 | 1583 | `		 }` |
|   760374 | 1584 | `		 iRight = iCur;` |
|   380188 | 1585 | `	 }` |
|        - | 1586 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2767872 | 1587 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2214298 | 1588 | `		 iLeft = -1;` |
| 14259714 | 1589 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12045418 | 1590 | `			 if( apNode[iCur] == 0 ){` |
|  9830716 | 1591 | `				 continue;` |
|        - | 1592 | `			 }` |
|  2214704 | 1593 | `			 pNode = apNode[iCur];` |
|  2214704 | 1594 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1595 | `				 /* Get the right node */` |
|       72 | 1596 | `				 iRight = iCur + 1;` |
|      110 | 1597 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1598 | `					 iRight++;` |
|        2 | 1599 | `				 }` |
|       72 | 1600 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1601 | `					 /* Syntax error */` |
|      ! 0 | 1602 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1603 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1604 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1605 | `					 }` |
|      ! 0 | 1606 | `					 return rc;` |
|        - | 1607 | `				 }` |
|        - | 1608 | `				 /* Link the node to the tree */` |
|       72 | 1609 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1610 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1611 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1612 | `			 }` |
|  2214704 | 1613 | `			 iLeft = iCur;` |
|  1107353 | 1614 | `		 }` |
|  1107150 | 1615 | `	 }` |
|        - | 1616 | `	 /* Point to the root of the expression tree */` |
|  3011328 | 1617 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2457772 | 1618 | `		 if( apNode[iCur] ){` |
|   499680 | 1619 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1620 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1621 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1622 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1623 | `				  }` |
|       20 | 1624 | `				  return rc;` |
|        - | 1625 | `			 }` |
|   499662 | 1626 | `			 apNode[0] = apNode[iCur];` |
|   499662 | 1627 | `			 apNode[iCur] = 0;` |
|   249830 | 1628 | `		 }` |
|  1228878 | 1629 | `	 }` |
|   553558 | 1630 | `	 return SXRET_OK;` |
|   515163 | 1631 | ` }` |
|        - | 1632 | ` /*` |
|        - | 1633 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1634 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1635 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1636 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1637 | `  */` |
|   645510 | 1638 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1639 |  |
|        - | 1640 | `	ph7_expr_node **apNode;` |
|        - | 1641 | `	ph7_expr_node *pNode;` |
|        - | 1642 | `	sxi32 rc;` |
|        - | 1643 | `	/* Reset node container */` |
|   645512 | 1644 | `	SySetReset(pExprNode);` |
|   645512 | 1645 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1646 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1647 | `	{` |
|   645512 | 1648 | `		int iLastWasTerm = 0;` |
|  3491572 | 1649 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2846096 | 1650 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2846096 | 1651 | `			if( rc != SXRET_OK ){` |
|       35 | 1652 | `				return rc;` |
|        - | 1653 | `			}` |
|        - | 1654 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2846062 | 1655 | `			if( pNode->xCode ){` |
|        - | 1656 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1481732 | 1657 | `				iLastWasTerm = 1;` |
|  2105197 | 1658 | `			}else if( pNode->pOp ){` |
|        - | 1659 | `				/* Operator node */` |
|   640336 | 1660 | `				iLastWasTerm = 0;` |
|   320169 | 1661 | `			}else{` |
|        - | 1662 | `				/* Delimiter: ')' and ']' end terms */` |
|   723998 | 1663 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1664 | `			}` |
|        - | 1665 | `			/* Save the extracted node */` |
|  2846062 | 1666 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1667 | `		}` |
|        - | 1668 | `	}` |
|   645478 | 1669 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1670 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1671 | `		*ppRoot = 0;` |
|      ! 0 | 1672 | `		return SXRET_OK;` |
|        - | 1673 | `	}` |
|   645478 | 1674 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1675 | `	/* Make sure we are dealing with valid nodes */` |
|   645478 | 1676 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   645478 | 1677 | `	if( rc != SXRET_OK ){` |
|        - | 1678 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1679 | `		 * cleanup the mess left behind.` |
|        - | 1680 | `		 */` |
|       51 | 1681 | `		*ppRoot = 0;` |
|       51 | 1682 | `		return rc;` |
|        - | 1683 | `	}` |
|        - | 1684 | `	/* Build the tree */` |
|   645428 | 1685 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   645428 | 1686 | `	if( rc != SXRET_OK ){` |
|        - | 1687 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       82 | 1688 | `		*ppRoot = 0;` |
|       82 | 1689 | `		return rc;` |
|        - | 1690 | `	}` |
|        - | 1691 | `	/* Point to the root of the tree */` |
|   645348 | 1692 | `	*ppRoot = apNode[0];` |
|   645348 | 1693 | `	return SXRET_OK;` |
|   322757 | 1694 |  |
|        - | 1695 |  |
