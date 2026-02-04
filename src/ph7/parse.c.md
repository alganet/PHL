# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 820/944 lines (86.86%)

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
|  1270374 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        1 |  255 |  |
|  1270375 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
| 19474506 |  259 | `	for(;;){` |
| 38949013 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 38949013 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  4915181 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  2457591 |  266 | `		}else{` |
| 34033833 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 38949013 |  269 | `		if( rc == 0 ){` |
|  1270797 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1270135 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|      663 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      129 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|      535 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      121 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      121 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      113 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|      211 |  288 | `		}` |
| 37678639 |  289 | `		++n; /* Next operator in the table */` |
|        1 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   635188 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   538436 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        1 |  300 |  |
|   538437 |  301 | `	SyToken *pCur = pIn;` |
|   538437 |  302 | `	sxi32 iNest = 1;` |
|  3275085 |  303 | `	for(;;){` |
|  6550171 |  304 | `		if( pCur >= pEnd ){` |
|       49 |  305 | `			break;` |
|        - |  306 | `		}` |
|  6550123 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   370761 |  309 | `			iNest++;` |
|  6364743 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   909149 |  312 | `			iNest--;` |
|   909149 |  313 | `			if( iNest <= 0 ){` |
|   538389 |  314 | `				break;` |
|        - |  315 | `			}` |
|   185380 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  6011735 |  318 | `		pCur++;` |
|        1 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   538437 |  321 | `	*ppEnd = pCur;` |
|   538437 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    28088 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        1 |  333 |  |
|    42108 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    28066 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|       61 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    28029 |  339 | `	if( bCheckFunc ){` |
|      572 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       33 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       20 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|      561 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|        4 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    27469 |  347 | `	return FALSE;` |
|    14045 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|  1042104 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        1 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|  1042105 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        7 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        7 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        3 |  364 | `	}` |
|  1042105 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5607155 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4565083 |  367 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   468931 |  368 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    27392 |  369 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  370 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   425311 |  371 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  372 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  373 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  374 | `						 */` |
|   425311 |  375 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   425311 |  376 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   425311 |  377 | `						apNode[i]->pOp = &sFCallOp;` |
|   212655 |  378 | `					}` |
|   212655 |  379 | `			}` |
|   468931 |  380 | `			iParen++;` |
|  4330618 |  381 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   468925 |  382 | `			if( iParen <= 0 ){` |
|        9 |  383 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  384 | `				if( rc != SXERR_ABORT ){` |
|        9 |  385 | `					rc = SXERR_SYNTAX;` |
|        4 |  386 | `				}` |
|        9 |  387 | `				return rc;` |
|        - |  388 | `			}` |
|   468917 |  389 | `			iParen--;` |
|  3861687 |  390 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   123471 |  391 | `			iSquare++;` |
|  3565494 |  392 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   123485 |  393 | `			if( iSquare <= 0 ){` |
|        7 |  394 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  395 | `				if( rc != SXERR_ABORT ){` |
|        7 |  396 | `					rc = SXERR_SYNTAX;` |
|        3 |  397 | `				}` |
|        7 |  398 | `				return rc;` |
|        - |  399 | `			}` |
|   123479 |  400 | `			iSquare--;` |
|  3442014 |  401 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       13 |  402 | `			iBraces++;` |
|       13 |  403 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  404 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  405 | `				int iNest = 1;` |
|       11 |  406 | `				sxi32 j=i+1;` |
|        - |  407 | `				/*` |
|        - |  408 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  409 | `				 */` |
|       11 |  410 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  411 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  412 | `				pOp = aOpTable;` |
|       11 |  413 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       51 |  414 | `				while( pOp < pEnd ){` |
|       51 |  415 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  416 | `						break;` |
|        - |  417 | `					}` |
|       41 |  418 | `					pOp++;` |
|        1 |  419 | `				}` |
|       11 |  420 | `				if( pOp >= pEnd ){` |
|      ! 0 |  421 | `					pOp = 0;` |
|      ! 0 |  422 | `				}` |
|       11 |  423 | `				if( pOp ){` |
|       11 |  424 | `					apNode[i]->pOp = pOp;` |
|       11 |  425 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  426 | `				}` |
|       11 |  427 | `				iBraces--;` |
|       11 |  428 | `				iSquare++;` |
|       21 |  429 | `				while( j < nNode ){` |
|       21 |  430 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  431 | `						/* Increment nesting level */` |
|      ! 0 |  432 | `						iNest++;` |
|       21 |  433 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  434 | `						/* Decrement nesting level */` |
|       11 |  435 | `						iNest--;` |
|       11 |  436 | `						if( iNest < 1 ){` |
|       11 |  437 | `							break;` |
|        - |  438 | `						}` |
|      ! 0 |  439 | `					}` |
|       11 |  440 | `					j++;` |
|        1 |  441 | `				}` |
|       11 |  442 | `				if( j < nNode ){` |
|       11 |  443 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  444 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  445 | `				}` |
|        - |  446 |  |
|        6 |  447 | `			}` |
|  3380269 |  448 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       15 |  449 | `			if( iBraces <= 0 ){` |
|       13 |  450 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  451 | `				if( rc != SXERR_ABORT ){` |
|       13 |  452 | `					rc = SXERR_SYNTAX;` |
|        6 |  453 | `				}` |
|       13 |  454 | `				return rc;` |
|        - |  455 | `			}` |
|        3 |  456 | `			iBraces--;` |
|  3380250 |  457 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1551 |  458 | `			if( iQuesty <= 0 ){` |
|        7 |  459 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        7 |  460 | `				if( rc != SXERR_ABORT ){` |
|        7 |  461 | `					rc = SXERR_SYNTAX;` |
|        3 |  462 | `				}` |
|        7 |  463 | `				return rc;` |
|        - |  464 | `			}` |
|     1545 |  465 | `			iQuesty--;` |
|  3379471 |  466 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   990469 |  467 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   990469 |  468 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1547 |  469 | `				iQuesty++;` |
|   989696 |  470 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      233 |  471 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  472 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  473 | `					sxu32 n = 0;` |
|       11 |  474 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  475 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  476 | `					}` |
|        - |  477 | `					/*` |
|        - |  478 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  479 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  480 | `					 */` |
|      245 |  481 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      235 |  482 | `						++n;` |
|        1 |  483 | `					}` |
|       11 |  484 | `					pOp = &aOpTable[n];` |
|        - |  485 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  486 | `					apNode[i]->pOp = pOp;` |
|       11 |  487 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  488 | `				}` |
|      116 |  489 | `			}` |
|   495234 |  490 | `		}` |
|  2282526 |  491 | `	}` |
|  1042073 |  492 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       19 |  493 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       19 |  494 | `		if( rc != SXERR_ABORT ){` |
|       19 |  495 | `			rc = SXERR_SYNTAX;` |
|        9 |  496 | `		}` |
|       19 |  497 | `		return rc;` |
|        - |  498 | `	}` |
|  1042055 |  499 | `	return SXRET_OK;` |
|   521053 |  500 |  |
|        - |  501 | `/*` |
|        - |  502 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  503 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  504 | ` */` |
|   843894 |  505 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        1 |  506 |  |
|   843895 |  507 | `	SyToken *pIn = *ppCur;` |
|        - |  508 | `	/* Jump the first literal seen */` |
|   843895 |  509 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   843893 |  510 | `		pIn++;` |
|   421946 |  511 | `	}` |
|   421952 |  512 | `	for(;;){` |
|   843905 |  513 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       11 |  514 | `			pIn++;` |
|       11 |  515 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       11 |  516 | `				pIn++;` |
|        5 |  517 | `			}` |
|        6 |  518 | `		}else{` |
|   421948 |  519 | `			break;` |
|        - |  520 | `		}` |
|        1 |  521 | `	}` |
|        - |  522 | `	/* Synchronize pointers */` |
|   843895 |  523 | `	*ppCur = pIn;` |
|   843895 |  524 |  |
|        - |  525 | `/*` |
|        - |  526 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  527 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  528 | ` * Note on annonymous functions.` |
|        - |  529 | ` *  According to the PHP language reference manual:` |
|        - |  530 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  531 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  532 | ` *  parameters, but they have many other uses.` |
|        - |  533 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  534 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  535 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  536 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  537 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  538 | ` *` |
|        - |  539 | ` * Some example:` |
|        - |  540 | ` *  $greet = function($name)` |
|        - |  541 | ` * {` |
|        - |  542 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  543 | ` * };` |
|        - |  544 | ` *  $greet('World');` |
|        - |  545 | ` *  $greet('PHP');` |
|        - |  546 | ` *` |
|        - |  547 | ` * $double = function($a) {` |
|        - |  548 | ` *   return $a * 2;` |
|        - |  549 | ` * };` |
|        - |  550 | ` * // This is our range of numbers` |
|        - |  551 | ` * $numbers = range(1, 5);` |
|        - |  552 | ` * // Use the Annonymous function as a callback here to` |
|        - |  553 | ` * // double the size of each element in our` |
|        - |  554 | ` * // range` |
|        - |  555 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  556 | ` * print implode(' ', $new_numbers);` |
|        - |  557 | ` */` |
|       92 |  558 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        1 |  559 |  |
|       93 |  560 | `	SyToken *pIn = *ppCur;` |
|        - |  561 | `	sxu32 nLine;` |
|        - |  562 | `	sxi32 rc;` |
|        - |  563 | `	/* Jump the 'function' keyword */` |
|       93 |  564 | `	nLine = pIn->nLine;` |
|       93 |  565 | `	pIn++;` |
|       93 |  566 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        5 |  567 | `		pIn++;` |
|        2 |  568 | `	}` |
|       93 |  569 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  570 | `		/* Syntax error */` |
|        5 |  571 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  572 | `		if( rc != SXERR_ABORT ){` |
|        5 |  573 | `			rc = SXERR_SYNTAX;` |
|        2 |  574 | `		}` |
|        5 |  575 | `		goto Synchronize;` |
|        - |  576 | `	}` |
|       89 |  577 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       89 |  578 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       89 |  579 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  580 | `		/* Syntax error */` |
|        5 |  581 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  582 | `		if( rc != SXERR_ABORT ){` |
|        5 |  583 | `			rc = SXERR_SYNTAX;` |
|        2 |  584 | `		}` |
|        5 |  585 | `		goto Synchronize;` |
|        - |  586 | `	}` |
|       85 |  587 | `	pIn++; /* Jump the trailing parenthesis */` |
|       85 |  588 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       23 |  589 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  590 | `		/* Check if we are dealing with a closure */` |
|       23 |  591 | `		if( nKey == PH7_TKWRD_USE ){` |
|       15 |  592 | `			pIn++; /* Jump the 'use' keyword */` |
|       15 |  593 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  594 | `				/* Syntax error */` |
|        5 |  595 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  596 | `				if( rc != SXERR_ABORT ){` |
|        5 |  597 | `					rc = SXERR_SYNTAX;` |
|        2 |  598 | `				}` |
|        5 |  599 | `				goto Synchronize;` |
|        - |  600 | `			}` |
|       11 |  601 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       11 |  602 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       11 |  603 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  604 | `				/* Syntax error */` |
|        5 |  605 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  606 | `				if( rc != SXERR_ABORT ){` |
|        5 |  607 | `					rc = SXERR_SYNTAX;` |
|        2 |  608 | `				}` |
|        5 |  609 | `				goto Synchronize;` |
|        - |  610 | `			}` |
|        7 |  611 | `			pIn++;` |
|        4 |  612 | `		}else{` |
|        - |  613 | `			/* Syntax error */` |
|        9 |  614 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  615 | `			if( rc != SXERR_ABORT ){` |
|        9 |  616 | `				rc = SXERR_SYNTAX;` |
|        4 |  617 | `			}` |
|        9 |  618 | `			goto Synchronize;` |
|        - |  619 | `		}` |
|        3 |  620 | `	}` |
|       69 |  621 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|       69 |  622 | `		pIn++; /* Jump the leading curly '{' */` |
|       69 |  623 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       69 |  624 | `		if( pIn < pEnd ){` |
|       69 |  625 | `			pIn++;` |
|       34 |  626 | `		}` |
|       35 |  627 | `	}else{` |
|        - |  628 | `		/* Syntax error */` |
|      ! 0 |  629 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  630 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  631 | `			return SXERR_ABORT;` |
|        - |  632 | `		}` |
|        - |  633 | `	}` |
|       69 |  634 | `	rc = SXRET_OK;` |
|       46 |  635 | `Synchronize:` |
|        - |  636 | `	/* Synchronize pointers */` |
|       93 |  637 | `	*ppCur = pIn;` |
|       93 |  638 | `	return rc;` |
|       47 |  639 |  |
|        - |  640 | `/*` |
|        - |  641 | ` * Extract a single expression node from the input.` |
|        - |  642 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  643 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  644 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  645 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  646 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  647 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  648 | ` */` |
|  4565230 |  649 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode)` |
|        1 |  650 |  |
|        - |  651 | `	ph7_expr_node *pNode;` |
|        - |  652 | `	SyToken *pCur;` |
|        - |  653 | `	sxi32 rc;` |
|        - |  654 | `	/* Allocate a new node */` |
|  4565231 |  655 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4565231 |  656 | `	if( pNode == 0 ){` |
|        - |  657 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  658 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  659 | `		 */` |
|      ! 0 |  660 | `		return SXERR_MEM;` |
|        - |  661 | `	}` |
|        - |  662 | `	/* Zero the structure */` |
|  4565231 |  663 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4565231 |  664 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  665 | `	/* Point to the head of the token stream */` |
|  4565231 |  666 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  667 | `	/* Start collecting tokens */` |
|  4565231 |  668 | `	if( pCur->nType & PH7_TK_OP ){` |
|        - |  669 | `		/* Point to the instance that describe this operator */` |
|  1113977 |  670 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  671 | `		/* Advance the stream cursor */` |
|  1113977 |  672 | `		pCur++;` |
|  4008243 |  673 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  674 | `		/* Isolate variable */` |
|  2548725 |  675 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1274369 |  676 | `			pCur++; /* Variable variable */` |
|        1 |  677 | `		}` |
|  1274357 |  678 | `		if( pCur < pGen->pEnd ){` |
|  1274357 |  679 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  680 | `				/* Variable name */` |
|  1274333 |  681 | `				pCur++;` |
|   637191 |  682 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       19 |  683 | `				pCur++;` |
|        - |  684 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       19 |  685 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       19 |  686 | `				if( pCur < pGen->pEnd ){` |
|       15 |  687 | `					pCur++;` |
|        8 |  688 | `				}else{` |
|        5 |  689 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  690 | `					if( rc != SXERR_ABORT ){` |
|        5 |  691 | `						rc = SXERR_SYNTAX;` |
|        2 |  692 | `					}` |
|        5 |  693 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  694 | `					return rc;` |
|        - |  695 | `				}` |
|        7 |  696 | `			}` |
|   637176 |  697 | `		}` |
|  1274353 |  698 | `		pNode->xCode = PH7_CompileVariable;` |
|  2814075 |  699 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    44493 |  700 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    44493 |  701 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  702 | `			 /* List/Array node */` |
|    16905 |  703 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  704 | `				 /* Assume a literal */` |
|       17 |  705 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  706 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  707 | `			 }else{` |
|    16889 |  708 | `				 pCur += 2;` |
|        - |  709 | `				 /* Collect array/list tokens */` |
|    16889 |  710 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    16889 |  711 | `				 if( pCur < pGen->pEnd ){` |
|    16887 |  712 | `					 pCur++;` |
|     8444 |  713 | `				 }else{` |
|        - |  714 | `					 /* Syntax error */` |
|        4 |  715 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  716 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  717 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  718 | `						 rc = SXERR_SYNTAX;` |
|        1 |  719 | `					 }` |
|        3 |  720 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  721 | `					 return rc;` |
|        - |  722 | `				 }` |
|    16887 |  723 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    16887 |  724 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       27 |  725 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       27 |  726 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  727 | `						 /* Syntax error */` |
|        3 |  728 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  729 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  730 | `							 rc = SXERR_SYNTAX;` |
|        1 |  731 | `						 }` |
|        3 |  732 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  733 | `						 return rc;` |
|        - |  734 | `					 }` |
|       12 |  735 | `				 }` |
|        1 |  736 | `			 }` |
|    36039 |  737 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  738 | `			 /* Annonymous function */` |
|       93 |  739 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  740 | `				 /* Assume a literal */` |
|      ! 0 |  741 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  742 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  743 | `			 }else{` |
|        - |  744 | `				 /* Assemble annonymous functions body */` |
|       93 |  745 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       93 |  746 | `				 if( rc != SXRET_OK ){` |
|       25 |  747 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  748 | `					 return rc;` |
|        - |  749 | `				 }` |
|       69 |  750 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        1 |  751 | `			  }` |
|    27531 |  752 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  753 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       33 |  754 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       33 |  755 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       17 |  756 | `		 }else{` |
|        - |  757 | `			 /* Assume a literal */` |
|    27465 |  758 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    27465 |  759 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        1 |  760 | `		 }` |
|  2154639 |  761 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  762 | `		 /* Constants,function name,namespace path,class name... */` |
|   816415 |  763 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   816415 |  764 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   408208 |  765 | `	 }else{` |
|  1315993 |  766 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  767 | `			 /* Point to the code generator routine */` |
|   253063 |  768 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   253063 |  769 | `			 if( pNode->xCode == 0 ){` |
|        3 |  770 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  771 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  772 | `					 rc = SXERR_SYNTAX;` |
|        1 |  773 | `				 }` |
|        3 |  774 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  775 | `				 return rc;` |
|        - |  776 | `			 }` |
|   126530 |  777 | `		 }` |
|        - |  778 | `		/* Advance the stream cursor */` |
|  1315991 |  779 | `		pCur++;` |
|        - |  780 | `	 }` |
|        - |  781 | `	/* Point to the end of the token stream */` |
|  4565197 |  782 | `	pNode->pEnd = pCur;` |
|        - |  783 | `	/* Save the node for later processing */` |
|  4565197 |  784 | `	*ppNode = pNode;` |
|        - |  785 | `	/* Synchronize cursors */` |
|  4565197 |  786 | `	pGen->pIn = pCur;` |
|  4565197 |  787 | `	return SXRET_OK;` |
|  2282616 |  788 |  |
|        - |  789 | `/*` |
|        - |  790 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  791 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  792 | ` * level is zero.` |
|        - |  793 | ` */` |
|    35440 |  794 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        1 |  795 |  |
|    35441 |  796 | `	SyToken *pCur = pStart;` |
|    35441 |  797 | `	sxi32 iNest = 0;` |
|    35441 |  798 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  799 | `		/* Last expression */` |
|    25297 |  800 | `		return SXERR_EOF;` |
|        - |  801 | `	}` |
|    53085 |  802 | `	while( pCur < pEnd ){` |
|    52333 |  803 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|     9393 |  804 | `			break;` |
|        - |  805 | `		}` |
|    42941 |  806 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     3197 |  807 | `			iNest++;` |
|    41343 |  808 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     3197 |  809 | `			iNest--;` |
|     1598 |  810 | `		}` |
|    42941 |  811 | `		pCur++;` |
|        1 |  812 | `	}` |
|    10145 |  813 | `	*ppNext = pCur;` |
|    10145 |  814 | `	return SXRET_OK;` |
|    17721 |  815 |  |
|        - |  816 | `/*` |
|        - |  817 | ` * Free an expression tree.` |
|        - |  818 | ` */` |
|  3985196 |  819 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        1 |  820 |  |
|  3985197 |  821 | `	if( pNode->pLeft ){` |
|        - |  822 | `		/* Release the left tree */` |
|  1509463 |  823 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   754731 |  824 | `	}` |
|  3985197 |  825 | `	if( pNode->pRight ){` |
|        - |  826 | `		/* Release the right tree */` |
|   857999 |  827 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   428999 |  828 | `	}` |
|  3985197 |  829 | `	if( pNode->pCond ){` |
|        - |  830 | `		/* Release the conditional tree used by the ternary operator */` |
|     1543 |  831 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      771 |  832 | `	}` |
|  3985197 |  833 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  834 | `		ph7_expr_node **apArg;` |
|        - |  835 | `		sxu32 n;` |
|        - |  836 | `		/* Release node arguments */` |
|   456851 |  837 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   943381 |  838 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   486531 |  839 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   243266 |  840 | `		}` |
|   456851 |  841 | `		SySetRelease(&pNode->aNodeArgs);` |
|   228425 |  842 | `	}` |
|        - |  843 | `	/* Finally,release this node */` |
|  3985197 |  844 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3985197 |  845 |  |
|        - |  846 | `/*` |
|        - |  847 | ` * Free an expression tree.` |
|        - |  848 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  849 | ` */` |
|  1042138 |  850 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        1 |  851 |  |
|        - |  852 | `	ph7_expr_node **apNode;` |
|        - |  853 | `	sxu32 n;` |
|  1042139 |  854 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5607335 |  855 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4565197 |  856 | `		if( apNode[n] ){` |
|  1042453 |  857 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   521226 |  858 | `		}` |
|  2282599 |  859 | `	}` |
|  1042139 |  860 | `	return SXRET_OK;` |
|        1 |  861 |  |
|        - |  862 | `/*` |
|        - |  863 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  864 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  865 | ` */` |
|   378908 |  866 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        1 |  867 |  |
|        - |  868 | `	sxi32 iExprOp;` |
|   378909 |  869 | `	if( pNode->pOp == 0 ){` |
|   250543 |  870 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  871 | `	}` |
|   128367 |  872 | `	iExprOp = pNode->pOp->iOp;` |
|   128367 |  873 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    90745 |  874 | `			return TRUE;` |
|        - |  875 | `	}` |
|    37623 |  876 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    37619 |  877 | `		if( pNode->pLeft->pOp ) {` |
|        2 |  878 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  879 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  880 | `				return FALSE;` |
|        1 |  881 | `			}` |
|    37618 |  882 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  883 | `			return FALSE;` |
|        - |  884 | `		}` |
|    37619 |  885 | `		return TRUE;` |
|        - |  886 | `	}` |
|        5 |  887 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  888 | `		return TRUE;` |
|        - |  889 | `	}` |
|        - |  890 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  891 | `	return FALSE;` |
|   189455 |  892 |  |
|        - |  893 | `/* Forward declaration */` |
|        - |  894 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  895 | `/* Macro to check if the given node is a terminal */` |
|        - |  896 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft ))` |
|        - |  897 | `/*` |
|        - |  898 | ` * Buid an expression tree for each given function argument.` |
|        - |  899 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  900 | ` */` |
|   360080 |  901 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        1 |  902 |  |
|        - |  903 | `	sxi32 iNest,iCur,iNode;` |
|        - |  904 | `	sxi32 rc;` |
|        - |  905 | `	/* Process function arguments from left to right */` |
|   360081 |  906 | `	iCur = 0;` |
|   374919 |  907 | `	for(;;){` |
|   749839 |  908 | `		if( iCur >= nToken ){` |
|        - |  909 | `			/* No more arguments to process */` |
|   360079 |  910 | `			break;` |
|        - |  911 | `		}` |
|   389761 |  912 | `		iNode = iCur;` |
|   389761 |  913 | `		iNest = 0;` |
|   952157 |  914 | `		while( iCur < nToken ){` |
|   592079 |  915 | `			if( apNode[iCur] ){` |
|   592079 |  916 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    14842 |  917 | `					break;` |
|   562397 |  918 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    32533 |  919 | `					iNest++;` |
|   546131 |  920 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    32533 |  921 | `					iNest--;` |
|    16266 |  922 | `				}` |
|   281198 |  923 | `			}` |
|   562397 |  924 | `			iCur++;` |
|        1 |  925 | `		}` |
|   389761 |  926 | `		if( iCur > iNode ){` |
|   389758 |  927 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        1 |  928 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  929 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  930 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  931 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  932 | `					apNode[iNode] = 0;` |
|      ! 0 |  933 | `			}` |
|   389759 |  934 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   389759 |  935 | `			if( apNode[iNode] ){` |
|        - |  936 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   389759 |  937 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   194880 |  938 | `			}else{` |
|        - |  939 | `				/* Empty function argument */` |
|      ! 0 |  940 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  941 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  942 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  943 | `				}` |
|      ! 0 |  944 | `				return rc;` |
|        - |  945 | `			}` |
|   194880 |  946 | `		}else{` |
|        3 |  947 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  948 | `			if( rc != SXERR_ABORT ){` |
|        3 |  949 | `				rc = SXERR_SYNTAX;` |
|        1 |  950 | `			}` |
|        3 |  951 | `			return rc;` |
|        - |  952 | `		}` |
|        - |  953 | `		/* Jump trailing comma */` |
|   389759 |  954 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    29681 |  955 | `			iCur++;` |
|    29681 |  956 | `			if( iCur >= nToken ){` |
|        - |  957 | `				/* missing function argument */` |
|      ! 0 |  958 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 |  959 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  960 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  961 | `				}` |
|      ! 0 |  962 | `				return rc;` |
|        - |  963 | `			}` |
|    14840 |  964 | `		}` |
|        1 |  965 | `	}` |
|   360079 |  966 | `	return SXRET_OK;` |
|   180041 |  967 |  |
|        - |  968 | ` /*` |
|        - |  969 | `  * Create an expression tree from an array of tokens.` |
|        - |  970 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - |  971 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  972 | `  */` |
|  1580734 |  973 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        1 |  974 | ` {` |
|        - |  975 | `	 sxi32 i,iLeft,iRight;` |
|        - |  976 | `	 ph7_expr_node *pNode;` |
|        - |  977 | `	 sxi32 iCur;` |
|        - |  978 | `	 sxi32 rc;` |
|  1580735 |  979 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - |  980 | `		 /* TICKET 1433-17: self evaluating node */` |
|   662969 |  981 | `		 return SXRET_OK;` |
|        - |  982 | `	 }` |
|        - |  983 | `	 /* Process expressions enclosed in parenthesis first */` |
|  5438949 |  984 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - |  985 | `		 sxi32 iNest;` |
|        - |  986 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - |  987 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - |  988 | `		  */` |
|  4521185 |  989 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4477577 |  990 | `			 continue;` |
|        - |  991 | `		 }` |
|    43609 |  992 | `		 iNest = 1;` |
|    43609 |  993 | `		 iLeft = iCur;` |
|        - |  994 | `		 /* Find the closing parenthesis */` |
|    43609 |  995 | `		 iCur++;` |
|   247219 |  996 | `		 while( iCur < nToken ){` |
|   247219 |  997 | `			 if( apNode[iCur] ){` |
|   247219 |  998 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - |  999 | `					 /* Decrement nesting level */` |
|    70743 | 1000 | `					 iNest--;` |
|    70743 | 1001 | `					 if( iNest <= 0 ){` |
|    43609 | 1002 | `						 break;` |
|        1 | 1003 | `					 }` |
|   190044 | 1004 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1005 | `					 /* Increment nesting level */` |
|    27135 | 1006 | `					 iNest++;` |
|    13567 | 1007 | `				 }` |
|   101805 | 1008 | `			 }` |
|   203611 | 1009 | `			 iCur++;` |
|        1 | 1010 | `		 }` |
|    43609 | 1011 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1012 | `			 /* Recurse and process this expression */` |
|    43609 | 1013 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    43609 | 1014 | `			 if( rc != SXRET_OK ){` |
|        3 | 1015 | `				 return rc;` |
|        - | 1016 | `			 }` |
|    21803 | 1017 | `		 }` |
|        - | 1018 | `		 /* Free the left and right nodes */` |
|    43607 | 1019 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    43607 | 1020 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    43607 | 1021 | `		 apNode[iLeft] = 0;` |
|    43607 | 1022 | `		 apNode[iCur] = 0;` |
|    21804 | 1023 | `	 }` |
|        - | 1024 | `	  /* Process expressions enclosed in braces */` |
|  5675235 | 1025 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1026 | `		 sxi32 iNest;` |
|        - | 1027 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1028 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1029 | `		  */` |
|  4768385 | 1030 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4768383 | 1031 | `			 continue;` |
|        - | 1032 | `		 }` |
|        3 | 1033 | `		 iNest = 1;` |
|        3 | 1034 | `		 iLeft = iCur;` |
|        - | 1035 | `		 /* Find the closing parenthesis */` |
|        3 | 1036 | `		 iCur++;` |
|        7 | 1037 | `		 while( iCur < nToken ){` |
|        7 | 1038 | `			 if( apNode[iCur] ){` |
|        7 | 1039 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1040 | `					 /* Decrement nesting level */` |
|        3 | 1041 | `					 iNest--;` |
|        3 | 1042 | `					 if( iNest <= 0 ){` |
|        3 | 1043 | `						 break;` |
|      ! 0 | 1044 | `					 }` |
|        5 | 1045 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1046 | `					 /* Increment nesting level */` |
|      ! 0 | 1047 | `					 iNest++;` |
|      ! 0 | 1048 | `				 }` |
|        2 | 1049 | `			 }` |
|        5 | 1050 | `			 iCur++;` |
|        1 | 1051 | `		 }` |
|        3 | 1052 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1053 | `			 /* Recurse and process this expression */` |
|        3 | 1054 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        3 | 1055 | `			 if( rc != SXRET_OK ){` |
|        3 | 1056 | `				 return rc;` |
|        - | 1057 | `			 }` |
|      ! 0 | 1058 | `		 }` |
|        - | 1059 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1060 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1061 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1062 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1063 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1064 | `	 }` |
|        - | 1065 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   906851 | 1066 | `	 iLeft = -1;` |
|  5675207 | 1067 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4768367 | 1068 | `		 if( apNode[iCur] == 0 ){` |
|  1688491 | 1069 | `			 continue;` |
|        - | 1070 | `		 }` |
|  3079877 | 1071 | `		 pNode = apNode[iCur];` |
|  3079877 | 1072 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   751887 | 1073 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1074 | `				 /* Collect function arguments */` |
|   425307 | 1075 | `				 sxi32 iPtr = 0;` |
|   425307 | 1076 | `				 sxi32 nFuncTok = 0;` |
|  1442693 | 1077 | `				 while( nFuncTok + iCur < nToken ){` |
|  1442693 | 1078 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1442693 | 1079 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   430995 | 1080 | `							 iPtr++;` |
|  1227196 | 1081 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   430995 | 1082 | `							 iPtr--;` |
|   430995 | 1083 | `							 if( iPtr <= 0 ){` |
|   425307 | 1084 | `								 break;` |
|        - | 1085 | `							 }` |
|     2844 | 1086 | `						 }` |
|   508693 | 1087 | `					 }` |
|  1017387 | 1088 | `					 nFuncTok++;` |
|        1 | 1089 | `				 }` |
|   425307 | 1090 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1091 | `					 /* Syntax error */` |
|      ! 0 | 1092 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1093 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1094 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1095 | `					 }` |
|      ! 0 | 1096 | `					 return rc;` |
|        - | 1097 | `				 }` |
|   425307 | 1098 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1099 | `					 /* Syntax error */` |
|      ! 0 | 1100 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1101 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1102 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1103 | `					 }` |
|      ! 0 | 1104 | `					 return rc;` |
|        - | 1105 | `				 }` |
|   425307 | 1106 | `				 if( nFuncTok > 1 ){` |
|        - | 1107 | `					 /* Process function arguments */` |
|   360081 | 1108 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   360081 | 1109 | `					 if( rc != SXRET_OK ){` |
|        3 | 1110 | `						 return rc;` |
|        - | 1111 | `					 }` |
|   180039 | 1112 | `				 }` |
|        - | 1113 | `				 /* Link the node to the tree */` |
|   425305 | 1114 | `				 pNode->pLeft = apNode[iLeft];` |
|   425305 | 1115 | `				 apNode[iLeft] = 0;` |
|  1442685 | 1116 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1017381 | 1117 | `					 apNode[iCur+iPtr] = 0;` |
|   508691 | 1118 | `				 }` |
|   539233 | 1119 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1120 | `				 /* Subscripting */` |
|   123479 | 1121 | `				 sxi32 iArrTok = iCur + 1;` |
|   123479 | 1122 | `				 sxi32 iNest = 1;` |
|   123494 | 1123 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        1 | 1124 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString ) ) \|\|` |
|   123478 | 1125 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1126 | `						 /* Syntax error */` |
|        5 | 1127 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        5 | 1128 | `						 if( rc != SXERR_ABORT ){` |
|        5 | 1129 | `							 rc = SXERR_SYNTAX;` |
|        2 | 1130 | `						 }` |
|        5 | 1131 | `						 return rc;` |
|        - | 1132 | `				 }` |
|        - | 1133 | `				 /* Collect index tokens */` |
|   220353 | 1134 | `				 while( iArrTok < nToken ){` |
|   220353 | 1135 | `					 if( apNode[iArrTok] ){` |
|   220321 | 1136 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1137 | `							 /* Increment nesting level */` |
|      ! 0 | 1138 | `							 iNest++;` |
|   220321 | 1139 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1140 | `							 /* Decrement nesting level */` |
|   123475 | 1141 | `							 iNest--;` |
|   123475 | 1142 | `							 if( iNest <= 0 ){` |
|   123475 | 1143 | `								 break;` |
|        - | 1144 | `							 }` |
|      ! 0 | 1145 | `						 }` |
|    48423 | 1146 | `					 }` |
|    96879 | 1147 | `					 ++iArrTok;` |
|        1 | 1148 | `				 }` |
|   123475 | 1149 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1150 | `					 /* Recurse and process this expression */` |
|    96773 | 1151 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    96773 | 1152 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1153 | `						 return rc;` |
|        - | 1154 | `					 }` |
|        - | 1155 | `					 /* Link the node to it's index */` |
|    96773 | 1156 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    48386 | 1157 | `				 }` |
|        - | 1158 | `				 /* Link the node to the tree */` |
|   123475 | 1159 | `				 pNode->pLeft = apNode[iLeft];` |
|   123475 | 1160 | `				 pNode->pRight = 0;` |
|   123475 | 1161 | `				 apNode[iLeft] = 0;` |
|   343827 | 1162 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   220353 | 1163 | `					 apNode[iNest] = 0;` |
|   110177 | 1164 | `				 }` |
|    61738 | 1165 | `			 }else{` |
|        - | 1166 | `				 /* Member access operators [i.e: '->','::'] */` |
|   203103 | 1167 | `				  iRight = iCur + 1;` |
|   203103 | 1168 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1169 | `					 iRight++;` |
|      ! 0 | 1170 | `				 }` |
|   203103 | 1171 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1172 | `					 /* Syntax error */` |
|        5 | 1173 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1174 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1175 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1176 | `					 }` |
|        5 | 1177 | `					 return rc;` |
|        - | 1178 | `				 }` |
|        - | 1179 | `				 /* Link the node to the tree */` |
|   203099 | 1180 | `				 pNode->pLeft = apNode[iLeft];` |
|   203099 | 1181 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   203040 | 1182 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1183 | `						 /* Syntax error */` |
|      ! 0 | 1184 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1185 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1186 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1187 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1188 | `						 }` |
|      ! 0 | 1189 | `						 return rc;` |
|        - | 1190 | `				 }` |
|   203099 | 1191 | `				 pNode->pRight = apNode[iRight];` |
|   203099 | 1192 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1193 | `			 }` |
|   375938 | 1194 | `		 }` |
|  3079867 | 1195 | `		 iLeft = iCur;` |
|  1539934 | 1196 | `	 }` |
|        - | 1197 | `	 /* Handle left associative (new, clone) operators */` |
|  5675175 | 1198 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4768335 | 1199 | `		 if( apNode[iCur] == 0 ){` |
|  2451285 | 1200 | `			 continue;` |
|        - | 1201 | `		 }` |
|  2317051 | 1202 | `		 pNode = apNode[iCur];` |
|  2317051 | 1203 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1204 | `			 SyToken *pToken;` |
|        - | 1205 | `			 /* Get the left node */` |
|    10919 | 1206 | `			 iLeft = iCur + 1;` |
|    21815 | 1207 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    10897 | 1208 | `				 iLeft++;` |
|        1 | 1209 | `			 }` |
|    10919 | 1210 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1211 | `				  /* Syntax error */` |
|      ! 0 | 1212 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1213 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1214 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1215 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1216 | `				 }` |
|      ! 0 | 1217 | `				 return rc;` |
|        - | 1218 | `			 }` |
|        - | 1219 | `			 /* Make sure the operand are of a valid type */` |
|    10919 | 1220 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1221 | `				 /* Clone:` |
|        - | 1222 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1223 | `				  *  ++ function call (including annonymous)` |
|        - | 1224 | `				  *  ++ array member` |
|        - | 1225 | `				  *  ++ 'new' operator` |
|        - | 1226 | `				  * Example:` |
|        - | 1227 | `				  *   clone $pObj;` |
|        - | 1228 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1229 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1230 | `				  */` |
|       17 | 1231 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       15 | 1232 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1233 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1234 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1235 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1236 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1237 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1238 | `						 }` |
|      ! 0 | 1239 | `						 return rc;` |
|        - | 1240 | `					 }` |
|        7 | 1241 | `				 }` |
|        9 | 1242 | `			 }else{` |
|        - | 1243 | `				 /* New */` |
|    10903 | 1244 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|        9 | 1245 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|        9 | 1246 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1247 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1248 | `						 /* Syntax error */` |
|      ! 0 | 1249 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1250 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1251 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1252 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1253 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1254 | `						 }` |
|      ! 0 | 1255 | `						 return rc;` |
|        - | 1256 | `					 }` |
|        4 | 1257 | `				 }` |
|        - | 1258 | `			 }` |
|        - | 1259 | `			  /* Link the node to the tree */` |
|    10919 | 1260 | `			 pNode->pLeft = apNode[iLeft];` |
|    10919 | 1261 | `			 apNode[iLeft] = 0;` |
|    10919 | 1262 | `			 pNode->pRight = 0; /* Paranoid */` |
|     5459 | 1263 | `		 }` |
|  1158526 | 1264 | `	 }` |
|        - | 1265 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   906841 | 1266 | `	 iLeft = -1;` |
|  5675175 | 1267 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4762879 | 1268 | `		 if( apNode[iCur] == 0 ){` |
|  2451285 | 1269 | `			 continue;` |
|        - | 1270 | `		 }` |
|  2311595 | 1271 | `		 pNode = apNode[iCur];` |
|  2311595 | 1272 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    16015 | 1273 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     5468 | 1274 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1275 | `					 /* Link the node to the tree */` |
|     5463 | 1276 | `					 pNode->pLeft = apNode[iLeft];` |
|     5463 | 1277 | `					 apNode[iLeft] = 0;` |
|     2731 | 1278 | `			 }` |
|    10735 | 1279 | `		  }` |
|  2317051 | 1280 | `		 iLeft = iCur;` |
|  1158526 | 1281 | `	  }` |
|   912297 | 1282 | `	 iLeft = -1;` |
|  5680631 | 1283 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4768335 | 1284 | `		 if( apNode[iCur] == 0 ){` |
|  2456747 | 1285 | `			 continue;` |
|        - | 1286 | `		 }` |
|  2311589 | 1287 | `		 pNode = apNode[iCur];` |
|  2311589 | 1288 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    16008 | 1289 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    16009 | 1290 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1291 | `					 /* Syntax error */` |
|      ! 0 | 1292 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1293 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1294 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1295 | `					 }` |
|      ! 0 | 1296 | `					 return rc;` |
|        - | 1297 | `			 }` |
|        - | 1298 | `			 /* Link the node to the tree */` |
|    16009 | 1299 | `			 pNode->pLeft = apNode[iLeft];` |
|    16009 | 1300 | `			 apNode[iLeft] = 0;` |
|        - | 1301 | `			 /* Mark as pre-increment/decrement node */` |
|    16009 | 1302 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     8004 | 1303 | `		  }` |
|  2311589 | 1304 | `		 iLeft = iCur;` |
|  1155795 | 1305 | `	 }` |
|        - | 1306 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   912297 | 1307 | `	  iLeft = 0;` |
|  5680625 | 1308 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4768331 | 1309 | `		  if( apNode[iCur] ){` |
|  2295577 | 1310 | `			  pNode = apNode[iCur];` |
|  2295577 | 1311 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    70301 | 1312 | `				  if( iLeft > 0 ){` |
|        - | 1313 | `					  /* Link the node to the tree */` |
|    70299 | 1314 | `					  pNode->pLeft = apNode[iLeft];` |
|    70299 | 1315 | `					  apNode[iLeft] = 0;` |
|    70299 | 1316 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|        5 | 1317 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1318 | `							   /* Syntax error */` |
|      ! 0 | 1319 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1320 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1321 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1322 | `							  }` |
|      ! 0 | 1323 | `							  return rc;` |
|        - | 1324 | `						  }` |
|        2 | 1325 | `					  }` |
|    35150 | 1326 | `				  }else{` |
|        - | 1327 | `					  /* Syntax error */` |
|        3 | 1328 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1329 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1330 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1331 | `					  }` |
|        3 | 1332 | `					  return rc;` |
|        - | 1333 | `				  }` |
|    35149 | 1334 | `			  }` |
|        - | 1335 | `			  /* Save terminal position */` |
|  2295575 | 1336 | `			  iLeft = iCur;` |
|  1147787 | 1337 | `		  }` |
|  2384165 | 1338 | `	  }` |
|        - | 1339 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
| 10035159 | 1340 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  9122873 | 1341 | `		 iLeft = -1;` |
| 56805907 | 1342 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 47683043 | 1343 | `			 if( apNode[iCur] == 0 ){` |
| 28886059 | 1344 | `				 continue;` |
|        - | 1345 | `			 }` |
| 18796985 | 1346 | `			 pNode = apNode[iCur];` |
| 18796985 | 1347 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1348 | `				 /* Get the right node */` |
|   274467 | 1349 | `				 iRight = iCur + 1;` |
|   367239 | 1350 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    92773 | 1351 | `					 iRight++;` |
|        1 | 1352 | `				 }` |
|   274467 | 1353 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1354 | `					 /* Syntax error */` |
|        9 | 1355 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1356 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1357 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1358 | `					 }` |
|        9 | 1359 | `					 return rc;` |
|        - | 1360 | `				 }` |
|   274459 | 1361 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1362 | `					 sxi32  iTmp;` |
|        - | 1363 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       45 | 1364 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1365 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1366 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1367 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1368 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1369 | `						 }` |
|      ! 0 | 1370 | `						 return rc;` |
|        - | 1371 | `					 }` |
|       45 | 1372 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       33 | 1373 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1374 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1375 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1376 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1377 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1378 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1379 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1380 | `									 }` |
|      ! 0 | 1381 | `									 return rc;` |
|        - | 1382 | `							 }` |
|      ! 0 | 1383 | `						 }` |
|       16 | 1384 | `					 }` |
|        - | 1385 | `					 /* Swap operands */` |
|       45 | 1386 | `					 iTmp = iRight;` |
|       45 | 1387 | `					 iRight = iLeft;` |
|       45 | 1388 | `					 iLeft = iTmp;` |
|       22 | 1389 | `				 }` |
|        - | 1390 | `				 /* Link the node to the tree */` |
|   274459 | 1391 | `				 pNode->pLeft = apNode[iLeft];` |
|   274459 | 1392 | `				 pNode->pRight = apNode[iRight];` |
|   274459 | 1393 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   137229 | 1394 | `			 }` |
| 18796977 | 1395 | `			 iLeft = iCur;` |
|  9398489 | 1396 | `		 }` |
|  4561433 | 1397 | `	 }` |
|        - | 1398 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1399 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1400 | `	  * we are dealing with a single operator.` |
|        - | 1401 | `	  */` |
|   912287 | 1402 | `	  iLeft = -1;` |
|  5673963 | 1403 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4763221 | 1404 | `		  if( apNode[iCur] == 0 ){` |
|  3091521 | 1405 | `			  continue;` |
|        - | 1406 | `		  }` |
|  1671701 | 1407 | `		  pNode = apNode[iCur];` |
|  1671701 | 1408 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1545 | 1409 | `			  sxi32 iNest = 1;` |
|     1545 | 1410 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1411 | `				  /* Missing condition */` |
|        3 | 1412 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1413 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1414 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1415 | `				  }` |
|        3 | 1416 | `				  return rc;` |
|        - | 1417 | `			  }` |
|        - | 1418 | `			  /* Get the right node */` |
|     1543 | 1419 | `			  iRight = iCur + 1;` |
|     3305 | 1420 | `			  while( iRight < nToken  ){` |
|     3305 | 1421 | `				  if( apNode[iRight] ){` |
|     3085 | 1422 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1423 | `						  /* Increment nesting level */` |
|      ! 0 | 1424 | `						  ++iNest;` |
|     3085 | 1425 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1426 | `						  /* Decrement nesting level */` |
|     1543 | 1427 | `						  --iNest;` |
|     1543 | 1428 | `						  if( iNest <= 0 ){` |
|     1543 | 1429 | `							  break;` |
|        - | 1430 | `						  }` |
|      ! 0 | 1431 | `					  }` |
|      771 | 1432 | `				  }` |
|     1763 | 1433 | `				  iRight++;` |
|        1 | 1434 | `			  }` |
|     1543 | 1435 | `			  if( iRight > iCur + 1 ){` |
|        - | 1436 | `				  /* Recurse and process the then expression */` |
|     1543 | 1437 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1543 | 1438 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1439 | `					  return rc;` |
|        - | 1440 | `				  }` |
|        - | 1441 | `				  /* Link the node to the tree */` |
|     1543 | 1442 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      772 | 1443 | `			  }else{` |
|      ! 0 | 1444 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'then' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1445 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1446 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1447 | `				 }` |
|      ! 0 | 1448 | `				 return rc;` |
|        - | 1449 | `			  }` |
|     1543 | 1450 | `			  apNode[iCur + 1] = 0;` |
|     1543 | 1451 | `			  if( iRight + 1 < nToken ){` |
|        - | 1452 | `				  /* Recurse and process the else expression */` |
|     1543 | 1453 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1543 | 1454 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1455 | `					  return rc;` |
|        - | 1456 | `				  }` |
|        - | 1457 | `				  /* Link the node to the tree */` |
|     1543 | 1458 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1543 | 1459 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      772 | 1460 | `			  }else{` |
|      ! 0 | 1461 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1462 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1463 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1464 | `				 }` |
|      ! 0 | 1465 | `				 return rc;` |
|        - | 1466 | `			  }` |
|        - | 1467 | `			  /* Point to the condition */` |
|     1543 | 1468 | `			  pNode->pCond  = apNode[iLeft];` |
|     1543 | 1469 | `			  apNode[iLeft] = 0;` |
|     1543 | 1470 | `			  break;` |
|        - | 1471 | `		  }` |
|  1670157 | 1472 | `		  iLeft = iCur;` |
|   835079 | 1473 | `	  }` |
|        - | 1474 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1475 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1476 | `	  * so there is no need for a precedence loop here.` |
|        - | 1477 | `	  */` |
|   912285 | 1478 | `	 iRight = -1;` |
|  5680491 | 1479 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4768247 | 1480 | `		 if( apNode[iCur] == 0 ){` |
|  3476967 | 1481 | `			 continue;` |
|        - | 1482 | `		 }` |
|  1291281 | 1483 | `		 pNode = apNode[iCur];` |
|  1291281 | 1484 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1485 | `			 /* Get the left node */` |
|   378871 | 1486 | `			 iLeft = iCur - 1;` |
|   518207 | 1487 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   139337 | 1488 | `				 iLeft--;` |
|        1 | 1489 | `			 }` |
|   378871 | 1490 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1491 | `				 /* Syntax error */` |
|       39 | 1492 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1493 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1494 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1495 | `				 }` |
|       39 | 1496 | `				 return rc;` |
|        - | 1497 | `			 }` |
|   378833 | 1498 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       27 | 1499 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
|        - | 1500 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1501 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1502 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1503 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1504 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1505 | `					 }` |
|        3 | 1506 | `					 return rc;` |
|        - | 1507 | `				 }` |
|       12 | 1508 | `			 }` |
|        - | 1509 | `			 /* Link the node to the tree (Reverse) */` |
|   378831 | 1510 | `			 pNode->pLeft = apNode[iRight];` |
|   378831 | 1511 | `			 pNode->pRight = apNode[iLeft];` |
|   378831 | 1512 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   189415 | 1513 | `		 }` |
|  1291241 | 1514 | `		 iRight = iCur;` |
|   645621 | 1515 | `	 }` |
|        - | 1516 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  4561221 | 1517 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3648977 | 1518 | `		 iLeft = -1;` |
| 22721793 | 1519 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 19072817 | 1520 | `			 if( apNode[iCur] == 0 ){` |
| 15423427 | 1521 | `				 continue;` |
|        - | 1522 | `			 }` |
|  3649391 | 1523 | `			 pNode = apNode[iCur];` |
|  3649391 | 1524 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1525 | `				 /* Get the right node */` |
|       71 | 1526 | `				 iRight = iCur + 1;` |
|      109 | 1527 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       39 | 1528 | `					 iRight++;` |
|        1 | 1529 | `				 }` |
|       71 | 1530 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1531 | `					 /* Syntax error */` |
|      ! 0 | 1532 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1533 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1534 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1535 | `					 }` |
|      ! 0 | 1536 | `					 return rc;` |
|        - | 1537 | `				 }` |
|        - | 1538 | `				 /* Link the node to the tree */` |
|       71 | 1539 | `				 pNode->pLeft = apNode[iLeft];` |
|       71 | 1540 | `				 pNode->pRight = apNode[iRight];` |
|       71 | 1541 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1542 | `			 }` |
|  3649391 | 1543 | `			 iLeft = iCur;` |
|  1824696 | 1544 | `		 }` |
|  1824489 | 1545 | `	 }` |
|        - | 1546 | `	 /* Point to the root of the expression tree */` |
|  4768175 | 1547 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3855951 | 1548 | `		 if( apNode[iCur] ){` |
|   821037 | 1549 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       21 | 1550 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       21 | 1551 | `				  if( rc != SXERR_ABORT ){` |
|       21 | 1552 | `					  rc = SXERR_SYNTAX;` |
|       10 | 1553 | `				  }` |
|       21 | 1554 | `				  return rc;` |
|        - | 1555 | `			 }` |
|   821017 | 1556 | `			 apNode[0] = apNode[iCur];` |
|   821017 | 1557 | `			 apNode[iCur] = 0;` |
|   410508 | 1558 | `		 }` |
|  1927966 | 1559 | `	 }` |
|   912225 | 1560 | `	 return SXRET_OK;` |
|   787640 | 1561 | ` }` |
|        - | 1562 | ` /*` |
|        - | 1563 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1564 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1565 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1566 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1567 | `  */` |
|  1042138 | 1568 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        1 | 1569 |  |
|        - | 1570 | `	ph7_expr_node **apNode;` |
|        - | 1571 | `	ph7_expr_node *pNode;` |
|        - | 1572 | `	sxi32 rc;` |
|        - | 1573 | `	/* Reset node container */` |
|  1042139 | 1574 | `	SySetReset(pExprNode);` |
|  1042139 | 1575 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1576 | `	/* Extract nodes one after one until we hit the end of the input */` |
|  5607335 | 1577 | `	while( pGen->pIn < pGen->pEnd ){` |
|  4565231 | 1578 | `		rc = ExprExtractNode(&(*pGen),&pNode);` |
|  4565231 | 1579 | `		if( rc != SXRET_OK ){` |
|       35 | 1580 | `			return rc;` |
|        - | 1581 | `		}` |
|        - | 1582 | `		/* Save the extracted node */` |
|  4565197 | 1583 | `		SySetPut(pExprNode,(const void *)&pNode);` |
|        1 | 1584 | `	}` |
|  1042105 | 1585 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1586 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1587 | `		*ppRoot = 0;` |
|      ! 0 | 1588 | `		return SXRET_OK;` |
|        - | 1589 | `	}` |
|  1042105 | 1590 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1591 | `	/* Make sure we are dealing with valid nodes */` |
|  1042105 | 1592 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1042105 | 1593 | `	if( rc != SXRET_OK ){` |
|        - | 1594 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1595 | `		 * cleanup the mess left behind.` |
|        - | 1596 | `		 */` |
|       51 | 1597 | `		*ppRoot = 0;` |
|       51 | 1598 | `		return rc;` |
|        - | 1599 | `	}` |
|        - | 1600 | `	/* Build the tree */` |
|  1042055 | 1601 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1042055 | 1602 | `	if( rc != SXRET_OK ){` |
|        - | 1603 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       83 | 1604 | `		*ppRoot = 0;` |
|       83 | 1605 | `		return rc;` |
|        - | 1606 | `	}` |
|        - | 1607 | `	/* Point to the root of the tree */` |
|  1041973 | 1608 | `	*ppRoot = apNode[0];` |
|  1041973 | 1609 | `	return SXRET_OK;` |
|   521070 | 1610 |  |
|        - | 1611 |  |
