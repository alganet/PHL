# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 825/975 lines (84.62%)

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
|   767004 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  255 |  |
|   767006 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
| 12142373 |  259 | `	for(;;){` |
| 24284748 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 24284748 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3055976 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1527989 |  266 | `		}else{` |
| 21228774 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 24284748 |  269 | `		if( rc == 0 ){` |
|   770316 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   766688 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|     3630 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      210 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|     3422 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|     1655 |  288 | `		}` |
| 23517744 |  289 | `		++n; /* Next operator in the table */` |
|        2 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   383504 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   390138 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  300 |  |
|   390140 |  301 | `	SyToken *pCur = pIn;` |
|   390140 |  302 | `	sxi32 iNest = 1;` |
|  2249022 |  303 | `	for(;;){` |
|  4498046 |  304 | `		if( pCur >= pEnd ){` |
|      122 |  305 | `			break;` |
|        - |  306 | `		}` |
|  4497926 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   249284 |  309 | `			iNest++;` |
|  4373285 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   639302 |  312 | `			iNest--;` |
|   639302 |  313 | `			if( iNest <= 0 ){` |
|   390020 |  314 | `				break;` |
|        - |  315 | `			}` |
|   124641 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  4107908 |  318 | `		pCur++;` |
|        2 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   390140 |  321 | `	*ppEnd = pCur;` |
|   390140 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    11800 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  333 |  |
|    17638 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    11711 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|      138 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    11666 |  339 | `	if( bCheckFunc ){` |
|       86 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       65 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       28 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|       20 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    11640 |  347 | `	return FALSE;` |
|     5902 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|   675206 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|   675208 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       10 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       10 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        4 |  364 | `	}` |
|   675208 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3655596 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2980420 |  367 | `		if( apNode[i]->xCode == PH7_CompileShortArray ){` |
|        - |  368 | `			/* Short array literal: brackets are self-contained, skip */` |
|      152 |  369 | `			continue;` |
|        - |  370 | `		}` |
|  2980270 |  371 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   342686 |  372 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    17538 |  373 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  374 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   319238 |  375 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  376 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  377 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  378 | `						 */` |
|   319238 |  379 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   319238 |  380 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   319238 |  381 | `						apNode[i]->pOp = &sFCallOp;` |
|   159618 |  382 | `					}` |
|   159618 |  383 | `			}` |
|   342686 |  384 | `			iParen++;` |
|  2808928 |  385 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   342682 |  386 | `			if( iParen <= 0 ){` |
|        9 |  387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  388 | `				if( rc != SXERR_ABORT ){` |
|        9 |  389 | `					rc = SXERR_SYNTAX;` |
|        4 |  390 | `				}` |
|        9 |  391 | `				return rc;` |
|        - |  392 | `			}` |
|   342674 |  393 | `			iParen--;` |
|  2466242 |  394 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    71628 |  395 | `			iSquare++;` |
|  2259093 |  396 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    71642 |  397 | `			if( iSquare <= 0 ){` |
|        7 |  398 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  399 | `				if( rc != SXERR_ABORT ){` |
|        7 |  400 | `					rc = SXERR_SYNTAX;` |
|        3 |  401 | `				}` |
|        7 |  402 | `				return rc;` |
|        - |  403 | `			}` |
|    71636 |  404 | `			iSquare--;` |
|  2187457 |  405 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  2151635 |  452 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  453 | `			if( iBraces <= 0 ){` |
|       13 |  454 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  455 | `				if( rc != SXERR_ABORT ){` |
|       13 |  456 | `					rc = SXERR_SYNTAX;` |
|        6 |  457 | `				}` |
|       13 |  458 | `				return rc;` |
|        - |  459 | `			}` |
|      ! 0 |  460 | `			iBraces--;` |
|  2151618 |  461 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1810 |  462 | `			if( iQuesty <= 0 ){` |
|        5 |  463 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  464 | `				if( rc != SXERR_ABORT ){` |
|        5 |  465 | `					rc = SXERR_SYNTAX;` |
|        2 |  466 | `				}` |
|        5 |  467 | `				return rc;` |
|        - |  468 | `			}` |
|     1806 |  469 | `			iQuesty--;` |
|  2150712 |  470 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   598852 |  471 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   598852 |  472 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1808 |  473 | `				iQuesty++;` |
|   597949 |  474 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
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
|   299425 |  494 | `		}` |
|  1490121 |  495 | `	}` |
|   675178 |  496 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  497 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  498 | `		if( rc != SXERR_ABORT ){` |
|       17 |  499 | `			rc = SXERR_SYNTAX;` |
|        8 |  500 | `		}` |
|       17 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|   675162 |  503 | `	return SXRET_OK;` |
|   337605 |  504 |  |
|        - |  505 | `/*` |
|        - |  506 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  507 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  508 | ` */` |
|   546478 |  509 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  510 |  |
|   546480 |  511 | `	SyToken *pIn = *ppCur;` |
|        - |  512 | `	/* Jump the first literal seen */` |
|   546480 |  513 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   546462 |  514 | `		pIn++;` |
|   273230 |  515 | `	}` |
|   273263 |  516 | `	for(;;){` |
|   546528 |  517 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       49 |  518 | `			pIn++;` |
|       49 |  519 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       49 |  520 | `				pIn++;` |
|       24 |  521 | `			}` |
|       25 |  522 | `		}else{` |
|   273241 |  523 | `			break;` |
|        - |  524 | `		}` |
|        1 |  525 | `	}` |
|        - |  526 | `	/* Synchronize pointers */` |
|   546480 |  527 | `	*ppCur = pIn;` |
|   546480 |  528 |  |
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
|      172 |  562 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  563 |  |
|      174 |  564 | `	SyToken *pIn = *ppCur;` |
|        - |  565 | `	sxu32 nLine;` |
|        - |  566 | `	sxi32 rc;` |
|        - |  567 | `	/* Jump the 'function' keyword */` |
|      174 |  568 | `	nLine = pIn->nLine;` |
|      174 |  569 | `	pIn++;` |
|      174 |  570 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  571 | `		pIn++;` |
|        1 |  572 | `	}` |
|      174 |  573 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  574 | `		/* Syntax error */` |
|        5 |  575 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  576 | `		if( rc != SXERR_ABORT ){` |
|        5 |  577 | `			rc = SXERR_SYNTAX;` |
|        2 |  578 | `		}` |
|        5 |  579 | `		goto Synchronize;` |
|        - |  580 | `	}` |
|      170 |  581 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      170 |  582 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      170 |  583 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  584 | `		/* Syntax error */` |
|        5 |  585 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  586 | `		if( rc != SXERR_ABORT ){` |
|        5 |  587 | `			rc = SXERR_SYNTAX;` |
|        2 |  588 | `		}` |
|        5 |  589 | `		goto Synchronize;` |
|        - |  590 | `	}` |
|      166 |  591 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  592 | `	/* Skip optional return type declaration ': [?] type' */` |
|      166 |  593 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  594 | `		pIn++; /* Skip ':' */` |
|        - |  595 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  596 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  597 | `			pIn++;` |
|      ! 0 |  598 | `		}` |
|        - |  599 | `		/* Skip the type name (keyword or identifier) */` |
|        5 |  600 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  601 | `			pIn++;` |
|        2 |  602 | `		}` |
|        2 |  603 | `	}` |
|      166 |  604 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       30 |  605 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  606 | `		/* Check if we are dealing with a closure */` |
|       30 |  607 | `		if( nKey == PH7_TKWRD_USE ){` |
|       22 |  608 | `			pIn++; /* Jump the 'use' keyword */` |
|       22 |  609 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  610 | `				/* Syntax error */` |
|        5 |  611 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  612 | `				if( rc != SXERR_ABORT ){` |
|        5 |  613 | `					rc = SXERR_SYNTAX;` |
|        2 |  614 | `				}` |
|        5 |  615 | `				goto Synchronize;` |
|        - |  616 | `			}` |
|       18 |  617 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       18 |  618 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       18 |  619 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  620 | `				/* Syntax error */` |
|        5 |  621 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  622 | `				if( rc != SXERR_ABORT ){` |
|        5 |  623 | `					rc = SXERR_SYNTAX;` |
|        2 |  624 | `				}` |
|        5 |  625 | `				goto Synchronize;` |
|        - |  626 | `			}` |
|       14 |  627 | `			pIn++;` |
|        8 |  628 | `		}else{` |
|        - |  629 | `			/* Syntax error */` |
|        9 |  630 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  631 | `			if( rc != SXERR_ABORT ){` |
|        9 |  632 | `				rc = SXERR_SYNTAX;` |
|        4 |  633 | `			}` |
|        9 |  634 | `			goto Synchronize;` |
|        - |  635 | `		}` |
|        6 |  636 | `	}` |
|      150 |  637 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      150 |  638 | `		pIn++; /* Jump the leading curly '{' */` |
|      150 |  639 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      150 |  640 | `		if( pIn < pEnd ){` |
|      150 |  641 | `			pIn++;` |
|       74 |  642 | `		}` |
|       76 |  643 | `	}else{` |
|        - |  644 | `		/* Syntax error */` |
|      ! 0 |  645 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  646 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  647 | `			return SXERR_ABORT;` |
|        - |  648 | `		}` |
|        - |  649 | `	}` |
|      150 |  650 | `	rc = SXRET_OK;` |
|       86 |  651 | `Synchronize:` |
|        - |  652 | `	/* Synchronize pointers */` |
|      174 |  653 | `	*ppCur = pIn;` |
|      174 |  654 | `	return rc;` |
|       88 |  655 |  |
|        - |  656 | `/*` |
|        - |  657 | ` * Extract a single expression node from the input.` |
|        - |  658 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  659 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  660 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  661 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  662 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  663 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  664 | ` */` |
|  2980556 |  665 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  666 |  |
|        - |  667 | `	ph7_expr_node *pNode;` |
|        - |  668 | `	SyToken *pCur;` |
|        - |  669 | `	sxi32 rc;` |
|        - |  670 | `	/* Allocate a new node */` |
|  2980558 |  671 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2980558 |  672 | `	if( pNode == 0 ){` |
|        - |  673 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  674 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  675 | `		 */` |
|      ! 0 |  676 | `		return SXERR_MEM;` |
|        - |  677 | `	}` |
|        - |  678 | `	/* Zero the structure */` |
|  2980558 |  679 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2980558 |  680 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  681 | `	/* Point to the head of the token stream */` |
|  2980558 |  682 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  683 | `	/* Start collecting tokens */` |
|  2980558 |  684 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  685 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  686 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  687 | `		 */` |
|      154 |  688 | `		pCur++; /* Skip the opening '[' */` |
|      154 |  689 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      154 |  690 | `		if( pCur < pGen->pEnd ){` |
|      154 |  691 | `			pCur++; /* Skip past the closing ']' */` |
|       78 |  692 | `		}else{` |
|      ! 0 |  693 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  694 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  695 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  696 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  697 | `			}` |
|      ! 0 |  698 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  699 | `			return rc;` |
|        - |  700 | `		}` |
|      154 |  701 | `		pNode->xCode = PH7_CompileShortArray;` |
|  2980482 |  702 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  703 | `		/* Point to the instance that describe this operator */` |
|   670512 |  704 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  705 | `		/* Advance the stream cursor */` |
|   670512 |  706 | `		pCur++;` |
|  2645151 |  707 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  708 | `		/* Isolate variable */` |
|  1629426 |  709 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   814720 |  710 | `			pCur++; /* Variable variable */` |
|        2 |  711 | `		}` |
|   814708 |  712 | `		if( pCur < pGen->pEnd ){` |
|   814708 |  713 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  714 | `				/* Variable name */` |
|   814680 |  715 | `				pCur++;` |
|   407369 |  716 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  717 | `				pCur++;` |
|        - |  718 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  719 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  720 | `				if( pCur < pGen->pEnd ){` |
|       18 |  721 | `					pCur++;` |
|       10 |  722 | `				}else{` |
|        5 |  723 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  724 | `					if( rc != SXERR_ABORT ){` |
|        5 |  725 | `						rc = SXERR_SYNTAX;` |
|        2 |  726 | `					}` |
|        5 |  727 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  728 | `					return rc;` |
|        - |  729 | `				}` |
|        8 |  730 | `			}` |
|   407351 |  731 | `		}` |
|   814704 |  732 | `		pNode->xCode = PH7_CompileVariable;` |
|  1902541 |  733 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    36044 |  734 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    36044 |  735 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  736 | `			 /* List/Array node */` |
|    24166 |  737 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  738 | `				 /* Assume a literal */` |
|       17 |  739 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  740 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  741 | `			 }else{` |
|    24150 |  742 | `				 pCur += 2;` |
|        - |  743 | `				 /* Collect array/list tokens */` |
|    24150 |  744 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    24150 |  745 | `				 if( pCur < pGen->pEnd ){` |
|    24148 |  746 | `					 pCur++;` |
|    12075 |  747 | `				 }else{` |
|        - |  748 | `					 /* Syntax error */` |
|        4 |  749 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  750 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  751 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  752 | `						 rc = SXERR_SYNTAX;` |
|        1 |  753 | `					 }` |
|        3 |  754 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  755 | `					 return rc;` |
|        - |  756 | `				 }` |
|    24148 |  757 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    24148 |  758 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  759 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  760 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  761 | `						 /* Syntax error */` |
|        3 |  762 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  763 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  764 | `							 rc = SXERR_SYNTAX;` |
|        1 |  765 | `						 }` |
|        3 |  766 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  767 | `						 return rc;` |
|        - |  768 | `					 }` |
|       12 |  769 | `				 }` |
|        2 |  770 | `			 }` |
|    23960 |  771 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  772 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       33 |  773 | `			 pCur++; /* Skip 'yield' keyword */` |
|       33 |  774 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  775 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  776 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       33 |  777 | `			 pNode->xCode = PH7_CompileYield;` |
|    11864 |  778 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  779 | `			 /* Annonymous function */` |
|      174 |  780 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  781 | `				 /* Assume a literal */` |
|      ! 0 |  782 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  783 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  784 | `			 }else{` |
|        - |  785 | `				 /* Assemble annonymous functions body */` |
|      174 |  786 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      174 |  787 | `				 if( rc != SXRET_OK ){` |
|       25 |  788 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  789 | `					 return rc;` |
|        - |  790 | `				 }` |
|      150 |  791 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  792 | `			  }` |
|    11750 |  793 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  794 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       74 |  795 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       74 |  796 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       38 |  797 | `		 }else{` |
|        - |  798 | `			 /* Assume a literal */` |
|    11604 |  799 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    11604 |  800 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  801 | `		 }` |
|  1477155 |  802 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  803 | `		 /* Constants,function name,namespace path,class name... */` |
|   534862 |  804 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   534862 |  805 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   267432 |  806 | `	 }else{` |
|   924288 |  807 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  808 | `			 /* Point to the code generator routine */` |
|   165440 |  809 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   165440 |  810 | `			 if( pNode->xCode == 0 ){` |
|        3 |  811 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  812 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  813 | `					 rc = SXERR_SYNTAX;` |
|        1 |  814 | `				 }` |
|        3 |  815 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  816 | `				 return rc;` |
|        - |  817 | `			 }` |
|    82718 |  818 | `		 }` |
|        - |  819 | `		/* Advance the stream cursor */` |
|   924286 |  820 | `		pCur++;` |
|        - |  821 | `	 }` |
|        - |  822 | `	/* Point to the end of the token stream */` |
|  2980524 |  823 | `	pNode->pEnd = pCur;` |
|        - |  824 | `	/* Save the node for later processing */` |
|  2980524 |  825 | `	*ppNode = pNode;` |
|        - |  826 | `	/* Synchronize cursors */` |
|  2980524 |  827 | `	pGen->pIn = pCur;` |
|  2980524 |  828 | `	return SXRET_OK;` |
|  1490280 |  829 |  |
|        - |  830 | `/*` |
|        - |  831 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  832 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  833 | ` * level is zero.` |
|        - |  834 | ` */` |
|    68388 |  835 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  836 |  |
|    68390 |  837 | `	SyToken *pCur = pStart;` |
|    68390 |  838 | `	sxi32 iNest = 0;` |
|    68390 |  839 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  840 | `		/* Last expression */` |
|    36960 |  841 | `		return SXERR_EOF;` |
|        - |  842 | `	}` |
|   124572 |  843 | `	while( pCur < pEnd ){` |
|   112046 |  844 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    18906 |  845 | `			break;` |
|        - |  846 | `		}` |
|    93142 |  847 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     4982 |  848 | `			iNest++;` |
|    90652 |  849 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     4984 |  850 | `			iNest--;` |
|     2491 |  851 | `		}` |
|    93142 |  852 | `		pCur++;` |
|        2 |  853 | `	}` |
|    31432 |  854 | `	*ppNext = pCur;` |
|    31432 |  855 | `	return SXRET_OK;` |
|    34196 |  856 |  |
|        - |  857 | `/*` |
|        - |  858 | ` * Free an expression tree.` |
|        - |  859 | ` */` |
|  2550384 |  860 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  861 |  |
|  2550386 |  862 | `	if( pNode->pLeft ){` |
|        - |  863 | `		/* Release the left tree */` |
|   952078 |  864 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   476038 |  865 | `	}` |
|  2550386 |  866 | `	if( pNode->pRight ){` |
|        - |  867 | `		/* Release the right tree */` |
|   497976 |  868 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   248987 |  869 | `	}` |
|  2550386 |  870 | `	if( pNode->pCond ){` |
|        - |  871 | `		/* Release the conditional tree used by the ternary operator */` |
|     1804 |  872 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      901 |  873 | `	}` |
|  2550386 |  874 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  875 | `		ph7_expr_node **apArg;` |
|        - |  876 | `		sxu32 n;` |
|        - |  877 | `		/* Release node arguments */` |
|   338678 |  878 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   714826 |  879 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   376150 |  880 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   188076 |  881 | `		}` |
|   338678 |  882 | `		SySetRelease(&pNode->aNodeArgs);` |
|   169338 |  883 | `	}` |
|        - |  884 | `	/* Finally,release this node */` |
|  2550386 |  885 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2550386 |  886 |  |
|        - |  887 | `/*` |
|        - |  888 | ` * Free an expression tree.` |
|        - |  889 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  890 | ` */` |
|   675240 |  891 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  892 |  |
|        - |  893 | `	ph7_expr_node **apNode;` |
|        - |  894 | `	sxu32 n;` |
|   675242 |  895 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3655764 |  896 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2980524 |  897 | `		if( apNode[n] ){` |
|   675514 |  898 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   337756 |  899 | `		}` |
|  1490263 |  900 | `	}` |
|   675242 |  901 | `	return SXRET_OK;` |
|        2 |  902 |  |
|        - |  903 | `/*` |
|        - |  904 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  905 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  906 | ` */` |
|   216938 |  907 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  908 |  |
|        - |  909 | `	sxi32 iExprOp;` |
|   216940 |  910 | `	if( pNode->pOp == 0 ){` |
|   141020 |  911 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  912 | `	}` |
|    75922 |  913 | `	iExprOp = pNode->pOp->iOp;` |
|    75922 |  914 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    47626 |  915 | `			return TRUE;` |
|        - |  916 | `	}` |
|    28298 |  917 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    28294 |  918 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  919 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  920 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  921 | `				return FALSE;` |
|        1 |  922 | `			}` |
|    28291 |  923 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  924 | `			return FALSE;` |
|        - |  925 | `		}` |
|    28294 |  926 | `		return TRUE;` |
|        - |  927 | `	}` |
|        5 |  928 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  929 | `		return TRUE;` |
|        - |  930 | `	}` |
|        - |  931 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  932 | `	return FALSE;` |
|   108471 |  933 |  |
|        - |  934 | `/* Forward declaration */` |
|        - |  935 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  936 | `/* Macro to check if the given node is a terminal.` |
|        - |  937 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  938 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  939 | ` * linked ternary/elvis node). */` |
|        - |  940 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  941 | `/*` |
|        - |  942 | ` * Buid an expression tree for each given function argument.` |
|        - |  943 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  944 | ` */` |
|   281076 |  945 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  946 |  |
|        - |  947 | `	sxi32 iNest,iCur,iNode;` |
|        - |  948 | `	sxi32 rc;` |
|        - |  949 | `	/* Process function arguments from left to right */` |
|   281078 |  950 | `	iCur = 0;` |
|   299811 |  951 | `	for(;;){` |
|   599624 |  952 | `		if( iCur >= nToken ){` |
|        - |  953 | `			/* No more arguments to process */` |
|   281076 |  954 | `			break;` |
|        - |  955 | `		}` |
|   318550 |  956 | `		iNode = iCur;` |
|   318550 |  957 | `		iNest = 0;` |
|   797052 |  958 | `		while( iCur < nToken ){` |
|   515978 |  959 | `			if( apNode[iCur] ){` |
|   504794 |  960 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    18739 |  961 | `					break;` |
|   467320 |  962 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    25916 |  963 | `					iNest++;` |
|   454363 |  964 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    25906 |  965 | `					iNest--;` |
|    12952 |  966 | `				}` |
|   233659 |  967 | `			}` |
|   478504 |  968 | `			iCur++;` |
|        2 |  969 | `		}` |
|   318550 |  970 | `		if( iCur > iNode ){` |
|   318546 |  971 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  972 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  973 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  974 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  975 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  976 | `					apNode[iNode] = 0;` |
|      ! 0 |  977 | `			}` |
|   318548 |  978 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   318548 |  979 | `			if( apNode[iNode] ){` |
|        - |  980 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   318548 |  981 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   159275 |  982 | `			}else{` |
|        - |  983 | `				/* Empty function argument */` |
|      ! 0 |  984 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  985 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  986 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  987 | `				}` |
|      ! 0 |  988 | `				return rc;` |
|        - |  989 | `			}` |
|   159275 |  990 | `		}else{` |
|        3 |  991 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  992 | `			if( rc != SXERR_ABORT ){` |
|        3 |  993 | `				rc = SXERR_SYNTAX;` |
|        1 |  994 | `			}` |
|        3 |  995 | `			return rc;` |
|        - |  996 | `		}` |
|        - |  997 | `		/* Jump trailing comma */` |
|   318548 |  998 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    37474 |  999 | `			iCur++;` |
|    37474 | 1000 | `			if( iCur >= nToken ){` |
|        - | 1001 | `				/* missing function argument */` |
|      ! 0 | 1002 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 | 1003 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1004 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1005 | `				}` |
|      ! 0 | 1006 | `				return rc;` |
|        - | 1007 | `			}` |
|    18736 | 1008 | `		}` |
|        2 | 1009 | `	}` |
|   281076 | 1010 | `	return SXRET_OK;` |
|   140540 | 1011 |  |
|        - | 1012 | ` /*` |
|        - | 1013 | `  * Create an expression tree from an array of tokens.` |
|        - | 1014 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1015 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1016 | `  */` |
|  1081288 | 1017 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1018 | ` {` |
|        - | 1019 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1020 | `	 ph7_expr_node *pNode;` |
|        - | 1021 | `	 sxi32 iCur;` |
|        - | 1022 | `	 sxi32 rc;` |
|  1081290 | 1023 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1024 | `		 /* TICKET 1433-17: self evaluating node */` |
|   498206 | 1025 | `		 return SXRET_OK;` |
|        - | 1026 | `	 }` |
|        - | 1027 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3582072 | 1028 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1029 | `		 sxi32 iNest;` |
|        - | 1030 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1031 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1032 | `		  */` |
|  2998990 | 1033 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2975552 | 1034 | `			 continue;` |
|        - | 1035 | `		 }` |
|    23440 | 1036 | `		 iNest = 1;` |
|    23440 | 1037 | `		 iLeft = iCur;` |
|        - | 1038 | `		 /* Find the closing parenthesis */` |
|    23440 | 1039 | `		 iCur++;` |
|   156104 | 1040 | `		 while( iCur < nToken ){` |
|   156104 | 1041 | `			 if( apNode[iCur] ){` |
|   156104 | 1042 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1043 | `					 /* Decrement nesting level */` |
|    40752 | 1044 | `					 iNest--;` |
|    40752 | 1045 | `					 if( iNest <= 0 ){` |
|    23440 | 1046 | `						 break;` |
|        2 | 1047 | `					 }` |
|   124010 | 1048 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1049 | `					 /* Increment nesting level */` |
|    17314 | 1050 | `					 iNest++;` |
|     8656 | 1051 | `				 }` |
|    66332 | 1052 | `			 }` |
|   132666 | 1053 | `			 iCur++;` |
|        2 | 1054 | `		 }` |
|    23440 | 1055 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1056 | `			 /* Recurse and process this expression */` |
|    23440 | 1057 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    23440 | 1058 | `			 if( rc != SXRET_OK ){` |
|        3 | 1059 | `				 return rc;` |
|        - | 1060 | `			 }` |
|    11718 | 1061 | `		 }` |
|        - | 1062 | `		 /* Free the left and right nodes */` |
|    23438 | 1063 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    23438 | 1064 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    23438 | 1065 | `		 apNode[iLeft] = 0;` |
|    23438 | 1066 | `		 apNode[iCur] = 0;` |
|    11720 | 1067 | `	 }` |
|        - | 1068 | `	  /* Process expressions enclosed in braces */` |
|  3732150 | 1069 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1070 | `		 sxi32 iNest;` |
|        - | 1071 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1072 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1073 | `		  */` |
|  3155080 | 1074 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  3155080 | 1075 | `			 continue;` |
|        - | 1076 | `		 }` |
|      ! 0 | 1077 | `		 iNest = 1;` |
|      ! 0 | 1078 | `		 iLeft = iCur;` |
|        - | 1079 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1080 | `		 iCur++;` |
|      ! 0 | 1081 | `		 while( iCur < nToken ){` |
|      ! 0 | 1082 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1083 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1084 | `					 /* Decrement nesting level */` |
|      ! 0 | 1085 | `					 iNest--;` |
|      ! 0 | 1086 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1087 | `						 break;` |
|      ! 0 | 1088 | `					 }` |
|      ! 0 | 1089 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1090 | `					 /* Increment nesting level */` |
|      ! 0 | 1091 | `					 iNest++;` |
|      ! 0 | 1092 | `				 }` |
|      ! 0 | 1093 | `			 }` |
|      ! 0 | 1094 | `			 iCur++;` |
|      ! 0 | 1095 | `		 }` |
|      ! 0 | 1096 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1097 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1098 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1099 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1100 | `				 return rc;` |
|        - | 1101 | `			 }` |
|      ! 0 | 1102 | `		 }` |
|        - | 1103 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1104 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1105 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1106 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1107 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1108 | `	 }` |
|        - | 1109 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   577072 | 1110 | `	 iLeft = -1;` |
|  3732138 | 1111 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3155074 | 1112 | `		 if( apNode[iCur] == 0 ){` |
|  1228300 | 1113 | `			 continue;` |
|        - | 1114 | `		 }` |
|  1926776 | 1115 | `		 pNode = apNode[iCur];` |
|  1926776 | 1116 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   497960 | 1117 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1118 | `				 /* Collect function arguments */` |
|   319234 | 1119 | `				 sxi32 iPtr = 0;` |
|   319234 | 1120 | `				 sxi32 nFuncTok = 0;` |
|  1154444 | 1121 | `				 while( nFuncTok + iCur < nToken ){` |
|  1154444 | 1122 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1143260 | 1123 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   330852 | 1124 | `							 iPtr++;` |
|   977835 | 1125 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   330852 | 1126 | `							 iPtr--;` |
|   330852 | 1127 | `							 if( iPtr <= 0 ){` |
|   319234 | 1128 | `								 break;` |
|        - | 1129 | `							 }` |
|     5809 | 1130 | `						 }` |
|   412013 | 1131 | `					 }` |
|   835212 | 1132 | `					 nFuncTok++;` |
|        2 | 1133 | `				 }` |
|   319234 | 1134 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1135 | `					 /* Syntax error */` |
|      ! 0 | 1136 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1137 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1138 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1139 | `					 }` |
|      ! 0 | 1140 | `					 return rc;` |
|        - | 1141 | `				 }` |
|   319234 | 1142 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1143 | `					 /* Syntax error */` |
|      ! 0 | 1144 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1145 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1146 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1147 | `					 }` |
|      ! 0 | 1148 | `					 return rc;` |
|        - | 1149 | `				 }` |
|   319234 | 1150 | `				 if( nFuncTok > 1 ){` |
|        - | 1151 | `					 /* Process function arguments */` |
|   281078 | 1152 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   281078 | 1153 | `					 if( rc != SXRET_OK ){` |
|        3 | 1154 | `						 return rc;` |
|        - | 1155 | `					 }` |
|   140537 | 1156 | `				 }` |
|        - | 1157 | `				 /* Link the node to the tree */` |
|   319232 | 1158 | `				 pNode->pLeft = apNode[iLeft];` |
|   319232 | 1159 | `				 apNode[iLeft] = 0;` |
|  1154436 | 1160 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   835206 | 1161 | `					 apNode[iCur+iPtr] = 0;` |
|   417604 | 1162 | `				 }` |
|   338343 | 1163 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1164 | `				 /* Subscripting */` |
|    71636 | 1165 | `				 sxi32 iArrTok = iCur + 1;` |
|    71636 | 1166 | `				 sxi32 iNest = 1;` |
|    71703 | 1167 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1168 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1169 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    71634 | 1170 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1171 | `						 /* Syntax error */` |
|      ! 0 | 1172 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1173 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1174 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1175 | `						 }` |
|      ! 0 | 1176 | `						 return rc;` |
|        - | 1177 | `				 }` |
|        - | 1178 | `				 /* Collect index tokens */` |
|   129348 | 1179 | `				 while( iArrTok < nToken ){` |
|   129348 | 1180 | `					 if( apNode[iArrTok] ){` |
|   129316 | 1181 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1182 | `							 /* Increment nesting level */` |
|      ! 0 | 1183 | `							 iNest++;` |
|   129316 | 1184 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1185 | `							 /* Decrement nesting level */` |
|    71636 | 1186 | `							 iNest--;` |
|    71636 | 1187 | `							 if( iNest <= 0 ){` |
|    71636 | 1188 | `								 break;` |
|        - | 1189 | `							 }` |
|      ! 0 | 1190 | `						 }` |
|    28840 | 1191 | `					 }` |
|    57714 | 1192 | `					 ++iArrTok;` |
|        2 | 1193 | `				 }` |
|    71636 | 1194 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1195 | `					 /* Recurse and process this expression */` |
|    57604 | 1196 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    57604 | 1197 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1198 | `						 return rc;` |
|        - | 1199 | `					 }` |
|        - | 1200 | `					 /* Link the node to it's index */` |
|    57604 | 1201 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    28801 | 1202 | `				 }` |
|        - | 1203 | `				 /* Link the node to the tree */` |
|    71636 | 1204 | `				 pNode->pLeft = apNode[iLeft];` |
|    71636 | 1205 | `				 pNode->pRight = 0;` |
|    71636 | 1206 | `				 apNode[iLeft] = 0;` |
|   200982 | 1207 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   129348 | 1208 | `					 apNode[iNest] = 0;` |
|    64675 | 1209 | `				 }` |
|    35819 | 1210 | `			 }else{` |
|        - | 1211 | `				 /* Member access operators [i.e: '->','::'] */` |
|   107094 | 1212 | `				  iRight = iCur + 1;` |
|   107094 | 1213 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1214 | `					 iRight++;` |
|      ! 0 | 1215 | `				 }` |
|   107094 | 1216 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1217 | `					 /* Syntax error */` |
|        5 | 1218 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1219 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1220 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1221 | `					 }` |
|        5 | 1222 | `					 return rc;` |
|        - | 1223 | `				 }` |
|        - | 1224 | `				 /* Link the node to the tree */` |
|   107090 | 1225 | `				 pNode->pLeft = apNode[iLeft];` |
|   107090 | 1226 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|   106962 | 1227 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1228 | `						 /* Syntax error */` |
|      ! 0 | 1229 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1230 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1231 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1232 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1233 | `						 }` |
|      ! 0 | 1234 | `						 return rc;` |
|        - | 1235 | `				 }` |
|   107090 | 1236 | `				 pNode->pRight = apNode[iRight];` |
|   107090 | 1237 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1238 | `			 }` |
|   248976 | 1239 | `		 }` |
|  1926770 | 1240 | `		 iLeft = iCur;` |
|   963386 | 1241 | `	 }` |
|        - | 1242 | `	 /* Handle left associative (new, clone) operators */` |
|  3732118 | 1243 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3155054 | 1244 | `		 if( apNode[iCur] == 0 ){` |
|  1740616 | 1245 | `			 continue;` |
|        - | 1246 | `		 }` |
|  1414440 | 1247 | `		 pNode = apNode[iCur];` |
|  1414440 | 1248 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1249 | `			 SyToken *pToken;` |
|        - | 1250 | `			 /* Get the left node */` |
|    14366 | 1251 | `			 iLeft = iCur + 1;` |
|    28704 | 1252 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    14340 | 1253 | `				 iLeft++;` |
|        2 | 1254 | `			 }` |
|    14366 | 1255 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1256 | `				  /* Syntax error */` |
|      ! 0 | 1257 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1258 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1259 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1260 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1261 | `				 }` |
|      ! 0 | 1262 | `				 return rc;` |
|        - | 1263 | `			 }` |
|        - | 1264 | `			 /* Make sure the operand are of a valid type */` |
|    14366 | 1265 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1266 | `				 /* Clone:` |
|        - | 1267 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1268 | `				  *  ++ function call (including annonymous)` |
|        - | 1269 | `				  *  ++ array member` |
|        - | 1270 | `				  *  ++ 'new' operator` |
|        - | 1271 | `				  * Example:` |
|        - | 1272 | `				  *   clone $pObj;` |
|        - | 1273 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1274 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1275 | `				  */` |
|       18 | 1276 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1277 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1278 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1279 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1280 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1281 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1282 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1283 | `						 }` |
|      ! 0 | 1284 | `						 return rc;` |
|        - | 1285 | `					 }` |
|        7 | 1286 | `				 }` |
|       10 | 1287 | `			 }else{` |
|        - | 1288 | `				 /* New */` |
|    14350 | 1289 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       14 | 1290 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       14 | 1291 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1292 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1293 | `						 /* Syntax error */` |
|      ! 0 | 1294 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1295 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1296 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1297 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1298 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1299 | `						 }` |
|      ! 0 | 1300 | `						 return rc;` |
|        - | 1301 | `					 }` |
|        6 | 1302 | `				 }` |
|        - | 1303 | `			 }` |
|        - | 1304 | `			  /* Link the node to the tree */` |
|    14366 | 1305 | `			 pNode->pLeft = apNode[iLeft];` |
|    14366 | 1306 | `			 apNode[iLeft] = 0;` |
|    14366 | 1307 | `			 pNode->pRight = 0; /* Paranoid */` |
|     7182 | 1308 | `		 }` |
|   707221 | 1309 | `	 }` |
|        - | 1310 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   577066 | 1311 | `	 iLeft = -1;` |
|  3735124 | 1312 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3155054 | 1313 | `		 if( apNode[iCur] == 0 ){` |
|  1740616 | 1314 | `			 continue;` |
|        - | 1315 | `		 }` |
|  1414440 | 1316 | `		 pNode = apNode[iCur];` |
|  1414440 | 1317 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8414 | 1318 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     3024 | 1319 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1320 | `					 /* Link the node to the tree */` |
|     3026 | 1321 | `					 pNode->pLeft = apNode[iLeft];` |
|     3026 | 1322 | `					 apNode[iLeft] = 0;` |
|     1512 | 1323 | `			 }` |
|     5709 | 1324 | `		  }` |
|  1417446 | 1325 | `		 iLeft = iCur;` |
|   710227 | 1326 | `	  }` |
|   580072 | 1327 | `	 iLeft = -1;` |
|  3735124 | 1328 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3155054 | 1329 | `		 if( apNode[iCur] == 0 ){` |
|  1743640 | 1330 | `			 continue;` |
|        - | 1331 | `		 }` |
|  1411416 | 1332 | `		 pNode = apNode[iCur];` |
|  1411416 | 1333 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     8394 | 1334 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     8396 | 1335 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1336 | `					 /* Syntax error */` |
|      ! 0 | 1337 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1338 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1339 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1340 | `					 }` |
|      ! 0 | 1341 | `					 return rc;` |
|        - | 1342 | `			 }` |
|        - | 1343 | `			 /* Link the node to the tree */` |
|     8396 | 1344 | `			 pNode->pLeft = apNode[iLeft];` |
|     8396 | 1345 | `			 apNode[iLeft] = 0;` |
|        - | 1346 | `			 /* Mark as pre-increment/decrement node */` |
|     8396 | 1347 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     4197 | 1348 | `		  }` |
|  1411416 | 1349 | `		 iLeft = iCur;` |
|   705709 | 1350 | `	 }` |
|        - | 1351 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   580072 | 1352 | `	  iLeft = 0;` |
|  3735118 | 1353 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  3155050 | 1354 | `		  if( apNode[iCur] ){` |
|  1403018 | 1355 | `			  pNode = apNode[iCur];` |
|  1403018 | 1356 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    37528 | 1357 | `				  if( iLeft > 0 ){` |
|        - | 1358 | `					  /* Link the node to the tree */` |
|    37526 | 1359 | `					  pNode->pLeft = apNode[iLeft];` |
|    37526 | 1360 | `					  apNode[iLeft] = 0;` |
|    37526 | 1361 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       10 | 1362 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1363 | `							   /* Syntax error */` |
|      ! 0 | 1364 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1365 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1366 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1367 | `							  }` |
|      ! 0 | 1368 | `							  return rc;` |
|        - | 1369 | `						  }` |
|        4 | 1370 | `					  }` |
|    18764 | 1371 | `				  }else{` |
|        - | 1372 | `					  /* Syntax error */` |
|        3 | 1373 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1374 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1375 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1376 | `					  }` |
|        3 | 1377 | `					  return rc;` |
|        - | 1378 | `				  }` |
|    18762 | 1379 | `			  }` |
|        - | 1380 | `			  /* Save terminal position */` |
|  1403016 | 1381 | `			  iLeft = iCur;` |
|   701507 | 1382 | `		  }` |
|  1577525 | 1383 | `	  }` |
|        - | 1384 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  6380674 | 1385 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5800614 | 1386 | `		 iLeft = -1;` |
| 37350828 | 1387 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 31550224 | 1388 | `			 if( apNode[iCur] == 0 ){` |
| 20136882 | 1389 | `				 continue;` |
|        - | 1390 | `			 }` |
| 11413344 | 1391 | `			 pNode = apNode[iCur];` |
| 11413344 | 1392 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1393 | `				 /* Get the right node */` |
|   172162 | 1394 | `				 iRight = iCur + 1;` |
|   244832 | 1395 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    72672 | 1396 | `					 iRight++;` |
|        2 | 1397 | `				 }` |
|   172162 | 1398 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1399 | `					 /* Syntax error */` |
|        9 | 1400 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1401 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1402 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1403 | `					 }` |
|        9 | 1404 | `					 return rc;` |
|        - | 1405 | `				 }` |
|   172154 | 1406 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1407 | `					 sxi32  iTmp;` |
|        - | 1408 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1409 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1410 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1411 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1412 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1413 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1414 | `						 }` |
|      ! 0 | 1415 | `						 return rc;` |
|        - | 1416 | `					 }` |
|       46 | 1417 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1418 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1419 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1420 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1421 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1422 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1423 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1424 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1425 | `									 }` |
|      ! 0 | 1426 | `									 return rc;` |
|        - | 1427 | `							 }` |
|      ! 0 | 1428 | `						 }` |
|       15 | 1429 | `					 }` |
|        - | 1430 | `					 /* Swap operands */` |
|       46 | 1431 | `					 iTmp = iRight;` |
|       46 | 1432 | `					 iRight = iLeft;` |
|       46 | 1433 | `					 iLeft = iTmp;` |
|       22 | 1434 | `				 }` |
|        - | 1435 | `				 /* Link the node to the tree */` |
|   172154 | 1436 | `				 pNode->pLeft = apNode[iLeft];` |
|   172154 | 1437 | `				 pNode->pRight = apNode[iRight];` |
|   172154 | 1438 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    86076 | 1439 | `			 }` |
| 11413336 | 1440 | `			 iLeft = iCur;` |
|  5706669 | 1441 | `		 }` |
|  2900304 | 1442 | `	 }` |
|        - | 1443 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1444 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1445 | `	  * we are dealing with a single operator.` |
|        - | 1446 | `	  */` |
|   580062 | 1447 | `	  iLeft = -1;` |
|  3727350 | 1448 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  3149094 | 1449 | `		  if( apNode[iCur] == 0 ){` |
|  2133298 | 1450 | `			  continue;` |
|        - | 1451 | `		  }` |
|  1015798 | 1452 | `		  pNode = apNode[iCur];` |
|  1015798 | 1453 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1806 | 1454 | `			  sxi32 iNest = 1;` |
|     1806 | 1455 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1456 | `				  /* Missing condition */` |
|        3 | 1457 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1458 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1459 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1460 | `				  }` |
|        3 | 1461 | `				  return rc;` |
|        - | 1462 | `			  }` |
|        - | 1463 | `			  /* Get the right node */` |
|     1804 | 1464 | `			  iRight = iCur + 1;` |
|     3830 | 1465 | `			  while( iRight < nToken  ){` |
|     3830 | 1466 | `				  if( apNode[iRight] ){` |
|     3538 | 1467 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1468 | `						  /* Increment nesting level */` |
|      ! 0 | 1469 | `						  ++iNest;` |
|     3538 | 1470 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1471 | `						  /* Decrement nesting level */` |
|     1804 | 1472 | `						  --iNest;` |
|     1804 | 1473 | `						  if( iNest <= 0 ){` |
|     1804 | 1474 | `							  break;` |
|        - | 1475 | `						  }` |
|      ! 0 | 1476 | `					  }` |
|      867 | 1477 | `				  }` |
|     2028 | 1478 | `				  iRight++;` |
|        2 | 1479 | `			  }` |
|     1804 | 1480 | `			  if( iRight > iCur + 1 ){` |
|        - | 1481 | `				  /* Recurse and process the then expression */` |
|     1736 | 1482 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1736 | 1483 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1484 | `					  return rc;` |
|        - | 1485 | `				  }` |
|        - | 1486 | `				  /* Link the node to the tree */` |
|     1736 | 1487 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      867 | 1488 | `			  }else{` |
|        - | 1489 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1490 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1491 | `			  }` |
|     1804 | 1492 | `			  apNode[iCur + 1] = 0;` |
|     1804 | 1493 | `			  if( iRight + 1 < nToken ){` |
|        - | 1494 | `				  /* Recurse and process the else expression */` |
|     1804 | 1495 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1804 | 1496 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1497 | `					  return rc;` |
|        - | 1498 | `				  }` |
|        - | 1499 | `				  /* Link the node to the tree */` |
|     1804 | 1500 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1804 | 1501 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      903 | 1502 | `			  }else{` |
|      ! 0 | 1503 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1504 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1505 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1506 | `				 }` |
|      ! 0 | 1507 | `				 return rc;` |
|        - | 1508 | `			  }` |
|        - | 1509 | `			  /* Point to the condition */` |
|     1804 | 1510 | `			  pNode->pCond  = apNode[iLeft];` |
|     1804 | 1511 | `			  apNode[iLeft] = 0;` |
|     1804 | 1512 | `			  break;` |
|        - | 1513 | `		  }` |
|  1013994 | 1514 | `		  iLeft = iCur;` |
|   506998 | 1515 | `	  }` |
|        - | 1516 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1517 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1518 | `	  * so there is no need for a precedence loop here.` |
|        - | 1519 | `	  */` |
|   580060 | 1520 | `	 iRight = -1;` |
|  3734984 | 1521 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  3154966 | 1522 | `		 if( apNode[iCur] == 0 ){` |
|  2357882 | 1523 | `			 continue;` |
|        - | 1524 | `		 }` |
|   797086 | 1525 | `		 pNode = apNode[iCur];` |
|   797086 | 1526 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1527 | `			 /* Get the left node */` |
|   216904 | 1528 | `			 iLeft = iCur - 1;` |
|   307140 | 1529 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    90238 | 1530 | `				 iLeft--;` |
|        2 | 1531 | `			 }` |
|   216904 | 1532 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1533 | `				 /* Syntax error */` |
|       39 | 1534 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1535 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1536 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1537 | `				 }` |
|       39 | 1538 | `				 return rc;` |
|        - | 1539 | `			 }` |
|   216866 | 1540 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       28 | 1541 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
|        - | 1542 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1543 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1544 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1545 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1546 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1547 | `					 }` |
|        3 | 1548 | `					 return rc;` |
|        - | 1549 | `				 }` |
|       12 | 1550 | `			 }` |
|        - | 1551 | `			 /* Link the node to the tree (Reverse) */` |
|   216864 | 1552 | `			 pNode->pLeft = apNode[iRight];` |
|   216864 | 1553 | `			 pNode->pRight = apNode[iLeft];` |
|   216864 | 1554 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   108431 | 1555 | `		 }` |
|   797046 | 1556 | `		 iRight = iCur;` |
|   398524 | 1557 | `	 }` |
|        - | 1558 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2900092 | 1559 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2320074 | 1560 | `		 iLeft = -1;` |
| 14939762 | 1561 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 12619690 | 1562 | `			 if( apNode[iCur] == 0 ){` |
| 10299212 | 1563 | `				 continue;` |
|        - | 1564 | `			 }` |
|  2320480 | 1565 | `			 pNode = apNode[iCur];` |
|  2320480 | 1566 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1567 | `				 /* Get the right node */` |
|       72 | 1568 | `				 iRight = iCur + 1;` |
|      110 | 1569 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1570 | `					 iRight++;` |
|        2 | 1571 | `				 }` |
|       72 | 1572 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1573 | `					 /* Syntax error */` |
|      ! 0 | 1574 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1575 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1576 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1577 | `					 }` |
|      ! 0 | 1578 | `					 return rc;` |
|        - | 1579 | `				 }` |
|        - | 1580 | `				 /* Link the node to the tree */` |
|       72 | 1581 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1582 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1583 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1584 | `			 }` |
|  2320480 | 1585 | `			 iLeft = iCur;` |
|  1160241 | 1586 | `		 }` |
|  1160038 | 1587 | `	 }` |
|        - | 1588 | `	 /* Point to the root of the expression tree */` |
|  3154896 | 1589 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2574896 | 1590 | `		 if( apNode[iCur] ){` |
|   523382 | 1591 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1592 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1593 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1594 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1595 | `				  }` |
|       20 | 1596 | `				  return rc;` |
|        - | 1597 | `			 }` |
|   523364 | 1598 | `			 apNode[0] = apNode[iCur];` |
|   523364 | 1599 | `			 apNode[iCur] = 0;` |
|   261681 | 1600 | `		 }` |
|  1287440 | 1601 | `	 }` |
|   580002 | 1602 | `	 return SXRET_OK;` |
|   539143 | 1603 | ` }` |
|        - | 1604 | ` /*` |
|        - | 1605 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1606 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1607 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1608 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1609 | `  */` |
|   675240 | 1610 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1611 |  |
|        - | 1612 | `	ph7_expr_node **apNode;` |
|        - | 1613 | `	ph7_expr_node *pNode;` |
|        - | 1614 | `	sxi32 rc;` |
|        - | 1615 | `	/* Reset node container */` |
|   675242 | 1616 | `	SySetReset(pExprNode);` |
|   675242 | 1617 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1618 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1619 | `	{` |
|   675242 | 1620 | `		int iLastWasTerm = 0;` |
|  3655764 | 1621 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2980558 | 1622 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2980558 | 1623 | `			if( rc != SXRET_OK ){` |
|       35 | 1624 | `				return rc;` |
|        - | 1625 | `			}` |
|        - | 1626 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2980524 | 1627 | `			if( pNode->xCode ){` |
|        - | 1628 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1551166 | 1629 | `				iLastWasTerm = 1;` |
|  2204942 | 1630 | `			}else if( pNode->pOp ){` |
|        - | 1631 | `				/* Operator node */` |
|   670512 | 1632 | `				iLastWasTerm = 0;` |
|   335257 | 1633 | `			}else{` |
|        - | 1634 | `				/* Delimiter: ')' and ']' end terms */` |
|   758850 | 1635 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1636 | `			}` |
|        - | 1637 | `			/* Save the extracted node */` |
|  2980524 | 1638 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1639 | `		}` |
|        - | 1640 | `	}` |
|   675208 | 1641 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1642 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1643 | `		*ppRoot = 0;` |
|      ! 0 | 1644 | `		return SXRET_OK;` |
|        - | 1645 | `	}` |
|   675208 | 1646 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1647 | `	/* Make sure we are dealing with valid nodes */` |
|   675208 | 1648 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   675208 | 1649 | `	if( rc != SXRET_OK ){` |
|        - | 1650 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1651 | `		 * cleanup the mess left behind.` |
|        - | 1652 | `		 */` |
|       47 | 1653 | `		*ppRoot = 0;` |
|       47 | 1654 | `		return rc;` |
|        - | 1655 | `	}` |
|        - | 1656 | `	/* Build the tree */` |
|   675162 | 1657 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   675162 | 1658 | `	if( rc != SXRET_OK ){` |
|        - | 1659 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       78 | 1660 | `		*ppRoot = 0;` |
|       78 | 1661 | `		return rc;` |
|        - | 1662 | `	}` |
|        - | 1663 | `	/* Point to the root of the tree */` |
|   675086 | 1664 | `	*ppRoot = apNode[0];` |
|   675086 | 1665 | `	return SXRET_OK;` |
|   337622 | 1666 |  |
|        - | 1667 |  |
