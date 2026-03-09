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
|   438930 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  255 |  |
|   438932 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
|  6782146 |  259 | `	for(;;){` |
| 13564294 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 13564294 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  1711946 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   855974 |  266 | `		}else{` |
| 11852350 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 13564294 |  269 | `		if( rc == 0 ){` |
|   439400 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   438652 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|      750 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      170 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|      582 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      122 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      122 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      114 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|      234 |  288 | `		}` |
| 13125364 |  289 | `		++n; /* Next operator in the table */` |
|        2 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   219467 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   187626 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  300 |  |
|   187628 |  301 | `	SyToken *pCur = pIn;` |
|   187628 |  302 | `	sxi32 iNest = 1;` |
|  1080086 |  303 | `	for(;;){` |
|  2160174 |  304 | `		if( pCur >= pEnd ){` |
|       90 |  305 | `			break;` |
|        - |  306 | `		}` |
|  2160086 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   122256 |  309 | `			iNest++;` |
|  2098959 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   309794 |  312 | `			iNest--;` |
|   309794 |  313 | `			if( iNest <= 0 ){` |
|   187540 |  314 | `				break;` |
|        - |  315 | `			}` |
|    61127 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  1972548 |  318 | `		pCur++;` |
|        2 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   187628 |  321 | `	*ppEnd = pCur;` |
|   187628 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    13594 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  333 |  |
|    20329 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    13505 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|      138 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    13460 |  339 | `	if( bCheckFunc ){` |
|     2248 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       33 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       21 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|     2238 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|        4 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    11224 |  347 | `	return FALSE;` |
|     6799 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|   362906 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|   362908 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        7 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        7 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        3 |  364 | `	}` |
|   362908 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  1959840 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  1596964 |  367 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   166948 |  368 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|     9364 |  369 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  370 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   152110 |  371 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  372 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  373 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  374 | `						 */` |
|   152110 |  375 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   152110 |  376 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   152110 |  377 | `						apNode[i]->pOp = &sFCallOp;` |
|    76054 |  378 | `					}` |
|    76054 |  379 | `			}` |
|   166948 |  380 | `			iParen++;` |
|  1513491 |  381 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   166944 |  382 | `			if( iParen <= 0 ){` |
|        9 |  383 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  384 | `				if( rc != SXERR_ABORT ){` |
|        9 |  385 | `					rc = SXERR_SYNTAX;` |
|        4 |  386 | `				}` |
|        9 |  387 | `				return rc;` |
|        - |  388 | `			}` |
|   166936 |  389 | `			iParen--;` |
|  1346543 |  390 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    41086 |  391 | `			iSquare++;` |
|  1242534 |  392 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    41100 |  393 | `			if( iSquare <= 0 ){` |
|        7 |  394 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  395 | `				if( rc != SXERR_ABORT ){` |
|        7 |  396 | `					rc = SXERR_SYNTAX;` |
|        3 |  397 | `				}` |
|        7 |  398 | `				return rc;` |
|        - |  399 | `			}` |
|    41094 |  400 | `			iSquare--;` |
|  1201440 |  401 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       14 |  402 | `			iBraces++;` |
|       14 |  403 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|        7 |  447 | `			}` |
|  1180888 |  448 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       15 |  449 | `			if( iBraces <= 0 ){` |
|       13 |  450 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  451 | `				if( rc != SXERR_ABORT ){` |
|       13 |  452 | `					rc = SXERR_SYNTAX;` |
|        6 |  453 | `				}` |
|       13 |  454 | `				return rc;` |
|        - |  455 | `			}` |
|        3 |  456 | `			iBraces--;` |
|  1180869 |  457 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1600 |  458 | `			if( iQuesty <= 0 ){` |
|        5 |  459 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  460 | `				if( rc != SXERR_ABORT ){` |
|        5 |  461 | `					rc = SXERR_SYNTAX;` |
|        2 |  462 | `				}` |
|        5 |  463 | `				return rc;` |
|        - |  464 | `			}` |
|     1596 |  465 | `			iQuesty--;` |
|  1180067 |  466 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   345116 |  467 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   345116 |  468 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1598 |  469 | `				iQuesty++;` |
|   344318 |  470 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      268 |  471 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      133 |  489 | `			}` |
|   172557 |  490 | `		}` |
|   798468 |  491 | `	}` |
|   362878 |  492 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  493 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  494 | `		if( rc != SXERR_ABORT ){` |
|       17 |  495 | `			rc = SXERR_SYNTAX;` |
|        8 |  496 | `		}` |
|       17 |  497 | `		return rc;` |
|        - |  498 | `	}` |
|   362862 |  499 | `	return SXRET_OK;` |
|   181455 |  500 |  |
|        - |  501 | `/*` |
|        - |  502 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  503 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  504 | ` */` |
|   290162 |  505 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  506 |  |
|   290164 |  507 | `	SyToken *pIn = *ppCur;` |
|        - |  508 | `	/* Jump the first literal seen */` |
|   290164 |  509 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   290162 |  510 | `		pIn++;` |
|   145080 |  511 | `	}` |
|   145086 |  512 | `	for(;;){` |
|   290174 |  513 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       11 |  514 | `			pIn++;` |
|       11 |  515 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       11 |  516 | `				pIn++;` |
|        5 |  517 | `			}` |
|        6 |  518 | `		}else{` |
|   145083 |  519 | `			break;` |
|        - |  520 | `		}` |
|        1 |  521 | `	}` |
|        - |  522 | `	/* Synchronize pointers */` |
|   290164 |  523 | `	*ppCur = pIn;` |
|   290164 |  524 |  |
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
|      106 |  558 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  559 |  |
|      108 |  560 | `	SyToken *pIn = *ppCur;` |
|        - |  561 | `	sxu32 nLine;` |
|        - |  562 | `	sxi32 rc;` |
|        - |  563 | `	/* Jump the 'function' keyword */` |
|      108 |  564 | `	nLine = pIn->nLine;` |
|      108 |  565 | `	pIn++;` |
|      108 |  566 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        5 |  567 | `		pIn++;` |
|        2 |  568 | `	}` |
|      108 |  569 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  570 | `		/* Syntax error */` |
|        5 |  571 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  572 | `		if( rc != SXERR_ABORT ){` |
|        5 |  573 | `			rc = SXERR_SYNTAX;` |
|        2 |  574 | `		}` |
|        5 |  575 | `		goto Synchronize;` |
|        - |  576 | `	}` |
|      104 |  577 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      104 |  578 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      104 |  579 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  580 | `		/* Syntax error */` |
|        5 |  581 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  582 | `		if( rc != SXERR_ABORT ){` |
|        5 |  583 | `			rc = SXERR_SYNTAX;` |
|        2 |  584 | `		}` |
|        5 |  585 | `		goto Synchronize;` |
|        - |  586 | `	}` |
|      100 |  587 | `	pIn++; /* Jump the trailing parenthesis */` |
|      100 |  588 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       26 |  589 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  590 | `		/* Check if we are dealing with a closure */` |
|       26 |  591 | `		if( nKey == PH7_TKWRD_USE ){` |
|       18 |  592 | `			pIn++; /* Jump the 'use' keyword */` |
|       18 |  593 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  594 | `				/* Syntax error */` |
|        5 |  595 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  596 | `				if( rc != SXERR_ABORT ){` |
|        5 |  597 | `					rc = SXERR_SYNTAX;` |
|        2 |  598 | `				}` |
|        5 |  599 | `				goto Synchronize;` |
|        - |  600 | `			}` |
|       14 |  601 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       14 |  602 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       14 |  603 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  604 | `				/* Syntax error */` |
|        5 |  605 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  606 | `				if( rc != SXERR_ABORT ){` |
|        5 |  607 | `					rc = SXERR_SYNTAX;` |
|        2 |  608 | `				}` |
|        5 |  609 | `				goto Synchronize;` |
|        - |  610 | `			}` |
|       10 |  611 | `			pIn++;` |
|        6 |  612 | `		}else{` |
|        - |  613 | `			/* Syntax error */` |
|        9 |  614 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  615 | `			if( rc != SXERR_ABORT ){` |
|        9 |  616 | `				rc = SXERR_SYNTAX;` |
|        4 |  617 | `			}` |
|        9 |  618 | `			goto Synchronize;` |
|        - |  619 | `		}` |
|        4 |  620 | `	}` |
|       84 |  621 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|       84 |  622 | `		pIn++; /* Jump the leading curly '{' */` |
|       84 |  623 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       84 |  624 | `		if( pIn < pEnd ){` |
|       84 |  625 | `			pIn++;` |
|       41 |  626 | `		}` |
|       43 |  627 | `	}else{` |
|        - |  628 | `		/* Syntax error */` |
|      ! 0 |  629 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  630 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  631 | `			return SXERR_ABORT;` |
|        - |  632 | `		}` |
|        - |  633 | `	}` |
|       84 |  634 | `	rc = SXRET_OK;` |
|       53 |  635 | `Synchronize:` |
|        - |  636 | `	/* Synchronize pointers */` |
|      108 |  637 | `	*ppCur = pIn;` |
|      108 |  638 | `	return rc;` |
|       55 |  639 |  |
|        - |  640 | `/*` |
|        - |  641 | ` * Extract a single expression node from the input.` |
|        - |  642 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  643 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  644 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  645 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  646 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  647 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  648 | ` */` |
|  1597108 |  649 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode)` |
|        2 |  650 |  |
|        - |  651 | `	ph7_expr_node *pNode;` |
|        - |  652 | `	SyToken *pCur;` |
|        - |  653 | `	sxi32 rc;` |
|        - |  654 | `	/* Allocate a new node */` |
|  1597110 |  655 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  1597110 |  656 | `	if( pNode == 0 ){` |
|        - |  657 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  658 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  659 | `		 */` |
|      ! 0 |  660 | `		return SXERR_MEM;` |
|        - |  661 | `	}` |
|        - |  662 | `	/* Zero the structure */` |
|  1597110 |  663 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  1597110 |  664 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  665 | `	/* Point to the head of the token stream */` |
|  1597110 |  666 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  667 | `	/* Start collecting tokens */` |
|  1597110 |  668 | `	if( pCur->nType & PH7_TK_OP ){` |
|        - |  669 | `		/* Point to the instance that describe this operator */` |
|   386238 |  670 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  671 | `		/* Advance the stream cursor */` |
|   386238 |  672 | `		pCur++;` |
|  1403992 |  673 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  674 | `		/* Isolate variable */` |
|   865558 |  675 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   432786 |  676 | `			pCur++; /* Variable variable */` |
|        2 |  677 | `		}` |
|   432774 |  678 | `		if( pCur < pGen->pEnd ){` |
|   432774 |  679 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  680 | `				/* Variable name */` |
|   432746 |  681 | `				pCur++;` |
|   216402 |  682 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  683 | `				pCur++;` |
|        - |  684 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  685 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  686 | `				if( pCur < pGen->pEnd ){` |
|       18 |  687 | `					pCur++;` |
|       10 |  688 | `				}else{` |
|        5 |  689 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  690 | `					if( rc != SXERR_ABORT ){` |
|        5 |  691 | `						rc = SXERR_SYNTAX;` |
|        2 |  692 | `					}` |
|        5 |  693 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  694 | `					return rc;` |
|        - |  695 | `				}` |
|        8 |  696 | `			}` |
|   216384 |  697 | `		}` |
|   432770 |  698 | `		pNode->xCode = PH7_CompileVariable;` |
|   994486 |  699 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    18072 |  700 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    18072 |  701 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  702 | `			 /* List/Array node */` |
|     6676 |  703 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  704 | `				 /* Assume a literal */` |
|       17 |  705 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  706 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  707 | `			 }else{` |
|     6660 |  708 | `				 pCur += 2;` |
|        - |  709 | `				 /* Collect array/list tokens */` |
|     6660 |  710 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     6660 |  711 | `				 if( pCur < pGen->pEnd ){` |
|     6658 |  712 | `					 pCur++;` |
|     3330 |  713 | `				 }else{` |
|        - |  714 | `					 /* Syntax error */` |
|        4 |  715 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  716 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  717 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  718 | `						 rc = SXERR_SYNTAX;` |
|        1 |  719 | `					 }` |
|        3 |  720 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  721 | `					 return rc;` |
|        - |  722 | `				 }` |
|     6658 |  723 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     6658 |  724 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  725 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  726 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  727 | `						 /* Syntax error */` |
|        3 |  728 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  729 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  730 | `							 rc = SXERR_SYNTAX;` |
|        1 |  731 | `						 }` |
|        3 |  732 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  733 | `						 return rc;` |
|        - |  734 | `					 }` |
|       12 |  735 | `				 }` |
|        2 |  736 | `			 }` |
|    14733 |  737 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  738 | `			 /* Annonymous function */` |
|      108 |  739 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  740 | `				 /* Assume a literal */` |
|      ! 0 |  741 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  742 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  743 | `			 }else{` |
|        - |  744 | `				 /* Assemble annonymous functions body */` |
|      108 |  745 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      108 |  746 | `				 if( rc != SXRET_OK ){` |
|       25 |  747 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  748 | `					 return rc;` |
|        - |  749 | `				 }` |
|       84 |  750 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  751 | `			  }` |
|    11333 |  752 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  753 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       74 |  754 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       74 |  755 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       38 |  756 | `		 }else{` |
|        - |  757 | `			 /* Assume a literal */` |
|    11220 |  758 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11220 |  759 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  760 | `		 }` |
|   769053 |  761 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  762 | `		 /* Constants,function name,namespace path,class name... */` |
|   278930 |  763 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   278930 |  764 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   139466 |  765 | `	 }else{` |
|   481104 |  766 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  767 | `			 /* Point to the code generator routine */` |
|   104478 |  768 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   104478 |  769 | `			 if( pNode->xCode == 0 ){` |
|        3 |  770 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  771 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  772 | `					 rc = SXERR_SYNTAX;` |
|        1 |  773 | `				 }` |
|        3 |  774 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  775 | `				 return rc;` |
|        - |  776 | `			 }` |
|    52237 |  777 | `		 }` |
|        - |  778 | `		/* Advance the stream cursor */` |
|   481102 |  779 | `		pCur++;` |
|        - |  780 | `	 }` |
|        - |  781 | `	/* Point to the end of the token stream */` |
|  1597076 |  782 | `	pNode->pEnd = pCur;` |
|        - |  783 | `	/* Save the node for later processing */` |
|  1597076 |  784 | `	*ppNode = pNode;` |
|        - |  785 | `	/* Synchronize cursors */` |
|  1597076 |  786 | `	pGen->pIn = pCur;` |
|  1597076 |  787 | `	return SXRET_OK;` |
|   798556 |  788 |  |
|        - |  789 | `/*` |
|        - |  790 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  791 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  792 | ` * level is zero.` |
|        - |  793 | ` */` |
|    27340 |  794 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  795 |  |
|    27342 |  796 | `	SyToken *pCur = pStart;` |
|    27342 |  797 | `	sxi32 iNest = 0;` |
|    27342 |  798 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  799 | `		/* Last expression */` |
|    15520 |  800 | `		return SXERR_EOF;` |
|        - |  801 | `	}` |
|    60818 |  802 | `	while( pCur < pEnd ){` |
|    59612 |  803 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    10618 |  804 | `			break;` |
|        - |  805 | `		}` |
|    48996 |  806 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     3750 |  807 | `			iNest++;` |
|    47122 |  808 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     3752 |  809 | `			iNest--;` |
|     1875 |  810 | `		}` |
|    48996 |  811 | `		pCur++;` |
|        2 |  812 | `	}` |
|    11824 |  813 | `	*ppNext = pCur;` |
|    11824 |  814 | `	return SXRET_OK;` |
|    13672 |  815 |  |
|        - |  816 | `/*` |
|        - |  817 | ` * Free an expression tree.` |
|        - |  818 | ` */` |
|  1387274 |  819 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  820 |  |
|  1387276 |  821 | `	if( pNode->pLeft ){` |
|        - |  822 | `		/* Release the left tree */` |
|   523188 |  823 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   261593 |  824 | `	}` |
|  1387276 |  825 | `	if( pNode->pRight ){` |
|        - |  826 | `		/* Release the right tree */` |
|   292138 |  827 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   146068 |  828 | `	}` |
|  1387276 |  829 | `	if( pNode->pCond ){` |
|        - |  830 | `		/* Release the conditional tree used by the ternary operator */` |
|     1594 |  831 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      796 |  832 | `	}` |
|  1387276 |  833 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  834 | `		ph7_expr_node **apArg;` |
|        - |  835 | `		sxu32 n;` |
|        - |  836 | `		/* Release node arguments */` |
|   162470 |  837 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   339934 |  838 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   177466 |  839 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    88734 |  840 | `		}` |
|   162470 |  841 | `		SySetRelease(&pNode->aNodeArgs);` |
|    81234 |  842 | `	}` |
|        - |  843 | `	/* Finally,release this node */` |
|  1387276 |  844 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  1387276 |  845 |  |
|        - |  846 | `/*` |
|        - |  847 | ` * Free an expression tree.` |
|        - |  848 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  849 | ` */` |
|   362940 |  850 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  851 |  |
|        - |  852 | `	ph7_expr_node **apNode;` |
|        - |  853 | `	sxu32 n;` |
|   362942 |  854 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  1960016 |  855 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  1597076 |  856 | `		if( apNode[n] ){` |
|   363246 |  857 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   181622 |  858 | `		}` |
|   798539 |  859 | `	}` |
|   362942 |  860 | `	return SXRET_OK;` |
|        2 |  861 |  |
|        - |  862 | `/*` |
|        - |  863 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  864 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  865 | ` */` |
|   127430 |  866 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  867 |  |
|        - |  868 | `	sxi32 iExprOp;` |
|   127432 |  869 | `	if( pNode->pOp == 0 ){` |
|    85546 |  870 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  871 | `	}` |
|    41888 |  872 | `	iExprOp = pNode->pOp->iOp;` |
|    41888 |  873 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    29478 |  874 | `			return TRUE;` |
|        - |  875 | `	}` |
|    12412 |  876 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    12408 |  877 | `		if( pNode->pLeft->pOp ) {` |
|        2 |  878 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  879 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  880 | `				return FALSE;` |
|        1 |  881 | `			}` |
|    12407 |  882 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  883 | `			return FALSE;` |
|        - |  884 | `		}` |
|    12408 |  885 | `		return TRUE;` |
|        - |  886 | `	}` |
|        5 |  887 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  888 | `		return TRUE;` |
|        - |  889 | `	}` |
|        - |  890 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  891 | `	return FALSE;` |
|    63717 |  892 |  |
|        - |  893 | `/* Forward declaration */` |
|        - |  894 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  895 | `/* Macro to check if the given node is a terminal */` |
|        - |  896 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft ))` |
|        - |  897 | `/*` |
|        - |  898 | ` * Buid an expression tree for each given function argument.` |
|        - |  899 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  900 | ` */` |
|   130078 |  901 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  902 |  |
|        - |  903 | `	sxi32 iNest,iCur,iNode;` |
|        - |  904 | `	sxi32 rc;` |
|        - |  905 | `	/* Process function arguments from left to right */` |
|   130080 |  906 | `	iCur = 0;` |
|   137575 |  907 | `	for(;;){` |
|   275152 |  908 | `		if( iCur >= nToken ){` |
|        - |  909 | `			/* No more arguments to process */` |
|   130078 |  910 | `			break;` |
|        - |  911 | `		}` |
|   145076 |  912 | `		iNode = iCur;` |
|   145076 |  913 | `		iNest = 0;` |
|   360356 |  914 | `		while( iCur < nToken ){` |
|   230280 |  915 | `			if( apNode[iCur] ){` |
|   230280 |  916 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|     7501 |  917 | `					break;` |
|   215282 |  918 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    12826 |  919 | `					iNest++;` |
|   208870 |  920 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    12826 |  921 | `					iNest--;` |
|     6412 |  922 | `				}` |
|   107640 |  923 | `			}` |
|   215282 |  924 | `			iCur++;` |
|        2 |  925 | `		}` |
|   145076 |  926 | `		if( iCur > iNode ){` |
|   145072 |  927 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  928 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  929 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  930 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  931 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  932 | `					apNode[iNode] = 0;` |
|      ! 0 |  933 | `			}` |
|   145074 |  934 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   145074 |  935 | `			if( apNode[iNode] ){` |
|        - |  936 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   145074 |  937 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    72538 |  938 | `			}else{` |
|        - |  939 | `				/* Empty function argument */` |
|      ! 0 |  940 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  941 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  942 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  943 | `				}` |
|      ! 0 |  944 | `				return rc;` |
|        - |  945 | `			}` |
|    72538 |  946 | `		}else{` |
|        3 |  947 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  948 | `			if( rc != SXERR_ABORT ){` |
|        3 |  949 | `				rc = SXERR_SYNTAX;` |
|        1 |  950 | `			}` |
|        3 |  951 | `			return rc;` |
|        - |  952 | `		}` |
|        - |  953 | `		/* Jump trailing comma */` |
|   145074 |  954 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    14998 |  955 | `			iCur++;` |
|    14998 |  956 | `			if( iCur >= nToken ){` |
|        - |  957 | `				/* missing function argument */` |
|      ! 0 |  958 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 |  959 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  960 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  961 | `				}` |
|      ! 0 |  962 | `				return rc;` |
|        - |  963 | `			}` |
|     7498 |  964 | `		}` |
|        2 |  965 | `	}` |
|   130078 |  966 | `	return SXRET_OK;` |
|    65041 |  967 |  |
|        - |  968 | ` /*` |
|        - |  969 | `  * Create an expression tree from an array of tokens.` |
|        - |  970 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - |  971 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  972 | `  */` |
|   560252 |  973 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  974 | ` {` |
|        - |  975 | `	 sxi32 i,iLeft,iRight;` |
|        - |  976 | `	 ph7_expr_node *pNode;` |
|        - |  977 | `	 sxi32 iCur;` |
|        - |  978 | `	 sxi32 rc;` |
|   560254 |  979 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - |  980 | `		 /* TICKET 1433-17: self evaluating node */` |
|   240762 |  981 | `		 return SXRET_OK;` |
|        - |  982 | `	 }` |
|        - |  983 | `	 /* Process expressions enclosed in parenthesis first */` |
|  1912220 |  984 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - |  985 | `		 sxi32 iNest;` |
|        - |  986 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - |  987 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - |  988 | `		  */` |
|  1592730 |  989 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  1577902 |  990 | `			 continue;` |
|        - |  991 | `		 }` |
|    14830 |  992 | `		 iNest = 1;` |
|    14830 |  993 | `		 iLeft = iCur;` |
|        - |  994 | `		 /* Find the closing parenthesis */` |
|    14830 |  995 | `		 iCur++;` |
|    85180 |  996 | `		 while( iCur < nToken ){` |
|    85180 |  997 | `			 if( apNode[iCur] ){` |
|    85180 |  998 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - |  999 | `					 /* Decrement nesting level */` |
|    23954 | 1000 | `					 iNest--;` |
|    23954 | 1001 | `					 if( iNest <= 0 ){` |
|    14830 | 1002 | `						 break;` |
|        2 | 1003 | `					 }` |
|    65790 | 1004 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1005 | `					 /* Increment nesting level */` |
|     9126 | 1006 | `					 iNest++;` |
|     4562 | 1007 | `				 }` |
|    35175 | 1008 | `			 }` |
|    70352 | 1009 | `			 iCur++;` |
|        2 | 1010 | `		 }` |
|    14830 | 1011 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1012 | `			 /* Recurse and process this expression */` |
|    14830 | 1013 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    14830 | 1014 | `			 if( rc != SXRET_OK ){` |
|        3 | 1015 | `				 return rc;` |
|        - | 1016 | `			 }` |
|     7413 | 1017 | `		 }` |
|        - | 1018 | `		 /* Free the left and right nodes */` |
|    14828 | 1019 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    14828 | 1020 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    14828 | 1021 | `		 apNode[iLeft] = 0;` |
|    14828 | 1022 | `		 apNode[iCur] = 0;` |
|     7415 | 1023 | `	 }` |
|        - | 1024 | `	  /* Process expressions enclosed in braces */` |
|  1993550 | 1025 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1026 | `		 sxi32 iNest;` |
|        - | 1027 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1028 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1029 | `		  */` |
|  1677890 | 1030 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  1677888 | 1031 | `			 continue;` |
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
|   315662 | 1066 | `	 iLeft = -1;` |
|  1993522 | 1067 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  1677872 | 1068 | `		 if( apNode[iCur] == 0 ){` |
|   607790 | 1069 | `			 continue;` |
|        - | 1070 | `		 }` |
|  1070084 | 1071 | `		 pNode = apNode[iCur];` |
|  1070084 | 1072 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   259346 | 1073 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1074 | `				 /* Collect function arguments */` |
|   152106 | 1075 | `				 sxi32 iPtr = 0;` |
|   152106 | 1076 | `				 sxi32 nFuncTok = 0;` |
|   534490 | 1077 | `				 while( nFuncTok + iCur < nToken ){` |
|   534490 | 1078 | `					 if( apNode[nFuncTok+iCur] ){` |
|   534490 | 1079 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   155958 | 1080 | `							 iPtr++;` |
|   456512 | 1081 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   155958 | 1082 | `							 iPtr--;` |
|   155958 | 1083 | `							 if( iPtr <= 0 ){` |
|   152106 | 1084 | `								 break;` |
|        - | 1085 | `							 }` |
|     1926 | 1086 | `						 }` |
|   191192 | 1087 | `					 }` |
|   382386 | 1088 | `					 nFuncTok++;` |
|        2 | 1089 | `				 }` |
|   152106 | 1090 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1091 | `					 /* Syntax error */` |
|      ! 0 | 1092 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1093 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1094 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1095 | `					 }` |
|      ! 0 | 1096 | `					 return rc;` |
|        - | 1097 | `				 }` |
|   152106 | 1098 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1099 | `					 /* Syntax error */` |
|      ! 0 | 1100 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1101 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1102 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1103 | `					 }` |
|      ! 0 | 1104 | `					 return rc;` |
|        - | 1105 | `				 }` |
|   152106 | 1106 | `				 if( nFuncTok > 1 ){` |
|        - | 1107 | `					 /* Process function arguments */` |
|   130080 | 1108 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   130080 | 1109 | `					 if( rc != SXRET_OK ){` |
|        3 | 1110 | `						 return rc;` |
|        - | 1111 | `					 }` |
|    65038 | 1112 | `				 }` |
|        - | 1113 | `				 /* Link the node to the tree */` |
|   152104 | 1114 | `				 pNode->pLeft = apNode[iLeft];` |
|   152104 | 1115 | `				 apNode[iLeft] = 0;` |
|   534482 | 1116 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   382380 | 1117 | `					 apNode[iCur+iPtr] = 0;` |
|   191191 | 1118 | `				 }` |
|   183293 | 1119 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1120 | `				 /* Subscripting */` |
|    41094 | 1121 | `				 sxi32 iArrTok = iCur + 1;` |
|    41094 | 1122 | `				 sxi32 iNest = 1;` |
|    41111 | 1123 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        2 | 1124 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString ) ) \|\|` |
|    41092 | 1125 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1126 | `						 /* Syntax error */` |
|        5 | 1127 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        5 | 1128 | `						 if( rc != SXERR_ABORT ){` |
|        5 | 1129 | `							 rc = SXERR_SYNTAX;` |
|        2 | 1130 | `						 }` |
|        5 | 1131 | `						 return rc;` |
|        - | 1132 | `				 }` |
|        - | 1133 | `				 /* Collect index tokens */` |
|    73588 | 1134 | `				 while( iArrTok < nToken ){` |
|    73588 | 1135 | `					 if( apNode[iArrTok] ){` |
|    73556 | 1136 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1137 | `							 /* Increment nesting level */` |
|      ! 0 | 1138 | `							 iNest++;` |
|    73556 | 1139 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1140 | `							 /* Decrement nesting level */` |
|    41090 | 1141 | `							 iNest--;` |
|    41090 | 1142 | `							 if( iNest <= 0 ){` |
|    41090 | 1143 | `								 break;` |
|        - | 1144 | `							 }` |
|      ! 0 | 1145 | `						 }` |
|    16233 | 1146 | `					 }` |
|    32500 | 1147 | `					 ++iArrTok;` |
|        2 | 1148 | `				 }` |
|    41090 | 1149 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1150 | `					 /* Recurse and process this expression */` |
|    32394 | 1151 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    32394 | 1152 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1153 | `						 return rc;` |
|        - | 1154 | `					 }` |
|        - | 1155 | `					 /* Link the node to it's index */` |
|    32394 | 1156 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    16196 | 1157 | `				 }` |
|        - | 1158 | `				 /* Link the node to the tree */` |
|    41090 | 1159 | `				 pNode->pLeft = apNode[iLeft];` |
|    41090 | 1160 | `				 pNode->pRight = 0;` |
|    41090 | 1161 | `				 apNode[iLeft] = 0;` |
|   114676 | 1162 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    73588 | 1163 | `					 apNode[iNest] = 0;` |
|    36795 | 1164 | `				 }` |
|    20546 | 1165 | `			 }else{` |
|        - | 1166 | `				 /* Member access operators [i.e: '->','::'] */` |
|    66150 | 1167 | `				  iRight = iCur + 1;` |
|    66150 | 1168 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1169 | `					 iRight++;` |
|      ! 0 | 1170 | `				 }` |
|    66150 | 1171 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1172 | `					 /* Syntax error */` |
|        5 | 1173 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1174 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1175 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1176 | `					 }` |
|        5 | 1177 | `					 return rc;` |
|        - | 1178 | `				 }` |
|        - | 1179 | `				 /* Link the node to the tree */` |
|    66146 | 1180 | `				 pNode->pLeft = apNode[iLeft];` |
|    66146 | 1181 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|    66086 | 1182 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1183 | `						 /* Syntax error */` |
|      ! 0 | 1184 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1185 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1186 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1187 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1188 | `						 }` |
|      ! 0 | 1189 | `						 return rc;` |
|        - | 1190 | `				 }` |
|    66146 | 1191 | `				 pNode->pRight = apNode[iRight];` |
|    66146 | 1192 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1193 | `			 }` |
|   129667 | 1194 | `		 }` |
|  1070074 | 1195 | `		 iLeft = iCur;` |
|   535038 | 1196 | `	 }` |
|        - | 1197 | `	 /* Handle left associative (new, clone) operators */` |
|  1993490 | 1198 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  1677840 | 1199 | `		 if( apNode[iCur] == 0 ){` |
|   874290 | 1200 | `			 continue;` |
|        - | 1201 | `		 }` |
|   803552 | 1202 | `		 pNode = apNode[iCur];` |
|   803552 | 1203 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1204 | `			 SyToken *pToken;` |
|        - | 1205 | `			 /* Get the left node */` |
|     7168 | 1206 | `			 iLeft = iCur + 1;` |
|    14312 | 1207 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|     7146 | 1208 | `				 iLeft++;` |
|        2 | 1209 | `			 }` |
|     7168 | 1210 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1211 | `				  /* Syntax error */` |
|      ! 0 | 1212 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1213 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1214 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1215 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1216 | `				 }` |
|      ! 0 | 1217 | `				 return rc;` |
|        - | 1218 | `			 }` |
|        - | 1219 | `			 /* Make sure the operand are of a valid type */` |
|     7168 | 1220 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
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
|       18 | 1231 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1232 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1233 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1234 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1235 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1236 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1237 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1238 | `						 }` |
|      ! 0 | 1239 | `						 return rc;` |
|        - | 1240 | `					 }` |
|        7 | 1241 | `				 }` |
|       10 | 1242 | `			 }else{` |
|        - | 1243 | `				 /* New */` |
|     7152 | 1244 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       10 | 1245 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       10 | 1246 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
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
|     7168 | 1260 | `			 pNode->pLeft = apNode[iLeft];` |
|     7168 | 1261 | `			 apNode[iLeft] = 0;` |
|     7168 | 1262 | `			 pNode->pRight = 0; /* Paranoid */` |
|     3583 | 1263 | `		 }` |
|   401777 | 1264 | `	 }` |
|        - | 1265 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   315652 | 1266 | `	 iLeft = -1;` |
|  1993490 | 1267 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  1675926 | 1268 | `		 if( apNode[iCur] == 0 ){` |
|   874290 | 1269 | `			 continue;` |
|        - | 1270 | `		 }` |
|   801638 | 1271 | `		 pNode = apNode[iCur];` |
|   801638 | 1272 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     5204 | 1273 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     1926 | 1274 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1275 | `					 /* Link the node to the tree */` |
|     1922 | 1276 | `					 pNode->pLeft = apNode[iLeft];` |
|     1922 | 1277 | `					 apNode[iLeft] = 0;` |
|      960 | 1278 | `			 }` |
|     3558 | 1279 | `		  }` |
|   803552 | 1280 | `		 iLeft = iCur;` |
|   401777 | 1281 | `	  }` |
|   317566 | 1282 | `	 iLeft = -1;` |
|  1995404 | 1283 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  1677840 | 1284 | `		 if( apNode[iCur] == 0 ){` |
|   876210 | 1285 | `			 continue;` |
|        - | 1286 | `		 }` |
|   801632 | 1287 | `		 pNode = apNode[iCur];` |
|   801632 | 1288 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     5196 | 1289 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     5198 | 1290 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1291 | `					 /* Syntax error */` |
|      ! 0 | 1292 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1293 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1294 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1295 | `					 }` |
|      ! 0 | 1296 | `					 return rc;` |
|        - | 1297 | `			 }` |
|        - | 1298 | `			 /* Link the node to the tree */` |
|     5198 | 1299 | `			 pNode->pLeft = apNode[iLeft];` |
|     5198 | 1300 | `			 apNode[iLeft] = 0;` |
|        - | 1301 | `			 /* Mark as pre-increment/decrement node */` |
|     5198 | 1302 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     2598 | 1303 | `		  }` |
|   801632 | 1304 | `		 iLeft = iCur;` |
|   400817 | 1305 | `	 }` |
|        - | 1306 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   317566 | 1307 | `	  iLeft = 0;` |
|  1995398 | 1308 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  1677836 | 1309 | `		  if( apNode[iCur] ){` |
|   796432 | 1310 | `			  pNode = apNode[iCur];` |
|   796432 | 1311 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    23582 | 1312 | `				  if( iLeft > 0 ){` |
|        - | 1313 | `					  /* Link the node to the tree */` |
|    23580 | 1314 | `					  pNode->pLeft = apNode[iLeft];` |
|    23580 | 1315 | `					  apNode[iLeft] = 0;` |
|    23580 | 1316 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|        5 | 1317 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1318 | `							   /* Syntax error */` |
|      ! 0 | 1319 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1320 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1321 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1322 | `							  }` |
|      ! 0 | 1323 | `							  return rc;` |
|        - | 1324 | `						  }` |
|        2 | 1325 | `					  }` |
|    11791 | 1326 | `				  }else{` |
|        - | 1327 | `					  /* Syntax error */` |
|        3 | 1328 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1329 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1330 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1331 | `					  }` |
|        3 | 1332 | `					  return rc;` |
|        - | 1333 | `				  }` |
|    11789 | 1334 | `			  }` |
|        - | 1335 | `			  /* Save terminal position */` |
|   796430 | 1336 | `			  iLeft = iCur;` |
|   398214 | 1337 | `		  }` |
|   838918 | 1338 | `	  }` |
|        - | 1339 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  3492928 | 1340 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  3175394 | 1341 | `		 iLeft = -1;` |
| 19953128 | 1342 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 16777764 | 1343 | `			 if( apNode[iCur] == 0 ){` |
| 10319800 | 1344 | `				 continue;` |
|        - | 1345 | `			 }` |
|  6457966 | 1346 | `			 pNode = apNode[iCur];` |
|  6457966 | 1347 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1348 | `				 /* Get the right node */` |
|    97000 | 1349 | `				 iRight = iCur + 1;` |
|   130638 | 1350 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    33640 | 1351 | `					 iRight++;` |
|        2 | 1352 | `				 }` |
|    97000 | 1353 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1354 | `					 /* Syntax error */` |
|       29 | 1355 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       29 | 1356 | `					 if( rc != SXERR_ABORT ){` |
|       29 | 1357 | `						 rc = SXERR_SYNTAX;` |
|       14 | 1358 | `					 }` |
|       29 | 1359 | `					 return rc;` |
|        - | 1360 | `				 }` |
|    96972 | 1361 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1362 | `					 sxi32  iTmp;` |
|        - | 1363 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       42 | 1364 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1365 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1366 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1367 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1368 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1369 | `						 }` |
|      ! 0 | 1370 | `						 return rc;` |
|        - | 1371 | `					 }` |
|       42 | 1372 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       30 | 1373 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
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
|       14 | 1384 | `					 }` |
|        - | 1385 | `					 /* Swap operands */` |
|       42 | 1386 | `					 iTmp = iRight;` |
|       42 | 1387 | `					 iRight = iLeft;` |
|       42 | 1388 | `					 iLeft = iTmp;` |
|       20 | 1389 | `				 }` |
|        - | 1390 | `				 /* Link the node to the tree */` |
|    96972 | 1391 | `				 pNode->pLeft = apNode[iLeft];` |
|    96972 | 1392 | `				 pNode->pRight = apNode[iRight];` |
|    96972 | 1393 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    48485 | 1394 | `			 }` |
|  6457938 | 1395 | `			 iLeft = iCur;` |
|  3228970 | 1396 | `		 }` |
|  1587684 | 1397 | `	 }` |
|        - | 1398 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1399 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1400 | `	  * we are dealing with a single operator.` |
|        - | 1401 | `	  */` |
|   317536 | 1402 | `	  iLeft = -1;` |
|  1988416 | 1403 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  1672476 | 1404 | `		  if( apNode[iCur] == 0 ){` |
|  1098416 | 1405 | `			  continue;` |
|        - | 1406 | `		  }` |
|   574062 | 1407 | `		  pNode = apNode[iCur];` |
|   574062 | 1408 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1596 | 1409 | `			  sxi32 iNest = 1;` |
|     1596 | 1410 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1411 | `				  /* Missing condition */` |
|        3 | 1412 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1413 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1414 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1415 | `				  }` |
|        3 | 1416 | `				  return rc;` |
|        - | 1417 | `			  }` |
|        - | 1418 | `			  /* Get the right node */` |
|     1594 | 1419 | `			  iRight = iCur + 1;` |
|     3454 | 1420 | `			  while( iRight < nToken  ){` |
|     3454 | 1421 | `				  if( apNode[iRight] ){` |
|     3186 | 1422 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1423 | `						  /* Increment nesting level */` |
|      ! 0 | 1424 | `						  ++iNest;` |
|     3186 | 1425 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1426 | `						  /* Decrement nesting level */` |
|     1594 | 1427 | `						  --iNest;` |
|     1594 | 1428 | `						  if( iNest <= 0 ){` |
|     1594 | 1429 | `							  break;` |
|        - | 1430 | `						  }` |
|      ! 0 | 1431 | `					  }` |
|      796 | 1432 | `				  }` |
|     1862 | 1433 | `				  iRight++;` |
|        2 | 1434 | `			  }` |
|     1594 | 1435 | `			  if( iRight > iCur + 1 ){` |
|        - | 1436 | `				  /* Recurse and process the then expression */` |
|     1594 | 1437 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1594 | 1438 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1439 | `					  return rc;` |
|        - | 1440 | `				  }` |
|        - | 1441 | `				  /* Link the node to the tree */` |
|     1594 | 1442 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      798 | 1443 | `			  }else{` |
|      ! 0 | 1444 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'then' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1445 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1446 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1447 | `				 }` |
|      ! 0 | 1448 | `				 return rc;` |
|        - | 1449 | `			  }` |
|     1594 | 1450 | `			  apNode[iCur + 1] = 0;` |
|     1594 | 1451 | `			  if( iRight + 1 < nToken ){` |
|        - | 1452 | `				  /* Recurse and process the else expression */` |
|     1594 | 1453 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1594 | 1454 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1455 | `					  return rc;` |
|        - | 1456 | `				  }` |
|        - | 1457 | `				  /* Link the node to the tree */` |
|     1594 | 1458 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1594 | 1459 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      798 | 1460 | `			  }else{` |
|      ! 0 | 1461 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1462 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1463 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1464 | `				 }` |
|      ! 0 | 1465 | `				 return rc;` |
|        - | 1466 | `			  }` |
|        - | 1467 | `			  /* Point to the condition */` |
|     1594 | 1468 | `			  pNode->pCond  = apNode[iLeft];` |
|     1594 | 1469 | `			  apNode[iLeft] = 0;` |
|     1594 | 1470 | `			  break;` |
|        - | 1471 | `		  }` |
|   572468 | 1472 | `		  iLeft = iCur;` |
|   286235 | 1473 | `	  }` |
|        - | 1474 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1475 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1476 | `	  * so there is no need for a precedence loop here.` |
|        - | 1477 | `	  */` |
|   317534 | 1478 | `	 iRight = -1;` |
|  1995204 | 1479 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  1677712 | 1480 | `		 if( apNode[iCur] == 0 ){` |
|  1232652 | 1481 | `			 continue;` |
|        - | 1482 | `		 }` |
|   445062 | 1483 | `		 pNode = apNode[iCur];` |
|   445062 | 1484 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1485 | `			 /* Get the left node */` |
|   127402 | 1486 | `			 iLeft = iCur - 1;` |
|   173056 | 1487 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    45656 | 1488 | `				 iLeft--;` |
|        2 | 1489 | `			 }` |
|   127402 | 1490 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1491 | `				 /* Syntax error */` |
|       39 | 1492 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1493 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1494 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1495 | `				 }` |
|       39 | 1496 | `				 return rc;` |
|        - | 1497 | `			 }` |
|   127364 | 1498 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       28 | 1499 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
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
|   127362 | 1510 | `			 pNode->pLeft = apNode[iRight];` |
|   127362 | 1511 | `			 pNode->pRight = apNode[iLeft];` |
|   127362 | 1512 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    63680 | 1513 | `		 }` |
|   445022 | 1514 | `		 iRight = iCur;` |
|   222512 | 1515 | `	 }` |
|        - | 1516 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  1587462 | 1517 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  1269970 | 1518 | `		 iLeft = -1;` |
|  7980642 | 1519 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  6710674 | 1520 | `			 if( apNode[iCur] == 0 ){` |
|  5440284 | 1521 | `				 continue;` |
|        - | 1522 | `			 }` |
|  1270392 | 1523 | `			 pNode = apNode[iCur];` |
|  1270392 | 1524 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1525 | `				 /* Get the right node */` |
|       72 | 1526 | `				 iRight = iCur + 1;` |
|      110 | 1527 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1528 | `					 iRight++;` |
|        2 | 1529 | `				 }` |
|       72 | 1530 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1531 | `					 /* Syntax error */` |
|      ! 0 | 1532 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1533 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1534 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1535 | `					 }` |
|      ! 0 | 1536 | `					 return rc;` |
|        - | 1537 | `				 }` |
|        - | 1538 | `				 /* Link the node to the tree */` |
|       72 | 1539 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1540 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1541 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1542 | `			 }` |
|  1270392 | 1543 | `			 iLeft = iCur;` |
|   635197 | 1544 | `		 }` |
|   634986 | 1545 | `	 }` |
|        - | 1546 | `	 /* Point to the root of the expression tree */` |
|  1677638 | 1547 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  1360168 | 1548 | `		 if( apNode[iCur] ){` |
|   282264 | 1549 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       24 | 1550 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       24 | 1551 | `				  if( rc != SXERR_ABORT ){` |
|       24 | 1552 | `					  rc = SXERR_SYNTAX;` |
|       11 | 1553 | `				  }` |
|       24 | 1554 | `				  return rc;` |
|        - | 1555 | `			 }` |
|   282242 | 1556 | `			 apNode[0] = apNode[iCur];` |
|   282242 | 1557 | `			 apNode[iCur] = 0;` |
|   141120 | 1558 | `		 }` |
|   680074 | 1559 | `	 }` |
|   317472 | 1560 | `	 return SXRET_OK;` |
|   279171 | 1561 | ` }` |
|        - | 1562 | ` /*` |
|        - | 1563 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1564 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1565 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1566 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1567 | `  */` |
|   362940 | 1568 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1569 |  |
|        - | 1570 | `	ph7_expr_node **apNode;` |
|        - | 1571 | `	ph7_expr_node *pNode;` |
|        - | 1572 | `	sxi32 rc;` |
|        - | 1573 | `	/* Reset node container */` |
|   362942 | 1574 | `	SySetReset(pExprNode);` |
|   362942 | 1575 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1576 | `	/* Extract nodes one after one until we hit the end of the input */` |
|  1960016 | 1577 | `	while( pGen->pIn < pGen->pEnd ){` |
|  1597110 | 1578 | `		rc = ExprExtractNode(&(*pGen),&pNode);` |
|  1597110 | 1579 | `		if( rc != SXRET_OK ){` |
|       35 | 1580 | `			return rc;` |
|        - | 1581 | `		}` |
|        - | 1582 | `		/* Save the extracted node */` |
|  1597076 | 1583 | `		SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1584 | `	}` |
|   362908 | 1585 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1586 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1587 | `		*ppRoot = 0;` |
|      ! 0 | 1588 | `		return SXRET_OK;` |
|        - | 1589 | `	}` |
|   362908 | 1590 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1591 | `	/* Make sure we are dealing with valid nodes */` |
|   362908 | 1592 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   362908 | 1593 | `	if( rc != SXRET_OK ){` |
|        - | 1594 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1595 | `		 * cleanup the mess left behind.` |
|        - | 1596 | `		 */` |
|       47 | 1597 | `		*ppRoot = 0;` |
|       47 | 1598 | `		return rc;` |
|        - | 1599 | `	}` |
|        - | 1600 | `	/* Build the tree */` |
|   362862 | 1601 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   362862 | 1602 | `	if( rc != SXRET_OK ){` |
|        - | 1603 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       84 | 1604 | `		*ppRoot = 0;` |
|       84 | 1605 | `		return rc;` |
|        - | 1606 | `	}` |
|        - | 1607 | `	/* Point to the root of the tree */` |
|   362780 | 1608 | `	*ppRoot = apNode[0];` |
|   362780 | 1609 | `	return SXRET_OK;` |
|   181472 | 1610 |  |
|        - | 1611 |  |
