# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 818/966 lines (84.68%)

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
|        - |  196 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  197 | `	/* Precedence 11,non-associative */` |
|        - |  198 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|        - |  199 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  200 | `	{ {"eq",sizeof(char)*2},  EXPR_OP_SEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_SEQ}, /* IMP-0137-EQ: Symisc eXtension */` |
|        - |  201 | `	{ {"ne",sizeof(char)*2},  EXPR_OP_SNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_SNE}, /* IMP-0138-NE: Symisc eXtension */` |
|        - |  202 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|        - |  203 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|        - |  204 | `	/* Precedence 12,left-associative */` |
|        - |  205 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|        - |  206 | `	/* Precedence 12,left-associative */` |
|        - |  207 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|        - |  208 | `	                         /* Binary operators */` |
|        - |  209 | `	/* Precedence 13,left-associative */` |
|        - |  210 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|        - |  211 | `	/* Precedence 14,left-associative */` |
|        - |  212 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|        - |  213 | `	/* Precedence 15,left-associative */` |
|        - |  214 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  215 | `	/* Precedence 16,left-associative */` |
|        - |  216 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  217 | `	                      /* Ternary operator */` |
|        - |  218 | `	/* Precedence 17,left-associative */` |
|        - |  219 | `    { {"?",sizeof(char)}, EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  220 | `	                     /* Combined binary operators */` |
|        - |  221 | `	/* Precedence 18,right-associative */` |
|        - |  222 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  223 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  224 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  225 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  226 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  227 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  228 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  229 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  230 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  231 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  232 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  233 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  234 | `	/* Precedence 19,left-associative */` |
|        - |  235 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  236 | `	/* Precedence 20,left-associative */` |
|        - |  237 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  238 | `	/* Precedence 21,left-associative */` |
|        - |  239 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  240 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  241 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  242 | `};` |
|        - |  243 | `/* Function call operator need special handling */` |
|        - |  244 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  245 | `/*` |
|        - |  246 | ` * Check if the given token is a potential operator or not.` |
|        - |  247 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  248 | ` * look like an operator.` |
|        - |  249 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  250 | ` * Otherwise NULL.` |
|        - |  251 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  252 | ` * a binary minus or unary minus.]` |
|        - |  253 | ` */` |
|   761706 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  255 |  |
|   761708 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
| 12059742 |  259 | `	for(;;){` |
| 24119486 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 24119486 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3035152 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1517577 |  266 | `		}else{` |
| 21084336 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 24119486 |  269 | `		if( rc == 0 ){` |
|   764998 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   761390 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|     3610 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      210 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|     3402 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|     1645 |  288 | `		}` |
| 23357780 |  289 | `		++n; /* Next operator in the table */` |
|        2 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   380855 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   387362 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  300 |  |
|   387364 |  301 | `	SyToken *pCur = pIn;` |
|   387364 |  302 | `	sxi32 iNest = 1;` |
|  2233061 |  303 | `	for(;;){` |
|  4466124 |  304 | `		if( pCur >= pEnd ){` |
|      122 |  305 | `			break;` |
|        - |  306 | `		}` |
|  4466004 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   247516 |  309 | `			iNest++;` |
|  4342247 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   634758 |  312 | `			iNest--;` |
|   634758 |  313 | `			if( iNest <= 0 ){` |
|   387244 |  314 | `				break;` |
|        - |  315 | `			}` |
|   123757 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  4078762 |  318 | `		pCur++;` |
|        2 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   387364 |  321 | `	*ppEnd = pCur;` |
|   387364 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    11720 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  333 |  |
|    17518 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11631 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|      138 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    11586 |  339 | `	if( bCheckFunc ){` |
|       86 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       65 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       28 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|       20 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    11560 |  347 | `	return FALSE;` |
|     5862 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|   670566 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|   670568 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       10 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       10 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        4 |  364 | `	}` |
|   670568 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3630298 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2959762 |  367 | `		if( apNode[i]->xCode == PH7_CompileShortArray ){` |
|        - |  368 | `			/* Short array literal: brackets are self-contained, skip */` |
|      150 |  369 | `			continue;` |
|        - |  370 | `		}` |
|  2959614 |  371 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   340278 |  372 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    17418 |  373 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  374 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   316994 |  375 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  376 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  377 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  378 | `						 */` |
|   316994 |  379 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   316994 |  380 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   316994 |  381 | `						apNode[i]->pOp = &sFCallOp;` |
|   158496 |  382 | `					}` |
|   158496 |  383 | `			}` |
|   340278 |  384 | `			iParen++;` |
|  2789476 |  385 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   340274 |  386 | `			if( iParen <= 0 ){` |
|        9 |  387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  388 | `				if( rc != SXERR_ABORT ){` |
|        9 |  389 | `					rc = SXERR_SYNTAX;` |
|        4 |  390 | `				}` |
|        9 |  391 | `				return rc;` |
|        - |  392 | `			}` |
|   340266 |  393 | `			iParen--;` |
|  2449198 |  394 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    71128 |  395 | `			iSquare++;` |
|  2243503 |  396 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    71142 |  397 | `			if( iSquare <= 0 ){` |
|        7 |  398 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  399 | `				if( rc != SXERR_ABORT ){` |
|        7 |  400 | `					rc = SXERR_SYNTAX;` |
|        3 |  401 | `				}` |
|        7 |  402 | `				return rc;` |
|        - |  403 | `			}` |
|    71136 |  404 | `			iSquare--;` |
|  2172367 |  405 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       11 |  406 | `			iBraces++;` |
|       11 |  407 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  408 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  409 | `				int iNest = 1;` |
|       11 |  410 | `				sxi32 j=i+1;` |
|        - |  411 | `				/*` |
|        - |  412 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  413 | `				 */` |
|       11 |  414 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  415 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  416 | `				pOp = aOpTable;` |
|       11 |  417 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       51 |  418 | `				while( pOp < pEnd ){` |
|       51 |  419 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  420 | `						break;` |
|        - |  421 | `					}` |
|       41 |  422 | `					pOp++;` |
|        1 |  423 | `				}` |
|       11 |  424 | `				if( pOp >= pEnd ){` |
|      ! 0 |  425 | `					pOp = 0;` |
|      ! 0 |  426 | `				}` |
|       11 |  427 | `				if( pOp ){` |
|       11 |  428 | `					apNode[i]->pOp = pOp;` |
|       11 |  429 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  430 | `				}` |
|       11 |  431 | `				iBraces--;` |
|       11 |  432 | `				iSquare++;` |
|       21 |  433 | `				while( j < nNode ){` |
|       21 |  434 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  435 | `						/* Increment nesting level */` |
|      ! 0 |  436 | `						iNest++;` |
|       21 |  437 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  438 | `						/* Decrement nesting level */` |
|       11 |  439 | `						iNest--;` |
|       11 |  440 | `						if( iNest < 1 ){` |
|       11 |  441 | `							break;` |
|        - |  442 | `						}` |
|      ! 0 |  443 | `					}` |
|       11 |  444 | `					j++;` |
|        1 |  445 | `				}` |
|       11 |  446 | `				if( j < nNode ){` |
|       11 |  447 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  448 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  449 | `				}` |
|        - |  450 |  |
|        6 |  451 | `			}` |
|  2136795 |  452 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  453 | `			if( iBraces <= 0 ){` |
|       13 |  454 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  455 | `				if( rc != SXERR_ABORT ){` |
|       13 |  456 | `					rc = SXERR_SYNTAX;` |
|        6 |  457 | `				}` |
|       13 |  458 | `				return rc;` |
|        - |  459 | `			}` |
|      ! 0 |  460 | `			iBraces--;` |
|  2136778 |  461 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1806 |  462 | `			if( iQuesty <= 0 ){` |
|        5 |  463 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  464 | `				if( rc != SXERR_ABORT ){` |
|        5 |  465 | `					rc = SXERR_SYNTAX;` |
|        2 |  466 | `				}` |
|        5 |  467 | `				return rc;` |
|        - |  468 | `			}` |
|     1802 |  469 | `			iQuesty--;` |
|  2135874 |  470 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   594706 |  471 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   594706 |  472 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1804 |  473 | `				iQuesty++;` |
|   593805 |  474 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      306 |  475 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  476 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  477 | `					sxu32 n = 0;` |
|       11 |  478 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  479 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  480 | `					}` |
|        - |  481 | `					/*` |
|        - |  482 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  483 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  484 | `					 */` |
|      245 |  485 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      235 |  486 | `						++n;` |
|        1 |  487 | `					}` |
|       11 |  488 | `					pOp = &aOpTable[n];` |
|        - |  489 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  490 | `					apNode[i]->pOp = pOp;` |
|       11 |  491 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  492 | `				}` |
|      152 |  493 | `			}` |
|   297352 |  494 | `		}` |
|  1479793 |  495 | `	}` |
|   670538 |  496 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  497 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  498 | `		if( rc != SXERR_ABORT ){` |
|       17 |  499 | `			rc = SXERR_SYNTAX;` |
|        8 |  500 | `		}` |
|       17 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|   670522 |  503 | `	return SXRET_OK;` |
|   335285 |  504 |  |
|        - |  505 | `/*` |
|        - |  506 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  507 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  508 | ` */` |
|   542632 |  509 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  510 |  |
|   542634 |  511 | `	SyToken *pIn = *ppCur;` |
|        - |  512 | `	/* Jump the first literal seen */` |
|   542634 |  513 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   542616 |  514 | `		pIn++;` |
|   271307 |  515 | `	}` |
|   271340 |  516 | `	for(;;){` |
|   542682 |  517 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       49 |  518 | `			pIn++;` |
|       49 |  519 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       49 |  520 | `				pIn++;` |
|       24 |  521 | `			}` |
|       25 |  522 | `		}else{` |
|   271318 |  523 | `			break;` |
|        - |  524 | `		}` |
|        1 |  525 | `	}` |
|        - |  526 | `	/* Synchronize pointers */` |
|   542634 |  527 | `	*ppCur = pIn;` |
|   542634 |  528 |  |
|        - |  529 | `/*` |
|        - |  530 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  531 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  532 | ` * Note on annonymous functions.` |
|        - |  533 | ` *  According to the PHP language reference manual:` |
|        - |  534 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  535 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  536 | ` *  parameters, but they have many other uses.` |
|        - |  537 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  538 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  539 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  540 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  541 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  542 | ` *` |
|        - |  543 | ` * Some example:` |
|        - |  544 | ` *  $greet = function($name)` |
|        - |  545 | ` * {` |
|        - |  546 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  547 | ` * };` |
|        - |  548 | ` *  $greet('World');` |
|        - |  549 | ` *  $greet('PHP');` |
|        - |  550 | ` *` |
|        - |  551 | ` * $double = function($a) {` |
|        - |  552 | ` *   return $a * 2;` |
|        - |  553 | ` * };` |
|        - |  554 | ` * // This is our range of numbers` |
|        - |  555 | ` * $numbers = range(1, 5);` |
|        - |  556 | ` * // Use the Annonymous function as a callback here to` |
|        - |  557 | ` * // double the size of each element in our` |
|        - |  558 | ` * // range` |
|        - |  559 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  560 | ` * print implode(' ', $new_numbers);` |
|        - |  561 | ` */` |
|      168 |  562 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  563 |  |
|      170 |  564 | `	SyToken *pIn = *ppCur;` |
|        - |  565 | `	sxu32 nLine;` |
|        - |  566 | `	sxi32 rc;` |
|        - |  567 | `	/* Jump the 'function' keyword */` |
|      170 |  568 | `	nLine = pIn->nLine;` |
|      170 |  569 | `	pIn++;` |
|      170 |  570 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  571 | `		pIn++;` |
|        1 |  572 | `	}` |
|      170 |  573 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  574 | `		/* Syntax error */` |
|        5 |  575 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  576 | `		if( rc != SXERR_ABORT ){` |
|        5 |  577 | `			rc = SXERR_SYNTAX;` |
|        2 |  578 | `		}` |
|        5 |  579 | `		goto Synchronize;` |
|        - |  580 | `	}` |
|      166 |  581 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      166 |  582 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      166 |  583 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  584 | `		/* Syntax error */` |
|        5 |  585 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  586 | `		if( rc != SXERR_ABORT ){` |
|        5 |  587 | `			rc = SXERR_SYNTAX;` |
|        2 |  588 | `		}` |
|        5 |  589 | `		goto Synchronize;` |
|        - |  590 | `	}` |
|      162 |  591 | `	pIn++; /* Jump the trailing parenthesis */` |
|      162 |  592 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       30 |  593 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  594 | `		/* Check if we are dealing with a closure */` |
|       30 |  595 | `		if( nKey == PH7_TKWRD_USE ){` |
|       22 |  596 | `			pIn++; /* Jump the 'use' keyword */` |
|       22 |  597 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  598 | `				/* Syntax error */` |
|        5 |  599 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  600 | `				if( rc != SXERR_ABORT ){` |
|        5 |  601 | `					rc = SXERR_SYNTAX;` |
|        2 |  602 | `				}` |
|        5 |  603 | `				goto Synchronize;` |
|        - |  604 | `			}` |
|       18 |  605 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       18 |  606 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       18 |  607 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  608 | `				/* Syntax error */` |
|        5 |  609 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  610 | `				if( rc != SXERR_ABORT ){` |
|        5 |  611 | `					rc = SXERR_SYNTAX;` |
|        2 |  612 | `				}` |
|        5 |  613 | `				goto Synchronize;` |
|        - |  614 | `			}` |
|       14 |  615 | `			pIn++;` |
|        8 |  616 | `		}else{` |
|        - |  617 | `			/* Syntax error */` |
|        9 |  618 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  619 | `			if( rc != SXERR_ABORT ){` |
|        9 |  620 | `				rc = SXERR_SYNTAX;` |
|        4 |  621 | `			}` |
|        9 |  622 | `			goto Synchronize;` |
|        - |  623 | `		}` |
|        6 |  624 | `	}` |
|      146 |  625 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      146 |  626 | `		pIn++; /* Jump the leading curly '{' */` |
|      146 |  627 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      146 |  628 | `		if( pIn < pEnd ){` |
|      146 |  629 | `			pIn++;` |
|       72 |  630 | `		}` |
|       74 |  631 | `	}else{` |
|        - |  632 | `		/* Syntax error */` |
|      ! 0 |  633 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  634 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  635 | `			return SXERR_ABORT;` |
|        - |  636 | `		}` |
|        - |  637 | `	}` |
|      146 |  638 | `	rc = SXRET_OK;` |
|       84 |  639 | `Synchronize:` |
|        - |  640 | `	/* Synchronize pointers */` |
|      170 |  641 | `	*ppCur = pIn;` |
|      170 |  642 | `	return rc;` |
|       86 |  643 |  |
|        - |  644 | `/*` |
|        - |  645 | ` * Extract a single expression node from the input.` |
|        - |  646 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  647 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  648 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  649 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  650 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  651 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  652 | ` */` |
|  2959898 |  653 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  654 |  |
|        - |  655 | `	ph7_expr_node *pNode;` |
|        - |  656 | `	SyToken *pCur;` |
|        - |  657 | `	sxi32 rc;` |
|        - |  658 | `	/* Allocate a new node */` |
|  2959900 |  659 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2959900 |  660 | `	if( pNode == 0 ){` |
|        - |  661 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  662 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  663 | `		 */` |
|      ! 0 |  664 | `		return SXERR_MEM;` |
|        - |  665 | `	}` |
|        - |  666 | `	/* Zero the structure */` |
|  2959900 |  667 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2959900 |  668 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  669 | `	/* Point to the head of the token stream */` |
|  2959900 |  670 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  671 | `	/* Start collecting tokens */` |
|  2959900 |  672 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  673 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  674 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  675 | `		 */` |
|      152 |  676 | `		pCur++; /* Skip the opening '[' */` |
|      152 |  677 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      152 |  678 | `		if( pCur < pGen->pEnd ){` |
|      152 |  679 | `			pCur++; /* Skip past the closing ']' */` |
|       77 |  680 | `		}else{` |
|      ! 0 |  681 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  682 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  683 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  684 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  685 | `			}` |
|      ! 0 |  686 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  687 | `			return rc;` |
|        - |  688 | `		}` |
|      152 |  689 | `		pNode->xCode = PH7_CompileShortArray;` |
|  2959825 |  690 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  691 | `		/* Point to the instance that describe this operator */` |
|   665866 |  692 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  693 | `		/* Advance the stream cursor */` |
|   665866 |  694 | `		pCur++;` |
|  2626818 |  695 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  696 | `		/* Isolate variable */` |
|  1618114 |  697 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   809064 |  698 | `			pCur++; /* Variable variable */` |
|        2 |  699 | `		}` |
|   809052 |  700 | `		if( pCur < pGen->pEnd ){` |
|   809052 |  701 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  702 | `				/* Variable name */` |
|   809024 |  703 | `				pCur++;` |
|   404541 |  704 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  705 | `				pCur++;` |
|        - |  706 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  707 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  708 | `				if( pCur < pGen->pEnd ){` |
|       18 |  709 | `					pCur++;` |
|       10 |  710 | `				}else{` |
|        5 |  711 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  712 | `					if( rc != SXERR_ABORT ){` |
|        5 |  713 | `						rc = SXERR_SYNTAX;` |
|        2 |  714 | `					}` |
|        5 |  715 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  716 | `					return rc;` |
|        - |  717 | `				}` |
|        8 |  718 | `			}` |
|   404523 |  719 | `		}` |
|   809048 |  720 | `		pNode->xCode = PH7_CompileVariable;` |
|  1889359 |  721 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    35800 |  722 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    35800 |  723 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  724 | `			 /* List/Array node */` |
|    24006 |  725 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  726 | `				 /* Assume a literal */` |
|       17 |  727 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  728 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  729 | `			 }else{` |
|    23990 |  730 | `				 pCur += 2;` |
|        - |  731 | `				 /* Collect array/list tokens */` |
|    23990 |  732 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    23990 |  733 | `				 if( pCur < pGen->pEnd ){` |
|    23988 |  734 | `					 pCur++;` |
|    11995 |  735 | `				 }else{` |
|        - |  736 | `					 /* Syntax error */` |
|        4 |  737 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  738 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  739 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  740 | `						 rc = SXERR_SYNTAX;` |
|        1 |  741 | `					 }` |
|        3 |  742 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  743 | `					 return rc;` |
|        - |  744 | `				 }` |
|    23988 |  745 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    23988 |  746 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  747 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  748 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  749 | `						 /* Syntax error */` |
|        3 |  750 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  751 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  752 | `							 rc = SXERR_SYNTAX;` |
|        1 |  753 | `						 }` |
|        3 |  754 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  755 | `						 return rc;` |
|        - |  756 | `					 }` |
|       12 |  757 | `				 }` |
|        2 |  758 | `			 }` |
|    23796 |  759 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  760 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       33 |  761 | `			 pCur++; /* Skip 'yield' keyword */` |
|       33 |  762 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  763 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  764 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       33 |  765 | `			 pNode->xCode = PH7_CompileYield;` |
|    11780 |  766 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  767 | `			 /* Annonymous function */` |
|      170 |  768 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  769 | `				 /* Assume a literal */` |
|      ! 0 |  770 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  771 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  772 | `			 }else{` |
|        - |  773 | `				 /* Assemble annonymous functions body */` |
|      170 |  774 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      170 |  775 | `				 if( rc != SXRET_OK ){` |
|       25 |  776 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  777 | `					 return rc;` |
|        - |  778 | `				 }` |
|      146 |  779 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  780 | `			  }` |
|    11668 |  781 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  782 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       74 |  783 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       74 |  784 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       38 |  785 | `		 }else{` |
|        - |  786 | `			 /* Assume a literal */` |
|    11524 |  787 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11524 |  788 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  789 | `		 }` |
|  1466923 |  790 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  791 | `		 /* Constants,function name,namespace path,class name... */` |
|   531096 |  792 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   531096 |  793 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   265549 |  794 | `	 }else{` |
|   917944 |  795 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  796 | `			 /* Point to the code generator routine */` |
|   164416 |  797 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   164416 |  798 | `			 if( pNode->xCode == 0 ){` |
|        3 |  799 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  800 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  801 | `					 rc = SXERR_SYNTAX;` |
|        1 |  802 | `				 }` |
|        3 |  803 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  804 | `				 return rc;` |
|        - |  805 | `			 }` |
|    82206 |  806 | `		 }` |
|        - |  807 | `		/* Advance the stream cursor */` |
|   917942 |  808 | `		pCur++;` |
|        - |  809 | `	 }` |
|        - |  810 | `	/* Point to the end of the token stream */` |
|  2959866 |  811 | `	pNode->pEnd = pCur;` |
|        - |  812 | `	/* Save the node for later processing */` |
|  2959866 |  813 | `	*ppNode = pNode;` |
|        - |  814 | `	/* Synchronize cursors */` |
|  2959866 |  815 | `	pGen->pIn = pCur;` |
|  2959866 |  816 | `	return SXRET_OK;` |
|  1479951 |  817 |  |
|        - |  818 | `/*` |
|        - |  819 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  820 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  821 | ` * level is zero.` |
|        - |  822 | ` */` |
|    68072 |  823 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  824 |  |
|    68074 |  825 | `	SyToken *pCur = pStart;` |
|    68074 |  826 | `	sxi32 iNest = 0;` |
|    68074 |  827 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  828 | `		/* Last expression */` |
|    36764 |  829 | `		return SXERR_EOF;` |
|        - |  830 | `	}` |
|   124074 |  831 | `	while( pCur < pEnd ){` |
|   111610 |  832 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    18848 |  833 | `			break;` |
|        - |  834 | `		}` |
|    92764 |  835 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     4940 |  836 | `			iNest++;` |
|    90295 |  837 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     4942 |  838 | `			iNest--;` |
|     2470 |  839 | `		}` |
|    92764 |  840 | `		pCur++;` |
|        2 |  841 | `	}` |
|    31312 |  842 | `	*ppNext = pCur;` |
|    31312 |  843 | `	return SXRET_OK;` |
|    34038 |  844 |  |
|        - |  845 | `/*` |
|        - |  846 | ` * Free an expression tree.` |
|        - |  847 | ` */` |
|  2532714 |  848 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  849 |  |
|  2532716 |  850 | `	if( pNode->pLeft ){` |
|        - |  851 | `		/* Release the left tree */` |
|   945428 |  852 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   472713 |  853 | `	}` |
|  2532716 |  854 | `	if( pNode->pRight ){` |
|        - |  855 | `		/* Release the right tree */` |
|   494516 |  856 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   247257 |  857 | `	}` |
|  2532716 |  858 | `	if( pNode->pCond ){` |
|        - |  859 | `		/* Release the conditional tree used by the ternary operator */` |
|     1800 |  860 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      899 |  861 | `	}` |
|  2532716 |  862 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  863 | `		ph7_expr_node **apArg;` |
|        - |  864 | `		sxu32 n;` |
|        - |  865 | `		/* Release node arguments */` |
|   336330 |  866 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   709890 |  867 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   373562 |  868 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   186782 |  869 | `		}` |
|   336330 |  870 | `		SySetRelease(&pNode->aNodeArgs);` |
|   168164 |  871 | `	}` |
|        - |  872 | `	/* Finally,release this node */` |
|  2532716 |  873 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2532716 |  874 |  |
|        - |  875 | `/*` |
|        - |  876 | ` * Free an expression tree.` |
|        - |  877 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  878 | ` */` |
|   670600 |  879 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  880 |  |
|        - |  881 | `	ph7_expr_node **apNode;` |
|        - |  882 | `	sxu32 n;` |
|   670602 |  883 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3630466 |  884 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2959866 |  885 | `		if( apNode[n] ){` |
|   670874 |  886 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   335436 |  887 | `		}` |
|  1479934 |  888 | `	}` |
|   670602 |  889 | `	return SXRET_OK;` |
|        2 |  890 |  |
|        - |  891 | `/*` |
|        - |  892 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  893 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  894 | ` */` |
|   215432 |  895 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  896 |  |
|        - |  897 | `	sxi32 iExprOp;` |
|   215434 |  898 | `	if( pNode->pOp == 0 ){` |
|   140054 |  899 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  900 | `	}` |
|    75382 |  901 | `	iExprOp = pNode->pOp->iOp;` |
|    75382 |  902 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    47286 |  903 | `			return TRUE;` |
|        - |  904 | `	}` |
|    28098 |  905 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    28094 |  906 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  907 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  908 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  909 | `				return FALSE;` |
|        1 |  910 | `			}` |
|    28091 |  911 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  912 | `			return FALSE;` |
|        - |  913 | `		}` |
|    28094 |  914 | `		return TRUE;` |
|        - |  915 | `	}` |
|        5 |  916 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  917 | `		return TRUE;` |
|        - |  918 | `	}` |
|        - |  919 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  920 | `	return FALSE;` |
|   107718 |  921 |  |
|        - |  922 | `/* Forward declaration */` |
|        - |  923 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  924 | `/* Macro to check if the given node is a terminal.` |
|        - |  925 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  926 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  927 | ` * linked ternary/elvis node). */` |
|        - |  928 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  929 | `/*` |
|        - |  930 | ` * Buid an expression tree for each given function argument.` |
|        - |  931 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  932 | ` */` |
|   279128 |  933 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  934 |  |
|        - |  935 | `	sxi32 iNest,iCur,iNode;` |
|        - |  936 | `	sxi32 rc;` |
|        - |  937 | `	/* Process function arguments from left to right */` |
|   279130 |  938 | `	iCur = 0;` |
|   297743 |  939 | `	for(;;){` |
|   595488 |  940 | `		if( iCur >= nToken ){` |
|        - |  941 | `			/* No more arguments to process */` |
|   279128 |  942 | `			break;` |
|        - |  943 | `		}` |
|   316362 |  944 | `		iNode = iCur;` |
|   316362 |  945 | `		iNest = 0;` |
|   791544 |  946 | `		while( iCur < nToken ){` |
|   512418 |  947 | `			if( apNode[iCur] ){` |
|   501314 |  948 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    18619 |  949 | `					break;` |
|   464080 |  950 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    25730 |  951 | `					iNest++;` |
|   451216 |  952 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    25720 |  953 | `					iNest--;` |
|    12859 |  954 | `				}` |
|   232039 |  955 | `			}` |
|   475184 |  956 | `			iCur++;` |
|        2 |  957 | `		}` |
|   316362 |  958 | `		if( iCur > iNode ){` |
|   316358 |  959 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  960 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  961 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  962 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  963 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  964 | `					apNode[iNode] = 0;` |
|      ! 0 |  965 | `			}` |
|   316360 |  966 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   316360 |  967 | `			if( apNode[iNode] ){` |
|        - |  968 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   316360 |  969 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   158181 |  970 | `			}else{` |
|        - |  971 | `				/* Empty function argument */` |
|      ! 0 |  972 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  973 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  974 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  975 | `				}` |
|      ! 0 |  976 | `				return rc;` |
|        - |  977 | `			}` |
|   158181 |  978 | `		}else{` |
|        3 |  979 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  980 | `			if( rc != SXERR_ABORT ){` |
|        3 |  981 | `				rc = SXERR_SYNTAX;` |
|        1 |  982 | `			}` |
|        3 |  983 | `			return rc;` |
|        - |  984 | `		}` |
|        - |  985 | `		/* Jump trailing comma */` |
|   316360 |  986 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    37234 |  987 | `			iCur++;` |
|    37234 |  988 | `			if( iCur >= nToken ){` |
|        - |  989 | `				/* missing function argument */` |
|      ! 0 |  990 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 |  991 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  992 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  993 | `				}` |
|      ! 0 |  994 | `				return rc;` |
|        - |  995 | `			}` |
|    18616 |  996 | `		}` |
|        2 |  997 | `	}` |
|   279128 |  998 | `	return SXRET_OK;` |
|   139566 |  999 |  |
|        - | 1000 | ` /*` |
|        - | 1001 | `  * Create an expression tree from an array of tokens.` |
|        - | 1002 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1003 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1004 | `  */` |
|  1073868 | 1005 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1006 | ` {` |
|        - | 1007 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1008 | `	 ph7_expr_node *pNode;` |
|        - | 1009 | `	 sxi32 iCur;` |
|        - | 1010 | `	 sxi32 rc;` |
|  1073870 | 1011 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1012 | `		 /* TICKET 1433-17: self evaluating node */` |
|   494852 | 1013 | `		 return SXRET_OK;` |
|        - | 1014 | `	 }` |
|        - | 1015 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3557138 | 1016 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1017 | `		 sxi32 iNest;` |
|        - | 1018 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1019 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1020 | `		  */` |
|  2978122 | 1021 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2954848 | 1022 | `			 continue;` |
|        - | 1023 | `		 }` |
|    23276 | 1024 | `		 iNest = 1;` |
|    23276 | 1025 | `		 iLeft = iCur;` |
|        - | 1026 | `		 /* Find the closing parenthesis */` |
|    23276 | 1027 | `		 iCur++;` |
|   155024 | 1028 | `		 while( iCur < nToken ){` |
|   155024 | 1029 | `			 if( apNode[iCur] ){` |
|   155024 | 1030 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1031 | `					 /* Decrement nesting level */` |
|    40464 | 1032 | `					 iNest--;` |
|    40464 | 1033 | `					 if( iNest <= 0 ){` |
|    23276 | 1034 | `						 break;` |
|        2 | 1035 | `					 }` |
|   123156 | 1036 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1037 | `					 /* Increment nesting level */` |
|    17190 | 1038 | `					 iNest++;` |
|     8594 | 1039 | `				 }` |
|    65874 | 1040 | `			 }` |
|   131750 | 1041 | `			 iCur++;` |
|        2 | 1042 | `		 }` |
|    23276 | 1043 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1044 | `			 /* Recurse and process this expression */` |
|    23276 | 1045 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    23276 | 1046 | `			 if( rc != SXRET_OK ){` |
|        3 | 1047 | `				 return rc;` |
|        - | 1048 | `			 }` |
|    11636 | 1049 | `		 }` |
|        - | 1050 | `		 /* Free the left and right nodes */` |
|    23274 | 1051 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    23274 | 1052 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    23274 | 1053 | `		 apNode[iLeft] = 0;` |
|    23274 | 1054 | `		 apNode[iCur] = 0;` |
|    11638 | 1055 | `	 }` |
|        - | 1056 | `	  /* Process expressions enclosed in braces */` |
|  3706176 | 1057 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1058 | `		 sxi32 iNest;` |
|        - | 1059 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1060 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1061 | `		  */` |
|  3133132 | 1062 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3133132 | 1063 | `			 continue;` |
|        - | 1064 | `		 }` |
|      ! 0 | 1065 | `		 iNest = 1;` |
|      ! 0 | 1066 | `		 iLeft = iCur;` |
|        - | 1067 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1068 | `		 iCur++;` |
|      ! 0 | 1069 | `		 while( iCur < nToken ){` |
|      ! 0 | 1070 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1071 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1072 | `					 /* Decrement nesting level */` |
|      ! 0 | 1073 | `					 iNest--;` |
|      ! 0 | 1074 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1075 | `						 break;` |
|      ! 0 | 1076 | `					 }` |
|      ! 0 | 1077 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1078 | `					 /* Increment nesting level */` |
|      ! 0 | 1079 | `					 iNest++;` |
|      ! 0 | 1080 | `				 }` |
|      ! 0 | 1081 | `			 }` |
|      ! 0 | 1082 | `			 iCur++;` |
|      ! 0 | 1083 | `		 }` |
|      ! 0 | 1084 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1085 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1086 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1087 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1088 | `				 return rc;` |
|        - | 1089 | `			 }` |
|      ! 0 | 1090 | `		 }` |
|        - | 1091 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1092 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1093 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1094 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1095 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1096 | `	 }` |
|        - | 1097 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   573046 | 1098 | `	 iLeft = -1;` |
|  3706164 | 1099 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3133126 | 1100 | `		 if( apNode[iCur] == 0 ){` |
|  1219748 | 1101 | `			 continue;` |
|        - | 1102 | `		 }` |
|  1913380 | 1103 | `		 pNode = apNode[iCur];` |
|  1913380 | 1104 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   494448 | 1105 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1106 | `				 /* Collect function arguments */` |
|   316990 | 1107 | `				 sxi32 iPtr = 0;` |
|   316990 | 1108 | `				 sxi32 nFuncTok = 0;` |
|  1146396 | 1109 | `				 while( nFuncTok + iCur < nToken ){` |
|  1146396 | 1110 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1135292 | 1111 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   328522 | 1112 | `							 iPtr++;` |
|   971032 | 1113 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   328522 | 1114 | `							 iPtr--;` |
|   328522 | 1115 | `							 if( iPtr <= 0 ){` |
|   316990 | 1116 | `								 break;` |
|        - | 1117 | `							 }` |
|     5766 | 1118 | `						 }` |
|   409151 | 1119 | `					 }` |
|   829408 | 1120 | `					 nFuncTok++;` |
|        2 | 1121 | `				 }` |
|   316990 | 1122 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1123 | `					 /* Syntax error */` |
|      ! 0 | 1124 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1125 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1126 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1127 | `					 }` |
|      ! 0 | 1128 | `					 return rc;` |
|        - | 1129 | `				 }` |
|   316990 | 1130 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1131 | `					 /* Syntax error */` |
|      ! 0 | 1132 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1133 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1134 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1135 | `					 }` |
|      ! 0 | 1136 | `					 return rc;` |
|        - | 1137 | `				 }` |
|   316990 | 1138 | `				 if( nFuncTok > 1 ){` |
|        - | 1139 | `					 /* Process function arguments */` |
|   279130 | 1140 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   279130 | 1141 | `					 if( rc != SXRET_OK ){` |
|        3 | 1142 | `						 return rc;` |
|        - | 1143 | `					 }` |
|   139563 | 1144 | `				 }` |
|        - | 1145 | `				 /* Link the node to the tree */` |
|   316988 | 1146 | `				 pNode->pLeft = apNode[iLeft];` |
|   316988 | 1147 | `				 apNode[iLeft] = 0;` |
|  1146388 | 1148 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   829402 | 1149 | `					 apNode[iCur+iPtr] = 0;` |
|   414702 | 1150 | `				 }` |
|   335953 | 1151 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1152 | `				 /* Subscripting */` |
|    71136 | 1153 | `				 sxi32 iArrTok = iCur + 1;` |
|    71136 | 1154 | `				 sxi32 iNest = 1;` |
|    71203 | 1155 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1156 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1157 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    71134 | 1158 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1159 | `						 /* Syntax error */` |
|      ! 0 | 1160 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1161 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1162 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1163 | `						 }` |
|      ! 0 | 1164 | `						 return rc;` |
|        - | 1165 | `				 }` |
|        - | 1166 | `				 /* Collect index tokens */` |
|   128448 | 1167 | `				 while( iArrTok < nToken ){` |
|   128448 | 1168 | `					 if( apNode[iArrTok] ){` |
|   128416 | 1169 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1170 | `							 /* Increment nesting level */` |
|      ! 0 | 1171 | `							 iNest++;` |
|   128416 | 1172 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1173 | `							 /* Decrement nesting level */` |
|    71136 | 1174 | `							 iNest--;` |
|    71136 | 1175 | `							 if( iNest <= 0 ){` |
|    71136 | 1176 | `								 break;` |
|        - | 1177 | `							 }` |
|      ! 0 | 1178 | `						 }` |
|    28640 | 1179 | `					 }` |
|    57314 | 1180 | `					 ++iArrTok;` |
|        2 | 1181 | `				 }` |
|    71136 | 1182 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1183 | `					 /* Recurse and process this expression */` |
|    57204 | 1184 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    57204 | 1185 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1186 | `						 return rc;` |
|        - | 1187 | `					 }` |
|        - | 1188 | `					 /* Link the node to it's index */` |
|    57204 | 1189 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    28601 | 1190 | `				 }` |
|        - | 1191 | `				 /* Link the node to the tree */` |
|    71136 | 1192 | `				 pNode->pLeft = apNode[iLeft];` |
|    71136 | 1193 | `				 pNode->pRight = 0;` |
|    71136 | 1194 | `				 apNode[iLeft] = 0;` |
|   199582 | 1195 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   128448 | 1196 | `					 apNode[iNest] = 0;` |
|    64225 | 1197 | `				 }` |
|    35569 | 1198 | `			 }else{` |
|        - | 1199 | `				 /* Member access operators [i.e: '->','::'] */` |
|   106326 | 1200 | `				  iRight = iCur + 1;` |
|   106326 | 1201 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1202 | `					 iRight++;` |
|      ! 0 | 1203 | `				 }` |
|   106326 | 1204 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1205 | `					 /* Syntax error */` |
|        5 | 1206 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1207 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1208 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1209 | `					 }` |
|        5 | 1210 | `					 return rc;` |
|        - | 1211 | `				 }` |
|        - | 1212 | `				 /* Link the node to the tree */` |
|   106322 | 1213 | `				 pNode->pLeft = apNode[iLeft];` |
|   106322 | 1214 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   106200 | 1215 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1216 | `						 /* Syntax error */` |
|      ! 0 | 1217 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1218 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1219 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1220 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1221 | `						 }` |
|      ! 0 | 1222 | `						 return rc;` |
|        - | 1223 | `				 }` |
|   106322 | 1224 | `				 pNode->pRight = apNode[iRight];` |
|   106322 | 1225 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1226 | `			 }` |
|   247220 | 1227 | `		 }` |
|  1913374 | 1228 | `		 iLeft = iCur;` |
|   956688 | 1229 | `	 }` |
|        - | 1230 | `	 /* Handle left associative (new, clone) operators */` |
|  3706144 | 1231 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3133106 | 1232 | `		 if( apNode[iCur] == 0 ){` |
|  1728446 | 1233 | `			 continue;` |
|        - | 1234 | `		 }` |
|  1404662 | 1235 | `		 pNode = apNode[iCur];` |
|  1404662 | 1236 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1237 | `			 SyToken *pToken;` |
|        - | 1238 | `			 /* Get the left node */` |
|    14260 | 1239 | `			 iLeft = iCur + 1;` |
|    28492 | 1240 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    14234 | 1241 | `				 iLeft++;` |
|        2 | 1242 | `			 }` |
|    14260 | 1243 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1244 | `				  /* Syntax error */` |
|      ! 0 | 1245 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1246 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1247 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1248 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1249 | `				 }` |
|      ! 0 | 1250 | `				 return rc;` |
|        - | 1251 | `			 }` |
|        - | 1252 | `			 /* Make sure the operand are of a valid type */` |
|    14260 | 1253 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1254 | `				 /* Clone:` |
|        - | 1255 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1256 | `				  *  ++ function call (including annonymous)` |
|        - | 1257 | `				  *  ++ array member` |
|        - | 1258 | `				  *  ++ 'new' operator` |
|        - | 1259 | `				  * Example:` |
|        - | 1260 | `				  *   clone $pObj;` |
|        - | 1261 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1262 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1263 | `				  */` |
|       18 | 1264 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1265 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1266 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1267 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1268 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1269 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1270 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1271 | `						 }` |
|      ! 0 | 1272 | `						 return rc;` |
|        - | 1273 | `					 }` |
|        7 | 1274 | `				 }` |
|       10 | 1275 | `			 }else{` |
|        - | 1276 | `				 /* New */` |
|    14244 | 1277 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       14 | 1278 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       14 | 1279 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1280 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1281 | `						 /* Syntax error */` |
|      ! 0 | 1282 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1283 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1284 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1285 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1286 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1287 | `						 }` |
|      ! 0 | 1288 | `						 return rc;` |
|        - | 1289 | `					 }` |
|        6 | 1290 | `				 }` |
|        - | 1291 | `			 }` |
|        - | 1292 | `			  /* Link the node to the tree */` |
|    14260 | 1293 | `			 pNode->pLeft = apNode[iLeft];` |
|    14260 | 1294 | `			 apNode[iLeft] = 0;` |
|    14260 | 1295 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7129 | 1296 | `		 }` |
|   702332 | 1297 | `	 }` |
|        - | 1298 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   573040 | 1299 | `	 iLeft = -1;` |
|  3709130 | 1300 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3133106 | 1301 | `		 if( apNode[iCur] == 0 ){` |
|  1728446 | 1302 | `			 continue;` |
|        - | 1303 | `		 }` |
|  1404662 | 1304 | `		 pNode = apNode[iCur];` |
|  1404662 | 1305 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8354 | 1306 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3004 | 1307 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1308 | `					 /* Link the node to the tree */` |
|     3006 | 1309 | `					 pNode->pLeft = apNode[iLeft];` |
|     3006 | 1310 | `					 apNode[iLeft] = 0;` |
|     1502 | 1311 | `			 }` |
|     5669 | 1312 | `		  }` |
|  1407648 | 1313 | `		 iLeft = iCur;` |
|   705318 | 1314 | `	  }` |
|   576026 | 1315 | `	 iLeft = -1;` |
|  3709130 | 1316 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3133106 | 1317 | `		 if( apNode[iCur] == 0 ){` |
|  1731450 | 1318 | `			 continue;` |
|        - | 1319 | `		 }` |
|  1401658 | 1320 | `		 pNode = apNode[iCur];` |
|  1401658 | 1321 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8334 | 1322 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8336 | 1323 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1324 | `					 /* Syntax error */` |
|      ! 0 | 1325 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1326 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1327 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1328 | `					 }` |
|      ! 0 | 1329 | `					 return rc;` |
|        - | 1330 | `			 }` |
|        - | 1331 | `			 /* Link the node to the tree */` |
|     8336 | 1332 | `			 pNode->pLeft = apNode[iLeft];` |
|     8336 | 1333 | `			 apNode[iLeft] = 0;` |
|        - | 1334 | `			 /* Mark as pre-increment/decrement node */` |
|     8336 | 1335 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4167 | 1336 | `		  }` |
|  1401658 | 1337 | `		 iLeft = iCur;` |
|   700830 | 1338 | `	 }` |
|        - | 1339 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   576026 | 1340 | `	  iLeft = 0;` |
|  3709124 | 1341 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3133102 | 1342 | `		  if( apNode[iCur] ){` |
|  1393320 | 1343 | `			  pNode = apNode[iCur];` |
|  1393320 | 1344 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    37268 | 1345 | `				  if( iLeft > 0 ){` |
|        - | 1346 | `					  /* Link the node to the tree */` |
|    37266 | 1347 | `					  pNode->pLeft = apNode[iLeft];` |
|    37266 | 1348 | `					  apNode[iLeft] = 0;` |
|    37266 | 1349 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       10 | 1350 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1351 | `							   /* Syntax error */` |
|      ! 0 | 1352 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1353 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1354 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1355 | `							  }` |
|      ! 0 | 1356 | `							  return rc;` |
|        - | 1357 | `						  }` |
|        4 | 1358 | `					  }` |
|    18634 | 1359 | `				  }else{` |
|        - | 1360 | `					  /* Syntax error */` |
|        3 | 1361 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1362 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1363 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1364 | `					  }` |
|        3 | 1365 | `					  return rc;` |
|        - | 1366 | `				  }` |
|    18632 | 1367 | `			  }` |
|        - | 1368 | `			  /* Save terminal position */` |
|  1393318 | 1369 | `			  iLeft = iCur;` |
|   696658 | 1370 | `		  }` |
|  1566551 | 1371 | `	  }` |
|        - | 1372 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6336168 | 1373 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5760154 | 1374 | `		 iLeft = -1;` |
| 37090888 | 1375 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 31330744 | 1376 | `			 if( apNode[iCur] == 0 ){` |
| 19996448 | 1377 | `				 continue;` |
|        - | 1378 | `			 }` |
| 11334298 | 1379 | `			 pNode = apNode[iCur];` |
| 11334298 | 1380 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1381 | `				 /* Get the right node */` |
|   170980 | 1382 | `				 iRight = iCur + 1;` |
|   243150 | 1383 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    72172 | 1384 | `					 iRight++;` |
|        2 | 1385 | `				 }` |
|   170980 | 1386 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1387 | `					 /* Syntax error */` |
|        9 | 1388 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1389 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1390 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1391 | `					 }` |
|        9 | 1392 | `					 return rc;` |
|        - | 1393 | `				 }` |
|   170972 | 1394 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1395 | `					 sxi32  iTmp;` |
|        - | 1396 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1397 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1398 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1399 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1400 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1401 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1402 | `						 }` |
|      ! 0 | 1403 | `						 return rc;` |
|        - | 1404 | `					 }` |
|       46 | 1405 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1406 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1407 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1408 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1409 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1410 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1411 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1412 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1413 | `									 }` |
|      ! 0 | 1414 | `									 return rc;` |
|        - | 1415 | `							 }` |
|      ! 0 | 1416 | `						 }` |
|       15 | 1417 | `					 }` |
|        - | 1418 | `					 /* Swap operands */` |
|       46 | 1419 | `					 iTmp = iRight;` |
|       46 | 1420 | `					 iRight = iLeft;` |
|       46 | 1421 | `					 iLeft = iTmp;` |
|       22 | 1422 | `				 }` |
|        - | 1423 | `				 /* Link the node to the tree */` |
|   170972 | 1424 | `				 pNode->pLeft = apNode[iLeft];` |
|   170972 | 1425 | `				 pNode->pRight = apNode[iRight];` |
|   170972 | 1426 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    85485 | 1427 | `			 }` |
| 11334290 | 1428 | `			 iLeft = iCur;` |
|  5667146 | 1429 | `		 }` |
|  2880074 | 1430 | `	 }` |
|        - | 1431 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1432 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1433 | `	  * we are dealing with a single operator.` |
|        - | 1434 | `	  */` |
|   576016 | 1435 | `	  iLeft = -1;` |
|  3701372 | 1436 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3127158 | 1437 | `		  if( apNode[iCur] == 0 ){` |
|  2118424 | 1438 | `			  continue;` |
|        - | 1439 | `		  }` |
|  1008736 | 1440 | `		  pNode = apNode[iCur];` |
|  1008736 | 1441 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1802 | 1442 | `			  sxi32 iNest = 1;` |
|     1802 | 1443 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1444 | `				  /* Missing condition */` |
|        3 | 1445 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1446 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1447 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1448 | `				  }` |
|        3 | 1449 | `				  return rc;` |
|        - | 1450 | `			  }` |
|        - | 1451 | `			  /* Get the right node */` |
|     1800 | 1452 | `			  iRight = iCur + 1;` |
|     3822 | 1453 | `			  while( iRight < nToken  ){` |
|     3822 | 1454 | `				  if( apNode[iRight] ){` |
|     3530 | 1455 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1456 | `						  /* Increment nesting level */` |
|      ! 0 | 1457 | `						  ++iNest;` |
|     3530 | 1458 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1459 | `						  /* Decrement nesting level */` |
|     1800 | 1460 | `						  --iNest;` |
|     1800 | 1461 | `						  if( iNest <= 0 ){` |
|     1800 | 1462 | `							  break;` |
|        - | 1463 | `						  }` |
|      ! 0 | 1464 | `					  }` |
|      865 | 1465 | `				  }` |
|     2024 | 1466 | `				  iRight++;` |
|        2 | 1467 | `			  }` |
|     1800 | 1468 | `			  if( iRight > iCur + 1 ){` |
|        - | 1469 | `				  /* Recurse and process the then expression */` |
|     1732 | 1470 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1732 | 1471 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1472 | `					  return rc;` |
|        - | 1473 | `				  }` |
|        - | 1474 | `				  /* Link the node to the tree */` |
|     1732 | 1475 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      865 | 1476 | `			  }else{` |
|        - | 1477 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1478 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1479 | `			  }` |
|     1800 | 1480 | `			  apNode[iCur + 1] = 0;` |
|     1800 | 1481 | `			  if( iRight + 1 < nToken ){` |
|        - | 1482 | `				  /* Recurse and process the else expression */` |
|     1800 | 1483 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1800 | 1484 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1485 | `					  return rc;` |
|        - | 1486 | `				  }` |
|        - | 1487 | `				  /* Link the node to the tree */` |
|     1800 | 1488 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1800 | 1489 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      901 | 1490 | `			  }else{` |
|      ! 0 | 1491 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1492 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1493 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1494 | `				 }` |
|      ! 0 | 1495 | `				 return rc;` |
|        - | 1496 | `			  }` |
|        - | 1497 | `			  /* Point to the condition */` |
|     1800 | 1498 | `			  pNode->pCond  = apNode[iLeft];` |
|     1800 | 1499 | `			  apNode[iLeft] = 0;` |
|     1800 | 1500 | `			  break;` |
|        - | 1501 | `		  }` |
|  1006936 | 1502 | `		  iLeft = iCur;` |
|   503469 | 1503 | `	  }` |
|        - | 1504 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1505 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1506 | `	  * so there is no need for a precedence loop here.` |
|        - | 1507 | `	  */` |
|   576014 | 1508 | `	 iRight = -1;` |
|  3708990 | 1509 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3133018 | 1510 | `		 if( apNode[iCur] == 0 ){` |
|  2341486 | 1511 | `			 continue;` |
|        - | 1512 | `		 }` |
|   791534 | 1513 | `		 pNode = apNode[iCur];` |
|   791534 | 1514 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1515 | `			 /* Get the left node */` |
|   215398 | 1516 | `			 iLeft = iCur - 1;` |
|   304994 | 1517 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    89598 | 1518 | `				 iLeft--;` |
|        2 | 1519 | `			 }` |
|   215398 | 1520 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1521 | `				 /* Syntax error */` |
|       39 | 1522 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1523 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1524 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1525 | `				 }` |
|       39 | 1526 | `				 return rc;` |
|        - | 1527 | `			 }` |
|   215360 | 1528 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       28 | 1529 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
|        - | 1530 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1531 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1532 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1533 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1534 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1535 | `					 }` |
|        3 | 1536 | `					 return rc;` |
|        - | 1537 | `				 }` |
|       12 | 1538 | `			 }` |
|        - | 1539 | `			 /* Link the node to the tree (Reverse) */` |
|   215358 | 1540 | `			 pNode->pLeft = apNode[iRight];` |
|   215358 | 1541 | `			 pNode->pRight = apNode[iLeft];` |
|   215358 | 1542 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   107678 | 1543 | `		 }` |
|   791494 | 1544 | `		 iRight = iCur;` |
|   395748 | 1545 | `	 }` |
|        - | 1546 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2879862 | 1547 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2303890 | 1548 | `		 iLeft = -1;` |
| 14835786 | 1549 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12531898 | 1550 | `			 if( apNode[iCur] == 0 ){` |
| 10227604 | 1551 | `				 continue;` |
|        - | 1552 | `			 }` |
|  2304296 | 1553 | `			 pNode = apNode[iCur];` |
|  2304296 | 1554 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1555 | `				 /* Get the right node */` |
|       72 | 1556 | `				 iRight = iCur + 1;` |
|      110 | 1557 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1558 | `					 iRight++;` |
|        2 | 1559 | `				 }` |
|       72 | 1560 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1561 | `					 /* Syntax error */` |
|      ! 0 | 1562 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1563 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1564 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1565 | `					 }` |
|      ! 0 | 1566 | `					 return rc;` |
|        - | 1567 | `				 }` |
|        - | 1568 | `				 /* Link the node to the tree */` |
|       72 | 1569 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1570 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1571 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1572 | `			 }` |
|  2304296 | 1573 | `			 iLeft = iCur;` |
|  1152149 | 1574 | `		 }` |
|  1151946 | 1575 | `	 }` |
|        - | 1576 | `	 /* Point to the root of the expression tree */` |
|  3132948 | 1577 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2556994 | 1578 | `		 if( apNode[iCur] ){` |
|   519740 | 1579 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1580 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1581 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1582 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1583 | `				  }` |
|       20 | 1584 | `				  return rc;` |
|        - | 1585 | `			 }` |
|   519722 | 1586 | `			 apNode[0] = apNode[iCur];` |
|   519722 | 1587 | `			 apNode[iCur] = 0;` |
|   259860 | 1588 | `		 }` |
|  1278489 | 1589 | `	 }` |
|   575956 | 1590 | `	 return SXRET_OK;` |
|   535443 | 1591 | ` }` |
|        - | 1592 | ` /*` |
|        - | 1593 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1594 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1595 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1596 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1597 | `  */` |
|   670600 | 1598 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1599 |  |
|        - | 1600 | `	ph7_expr_node **apNode;` |
|        - | 1601 | `	ph7_expr_node *pNode;` |
|        - | 1602 | `	sxi32 rc;` |
|        - | 1603 | `	/* Reset node container */` |
|   670602 | 1604 | `	SySetReset(pExprNode);` |
|   670602 | 1605 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1606 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1607 | `	{` |
|   670602 | 1608 | `		int iLastWasTerm = 0;` |
|  3630466 | 1609 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2959900 | 1610 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2959900 | 1611 | `			if( rc != SXRET_OK ){` |
|       35 | 1612 | `				return rc;` |
|        - | 1613 | `			}` |
|        - | 1614 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2959866 | 1615 | `			if( pNode->xCode ){` |
|        - | 1616 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1540474 | 1617 | `				iLastWasTerm = 1;` |
|  2189630 | 1618 | `			}else if( pNode->pOp ){` |
|        - | 1619 | `				/* Operator node */` |
|   665866 | 1620 | `				iLastWasTerm = 0;` |
|   332934 | 1621 | `			}else{` |
|        - | 1622 | `				/* Delimiter: ')' and ']' end terms */` |
|   753530 | 1623 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1624 | `			}` |
|        - | 1625 | `			/* Save the extracted node */` |
|  2959866 | 1626 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1627 | `		}` |
|        - | 1628 | `	}` |
|   670568 | 1629 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1630 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1631 | `		*ppRoot = 0;` |
|      ! 0 | 1632 | `		return SXRET_OK;` |
|        - | 1633 | `	}` |
|   670568 | 1634 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1635 | `	/* Make sure we are dealing with valid nodes */` |
|   670568 | 1636 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   670568 | 1637 | `	if( rc != SXRET_OK ){` |
|        - | 1638 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1639 | `		 * cleanup the mess left behind.` |
|        - | 1640 | `		 */` |
|       47 | 1641 | `		*ppRoot = 0;` |
|       47 | 1642 | `		return rc;` |
|        - | 1643 | `	}` |
|        - | 1644 | `	/* Build the tree */` |
|   670522 | 1645 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   670522 | 1646 | `	if( rc != SXRET_OK ){` |
|        - | 1647 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       78 | 1648 | `		*ppRoot = 0;` |
|       78 | 1649 | `		return rc;` |
|        - | 1650 | `	}` |
|        - | 1651 | `	/* Point to the root of the tree */` |
|   670446 | 1652 | `	*ppRoot = apNode[0];` |
|   670446 | 1653 | `	return SXRET_OK;` |
|   335302 | 1654 |  |
|        - | 1655 |  |
