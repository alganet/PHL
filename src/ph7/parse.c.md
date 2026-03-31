# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 814/962 lines (84.62%)

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
|   680368 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  255 |  |
|   680370 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
| 10529173 |  259 | `	for(;;){` |
| 21058348 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 21058348 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2654162 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1327082 |  266 | `		}else{` |
| 18404188 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 21058348 |  269 | `		if( rc == 0 ){` |
|   683414 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   680052 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|     3364 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      210 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|     3156 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|     1522 |  288 | `		}` |
| 20377980 |  289 | `		++n; /* Next operator in the table */` |
|        2 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   340186 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   300298 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  300 |  |
|   300300 |  301 | `	SyToken *pCur = pIn;` |
|   300300 |  302 | `	sxi32 iNest = 1;` |
|  1611800 |  303 | `	for(;;){` |
|  3223602 |  304 | `		if( pCur >= pEnd ){` |
|       90 |  305 | `			break;` |
|        - |  306 | `		}` |
|  3223514 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   179036 |  309 | `			iNest++;` |
|  3133997 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   479246 |  312 | `			iNest--;` |
|   479246 |  313 | `			if( iNest <= 0 ){` |
|   300212 |  314 | `				break;` |
|        - |  315 | `			}` |
|    89517 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  2923304 |  318 | `		pCur++;` |
|        2 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   300300 |  321 | `	*ppEnd = pCur;` |
|   300300 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    10786 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  333 |  |
|    16117 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    10697 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|      138 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    10652 |  339 | `	if( bCheckFunc ){` |
|       44 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       37 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       25 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       28 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|        6 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    10626 |  347 | `	return FALSE;` |
|     5395 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|   561520 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|   561522 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       10 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       10 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        4 |  364 | `	}` |
|   561522 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3051778 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2490288 |  367 | `		if( apNode[i]->xCode == PH7_CompileShortArray ){` |
|        - |  368 | `			/* Short array literal: brackets are self-contained, skip */` |
|      150 |  369 | `			continue;` |
|        - |  370 | `		}` |
|  2490140 |  371 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   261706 |  372 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    16014 |  373 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  374 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   240280 |  375 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  376 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  377 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  378 | `						 */` |
|   240280 |  379 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   240280 |  380 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   240280 |  381 | `						apNode[i]->pOp = &sFCallOp;` |
|   120139 |  382 | `					}` |
|   120139 |  383 | `			}` |
|   261706 |  384 | `			iParen++;` |
|  2359288 |  385 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   261702 |  386 | `			if( iParen <= 0 ){` |
|        9 |  387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  388 | `				if( rc != SXERR_ABORT ){` |
|        9 |  389 | `					rc = SXERR_SYNTAX;` |
|        4 |  390 | `				}` |
|        9 |  391 | `				return rc;` |
|        - |  392 | `			}` |
|   261694 |  393 | `			iParen--;` |
|  2097582 |  394 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    65392 |  395 | `			iSquare++;` |
|  1934041 |  396 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    65406 |  397 | `			if( iSquare <= 0 ){` |
|        7 |  398 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  399 | `				if( rc != SXERR_ABORT ){` |
|        7 |  400 | `					rc = SXERR_SYNTAX;` |
|        3 |  401 | `				}` |
|        7 |  402 | `				return rc;` |
|        - |  403 | `			}` |
|    65400 |  404 | `			iSquare--;` |
|  1868641 |  405 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  1835937 |  452 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  453 | `			if( iBraces <= 0 ){` |
|       13 |  454 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  455 | `				if( rc != SXERR_ABORT ){` |
|       13 |  456 | `					rc = SXERR_SYNTAX;` |
|        6 |  457 | `				}` |
|       13 |  458 | `				return rc;` |
|        - |  459 | `			}` |
|      ! 0 |  460 | `			iBraces--;` |
|  1835920 |  461 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1760 |  462 | `			if( iQuesty <= 0 ){` |
|        5 |  463 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  464 | `				if( rc != SXERR_ABORT ){` |
|        5 |  465 | `					rc = SXERR_SYNTAX;` |
|        2 |  466 | `				}` |
|        5 |  467 | `				return rc;` |
|        - |  468 | `			}` |
|     1756 |  469 | `			iQuesty--;` |
|  1835039 |  470 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   534134 |  471 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   534134 |  472 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1758 |  473 | `				iQuesty++;` |
|   533256 |  474 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   267066 |  494 | `		}` |
|  1245056 |  495 | `	}` |
|   561492 |  496 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  497 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  498 | `		if( rc != SXERR_ABORT ){` |
|       17 |  499 | `			rc = SXERR_SYNTAX;` |
|        8 |  500 | `		}` |
|       17 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|   561476 |  503 | `	return SXRET_OK;` |
|   280762 |  504 |  |
|        - |  505 | `/*` |
|        - |  506 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  507 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  508 | ` */` |
|   440086 |  509 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  510 |  |
|   440088 |  511 | `	SyToken *pIn = *ppCur;` |
|        - |  512 | `	/* Jump the first literal seen */` |
|   440088 |  513 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   440070 |  514 | `		pIn++;` |
|   220034 |  515 | `	}` |
|   220067 |  516 | `	for(;;){` |
|   440136 |  517 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       49 |  518 | `			pIn++;` |
|       49 |  519 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       49 |  520 | `				pIn++;` |
|       24 |  521 | `			}` |
|       25 |  522 | `		}else{` |
|   220045 |  523 | `			break;` |
|        - |  524 | `		}` |
|        1 |  525 | `	}` |
|        - |  526 | `	/* Synchronize pointers */` |
|   440088 |  527 | `	*ppCur = pIn;` |
|   440088 |  528 |  |
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
|      158 |  562 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  563 |  |
|      160 |  564 | `	SyToken *pIn = *ppCur;` |
|        - |  565 | `	sxu32 nLine;` |
|        - |  566 | `	sxi32 rc;` |
|        - |  567 | `	/* Jump the 'function' keyword */` |
|      160 |  568 | `	nLine = pIn->nLine;` |
|      160 |  569 | `	pIn++;` |
|      160 |  570 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  571 | `		pIn++;` |
|        1 |  572 | `	}` |
|      160 |  573 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  574 | `		/* Syntax error */` |
|        5 |  575 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  576 | `		if( rc != SXERR_ABORT ){` |
|        5 |  577 | `			rc = SXERR_SYNTAX;` |
|        2 |  578 | `		}` |
|        5 |  579 | `		goto Synchronize;` |
|        - |  580 | `	}` |
|      156 |  581 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      156 |  582 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      156 |  583 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  584 | `		/* Syntax error */` |
|        5 |  585 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  586 | `		if( rc != SXERR_ABORT ){` |
|        5 |  587 | `			rc = SXERR_SYNTAX;` |
|        2 |  588 | `		}` |
|        5 |  589 | `		goto Synchronize;` |
|        - |  590 | `	}` |
|      152 |  591 | `	pIn++; /* Jump the trailing parenthesis */` |
|      152 |  592 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       28 |  593 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  594 | `		/* Check if we are dealing with a closure */` |
|       28 |  595 | `		if( nKey == PH7_TKWRD_USE ){` |
|       20 |  596 | `			pIn++; /* Jump the 'use' keyword */` |
|       20 |  597 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  598 | `				/* Syntax error */` |
|        5 |  599 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  600 | `				if( rc != SXERR_ABORT ){` |
|        5 |  601 | `					rc = SXERR_SYNTAX;` |
|        2 |  602 | `				}` |
|        5 |  603 | `				goto Synchronize;` |
|        - |  604 | `			}` |
|       16 |  605 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       16 |  606 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       16 |  607 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  608 | `				/* Syntax error */` |
|        5 |  609 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  610 | `				if( rc != SXERR_ABORT ){` |
|        5 |  611 | `					rc = SXERR_SYNTAX;` |
|        2 |  612 | `				}` |
|        5 |  613 | `				goto Synchronize;` |
|        - |  614 | `			}` |
|       12 |  615 | `			pIn++;` |
|        7 |  616 | `		}else{` |
|        - |  617 | `			/* Syntax error */` |
|        9 |  618 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  619 | `			if( rc != SXERR_ABORT ){` |
|        9 |  620 | `				rc = SXERR_SYNTAX;` |
|        4 |  621 | `			}` |
|        9 |  622 | `			goto Synchronize;` |
|        - |  623 | `		}` |
|        5 |  624 | `	}` |
|      136 |  625 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      136 |  626 | `		pIn++; /* Jump the leading curly '{' */` |
|      136 |  627 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      136 |  628 | `		if( pIn < pEnd ){` |
|      136 |  629 | `			pIn++;` |
|       67 |  630 | `		}` |
|       69 |  631 | `	}else{` |
|        - |  632 | `		/* Syntax error */` |
|      ! 0 |  633 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  634 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  635 | `			return SXERR_ABORT;` |
|        - |  636 | `		}` |
|        - |  637 | `	}` |
|      136 |  638 | `	rc = SXRET_OK;` |
|       79 |  639 | `Synchronize:` |
|        - |  640 | `	/* Synchronize pointers */` |
|      160 |  641 | `	*ppCur = pIn;` |
|      160 |  642 | `	return rc;` |
|       81 |  643 |  |
|        - |  644 | `/*` |
|        - |  645 | ` * Extract a single expression node from the input.` |
|        - |  646 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  647 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  648 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  649 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  650 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  651 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  652 | ` */` |
|  2490424 |  653 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  654 |  |
|        - |  655 | `	ph7_expr_node *pNode;` |
|        - |  656 | `	SyToken *pCur;` |
|        - |  657 | `	sxi32 rc;` |
|        - |  658 | `	/* Allocate a new node */` |
|  2490426 |  659 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2490426 |  660 | `	if( pNode == 0 ){` |
|        - |  661 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  662 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  663 | `		 */` |
|      ! 0 |  664 | `		return SXERR_MEM;` |
|        - |  665 | `	}` |
|        - |  666 | `	/* Zero the structure */` |
|  2490426 |  667 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2490426 |  668 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  669 | `	/* Point to the head of the token stream */` |
|  2490426 |  670 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  671 | `	/* Start collecting tokens */` |
|  2490426 |  672 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
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
|  2490351 |  690 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  691 | `		/* Point to the instance that describe this operator */` |
|   599558 |  692 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  693 | `		/* Advance the stream cursor */` |
|   599558 |  694 | `		pCur++;` |
|  2190498 |  695 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  696 | `		/* Isolate variable */` |
|  1370910 |  697 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   685462 |  698 | `			pCur++; /* Variable variable */` |
|        2 |  699 | `		}` |
|   685450 |  700 | `		if( pCur < pGen->pEnd ){` |
|   685450 |  701 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  702 | `				/* Variable name */` |
|   685422 |  703 | `				pCur++;` |
|   342740 |  704 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   342722 |  719 | `		}` |
|   685446 |  720 | `		pNode->xCode = PH7_CompileVariable;` |
|  1547994 |  721 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    33044 |  722 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    33044 |  723 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  724 | `			 /* List/Array node */` |
|    22198 |  725 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  726 | `				 /* Assume a literal */` |
|       17 |  727 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  728 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  729 | `			 }else{` |
|    22182 |  730 | `				 pCur += 2;` |
|        - |  731 | `				 /* Collect array/list tokens */` |
|    22182 |  732 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    22182 |  733 | `				 if( pCur < pGen->pEnd ){` |
|    22180 |  734 | `					 pCur++;` |
|    11091 |  735 | `				 }else{` |
|        - |  736 | `					 /* Syntax error */` |
|        4 |  737 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  738 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  739 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  740 | `						 rc = SXERR_SYNTAX;` |
|        1 |  741 | `					 }` |
|        3 |  742 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  743 | `					 return rc;` |
|        - |  744 | `				 }` |
|    22180 |  745 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    22180 |  746 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    21944 |  759 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  760 | `			 /* Annonymous function */` |
|      160 |  761 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  762 | `				 /* Assume a literal */` |
|      ! 0 |  763 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  764 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  765 | `			 }else{` |
|        - |  766 | `				 /* Assemble annonymous functions body */` |
|      160 |  767 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      160 |  768 | `				 if( rc != SXRET_OK ){` |
|       25 |  769 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  770 | `					 return rc;` |
|        - |  771 | `				 }` |
|      136 |  772 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  773 | `			  }` |
|    10757 |  774 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  775 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       74 |  776 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       74 |  777 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       38 |  778 | `		 }else{` |
|        - |  779 | `			 /* Assume a literal */` |
|    10618 |  780 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    10618 |  781 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  782 | `		 }` |
|  1188737 |  783 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  784 | `		 /* Constants,function name,namespace path,class name... */` |
|   429456 |  785 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   429456 |  786 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   214729 |  787 | `	 }else{` |
|   742776 |  788 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  789 | `			 /* Point to the code generator routine */` |
|   152174 |  790 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   152174 |  791 | `			 if( pNode->xCode == 0 ){` |
|        3 |  792 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  793 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  794 | `					 rc = SXERR_SYNTAX;` |
|        1 |  795 | `				 }` |
|        3 |  796 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  797 | `				 return rc;` |
|        - |  798 | `			 }` |
|    76085 |  799 | `		 }` |
|        - |  800 | `		/* Advance the stream cursor */` |
|   742774 |  801 | `		pCur++;` |
|        - |  802 | `	 }` |
|        - |  803 | `	/* Point to the end of the token stream */` |
|  2490392 |  804 | `	pNode->pEnd = pCur;` |
|        - |  805 | `	/* Save the node for later processing */` |
|  2490392 |  806 | `	*ppNode = pNode;` |
|        - |  807 | `	/* Synchronize cursors */` |
|  2490392 |  808 | `	pGen->pIn = pCur;` |
|  2490392 |  809 | `	return SXRET_OK;` |
|  1245214 |  810 |  |
|        - |  811 | `/*` |
|        - |  812 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  813 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  814 | ` * level is zero.` |
|        - |  815 | ` */` |
|    64442 |  816 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  817 |  |
|    64444 |  818 | `	SyToken *pCur = pStart;` |
|    64444 |  819 | `	sxi32 iNest = 0;` |
|    64444 |  820 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  821 | `		/* Last expression */` |
|    34568 |  822 | `		return SXERR_EOF;` |
|        - |  823 | `	}` |
|   118490 |  824 | `	while( pCur < pEnd ){` |
|   106722 |  825 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    18110 |  826 | `			break;` |
|        - |  827 | `		}` |
|    88614 |  828 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     4692 |  829 | `			iNest++;` |
|    86269 |  830 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     4694 |  831 | `			iNest--;` |
|     2346 |  832 | `		}` |
|    88614 |  833 | `		pCur++;` |
|        2 |  834 | `	}` |
|    29878 |  835 | `	*ppNext = pCur;` |
|    29878 |  836 | `	return SXRET_OK;` |
|    32223 |  837 |  |
|        - |  838 | `/*` |
|        - |  839 | ` * Free an expression tree.` |
|        - |  840 | ` */` |
|  2161452 |  841 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  842 |  |
|  2161454 |  843 | `	if( pNode->pLeft ){` |
|        - |  844 | `		/* Release the left tree */` |
|   818122 |  845 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   409060 |  846 | `	}` |
|  2161454 |  847 | `	if( pNode->pRight ){` |
|        - |  848 | `		/* Release the right tree */` |
|   454696 |  849 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   227347 |  850 | `	}` |
|  2161454 |  851 | `	if( pNode->pCond ){` |
|        - |  852 | `		/* Release the conditional tree used by the ternary operator */` |
|     1754 |  853 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      876 |  854 | `	}` |
|  2161454 |  855 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  856 | `		ph7_expr_node **apArg;` |
|        - |  857 | `		sxu32 n;` |
|        - |  858 | `		/* Release node arguments */` |
|   260716 |  859 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   542946 |  860 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   282232 |  861 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   141117 |  862 | `		}` |
|   260716 |  863 | `		SySetRelease(&pNode->aNodeArgs);` |
|   130357 |  864 | `	}` |
|        - |  865 | `	/* Finally,release this node */` |
|  2161454 |  866 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2161454 |  867 |  |
|        - |  868 | `/*` |
|        - |  869 | ` * Free an expression tree.` |
|        - |  870 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  871 | ` */` |
|   561554 |  872 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  873 |  |
|        - |  874 | `	ph7_expr_node **apNode;` |
|        - |  875 | `	sxu32 n;` |
|   561556 |  876 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3051946 |  877 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2490392 |  878 | `		if( apNode[n] ){` |
|   561830 |  879 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   280914 |  880 | `		}` |
|  1245197 |  881 | `	}` |
|   561556 |  882 | `	return SXRET_OK;` |
|        2 |  883 |  |
|        - |  884 | `/*` |
|        - |  885 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  886 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  887 | ` */` |
|   198150 |  888 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  889 |  |
|        - |  890 | `	sxi32 iExprOp;` |
|   198152 |  891 | `	if( pNode->pOp == 0 ){` |
|   128874 |  892 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  893 | `	}` |
|    69280 |  894 | `	iExprOp = pNode->pOp->iOp;` |
|    69280 |  895 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    43444 |  896 | `			return TRUE;` |
|        - |  897 | `	}` |
|    25838 |  898 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    25834 |  899 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  900 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  901 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  902 | `				return FALSE;` |
|        1 |  903 | `			}` |
|    25831 |  904 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  905 | `			return FALSE;` |
|        - |  906 | `		}` |
|    25834 |  907 | `		return TRUE;` |
|        - |  908 | `	}` |
|        5 |  909 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  910 | `		return TRUE;` |
|        - |  911 | `	}` |
|        - |  912 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  913 | `	return FALSE;` |
|    99077 |  914 |  |
|        - |  915 | `/* Forward declaration */` |
|        - |  916 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  917 | `/* Macro to check if the given node is a terminal.` |
|        - |  918 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  919 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  920 | ` * linked ternary/elvis node). */` |
|        - |  921 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  922 | `/*` |
|        - |  923 | ` * Buid an expression tree for each given function argument.` |
|        - |  924 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  925 | ` */` |
|   208120 |  926 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  927 |  |
|        - |  928 | `	sxi32 iNest,iCur,iNode;` |
|        - |  929 | `	sxi32 rc;` |
|        - |  930 | `	/* Process function arguments from left to right */` |
|   208122 |  931 | `	iCur = 0;` |
|   218877 |  932 | `	for(;;){` |
|   437756 |  933 | `		if( iCur >= nToken ){` |
|        - |  934 | `			/* No more arguments to process */` |
|   208120 |  935 | `			break;` |
|        - |  936 | `		}` |
|   229638 |  937 | `		iNode = iCur;` |
|   229638 |  938 | `		iNest = 0;` |
|   599820 |  939 | `		while( iCur < nToken ){` |
|   391702 |  940 | `			if( apNode[iCur] ){` |
|   381502 |  941 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    10761 |  942 | `					break;` |
|   359984 |  943 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    21120 |  944 | `					iNest++;` |
|   349425 |  945 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    21110 |  946 | `					iNest--;` |
|    10554 |  947 | `				}` |
|   179991 |  948 | `			}` |
|   370184 |  949 | `			iCur++;` |
|        2 |  950 | `		}` |
|   229638 |  951 | `		if( iCur > iNode ){` |
|   229634 |  952 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  953 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  954 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  955 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  956 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  957 | `					apNode[iNode] = 0;` |
|      ! 0 |  958 | `			}` |
|   229636 |  959 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   229636 |  960 | `			if( apNode[iNode] ){` |
|        - |  961 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   229636 |  962 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   114819 |  963 | `			}else{` |
|        - |  964 | `				/* Empty function argument */` |
|      ! 0 |  965 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  966 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  967 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  968 | `				}` |
|      ! 0 |  969 | `				return rc;` |
|        - |  970 | `			}` |
|   114819 |  971 | `		}else{` |
|        3 |  972 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  973 | `			if( rc != SXERR_ABORT ){` |
|        3 |  974 | `				rc = SXERR_SYNTAX;` |
|        1 |  975 | `			}` |
|        3 |  976 | `			return rc;` |
|        - |  977 | `		}` |
|        - |  978 | `		/* Jump trailing comma */` |
|   229636 |  979 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    21518 |  980 | `			iCur++;` |
|    21518 |  981 | `			if( iCur >= nToken ){` |
|        - |  982 | `				/* missing function argument */` |
|      ! 0 |  983 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 |  984 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  985 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  986 | `				}` |
|      ! 0 |  987 | `				return rc;` |
|        - |  988 | `			}` |
|    10758 |  989 | `		}` |
|        2 |  990 | `	}` |
|   208120 |  991 | `	return SXRET_OK;` |
|   104062 |  992 |  |
|        - |  993 | ` /*` |
|        - |  994 | `  * Create an expression tree from an array of tokens.` |
|        - |  995 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - |  996 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  997 | `  */` |
|   871304 |  998 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  999 | ` {` |
|        - | 1000 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1001 | `	 ph7_expr_node *pNode;` |
|        - | 1002 | `	 sxi32 iCur;` |
|        - | 1003 | `	 sxi32 rc;` |
|   871306 | 1004 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1005 | `		 /* TICKET 1433-17: self evaluating node */` |
|   389850 | 1006 | `		 return SXRET_OK;` |
|        - | 1007 | `	 }` |
|        - | 1008 | `	 /* Process expressions enclosed in parenthesis first */` |
|  2987252 | 1009 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1010 | `		 sxi32 iNest;` |
|        - | 1011 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1012 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1013 | `		  */` |
|  2505798 | 1014 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2484382 | 1015 | `			 continue;` |
|        - | 1016 | `		 }` |
|    21418 | 1017 | `		 iNest = 1;` |
|    21418 | 1018 | `		 iLeft = iCur;` |
|        - | 1019 | `		 /* Find the closing parenthesis */` |
|    21418 | 1020 | `		 iCur++;` |
|   142654 | 1021 | `		 while( iCur < nToken ){` |
|   142654 | 1022 | `			 if( apNode[iCur] ){` |
|   142654 | 1023 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1024 | `					 /* Decrement nesting level */` |
|    37206 | 1025 | `					 iNest--;` |
|    37206 | 1026 | `					 if( iNest <= 0 ){` |
|    21418 | 1027 | `						 break;` |
|        2 | 1028 | `					 }` |
|   113344 | 1029 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1030 | `					 /* Increment nesting level */` |
|    15790 | 1031 | `					 iNest++;` |
|     7894 | 1032 | `				 }` |
|    60618 | 1033 | `			 }` |
|   121238 | 1034 | `			 iCur++;` |
|        2 | 1035 | `		 }` |
|    21418 | 1036 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1037 | `			 /* Recurse and process this expression */` |
|    21418 | 1038 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    21418 | 1039 | `			 if( rc != SXRET_OK ){` |
|        3 | 1040 | `				 return rc;` |
|        - | 1041 | `			 }` |
|    10707 | 1042 | `		 }` |
|        - | 1043 | `		 /* Free the left and right nodes */` |
|    21416 | 1044 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    21416 | 1045 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    21416 | 1046 | `		 apNode[iLeft] = 0;` |
|    21416 | 1047 | `		 apNode[iCur] = 0;` |
|    10709 | 1048 | `	 }` |
|        - | 1049 | `	  /* Process expressions enclosed in braces */` |
|  3124396 | 1050 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1051 | `		 sxi32 iNest;` |
|        - | 1052 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1053 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1054 | `		  */` |
|  2648438 | 1055 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  2648438 | 1056 | `			 continue;` |
|        - | 1057 | `		 }` |
|      ! 0 | 1058 | `		 iNest = 1;` |
|      ! 0 | 1059 | `		 iLeft = iCur;` |
|        - | 1060 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1061 | `		 iCur++;` |
|      ! 0 | 1062 | `		 while( iCur < nToken ){` |
|      ! 0 | 1063 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1064 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1065 | `					 /* Decrement nesting level */` |
|      ! 0 | 1066 | `					 iNest--;` |
|      ! 0 | 1067 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1068 | `						 break;` |
|      ! 0 | 1069 | `					 }` |
|      ! 0 | 1070 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1071 | `					 /* Increment nesting level */` |
|      ! 0 | 1072 | `					 iNest++;` |
|      ! 0 | 1073 | `				 }` |
|      ! 0 | 1074 | `			 }` |
|      ! 0 | 1075 | `			 iCur++;` |
|      ! 0 | 1076 | `		 }` |
|      ! 0 | 1077 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1078 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1079 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1080 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1081 | `				 return rc;` |
|        - | 1082 | `			 }` |
|      ! 0 | 1083 | `		 }` |
|        - | 1084 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1085 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1086 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1087 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1088 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1089 | `	 }` |
|        - | 1090 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   475960 | 1091 | `	 iLeft = -1;` |
|  3124384 | 1092 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2648432 | 1093 | `		 if( apNode[iCur] == 0 ){` |
|   990852 | 1094 | `			 continue;` |
|        - | 1095 | `		 }` |
|  1657582 | 1096 | `		 pNode = apNode[iCur];` |
|  1657582 | 1097 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   403256 | 1098 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1099 | `				 /* Collect function arguments */` |
|   240276 | 1100 | `				 sxi32 iPtr = 0;` |
|   240276 | 1101 | `				 sxi32 nFuncTok = 0;` |
|   872252 | 1102 | `				 while( nFuncTok + iCur < nToken ){` |
|   872252 | 1103 | `					 if( apNode[nFuncTok+iCur] ){` |
|   862052 | 1104 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   248332 | 1105 | `							 iPtr++;` |
|   737887 | 1106 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   248332 | 1107 | `							 iPtr--;` |
|   248332 | 1108 | `							 if( iPtr <= 0 ){` |
|   240276 | 1109 | `								 break;` |
|        - | 1110 | `							 }` |
|     4028 | 1111 | `						 }` |
|   310888 | 1112 | `					 }` |
|   631978 | 1113 | `					 nFuncTok++;` |
|        2 | 1114 | `				 }` |
|   240276 | 1115 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1116 | `					 /* Syntax error */` |
|      ! 0 | 1117 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1118 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1119 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1120 | `					 }` |
|      ! 0 | 1121 | `					 return rc;` |
|        - | 1122 | `				 }` |
|   240276 | 1123 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1124 | `					 /* Syntax error */` |
|      ! 0 | 1125 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1126 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1127 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1128 | `					 }` |
|      ! 0 | 1129 | `					 return rc;` |
|        - | 1130 | `				 }` |
|   240276 | 1131 | `				 if( nFuncTok > 1 ){` |
|        - | 1132 | `					 /* Process function arguments */` |
|   208122 | 1133 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   208122 | 1134 | `					 if( rc != SXRET_OK ){` |
|        3 | 1135 | `						 return rc;` |
|        - | 1136 | `					 }` |
|   104059 | 1137 | `				 }` |
|        - | 1138 | `				 /* Link the node to the tree */` |
|   240274 | 1139 | `				 pNode->pLeft = apNode[iLeft];` |
|   240274 | 1140 | `				 apNode[iLeft] = 0;` |
|   872244 | 1141 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   631972 | 1142 | `					 apNode[iCur+iPtr] = 0;` |
|   315987 | 1143 | `				 }` |
|   283118 | 1144 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1145 | `				 /* Subscripting */` |
|    65400 | 1146 | `				 sxi32 iArrTok = iCur + 1;` |
|    65400 | 1147 | `				 sxi32 iNest = 1;` |
|    65455 | 1148 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1149 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1150 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    65398 | 1151 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1152 | `						 /* Syntax error */` |
|      ! 0 | 1153 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1154 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1155 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1156 | `						 }` |
|      ! 0 | 1157 | `						 return rc;` |
|        - | 1158 | `				 }` |
|        - | 1159 | `				 /* Collect index tokens */` |
|   118106 | 1160 | `				 while( iArrTok < nToken ){` |
|   118106 | 1161 | `					 if( apNode[iArrTok] ){` |
|   118074 | 1162 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1163 | `							 /* Increment nesting level */` |
|      ! 0 | 1164 | `							 iNest++;` |
|   118074 | 1165 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1166 | `							 /* Decrement nesting level */` |
|    65400 | 1167 | `							 iNest--;` |
|    65400 | 1168 | `							 if( iNest <= 0 ){` |
|    65400 | 1169 | `								 break;` |
|        - | 1170 | `							 }` |
|      ! 0 | 1171 | `						 }` |
|    26337 | 1172 | `					 }` |
|    52708 | 1173 | `					 ++iArrTok;` |
|        2 | 1174 | `				 }` |
|    65400 | 1175 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1176 | `					 /* Recurse and process this expression */` |
|    52598 | 1177 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    52598 | 1178 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1179 | `						 return rc;` |
|        - | 1180 | `					 }` |
|        - | 1181 | `					 /* Link the node to it's index */` |
|    52598 | 1182 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    26298 | 1183 | `				 }` |
|        - | 1184 | `				 /* Link the node to the tree */` |
|    65400 | 1185 | `				 pNode->pLeft = apNode[iLeft];` |
|    65400 | 1186 | `				 pNode->pRight = 0;` |
|    65400 | 1187 | `				 apNode[iLeft] = 0;` |
|   183504 | 1188 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   118106 | 1189 | `					 apNode[iNest] = 0;` |
|    59054 | 1190 | `				 }` |
|    32701 | 1191 | `			 }else{` |
|        - | 1192 | `				 /* Member access operators [i.e: '->','::'] */` |
|    97584 | 1193 | `				  iRight = iCur + 1;` |
|    97584 | 1194 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1195 | `					 iRight++;` |
|      ! 0 | 1196 | `				 }` |
|    97584 | 1197 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1198 | `					 /* Syntax error */` |
|        5 | 1199 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1200 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1201 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1202 | `					 }` |
|        5 | 1203 | `					 return rc;` |
|        - | 1204 | `				 }` |
|        - | 1205 | `				 /* Link the node to the tree */` |
|    97580 | 1206 | `				 pNode->pLeft = apNode[iLeft];` |
|    97580 | 1207 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|    97484 | 1208 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1209 | `						 /* Syntax error */` |
|      ! 0 | 1210 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1211 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1212 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1213 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1214 | `						 }` |
|      ! 0 | 1215 | `						 return rc;` |
|        - | 1216 | `				 }` |
|    97580 | 1217 | `				 pNode->pRight = apNode[iRight];` |
|    97580 | 1218 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1219 | `			 }` |
|   201624 | 1220 | `		 }` |
|  1657576 | 1221 | `		 iLeft = iCur;` |
|   828789 | 1222 | `	 }` |
|        - | 1223 | `	 /* Handle left associative (new, clone) operators */` |
|  3124364 | 1224 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2648412 | 1225 | `		 if( apNode[iCur] == 0 ){` |
|  1407202 | 1226 | `			 continue;` |
|        - | 1227 | `		 }` |
|  1241212 | 1228 | `		 pNode = apNode[iCur];` |
|  1241212 | 1229 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1230 | `			 SyToken *pToken;` |
|        - | 1231 | `			 /* Get the left node */` |
|    13104 | 1232 | `			 iLeft = iCur + 1;` |
|    26180 | 1233 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13078 | 1234 | `				 iLeft++;` |
|        2 | 1235 | `			 }` |
|    13104 | 1236 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1237 | `				  /* Syntax error */` |
|      ! 0 | 1238 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1239 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1240 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1241 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1242 | `				 }` |
|      ! 0 | 1243 | `				 return rc;` |
|        - | 1244 | `			 }` |
|        - | 1245 | `			 /* Make sure the operand are of a valid type */` |
|    13104 | 1246 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1247 | `				 /* Clone:` |
|        - | 1248 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1249 | `				  *  ++ function call (including annonymous)` |
|        - | 1250 | `				  *  ++ array member` |
|        - | 1251 | `				  *  ++ 'new' operator` |
|        - | 1252 | `				  * Example:` |
|        - | 1253 | `				  *   clone $pObj;` |
|        - | 1254 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1255 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1256 | `				  */` |
|       18 | 1257 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1258 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1259 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1260 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1261 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1262 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1263 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1264 | `						 }` |
|      ! 0 | 1265 | `						 return rc;` |
|        - | 1266 | `					 }` |
|        7 | 1267 | `				 }` |
|       10 | 1268 | `			 }else{` |
|        - | 1269 | `				 /* New */` |
|    13088 | 1270 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       14 | 1271 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       14 | 1272 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1273 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1274 | `						 /* Syntax error */` |
|      ! 0 | 1275 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1276 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1277 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1278 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1279 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1280 | `						 }` |
|      ! 0 | 1281 | `						 return rc;` |
|        - | 1282 | `					 }` |
|        6 | 1283 | `				 }` |
|        - | 1284 | `			 }` |
|        - | 1285 | `			  /* Link the node to the tree */` |
|    13104 | 1286 | `			 pNode->pLeft = apNode[iLeft];` |
|    13104 | 1287 | `			 apNode[iLeft] = 0;` |
|    13104 | 1288 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6551 | 1289 | `		 }` |
|   620607 | 1290 | `	 }` |
|        - | 1291 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   475954 | 1292 | `	 iLeft = -1;` |
|  3127112 | 1293 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2648412 | 1294 | `		 if( apNode[iCur] == 0 ){` |
|  1407202 | 1295 | `			 continue;` |
|        - | 1296 | `		 }` |
|  1241212 | 1297 | `		 pNode = apNode[iCur];` |
|  1241212 | 1298 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7676 | 1299 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2766 | 1300 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1301 | `					 /* Link the node to the tree */` |
|     2768 | 1302 | `					 pNode->pLeft = apNode[iLeft];` |
|     2768 | 1303 | `					 apNode[iLeft] = 0;` |
|     1383 | 1304 | `			 }` |
|     5211 | 1305 | `		  }` |
|  1243960 | 1306 | `		 iLeft = iCur;` |
|   623355 | 1307 | `	  }` |
|   478702 | 1308 | `	 iLeft = -1;` |
|  3127112 | 1309 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2648412 | 1310 | `		 if( apNode[iCur] == 0 ){` |
|  1409968 | 1311 | `			 continue;` |
|        - | 1312 | `		 }` |
|  1238446 | 1313 | `		 pNode = apNode[iCur];` |
|  1238446 | 1314 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7656 | 1315 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     7658 | 1316 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1317 | `					 /* Syntax error */` |
|      ! 0 | 1318 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1319 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1320 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1321 | `					 }` |
|      ! 0 | 1322 | `					 return rc;` |
|        - | 1323 | `			 }` |
|        - | 1324 | `			 /* Link the node to the tree */` |
|     7658 | 1325 | `			 pNode->pLeft = apNode[iLeft];` |
|     7658 | 1326 | `			 apNode[iLeft] = 0;` |
|        - | 1327 | `			 /* Mark as pre-increment/decrement node */` |
|     7658 | 1328 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3828 | 1329 | `		  }` |
|  1238446 | 1330 | `		 iLeft = iCur;` |
|   619224 | 1331 | `	 }` |
|        - | 1332 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   478702 | 1333 | `	  iLeft = 0;` |
|  3127106 | 1334 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2648408 | 1335 | `		  if( apNode[iCur] ){` |
|  1230786 | 1336 | `			  pNode = apNode[iCur];` |
|  1230786 | 1337 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    34304 | 1338 | `				  if( iLeft > 0 ){` |
|        - | 1339 | `					  /* Link the node to the tree */` |
|    34302 | 1340 | `					  pNode->pLeft = apNode[iLeft];` |
|    34302 | 1341 | `					  apNode[iLeft] = 0;` |
|    34302 | 1342 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       10 | 1343 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1344 | `							   /* Syntax error */` |
|      ! 0 | 1345 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1346 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1347 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1348 | `							  }` |
|      ! 0 | 1349 | `							  return rc;` |
|        - | 1350 | `						  }` |
|        4 | 1351 | `					  }` |
|    17152 | 1352 | `				  }else{` |
|        - | 1353 | `					  /* Syntax error */` |
|        3 | 1354 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1355 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1356 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1357 | `					  }` |
|        3 | 1358 | `					  return rc;` |
|        - | 1359 | `				  }` |
|    17150 | 1360 | `			  }` |
|        - | 1361 | `			  /* Save terminal position */` |
|  1230784 | 1362 | `			  iLeft = iCur;` |
|   615391 | 1363 | `		  }` |
|  1324204 | 1364 | `	  }` |
|        - | 1365 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  5265604 | 1366 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  4786914 | 1367 | `		 iLeft = -1;` |
| 31270708 | 1368 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 26483804 | 1369 | `			 if( apNode[iCur] == 0 ){` |
| 16566002 | 1370 | `				 continue;` |
|        - | 1371 | `			 }` |
|  9917804 | 1372 | `			 pNode = apNode[iCur];` |
|  9917804 | 1373 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1374 | `				 /* Get the right node */` |
|   157230 | 1375 | `				 iRight = iCur + 1;` |
|   223582 | 1376 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    66354 | 1377 | `					 iRight++;` |
|        2 | 1378 | `				 }` |
|   157230 | 1379 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1380 | `					 /* Syntax error */` |
|        9 | 1381 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1382 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1383 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1384 | `					 }` |
|        9 | 1385 | `					 return rc;` |
|        - | 1386 | `				 }` |
|   157222 | 1387 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1388 | `					 sxi32  iTmp;` |
|        - | 1389 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1390 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1391 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1392 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1393 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1394 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1395 | `						 }` |
|      ! 0 | 1396 | `						 return rc;` |
|        - | 1397 | `					 }` |
|       46 | 1398 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1399 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1400 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1401 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1402 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1403 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1404 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1405 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1406 | `									 }` |
|      ! 0 | 1407 | `									 return rc;` |
|        - | 1408 | `							 }` |
|      ! 0 | 1409 | `						 }` |
|       15 | 1410 | `					 }` |
|        - | 1411 | `					 /* Swap operands */` |
|       46 | 1412 | `					 iTmp = iRight;` |
|       46 | 1413 | `					 iRight = iLeft;` |
|       46 | 1414 | `					 iLeft = iTmp;` |
|       22 | 1415 | `				 }` |
|        - | 1416 | `				 /* Link the node to the tree */` |
|   157222 | 1417 | `				 pNode->pLeft = apNode[iLeft];` |
|   157222 | 1418 | `				 pNode->pRight = apNode[iRight];` |
|   157222 | 1419 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    78610 | 1420 | `			 }` |
|  9917796 | 1421 | `			 iLeft = iCur;` |
|  4958899 | 1422 | `		 }` |
|  2393454 | 1423 | `	 }` |
|        - | 1424 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1425 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1426 | `	  * we are dealing with a single operator.` |
|        - | 1427 | `	  */` |
|   478692 | 1428 | `	  iLeft = -1;` |
|  3119550 | 1429 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2642614 | 1430 | `		  if( apNode[iCur] == 0 ){` |
|  1765812 | 1431 | `			  continue;` |
|        - | 1432 | `		  }` |
|   876804 | 1433 | `		  pNode = apNode[iCur];` |
|   876804 | 1434 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1756 | 1435 | `			  sxi32 iNest = 1;` |
|     1756 | 1436 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1437 | `				  /* Missing condition */` |
|        3 | 1438 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1439 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1440 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1441 | `				  }` |
|        3 | 1442 | `				  return rc;` |
|        - | 1443 | `			  }` |
|        - | 1444 | `			  /* Get the right node */` |
|     1754 | 1445 | `			  iRight = iCur + 1;` |
|     3724 | 1446 | `			  while( iRight < nToken  ){` |
|     3724 | 1447 | `				  if( apNode[iRight] ){` |
|     3438 | 1448 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1449 | `						  /* Increment nesting level */` |
|      ! 0 | 1450 | `						  ++iNest;` |
|     3438 | 1451 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1452 | `						  /* Decrement nesting level */` |
|     1754 | 1453 | `						  --iNest;` |
|     1754 | 1454 | `						  if( iNest <= 0 ){` |
|     1754 | 1455 | `							  break;` |
|        - | 1456 | `						  }` |
|      ! 0 | 1457 | `					  }` |
|      842 | 1458 | `				  }` |
|     1972 | 1459 | `				  iRight++;` |
|        2 | 1460 | `			  }` |
|     1754 | 1461 | `			  if( iRight > iCur + 1 ){` |
|        - | 1462 | `				  /* Recurse and process the then expression */` |
|     1686 | 1463 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1686 | 1464 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1465 | `					  return rc;` |
|        - | 1466 | `				  }` |
|        - | 1467 | `				  /* Link the node to the tree */` |
|     1686 | 1468 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      842 | 1469 | `			  }else{` |
|        - | 1470 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1471 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1472 | `			  }` |
|     1754 | 1473 | `			  apNode[iCur + 1] = 0;` |
|     1754 | 1474 | `			  if( iRight + 1 < nToken ){` |
|        - | 1475 | `				  /* Recurse and process the else expression */` |
|     1754 | 1476 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1754 | 1477 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1478 | `					  return rc;` |
|        - | 1479 | `				  }` |
|        - | 1480 | `				  /* Link the node to the tree */` |
|     1754 | 1481 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1754 | 1482 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      878 | 1483 | `			  }else{` |
|      ! 0 | 1484 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1485 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1486 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1487 | `				 }` |
|      ! 0 | 1488 | `				 return rc;` |
|        - | 1489 | `			  }` |
|        - | 1490 | `			  /* Point to the condition */` |
|     1754 | 1491 | `			  pNode->pCond  = apNode[iLeft];` |
|     1754 | 1492 | `			  apNode[iLeft] = 0;` |
|     1754 | 1493 | `			  break;` |
|        - | 1494 | `		  }` |
|   875050 | 1495 | `		  iLeft = iCur;` |
|   437526 | 1496 | `	  }` |
|        - | 1497 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1498 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1499 | `	  * so there is no need for a precedence loop here.` |
|        - | 1500 | `	  */` |
|   478690 | 1501 | `	 iRight = -1;` |
|  3126972 | 1502 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  2648324 | 1503 | `		 if( apNode[iCur] == 0 ){` |
|  1971396 | 1504 | `			 continue;` |
|        - | 1505 | `		 }` |
|   676930 | 1506 | `		 pNode = apNode[iCur];` |
|   676930 | 1507 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1508 | `			 /* Get the left node */` |
|   198116 | 1509 | `			 iLeft = iCur - 1;` |
|   280480 | 1510 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    82366 | 1511 | `				 iLeft--;` |
|        2 | 1512 | `			 }` |
|   198116 | 1513 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1514 | `				 /* Syntax error */` |
|       39 | 1515 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1516 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1517 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1518 | `				 }` |
|       39 | 1519 | `				 return rc;` |
|        - | 1520 | `			 }` |
|   198078 | 1521 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       28 | 1522 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
|        - | 1523 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1524 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1525 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1526 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1527 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1528 | `					 }` |
|        3 | 1529 | `					 return rc;` |
|        - | 1530 | `				 }` |
|       12 | 1531 | `			 }` |
|        - | 1532 | `			 /* Link the node to the tree (Reverse) */` |
|   198076 | 1533 | `			 pNode->pLeft = apNode[iRight];` |
|   198076 | 1534 | `			 pNode->pRight = apNode[iLeft];` |
|   198076 | 1535 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    99037 | 1536 | `		 }` |
|   676890 | 1537 | `		 iRight = iCur;` |
|   338446 | 1538 | `	 }` |
|        - | 1539 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2393242 | 1540 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  1914594 | 1541 | `		 iLeft = -1;` |
| 12507714 | 1542 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 10593122 | 1543 | `			 if( apNode[iCur] == 0 ){` |
|  8678116 | 1544 | `				 continue;` |
|        - | 1545 | `			 }` |
|  1915008 | 1546 | `			 pNode = apNode[iCur];` |
|  1915008 | 1547 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1548 | `				 /* Get the right node */` |
|       72 | 1549 | `				 iRight = iCur + 1;` |
|      110 | 1550 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1551 | `					 iRight++;` |
|        2 | 1552 | `				 }` |
|       72 | 1553 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1554 | `					 /* Syntax error */` |
|      ! 0 | 1555 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1556 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1557 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1558 | `					 }` |
|      ! 0 | 1559 | `					 return rc;` |
|        - | 1560 | `				 }` |
|        - | 1561 | `				 /* Link the node to the tree */` |
|       72 | 1562 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1563 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1564 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1565 | `			 }` |
|  1915008 | 1566 | `			 iLeft = iCur;` |
|   957505 | 1567 | `		 }` |
|   957298 | 1568 | `	 }` |
|        - | 1569 | `	 /* Point to the root of the expression tree */` |
|  2648252 | 1570 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2169624 | 1571 | `		 if( apNode[iCur] ){` |
|   426964 | 1572 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       22 | 1573 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       22 | 1574 | `				  if( rc != SXERR_ABORT ){` |
|       22 | 1575 | `					  rc = SXERR_SYNTAX;` |
|       10 | 1576 | `				  }` |
|       22 | 1577 | `				  return rc;` |
|        - | 1578 | `			 }` |
|   426944 | 1579 | `			 apNode[0] = apNode[iCur];` |
|   426944 | 1580 | `			 apNode[iCur] = 0;` |
|   213471 | 1581 | `		 }` |
|  1084803 | 1582 | `	 }` |
|   478630 | 1583 | `	 return SXRET_OK;` |
|   434280 | 1584 | ` }` |
|        - | 1585 | ` /*` |
|        - | 1586 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1587 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1588 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1589 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1590 | `  */` |
|   561554 | 1591 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1592 |  |
|        - | 1593 | `	ph7_expr_node **apNode;` |
|        - | 1594 | `	ph7_expr_node *pNode;` |
|        - | 1595 | `	sxi32 rc;` |
|        - | 1596 | `	/* Reset node container */` |
|   561556 | 1597 | `	SySetReset(pExprNode);` |
|   561556 | 1598 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1599 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1600 | `	{` |
|   561556 | 1601 | `		int iLastWasTerm = 0;` |
|  3051946 | 1602 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2490426 | 1603 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2490426 | 1604 | `			if( rc != SXRET_OK ){` |
|       35 | 1605 | `				return rc;` |
|        - | 1606 | `			}` |
|        - | 1607 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2490392 | 1608 | `			if( pNode->xCode ){` |
|        - | 1609 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1300234 | 1610 | `				iLastWasTerm = 1;` |
|  1840276 | 1611 | `			}else if( pNode->pOp ){` |
|        - | 1612 | `				/* Operator node */` |
|   599558 | 1613 | `				iLastWasTerm = 0;` |
|   299780 | 1614 | `			}else{` |
|        - | 1615 | `				/* Delimiter: ')' and ']' end terms */` |
|   590604 | 1616 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1617 | `			}` |
|        - | 1618 | `			/* Save the extracted node */` |
|  2490392 | 1619 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1620 | `		}` |
|        - | 1621 | `	}` |
|   561522 | 1622 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1623 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1624 | `		*ppRoot = 0;` |
|      ! 0 | 1625 | `		return SXRET_OK;` |
|        - | 1626 | `	}` |
|   561522 | 1627 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1628 | `	/* Make sure we are dealing with valid nodes */` |
|   561522 | 1629 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   561522 | 1630 | `	if( rc != SXRET_OK ){` |
|        - | 1631 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1632 | `		 * cleanup the mess left behind.` |
|        - | 1633 | `		 */` |
|       47 | 1634 | `		*ppRoot = 0;` |
|       47 | 1635 | `		return rc;` |
|        - | 1636 | `	}` |
|        - | 1637 | `	/* Build the tree */` |
|   561476 | 1638 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   561476 | 1639 | `	if( rc != SXRET_OK ){` |
|        - | 1640 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       80 | 1641 | `		*ppRoot = 0;` |
|       80 | 1642 | `		return rc;` |
|        - | 1643 | `	}` |
|        - | 1644 | `	/* Point to the root of the tree */` |
|   561398 | 1645 | `	*ppRoot = apNode[0];` |
|   561398 | 1646 | `	return SXRET_OK;` |
|   280779 | 1647 |  |
|        - | 1648 |  |
