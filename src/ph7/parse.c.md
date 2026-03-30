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
|   663280 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  255 |  |
|   663282 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
| 10268888 |  259 | `	for(;;){` |
| 20537778 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 20537778 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2588266 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1294134 |  266 | `		}else{` |
| 17949514 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 20537778 |  269 | `		if( rc == 0 ){` |
|   666262 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   662964 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|     3300 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      210 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|     3092 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|     1490 |  288 | `		}` |
| 19874498 |  289 | `		++n; /* Next operator in the table */` |
|        2 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   331642 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   290320 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  300 |  |
|   290322 |  301 | `	SyToken *pCur = pIn;` |
|   290322 |  302 | `	sxi32 iNest = 1;` |
|  1562855 |  303 | `	for(;;){` |
|  3125712 |  304 | `		if( pCur >= pEnd ){` |
|       90 |  305 | `			break;` |
|        - |  306 | `		}` |
|  3125624 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   174560 |  309 | `			iNest++;` |
|  3038345 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   464792 |  312 | `			iNest--;` |
|   464792 |  313 | `			if( iNest <= 0 ){` |
|   290234 |  314 | `				break;` |
|        - |  315 | `			}` |
|    87279 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  2835392 |  318 | `		pCur++;` |
|        2 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   290322 |  321 | `	*ppEnd = pCur;` |
|   290322 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    15368 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  333 |  |
|    22990 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    15279 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|      138 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    15234 |  339 | `	if( bCheckFunc ){` |
|     2468 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       37 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       25 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|     2452 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|        6 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    12784 |  347 | `	return FALSE;` |
|     7686 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|   544442 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|   544444 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       10 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       10 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        4 |  364 | `	}` |
|   544444 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  2983380 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2438968 |  367 | `		if( apNode[i]->xCode == PH7_CompileShortArray ){` |
|        - |  368 | `			/* Short array literal: brackets are self-contained, skip */` |
|       89 |  369 | `			continue;` |
|        - |  370 | `		}` |
|  2438880 |  371 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   257766 |  372 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    15630 |  373 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  374 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   236852 |  375 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  376 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  377 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  378 | `						 */` |
|   236852 |  379 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   236852 |  380 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   236852 |  381 | `						apNode[i]->pOp = &sFCallOp;` |
|   118425 |  382 | `					}` |
|   118425 |  383 | `			}` |
|   257766 |  384 | `			iParen++;` |
|  2309998 |  385 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   257762 |  386 | `			if( iParen <= 0 ){` |
|        9 |  387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  388 | `				if( rc != SXERR_ABORT ){` |
|        9 |  389 | `					rc = SXERR_SYNTAX;` |
|        4 |  390 | `				}` |
|        9 |  391 | `				return rc;` |
|        - |  392 | `			}` |
|   257754 |  393 | `			iParen--;` |
|  2052232 |  394 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    63628 |  395 | `			iSquare++;` |
|  1891543 |  396 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    63642 |  397 | `			if( iSquare <= 0 ){` |
|        7 |  398 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  399 | `				if( rc != SXERR_ABORT ){` |
|        7 |  400 | `					rc = SXERR_SYNTAX;` |
|        3 |  401 | `				}` |
|        7 |  402 | `				return rc;` |
|        - |  403 | `			}` |
|    63636 |  404 | `			iSquare--;` |
|  1827907 |  405 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  1796085 |  452 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  453 | `			if( iBraces <= 0 ){` |
|       13 |  454 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  455 | `				if( rc != SXERR_ABORT ){` |
|       13 |  456 | `					rc = SXERR_SYNTAX;` |
|        6 |  457 | `				}` |
|       13 |  458 | `				return rc;` |
|        - |  459 | `			}` |
|      ! 0 |  460 | `			iBraces--;` |
|  1796068 |  461 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1754 |  462 | `			if( iQuesty <= 0 ){` |
|        5 |  463 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  464 | `				if( rc != SXERR_ABORT ){` |
|        5 |  465 | `					rc = SXERR_SYNTAX;` |
|        2 |  466 | `				}` |
|        5 |  467 | `				return rc;` |
|        - |  468 | `			}` |
|     1750 |  469 | `			iQuesty--;` |
|  1795190 |  470 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   524046 |  471 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   524046 |  472 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1752 |  473 | `				iQuesty++;` |
|   523171 |  474 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   262022 |  494 | `		}` |
|  1219426 |  495 | `	}` |
|   544414 |  496 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  497 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  498 | `		if( rc != SXERR_ABORT ){` |
|       17 |  499 | `			rc = SXERR_SYNTAX;` |
|        8 |  500 | `		}` |
|       17 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|   544398 |  503 | `	return SXRET_OK;` |
|   272223 |  504 |  |
|        - |  505 | `/*` |
|        - |  506 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  507 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  508 | ` */` |
|   431698 |  509 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  510 |  |
|   431700 |  511 | `	SyToken *pIn = *ppCur;` |
|        - |  512 | `	/* Jump the first literal seen */` |
|   431700 |  513 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   431682 |  514 | `		pIn++;` |
|   215840 |  515 | `	}` |
|   215873 |  516 | `	for(;;){` |
|   431748 |  517 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       49 |  518 | `			pIn++;` |
|       49 |  519 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       49 |  520 | `				pIn++;` |
|       24 |  521 | `			}` |
|       25 |  522 | `		}else{` |
|   215851 |  523 | `			break;` |
|        - |  524 | `		}` |
|        1 |  525 | `	}` |
|        - |  526 | `	/* Synchronize pointers */` |
|   431700 |  527 | `	*ppCur = pIn;` |
|   431700 |  528 |  |
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
|      154 |  562 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  563 |  |
|      156 |  564 | `	SyToken *pIn = *ppCur;` |
|        - |  565 | `	sxu32 nLine;` |
|        - |  566 | `	sxi32 rc;` |
|        - |  567 | `	/* Jump the 'function' keyword */` |
|      156 |  568 | `	nLine = pIn->nLine;` |
|      156 |  569 | `	pIn++;` |
|      156 |  570 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  571 | `		pIn++;` |
|        1 |  572 | `	}` |
|      156 |  573 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  574 | `		/* Syntax error */` |
|        5 |  575 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  576 | `		if( rc != SXERR_ABORT ){` |
|        5 |  577 | `			rc = SXERR_SYNTAX;` |
|        2 |  578 | `		}` |
|        5 |  579 | `		goto Synchronize;` |
|        - |  580 | `	}` |
|      152 |  581 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      152 |  582 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      152 |  583 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  584 | `		/* Syntax error */` |
|        5 |  585 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  586 | `		if( rc != SXERR_ABORT ){` |
|        5 |  587 | `			rc = SXERR_SYNTAX;` |
|        2 |  588 | `		}` |
|        5 |  589 | `		goto Synchronize;` |
|        - |  590 | `	}` |
|      148 |  591 | `	pIn++; /* Jump the trailing parenthesis */` |
|      148 |  592 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       26 |  593 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  594 | `		/* Check if we are dealing with a closure */` |
|       26 |  595 | `		if( nKey == PH7_TKWRD_USE ){` |
|       18 |  596 | `			pIn++; /* Jump the 'use' keyword */` |
|       18 |  597 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  598 | `				/* Syntax error */` |
|        5 |  599 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  600 | `				if( rc != SXERR_ABORT ){` |
|        5 |  601 | `					rc = SXERR_SYNTAX;` |
|        2 |  602 | `				}` |
|        5 |  603 | `				goto Synchronize;` |
|        - |  604 | `			}` |
|       14 |  605 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       14 |  606 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       14 |  607 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  608 | `				/* Syntax error */` |
|        5 |  609 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  610 | `				if( rc != SXERR_ABORT ){` |
|        5 |  611 | `					rc = SXERR_SYNTAX;` |
|        2 |  612 | `				}` |
|        5 |  613 | `				goto Synchronize;` |
|        - |  614 | `			}` |
|       10 |  615 | `			pIn++;` |
|        6 |  616 | `		}else{` |
|        - |  617 | `			/* Syntax error */` |
|        9 |  618 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  619 | `			if( rc != SXERR_ABORT ){` |
|        9 |  620 | `				rc = SXERR_SYNTAX;` |
|        4 |  621 | `			}` |
|        9 |  622 | `			goto Synchronize;` |
|        - |  623 | `		}` |
|        4 |  624 | `	}` |
|      132 |  625 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      132 |  626 | `		pIn++; /* Jump the leading curly '{' */` |
|      132 |  627 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      132 |  628 | `		if( pIn < pEnd ){` |
|      132 |  629 | `			pIn++;` |
|       65 |  630 | `		}` |
|       67 |  631 | `	}else{` |
|        - |  632 | `		/* Syntax error */` |
|      ! 0 |  633 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  634 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  635 | `			return SXERR_ABORT;` |
|        - |  636 | `		}` |
|        - |  637 | `	}` |
|      132 |  638 | `	rc = SXRET_OK;` |
|       77 |  639 | `Synchronize:` |
|        - |  640 | `	/* Synchronize pointers */` |
|      156 |  641 | `	*ppCur = pIn;` |
|      156 |  642 | `	return rc;` |
|       79 |  643 |  |
|        - |  644 | `/*` |
|        - |  645 | ` * Extract a single expression node from the input.` |
|        - |  646 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  647 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  648 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  649 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  650 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  651 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  652 | ` */` |
|  2439104 |  653 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  654 |  |
|        - |  655 | `	ph7_expr_node *pNode;` |
|        - |  656 | `	SyToken *pCur;` |
|        - |  657 | `	sxi32 rc;` |
|        - |  658 | `	/* Allocate a new node */` |
|  2439106 |  659 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2439106 |  660 | `	if( pNode == 0 ){` |
|        - |  661 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  662 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  663 | `		 */` |
|      ! 0 |  664 | `		return SXERR_MEM;` |
|        - |  665 | `	}` |
|        - |  666 | `	/* Zero the structure */` |
|  2439106 |  667 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2439106 |  668 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  669 | `	/* Point to the head of the token stream */` |
|  2439106 |  670 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  671 | `	/* Start collecting tokens */` |
|  2439106 |  672 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  673 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  674 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  675 | `		 */` |
|       91 |  676 | `		pCur++; /* Skip the opening '[' */` |
|       91 |  677 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|       91 |  678 | `		if( pCur < pGen->pEnd ){` |
|       91 |  679 | `			pCur++; /* Skip past the closing ']' */` |
|       46 |  680 | `		}else{` |
|      ! 0 |  681 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  682 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  683 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  684 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  685 | `			}` |
|      ! 0 |  686 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  687 | `			return rc;` |
|        - |  688 | `		}` |
|       91 |  689 | `		pNode->xCode = PH7_CompileShortArray;` |
|  2439061 |  690 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  691 | `		/* Point to the instance that describe this operator */` |
|   587706 |  692 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  693 | `		/* Advance the stream cursor */` |
|   587706 |  694 | `		pCur++;` |
|  2145164 |  695 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  696 | `		/* Isolate variable */` |
|  1336586 |  697 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   668300 |  698 | `			pCur++; /* Variable variable */` |
|        2 |  699 | `		}` |
|   668288 |  700 | `		if( pCur < pGen->pEnd ){` |
|   668288 |  701 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  702 | `				/* Variable name */` |
|   668260 |  703 | `				pCur++;` |
|   334159 |  704 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   334141 |  719 | `		}` |
|   668284 |  720 | `		pNode->xCode = PH7_CompileVariable;` |
|  1517167 |  721 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    34672 |  722 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    34672 |  723 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  724 | `			 /* List/Array node */` |
|    21672 |  725 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  726 | `				 /* Assume a literal */` |
|       17 |  727 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  728 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  729 | `			 }else{` |
|    21656 |  730 | `				 pCur += 2;` |
|        - |  731 | `				 /* Collect array/list tokens */` |
|    21656 |  732 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    21656 |  733 | `				 if( pCur < pGen->pEnd ){` |
|    21654 |  734 | `					 pCur++;` |
|    10828 |  735 | `				 }else{` |
|        - |  736 | `					 /* Syntax error */` |
|        4 |  737 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  738 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  739 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  740 | `						 rc = SXERR_SYNTAX;` |
|        1 |  741 | `					 }` |
|        3 |  742 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  743 | `					 return rc;` |
|        - |  744 | `				 }` |
|    21654 |  745 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    21654 |  746 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    23835 |  759 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  760 | `			 /* Annonymous function */` |
|      156 |  761 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  762 | `				 /* Assume a literal */` |
|      ! 0 |  763 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  764 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  765 | `			 }else{` |
|        - |  766 | `				 /* Assemble annonymous functions body */` |
|      156 |  767 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      156 |  768 | `				 if( rc != SXRET_OK ){` |
|       25 |  769 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  770 | `					 return rc;` |
|        - |  771 | `				 }` |
|      132 |  772 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  773 | `			  }` |
|    12913 |  774 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  775 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       74 |  776 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       74 |  777 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       38 |  778 | `		 }else{` |
|        - |  779 | `			 /* Assume a literal */` |
|    12776 |  780 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    12776 |  781 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  782 | `		 }` |
|  1165677 |  783 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  784 | `		 /* Constants,function name,namespace path,class name... */` |
|   418910 |  785 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   418910 |  786 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   209456 |  787 | `	 }else{` |
|   729448 |  788 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  789 | `			 /* Point to the code generator routine */` |
|   148496 |  790 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   148496 |  791 | `			 if( pNode->xCode == 0 ){` |
|        3 |  792 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  793 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  794 | `					 rc = SXERR_SYNTAX;` |
|        1 |  795 | `				 }` |
|        3 |  796 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  797 | `				 return rc;` |
|        - |  798 | `			 }` |
|    74246 |  799 | `		 }` |
|        - |  800 | `		/* Advance the stream cursor */` |
|   729446 |  801 | `		pCur++;` |
|        - |  802 | `	 }` |
|        - |  803 | `	/* Point to the end of the token stream */` |
|  2439072 |  804 | `	pNode->pEnd = pCur;` |
|        - |  805 | `	/* Save the node for later processing */` |
|  2439072 |  806 | `	*ppNode = pNode;` |
|        - |  807 | `	/* Synchronize cursors */` |
|  2439072 |  808 | `	pGen->pIn = pCur;` |
|  2439072 |  809 | `	return SXRET_OK;` |
|  1219554 |  810 |  |
|        - |  811 | `/*` |
|        - |  812 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  813 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  814 | ` * level is zero.` |
|        - |  815 | ` */` |
|    54918 |  816 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  817 |  |
|    54920 |  818 | `	SyToken *pCur = pStart;` |
|    54920 |  819 | `	sxi32 iNest = 0;` |
|    54920 |  820 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  821 | `		/* Last expression */` |
|    31328 |  822 | `		return SXERR_EOF;` |
|        - |  823 | `	}` |
|    98672 |  824 | `	while( pCur < pEnd ){` |
|    89660 |  825 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    14582 |  826 | `			break;` |
|        - |  827 | `		}` |
|    75080 |  828 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     4466 |  829 | `			iNest++;` |
|    72848 |  830 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     4468 |  831 | `			iNest--;` |
|     2233 |  832 | `		}` |
|    75080 |  833 | `		pCur++;` |
|        2 |  834 | `	}` |
|    23594 |  835 | `	*ppNext = pCur;` |
|    23594 |  836 | `	return SXRET_OK;` |
|    27461 |  837 |  |
|        - |  838 | `/*` |
|        - |  839 | ` * Free an expression tree.` |
|        - |  840 | ` */` |
|  2112638 |  841 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  842 |  |
|  2112640 |  843 | `	if( pNode->pLeft ){` |
|        - |  844 | `		/* Release the left tree */` |
|   800152 |  845 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   400075 |  846 | `	}` |
|  2112640 |  847 | `	if( pNode->pRight ){` |
|        - |  848 | `		/* Release the right tree */` |
|   443348 |  849 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   221673 |  850 | `	}` |
|  2112640 |  851 | `	if( pNode->pCond ){` |
|        - |  852 | `		/* Release the conditional tree used by the ternary operator */` |
|     1748 |  853 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      873 |  854 | `	}` |
|  2112640 |  855 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  856 | `		ph7_expr_node **apArg;` |
|        - |  857 | `		sxu32 n;` |
|        - |  858 | `		/* Release node arguments */` |
|   256640 |  859 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   537484 |  860 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   280846 |  861 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   140424 |  862 | `		}` |
|   256640 |  863 | `		SySetRelease(&pNode->aNodeArgs);` |
|   128319 |  864 | `	}` |
|        - |  865 | `	/* Finally,release this node */` |
|  2112640 |  866 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2112640 |  867 |  |
|        - |  868 | `/*` |
|        - |  869 | ` * Free an expression tree.` |
|        - |  870 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  871 | ` */` |
|   544476 |  872 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  873 |  |
|        - |  874 | `	ph7_expr_node **apNode;` |
|        - |  875 | `	sxu32 n;` |
|   544478 |  876 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  2983548 |  877 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2439072 |  878 | `		if( apNode[n] ){` |
|   544750 |  879 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   272374 |  880 | `		}` |
|  1219537 |  881 | `	}` |
|   544478 |  882 | `	return SXRET_OK;` |
|        2 |  883 |  |
|        - |  884 | `/*` |
|        - |  885 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  886 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  887 | ` */` |
|   193176 |  888 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  889 |  |
|        - |  890 | `	sxi32 iExprOp;` |
|   193178 |  891 | `	if( pNode->pOp == 0 ){` |
|   125672 |  892 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  893 | `	}` |
|    67508 |  894 | `	iExprOp = pNode->pOp->iOp;` |
|    67508 |  895 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    42348 |  896 | `			return TRUE;` |
|        - |  897 | `	}` |
|    25162 |  898 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    25158 |  899 | `		if( pNode->pLeft->pOp ) {` |
|        2 |  900 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  901 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  902 | `				return FALSE;` |
|        1 |  903 | `			}` |
|    25157 |  904 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  905 | `			return FALSE;` |
|        - |  906 | `		}` |
|    25158 |  907 | `		return TRUE;` |
|        - |  908 | `	}` |
|        5 |  909 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  910 | `		return TRUE;` |
|        - |  911 | `	}` |
|        - |  912 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  913 | `	return FALSE;` |
|    96590 |  914 |  |
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
|   205484 |  926 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  927 |  |
|        - |  928 | `	sxi32 iNest,iCur,iNode;` |
|        - |  929 | `	sxi32 rc;` |
|        - |  930 | `	/* Process function arguments from left to right */` |
|   205486 |  931 | `	iCur = 0;` |
|   217586 |  932 | `	for(;;){` |
|   435174 |  933 | `		if( iCur >= nToken ){` |
|        - |  934 | `			/* No more arguments to process */` |
|   205484 |  935 | `			break;` |
|        - |  936 | `		}` |
|   229692 |  937 | `		iNode = iCur;` |
|   229692 |  938 | `		iNest = 0;` |
|   596544 |  939 | `		while( iCur < nToken ){` |
|   391062 |  940 | `			if( apNode[iCur] ){` |
|   381118 |  941 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    12106 |  942 | `					break;` |
|   356910 |  943 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    20632 |  944 | `					iNest++;` |
|   346595 |  945 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    20622 |  946 | `					iNest--;` |
|    10310 |  947 | `				}` |
|   178454 |  948 | `			}` |
|   366854 |  949 | `			iCur++;` |
|        2 |  950 | `		}` |
|   229692 |  951 | `		if( iCur > iNode ){` |
|   229688 |  952 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  953 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  954 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  955 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  956 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  957 | `					apNode[iNode] = 0;` |
|      ! 0 |  958 | `			}` |
|   229690 |  959 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   229690 |  960 | `			if( apNode[iNode] ){` |
|        - |  961 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   229690 |  962 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   114846 |  963 | `			}else{` |
|        - |  964 | `				/* Empty function argument */` |
|      ! 0 |  965 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  966 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  967 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  968 | `				}` |
|      ! 0 |  969 | `				return rc;` |
|        - |  970 | `			}` |
|   114846 |  971 | `		}else{` |
|        3 |  972 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  973 | `			if( rc != SXERR_ABORT ){` |
|        3 |  974 | `				rc = SXERR_SYNTAX;` |
|        1 |  975 | `			}` |
|        3 |  976 | `			return rc;` |
|        - |  977 | `		}` |
|        - |  978 | `		/* Jump trailing comma */` |
|   229690 |  979 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    24208 |  980 | `			iCur++;` |
|    24208 |  981 | `			if( iCur >= nToken ){` |
|        - |  982 | `				/* missing function argument */` |
|      ! 0 |  983 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 |  984 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  985 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  986 | `				}` |
|      ! 0 |  987 | `				return rc;` |
|        - |  988 | `			}` |
|    12103 |  989 | `		}` |
|        2 |  990 | `	}` |
|   205484 |  991 | `	return SXRET_OK;` |
|   102744 |  992 |  |
|        - |  993 | ` /*` |
|        - |  994 | `  * Create an expression tree from an array of tokens.` |
|        - |  995 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - |  996 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  997 | `  */` |
|   852250 |  998 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  999 | ` {` |
|        - | 1000 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1001 | `	 ph7_expr_node *pNode;` |
|        - | 1002 | `	 sxi32 iCur;` |
|        - | 1003 | `	 sxi32 rc;` |
|   852252 | 1004 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1005 | `		 /* TICKET 1433-17: self evaluating node */` |
|   380320 | 1006 | `		 return SXRET_OK;` |
|        - | 1007 | `	 }` |
|        - | 1008 | `	 /* Process expressions enclosed in parenthesis first */` |
|  2931664 | 1009 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1010 | `		 sxi32 iNest;` |
|        - | 1011 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1012 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1013 | `		  */` |
|  2459734 | 1014 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2438830 | 1015 | `			 continue;` |
|        - | 1016 | `		 }` |
|    20906 | 1017 | `		 iNest = 1;` |
|    20906 | 1018 | `		 iLeft = iCur;` |
|        - | 1019 | `		 /* Find the closing parenthesis */` |
|    20906 | 1020 | `		 iCur++;` |
|   139262 | 1021 | `		 while( iCur < nToken ){` |
|   139262 | 1022 | `			 if( apNode[iCur] ){` |
|   139262 | 1023 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1024 | `					 /* Decrement nesting level */` |
|    36310 | 1025 | `					 iNest--;` |
|    36310 | 1026 | `					 if( iNest <= 0 ){` |
|    20906 | 1027 | `						 break;` |
|        2 | 1028 | `					 }` |
|   110656 | 1029 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1030 | `					 /* Increment nesting level */` |
|    15406 | 1031 | `					 iNest++;` |
|     7702 | 1032 | `				 }` |
|    59178 | 1033 | `			 }` |
|   118358 | 1034 | `			 iCur++;` |
|        2 | 1035 | `		 }` |
|    20906 | 1036 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1037 | `			 /* Recurse and process this expression */` |
|    20906 | 1038 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    20906 | 1039 | `			 if( rc != SXRET_OK ){` |
|        3 | 1040 | `				 return rc;` |
|        - | 1041 | `			 }` |
|    10451 | 1042 | `		 }` |
|        - | 1043 | `		 /* Free the left and right nodes */` |
|    20904 | 1044 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    20904 | 1045 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    20904 | 1046 | `		 apNode[iLeft] = 0;` |
|    20904 | 1047 | `		 apNode[iCur] = 0;` |
|    10453 | 1048 | `	 }` |
|        - | 1049 | `	  /* Process expressions enclosed in braces */` |
|  3065548 | 1050 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1051 | `		 sxi32 iNest;` |
|        - | 1052 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1053 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1054 | `		  */` |
|  2598982 | 1055 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  2598982 | 1056 | `			 continue;` |
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
|   466568 | 1091 | `	 iLeft = -1;` |
|  3065536 | 1092 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2598976 | 1093 | `		 if( apNode[iCur] == 0 ){` |
|   977716 | 1094 | `			 continue;` |
|        - | 1095 | `		 }` |
|  1621262 | 1096 | `		 pNode = apNode[iCur];` |
|  1621262 | 1097 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   395596 | 1098 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1099 | `				 /* Collect function arguments */` |
|   236848 | 1100 | `				 sxi32 iPtr = 0;` |
|   236848 | 1101 | `				 sxi32 nFuncTok = 0;` |
|   864756 | 1102 | `				 while( nFuncTok + iCur < nToken ){` |
|   864756 | 1103 | `					 if( apNode[nFuncTok+iCur] ){` |
|   854812 | 1104 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   244712 | 1105 | `							 iPtr++;` |
|   732457 | 1106 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   244712 | 1107 | `							 iPtr--;` |
|   244712 | 1108 | `							 if( iPtr <= 0 ){` |
|   236848 | 1109 | `								 break;` |
|        - | 1110 | `							 }` |
|     3932 | 1111 | `						 }` |
|   308982 | 1112 | `					 }` |
|   627910 | 1113 | `					 nFuncTok++;` |
|        2 | 1114 | `				 }` |
|   236848 | 1115 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1116 | `					 /* Syntax error */` |
|      ! 0 | 1117 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1118 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1119 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1120 | `					 }` |
|      ! 0 | 1121 | `					 return rc;` |
|        - | 1122 | `				 }` |
|   236848 | 1123 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1124 | `					 /* Syntax error */` |
|      ! 0 | 1125 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1126 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1127 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1128 | `					 }` |
|      ! 0 | 1129 | `					 return rc;` |
|        - | 1130 | `				 }` |
|   236848 | 1131 | `				 if( nFuncTok > 1 ){` |
|        - | 1132 | `					 /* Process function arguments */` |
|   205486 | 1133 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   205486 | 1134 | `					 if( rc != SXRET_OK ){` |
|        3 | 1135 | `						 return rc;` |
|        - | 1136 | `					 }` |
|   102741 | 1137 | `				 }` |
|        - | 1138 | `				 /* Link the node to the tree */` |
|   236846 | 1139 | `				 pNode->pLeft = apNode[iLeft];` |
|   236846 | 1140 | `				 apNode[iLeft] = 0;` |
|   864748 | 1141 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   627904 | 1142 | `					 apNode[iCur+iPtr] = 0;` |
|   313953 | 1143 | `				 }` |
|   277172 | 1144 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1145 | `				 /* Subscripting */` |
|    63636 | 1146 | `				 sxi32 iArrTok = iCur + 1;` |
|    63636 | 1147 | `				 sxi32 iNest = 1;` |
|    63681 | 1148 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1149 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1150 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    63634 | 1151 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1152 | `						 /* Syntax error */` |
|      ! 0 | 1153 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1154 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1155 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1156 | `						 }` |
|      ! 0 | 1157 | `						 return rc;` |
|        - | 1158 | `				 }` |
|        - | 1159 | `				 /* Collect index tokens */` |
|   114898 | 1160 | `				 while( iArrTok < nToken ){` |
|   114898 | 1161 | `					 if( apNode[iArrTok] ){` |
|   114866 | 1162 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1163 | `							 /* Increment nesting level */` |
|      ! 0 | 1164 | `							 iNest++;` |
|   114866 | 1165 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1166 | `							 /* Decrement nesting level */` |
|    63636 | 1167 | `							 iNest--;` |
|    63636 | 1168 | `							 if( iNest <= 0 ){` |
|    63636 | 1169 | `								 break;` |
|        - | 1170 | `							 }` |
|      ! 0 | 1171 | `						 }` |
|    25615 | 1172 | `					 }` |
|    51264 | 1173 | `					 ++iArrTok;` |
|        2 | 1174 | `				 }` |
|    63636 | 1175 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1176 | `					 /* Recurse and process this expression */` |
|    51158 | 1177 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    51158 | 1178 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1179 | `						 return rc;` |
|        - | 1180 | `					 }` |
|        - | 1181 | `					 /* Link the node to it's index */` |
|    51158 | 1182 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    25578 | 1183 | `				 }` |
|        - | 1184 | `				 /* Link the node to the tree */` |
|    63636 | 1185 | `				 pNode->pLeft = apNode[iLeft];` |
|    63636 | 1186 | `				 pNode->pRight = 0;` |
|    63636 | 1187 | `				 apNode[iLeft] = 0;` |
|   178532 | 1188 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   114898 | 1189 | `					 apNode[iNest] = 0;` |
|    57450 | 1190 | `				 }` |
|    31819 | 1191 | `			 }else{` |
|        - | 1192 | `				 /* Member access operators [i.e: '->','::'] */` |
|    95116 | 1193 | `				  iRight = iCur + 1;` |
|    95116 | 1194 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1195 | `					 iRight++;` |
|      ! 0 | 1196 | `				 }` |
|    95116 | 1197 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1198 | `					 /* Syntax error */` |
|        5 | 1199 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1200 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1201 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1202 | `					 }` |
|        5 | 1203 | `					 return rc;` |
|        - | 1204 | `				 }` |
|        - | 1205 | `				 /* Link the node to the tree */` |
|    95112 | 1206 | `				 pNode->pLeft = apNode[iLeft];` |
|    95112 | 1207 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|    95018 | 1208 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1209 | `						 /* Syntax error */` |
|      ! 0 | 1210 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1211 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1212 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1213 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1214 | `						 }` |
|      ! 0 | 1215 | `						 return rc;` |
|        - | 1216 | `				 }` |
|    95112 | 1217 | `				 pNode->pRight = apNode[iRight];` |
|    95112 | 1218 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1219 | `			 }` |
|   197794 | 1220 | `		 }` |
|  1621256 | 1221 | `		 iLeft = iCur;` |
|   810629 | 1222 | `	 }` |
|        - | 1223 | `	 /* Handle left associative (new, clone) operators */` |
|  3065516 | 1224 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2598956 | 1225 | `		 if( apNode[iCur] == 0 ){` |
|  1386070 | 1226 | `			 continue;` |
|        - | 1227 | `		 }` |
|  1212888 | 1228 | `		 pNode = apNode[iCur];` |
|  1212888 | 1229 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1230 | `			 SyToken *pToken;` |
|        - | 1231 | `			 /* Get the left node */` |
|    12768 | 1232 | `			 iLeft = iCur + 1;` |
|    25508 | 1233 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    12742 | 1234 | `				 iLeft++;` |
|        2 | 1235 | `			 }` |
|    12768 | 1236 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1237 | `				  /* Syntax error */` |
|      ! 0 | 1238 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1239 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1240 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1241 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1242 | `				 }` |
|      ! 0 | 1243 | `				 return rc;` |
|        - | 1244 | `			 }` |
|        - | 1245 | `			 /* Make sure the operand are of a valid type */` |
|    12768 | 1246 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
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
|    12752 | 1270 | `				 if( apNode[iLeft]->pOp == 0 ){` |
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
|    12768 | 1286 | `			 pNode->pLeft = apNode[iLeft];` |
|    12768 | 1287 | `			 apNode[iLeft] = 0;` |
|    12768 | 1288 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6383 | 1289 | `		 }` |
|   606445 | 1290 | `	 }` |
|        - | 1291 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   466562 | 1292 | `	 iLeft = -1;` |
|  3068198 | 1293 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2598956 | 1294 | `		 if( apNode[iCur] == 0 ){` |
|  1386070 | 1295 | `			 continue;` |
|        - | 1296 | `		 }` |
|  1212888 | 1297 | `		 pNode = apNode[iCur];` |
|  1212888 | 1298 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7482 | 1299 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2699 | 1300 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1301 | `					 /* Link the node to the tree */` |
|     2700 | 1302 | `					 pNode->pLeft = apNode[iLeft];` |
|     2700 | 1303 | `					 apNode[iLeft] = 0;` |
|     1349 | 1304 | `			 }` |
|     5081 | 1305 | `		  }` |
|  1215570 | 1306 | `		 iLeft = iCur;` |
|   609127 | 1307 | `	  }` |
|   469244 | 1308 | `	 iLeft = -1;` |
|  3068198 | 1309 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2598956 | 1310 | `		 if( apNode[iCur] == 0 ){` |
|  1388768 | 1311 | `			 continue;` |
|        - | 1312 | `		 }` |
|  1210190 | 1313 | `		 pNode = apNode[iCur];` |
|  1210190 | 1314 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7464 | 1315 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     7466 | 1316 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1317 | `					 /* Syntax error */` |
|      ! 0 | 1318 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1319 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1320 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1321 | `					 }` |
|      ! 0 | 1322 | `					 return rc;` |
|        - | 1323 | `			 }` |
|        - | 1324 | `			 /* Link the node to the tree */` |
|     7466 | 1325 | `			 pNode->pLeft = apNode[iLeft];` |
|     7466 | 1326 | `			 apNode[iLeft] = 0;` |
|        - | 1327 | `			 /* Mark as pre-increment/decrement node */` |
|     7466 | 1328 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3732 | 1329 | `		  }` |
|  1210190 | 1330 | `		 iLeft = iCur;` |
|   605096 | 1331 | `	 }` |
|        - | 1332 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   469244 | 1333 | `	  iLeft = 0;` |
|  3068192 | 1334 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2598952 | 1335 | `		  if( apNode[iCur] ){` |
|  1202722 | 1336 | `			  pNode = apNode[iCur];` |
|  1202722 | 1337 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    33470 | 1338 | `				  if( iLeft > 0 ){` |
|        - | 1339 | `					  /* Link the node to the tree */` |
|    33468 | 1340 | `					  pNode->pLeft = apNode[iLeft];` |
|    33468 | 1341 | `					  apNode[iLeft] = 0;` |
|    33468 | 1342 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       10 | 1343 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1344 | `							   /* Syntax error */` |
|      ! 0 | 1345 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1346 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1347 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1348 | `							  }` |
|      ! 0 | 1349 | `							  return rc;` |
|        - | 1350 | `						  }` |
|        4 | 1351 | `					  }` |
|    16735 | 1352 | `				  }else{` |
|        - | 1353 | `					  /* Syntax error */` |
|        3 | 1354 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1355 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1356 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1357 | `					  }` |
|        3 | 1358 | `					  return rc;` |
|        - | 1359 | `				  }` |
|    16733 | 1360 | `			  }` |
|        - | 1361 | `			  /* Save terminal position */` |
|  1202720 | 1362 | `			  iLeft = iCur;` |
|   601359 | 1363 | `		  }` |
|  1299476 | 1364 | `	  }` |
|        - | 1365 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  5161566 | 1366 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  4692334 | 1367 | `		 iLeft = -1;` |
| 30681568 | 1368 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 25989244 | 1369 | `			 if( apNode[iCur] == 0 ){` |
| 16292678 | 1370 | `				 continue;` |
|        - | 1371 | `			 }` |
|  9696568 | 1372 | `			 pNode = apNode[iCur];` |
|  9696568 | 1373 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1374 | `				 /* Get the right node */` |
|   153326 | 1375 | `				 iRight = iCur + 1;` |
|   218052 | 1376 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    64728 | 1377 | `					 iRight++;` |
|        2 | 1378 | `				 }` |
|   153326 | 1379 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1380 | `					 /* Syntax error */` |
|        9 | 1381 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1382 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1383 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1384 | `					 }` |
|        9 | 1385 | `					 return rc;` |
|        - | 1386 | `				 }` |
|   153318 | 1387 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1388 | `					 sxi32  iTmp;` |
|        - | 1389 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       44 | 1390 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1391 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1392 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1393 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1394 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1395 | `						 }` |
|      ! 0 | 1396 | `						 return rc;` |
|        - | 1397 | `					 }` |
|       44 | 1398 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       30 | 1399 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
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
|       14 | 1410 | `					 }` |
|        - | 1411 | `					 /* Swap operands */` |
|       44 | 1412 | `					 iTmp = iRight;` |
|       44 | 1413 | `					 iRight = iLeft;` |
|       44 | 1414 | `					 iLeft = iTmp;` |
|       21 | 1415 | `				 }` |
|        - | 1416 | `				 /* Link the node to the tree */` |
|   153318 | 1417 | `				 pNode->pLeft = apNode[iLeft];` |
|   153318 | 1418 | `				 pNode->pRight = apNode[iRight];` |
|   153318 | 1419 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    76658 | 1420 | `			 }` |
|  9696560 | 1421 | `			 iLeft = iCur;` |
|  4848281 | 1422 | `		 }` |
|  2346164 | 1423 | `	 }` |
|        - | 1424 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1425 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1426 | `	  * we are dealing with a single operator.` |
|        - | 1427 | `	  */` |
|   469234 | 1428 | `	  iLeft = -1;` |
|  3060660 | 1429 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2593176 | 1430 | `		  if( apNode[iCur] == 0 ){` |
|  1735778 | 1431 | `			  continue;` |
|        - | 1432 | `		  }` |
|   857400 | 1433 | `		  pNode = apNode[iCur];` |
|   857400 | 1434 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1750 | 1435 | `			  sxi32 iNest = 1;` |
|     1750 | 1436 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1437 | `				  /* Missing condition */` |
|        3 | 1438 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1439 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1440 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1441 | `				  }` |
|        3 | 1442 | `				  return rc;` |
|        - | 1443 | `			  }` |
|        - | 1444 | `			  /* Get the right node */` |
|     1748 | 1445 | `			  iRight = iCur + 1;` |
|     3712 | 1446 | `			  while( iRight < nToken  ){` |
|     3712 | 1447 | `				  if( apNode[iRight] ){` |
|     3426 | 1448 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1449 | `						  /* Increment nesting level */` |
|      ! 0 | 1450 | `						  ++iNest;` |
|     3426 | 1451 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1452 | `						  /* Decrement nesting level */` |
|     1748 | 1453 | `						  --iNest;` |
|     1748 | 1454 | `						  if( iNest <= 0 ){` |
|     1748 | 1455 | `							  break;` |
|        - | 1456 | `						  }` |
|      ! 0 | 1457 | `					  }` |
|      839 | 1458 | `				  }` |
|     1966 | 1459 | `				  iRight++;` |
|        2 | 1460 | `			  }` |
|     1748 | 1461 | `			  if( iRight > iCur + 1 ){` |
|        - | 1462 | `				  /* Recurse and process the then expression */` |
|     1680 | 1463 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1680 | 1464 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1465 | `					  return rc;` |
|        - | 1466 | `				  }` |
|        - | 1467 | `				  /* Link the node to the tree */` |
|     1680 | 1468 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      839 | 1469 | `			  }else{` |
|        - | 1470 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1471 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1472 | `			  }` |
|     1748 | 1473 | `			  apNode[iCur + 1] = 0;` |
|     1748 | 1474 | `			  if( iRight + 1 < nToken ){` |
|        - | 1475 | `				  /* Recurse and process the else expression */` |
|     1748 | 1476 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1748 | 1477 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1478 | `					  return rc;` |
|        - | 1479 | `				  }` |
|        - | 1480 | `				  /* Link the node to the tree */` |
|     1748 | 1481 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1748 | 1482 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      875 | 1483 | `			  }else{` |
|      ! 0 | 1484 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1485 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1486 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1487 | `				 }` |
|      ! 0 | 1488 | `				 return rc;` |
|        - | 1489 | `			  }` |
|        - | 1490 | `			  /* Point to the condition */` |
|     1748 | 1491 | `			  pNode->pCond  = apNode[iLeft];` |
|     1748 | 1492 | `			  apNode[iLeft] = 0;` |
|     1748 | 1493 | `			  break;` |
|        - | 1494 | `		  }` |
|   855652 | 1495 | `		  iLeft = iCur;` |
|   427827 | 1496 | `	  }` |
|        - | 1497 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1498 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1499 | `	  * so there is no need for a precedence loop here.` |
|        - | 1500 | `	  */` |
|   469232 | 1501 | `	 iRight = -1;` |
|  3068058 | 1502 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  2598868 | 1503 | `		 if( apNode[iCur] == 0 ){` |
|  1936368 | 1504 | `			 continue;` |
|        - | 1505 | `		 }` |
|   662502 | 1506 | `		 pNode = apNode[iCur];` |
|   662502 | 1507 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1508 | `			 /* Get the left node */` |
|   193146 | 1509 | `			 iLeft = iCur - 1;` |
|   273386 | 1510 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    80242 | 1511 | `				 iLeft--;` |
|        2 | 1512 | `			 }` |
|   193146 | 1513 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1514 | `				 /* Syntax error */` |
|       39 | 1515 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1516 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1517 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1518 | `				 }` |
|       39 | 1519 | `				 return rc;` |
|        - | 1520 | `			 }` |
|   193108 | 1521 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
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
|   193106 | 1533 | `			 pNode->pLeft = apNode[iRight];` |
|   193106 | 1534 | `			 pNode->pRight = apNode[iLeft];` |
|   193106 | 1535 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    96552 | 1536 | `		 }` |
|   662462 | 1537 | `		 iRight = iCur;` |
|   331232 | 1538 | `	 }` |
|        - | 1539 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2345952 | 1540 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  1876762 | 1541 | `		 iLeft = -1;` |
| 12272058 | 1542 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 10395298 | 1543 | `			 if( apNode[iCur] == 0 ){` |
|  8518124 | 1544 | `				 continue;` |
|        - | 1545 | `			 }` |
|  1877176 | 1546 | `			 pNode = apNode[iCur];` |
|  1877176 | 1547 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
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
|  1877176 | 1566 | `			 iLeft = iCur;` |
|   938589 | 1567 | `		 }` |
|   938382 | 1568 | `	 }` |
|        - | 1569 | `	 /* Point to the root of the expression tree */` |
|  2598796 | 1570 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2129626 | 1571 | `		 if( apNode[iCur] ){` |
|   418790 | 1572 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       22 | 1573 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       22 | 1574 | `				  if( rc != SXERR_ABORT ){` |
|       22 | 1575 | `					  rc = SXERR_SYNTAX;` |
|       10 | 1576 | `				  }` |
|       22 | 1577 | `				  return rc;` |
|        - | 1578 | `			 }` |
|   418770 | 1579 | `			 apNode[0] = apNode[iCur];` |
|   418770 | 1580 | `			 apNode[iCur] = 0;` |
|   209384 | 1581 | `		 }` |
|  1064804 | 1582 | `	 }` |
|   469172 | 1583 | `	 return SXRET_OK;` |
|   424786 | 1584 | ` }` |
|        - | 1585 | ` /*` |
|        - | 1586 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1587 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1588 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1589 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1590 | `  */` |
|   544476 | 1591 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1592 |  |
|        - | 1593 | `	ph7_expr_node **apNode;` |
|        - | 1594 | `	ph7_expr_node *pNode;` |
|        - | 1595 | `	sxi32 rc;` |
|        - | 1596 | `	/* Reset node container */` |
|   544478 | 1597 | `	SySetReset(pExprNode);` |
|   544478 | 1598 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1599 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1600 | `	{` |
|   544478 | 1601 | `		int iLastWasTerm = 0;` |
|  2983548 | 1602 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2439106 | 1603 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2439106 | 1604 | `			if( rc != SXRET_OK ){` |
|       35 | 1605 | `				return rc;` |
|        - | 1606 | `			}` |
|        - | 1607 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2439072 | 1608 | `			if( pNode->xCode ){` |
|        - | 1609 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1270416 | 1610 | `				iLastWasTerm = 1;` |
|  1803865 | 1611 | `			}else if( pNode->pOp ){` |
|        - | 1612 | `				/* Operator node */` |
|   587706 | 1613 | `				iLastWasTerm = 0;` |
|   293854 | 1614 | `			}else{` |
|        - | 1615 | `				/* Delimiter: ')' and ']' end terms */` |
|   580954 | 1616 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1617 | `			}` |
|        - | 1618 | `			/* Save the extracted node */` |
|  2439072 | 1619 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1620 | `		}` |
|        - | 1621 | `	}` |
|   544444 | 1622 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1623 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1624 | `		*ppRoot = 0;` |
|      ! 0 | 1625 | `		return SXRET_OK;` |
|        - | 1626 | `	}` |
|   544444 | 1627 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1628 | `	/* Make sure we are dealing with valid nodes */` |
|   544444 | 1629 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   544444 | 1630 | `	if( rc != SXRET_OK ){` |
|        - | 1631 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1632 | `		 * cleanup the mess left behind.` |
|        - | 1633 | `		 */` |
|       47 | 1634 | `		*ppRoot = 0;` |
|       47 | 1635 | `		return rc;` |
|        - | 1636 | `	}` |
|        - | 1637 | `	/* Build the tree */` |
|   544398 | 1638 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   544398 | 1639 | `	if( rc != SXRET_OK ){` |
|        - | 1640 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       78 | 1641 | `		*ppRoot = 0;` |
|       78 | 1642 | `		return rc;` |
|        - | 1643 | `	}` |
|        - | 1644 | `	/* Point to the root of the tree */` |
|   544322 | 1645 | `	*ppRoot = apNode[0];` |
|   544322 | 1646 | `	return SXRET_OK;` |
|   272240 | 1647 |  |
|        - | 1648 |  |
