# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 820/939 lines (87.33%)

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
|   616108 |  254 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  255 |  |
|   616110 |  256 | `	sxu32 n = 0;` |
|        - |  257 | `	sxi32 rc;` |
|        - |  258 | `	/* Do a linear lookup on the operators table */` |
|  9550929 |  259 | `	for(;;){` |
| 19101860 |  260 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  261 | `			break;` |
|        - |  262 | `		}` |
| 19101860 |  263 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  264 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2406444 |  265 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1203223 |  266 | `		}else{` |
| 16695418 |  267 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  268 | `		}` |
| 19101860 |  269 | `		if( rc == 0 ){` |
|   618900 |  270 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  271 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   615792 |  272 | `				return &aOpTable[n];` |
|        - |  273 | `			}` |
|        - |  274 | `			/* Handle ambiguity */` |
|     3110 |  275 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  276 | `				/* Unary opertors have prcedence here over binary operators */` |
|      210 |  277 | `				return &aOpTable[n];` |
|        - |  278 | `			}` |
|     2902 |  279 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  280 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  281 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  282 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  283 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  284 | `					return &aOpTable[n];` |
|        - |  285 | `				}` |
|        - |  286 |  |
|        4 |  287 | `			}` |
|     1395 |  288 | `		}` |
| 18485752 |  289 | `		++n; /* Next operator in the table */` |
|        2 |  290 | `	}` |
|        - |  291 | `	/* No such operator */` |
|      ! 0 |  292 | `	return 0;` |
|   308056 |  293 |  |
|        - |  294 | `/*` |
|        - |  295 | ` * Delimit a set of token stream.` |
|        - |  296 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  297 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  298 | ` */` |
|   269158 |  299 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  300 |  |
|   269160 |  301 | `	SyToken *pCur = pIn;` |
|   269160 |  302 | `	sxi32 iNest = 1;` |
|  1448330 |  303 | `	for(;;){` |
|  2896662 |  304 | `		if( pCur >= pEnd ){` |
|       90 |  305 | `			break;` |
|        - |  306 | `		}` |
|  2896574 |  307 | `		if( pCur->nType & nTokStart ){` |
|        - |  308 | `			/* Increment nesting level */` |
|   161832 |  309 | `			iNest++;` |
|  2815659 |  310 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  311 | `			/* Decrement nesting level */` |
|   430902 |  312 | `			iNest--;` |
|   430902 |  313 | `			if( iNest <= 0 ){` |
|   269072 |  314 | `				break;` |
|        - |  315 | `			}` |
|    80915 |  316 | `		}` |
|        - |  317 | `		/* Advance cursor */` |
|  2627504 |  318 | `		pCur++;` |
|        2 |  319 | `	}` |
|        - |  320 | `	/* Point to the end of the chunk */` |
|   269160 |  321 | `	*ppEnd = pCur;` |
|   269160 |  322 |  |
|        - |  323 | `/*` |
|        - |  324 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  325 | ` * Note on reserved keywords.` |
|        - |  326 | ` *  According to the PHP language reference manual:` |
|        - |  327 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  328 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  329 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  330 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  331 | ` */` |
|    14588 |  332 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  333 |  |
|    21820 |  334 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    14499 |  335 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  336 | `		){` |
|      138 |  337 | `			return TRUE;` |
|        - |  338 | `	}` |
|    14454 |  339 | `	if( bCheckFunc ){` |
|     2462 |  340 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       33 |  341 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       21 |  342 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|     2452 |  343 | `				return TRUE;` |
|        - |  344 | `		}` |
|        4 |  345 | `	}` |
|        - |  346 | `	/* Not a language construct */` |
|    12004 |  347 | `	return FALSE;` |
|     7296 |  348 |  |
|        - |  349 | `/*` |
|        - |  350 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  351 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  352 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  353 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  354 | ` */` |
|   505974 |  355 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  356 |  |
|        - |  357 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  358 | `	sxi32 i,rc;` |
|        - |  359 |  |
|   505976 |  360 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  361 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        7 |  362 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        7 |  363 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        3 |  364 | `	}` |
|   505976 |  365 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  2773476 |  366 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2267532 |  367 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   239718 |  368 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    14538 |  369 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  370 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   220270 |  371 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  372 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  373 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  374 | `						 */` |
|   220270 |  375 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   220270 |  376 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   220270 |  377 | `						apNode[i]->pOp = &sFCallOp;` |
|   110134 |  378 | `					}` |
|   110134 |  379 | `			}` |
|   239718 |  380 | `			iParen++;` |
|  2147674 |  381 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   239714 |  382 | `			if( iParen <= 0 ){` |
|        9 |  383 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  384 | `				if( rc != SXERR_ABORT ){` |
|        9 |  385 | `					rc = SXERR_SYNTAX;` |
|        4 |  386 | `				}` |
|        9 |  387 | `				return rc;` |
|        - |  388 | `			}` |
|   239706 |  389 | `			iParen--;` |
|  1907956 |  390 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    58990 |  391 | `			iSquare++;` |
|  1758610 |  392 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    59004 |  393 | `			if( iSquare <= 0 ){` |
|        7 |  394 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  395 | `				if( rc != SXERR_ABORT ){` |
|        7 |  396 | `					rc = SXERR_SYNTAX;` |
|        3 |  397 | `				}` |
|        7 |  398 | `				return rc;` |
|        - |  399 | `			}` |
|    58998 |  400 | `			iSquare--;` |
|  1699612 |  401 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  1670108 |  448 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       15 |  449 | `			if( iBraces <= 0 ){` |
|       13 |  450 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  451 | `				if( rc != SXERR_ABORT ){` |
|       13 |  452 | `					rc = SXERR_SYNTAX;` |
|        6 |  453 | `				}` |
|       13 |  454 | `				return rc;` |
|        - |  455 | `			}` |
|        3 |  456 | `			iBraces--;` |
|  1670089 |  457 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1734 |  458 | `			if( iQuesty <= 0 ){` |
|        5 |  459 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  460 | `				if( rc != SXERR_ABORT ){` |
|        5 |  461 | `					rc = SXERR_SYNTAX;` |
|        2 |  462 | `				}` |
|        5 |  463 | `				return rc;` |
|        - |  464 | `			}` |
|     1730 |  465 | `			iQuesty--;` |
|  1669220 |  466 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   487222 |  467 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   487222 |  468 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1732 |  469 | `				iQuesty++;` |
|   486357 |  470 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      306 |  471 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      152 |  489 | `			}` |
|   243610 |  490 | `		}` |
|  1133752 |  491 | `	}` |
|   505946 |  492 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  493 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  494 | `		if( rc != SXERR_ABORT ){` |
|       17 |  495 | `			rc = SXERR_SYNTAX;` |
|        8 |  496 | `		}` |
|       17 |  497 | `		return rc;` |
|        - |  498 | `	}` |
|   505930 |  499 | `	return SXRET_OK;` |
|   252989 |  500 |  |
|        - |  501 | `/*` |
|        - |  502 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  503 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  504 | ` */` |
|   400974 |  505 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  506 |  |
|   400976 |  507 | `	SyToken *pIn = *ppCur;` |
|        - |  508 | `	/* Jump the first literal seen */` |
|   400976 |  509 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   400958 |  510 | `		pIn++;` |
|   200478 |  511 | `	}` |
|   200511 |  512 | `	for(;;){` |
|   401024 |  513 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       49 |  514 | `			pIn++;` |
|       49 |  515 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       49 |  516 | `				pIn++;` |
|       24 |  517 | `			}` |
|       25 |  518 | `		}else{` |
|   200489 |  519 | `			break;` |
|        - |  520 | `		}` |
|        1 |  521 | `	}` |
|        - |  522 | `	/* Synchronize pointers */` |
|   400976 |  523 | `	*ppCur = pIn;` |
|   400976 |  524 |  |
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
|      156 |  558 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  559 |  |
|      158 |  560 | `	SyToken *pIn = *ppCur;` |
|        - |  561 | `	sxu32 nLine;` |
|        - |  562 | `	sxi32 rc;` |
|        - |  563 | `	/* Jump the 'function' keyword */` |
|      158 |  564 | `	nLine = pIn->nLine;` |
|      158 |  565 | `	pIn++;` |
|      158 |  566 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        5 |  567 | `		pIn++;` |
|        2 |  568 | `	}` |
|      158 |  569 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  570 | `		/* Syntax error */` |
|        5 |  571 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  572 | `		if( rc != SXERR_ABORT ){` |
|        5 |  573 | `			rc = SXERR_SYNTAX;` |
|        2 |  574 | `		}` |
|        5 |  575 | `		goto Synchronize;` |
|        - |  576 | `	}` |
|      154 |  577 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      154 |  578 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      154 |  579 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  580 | `		/* Syntax error */` |
|        5 |  581 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  582 | `		if( rc != SXERR_ABORT ){` |
|        5 |  583 | `			rc = SXERR_SYNTAX;` |
|        2 |  584 | `		}` |
|        5 |  585 | `		goto Synchronize;` |
|        - |  586 | `	}` |
|      150 |  587 | `	pIn++; /* Jump the trailing parenthesis */` |
|      150 |  588 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
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
|      134 |  621 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      134 |  622 | `		pIn++; /* Jump the leading curly '{' */` |
|      134 |  623 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      134 |  624 | `		if( pIn < pEnd ){` |
|      134 |  625 | `			pIn++;` |
|       66 |  626 | `		}` |
|       68 |  627 | `	}else{` |
|        - |  628 | `		/* Syntax error */` |
|      ! 0 |  629 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  630 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  631 | `			return SXERR_ABORT;` |
|        - |  632 | `		}` |
|        - |  633 | `	}` |
|      134 |  634 | `	rc = SXRET_OK;` |
|       78 |  635 | `Synchronize:` |
|        - |  636 | `	/* Synchronize pointers */` |
|      158 |  637 | `	*ppCur = pIn;` |
|      158 |  638 | `	return rc;` |
|       80 |  639 |  |
|        - |  640 | `/*` |
|        - |  641 | ` * Extract a single expression node from the input.` |
|        - |  642 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  643 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  644 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  645 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  646 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  647 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  648 | ` */` |
|  2267676 |  649 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode)` |
|        2 |  650 |  |
|        - |  651 | `	ph7_expr_node *pNode;` |
|        - |  652 | `	SyToken *pCur;` |
|        - |  653 | `	sxi32 rc;` |
|        - |  654 | `	/* Allocate a new node */` |
|  2267678 |  655 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2267678 |  656 | `	if( pNode == 0 ){` |
|        - |  657 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  658 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  659 | `		 */` |
|      ! 0 |  660 | `		return SXERR_MEM;` |
|        - |  661 | `	}` |
|        - |  662 | `	/* Zero the structure */` |
|  2267678 |  663 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2267678 |  664 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  665 | `	/* Point to the head of the token stream */` |
|  2267678 |  666 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  667 | `	/* Start collecting tokens */` |
|  2267678 |  668 | `	if( pCur->nType & PH7_TK_OP ){` |
|        - |  669 | `		/* Point to the instance that describe this operator */` |
|   546248 |  670 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  671 | `		/* Advance the stream cursor */` |
|   546248 |  672 | `		pCur++;` |
|  1994555 |  673 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  674 | `		/* Isolate variable */` |
|  1241542 |  675 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   620778 |  676 | `			pCur++; /* Variable variable */` |
|        2 |  677 | `		}` |
|   620766 |  678 | `		if( pCur < pGen->pEnd ){` |
|   620766 |  679 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  680 | `				/* Variable name */` |
|   620738 |  681 | `				pCur++;` |
|   310398 |  682 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   310380 |  697 | `		}` |
|   620762 |  698 | `		pNode->xCode = PH7_CompileVariable;` |
|  1411048 |  699 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    32434 |  700 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    32434 |  701 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  702 | `			 /* List/Array node */` |
|    20208 |  703 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  704 | `				 /* Assume a literal */` |
|       17 |  705 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  706 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  707 | `			 }else{` |
|    20192 |  708 | `				 pCur += 2;` |
|        - |  709 | `				 /* Collect array/list tokens */` |
|    20192 |  710 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    20192 |  711 | `				 if( pCur < pGen->pEnd ){` |
|    20190 |  712 | `					 pCur++;` |
|    10096 |  713 | `				 }else{` |
|        - |  714 | `					 /* Syntax error */` |
|        4 |  715 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  716 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  717 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  718 | `						 rc = SXERR_SYNTAX;` |
|        1 |  719 | `					 }` |
|        3 |  720 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  721 | `					 return rc;` |
|        - |  722 | `				 }` |
|    20190 |  723 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    20190 |  724 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    22329 |  737 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  738 | `			 /* Annonymous function */` |
|      158 |  739 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  740 | `				 /* Assume a literal */` |
|      ! 0 |  741 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  742 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  743 | `			 }else{` |
|        - |  744 | `				 /* Assemble annonymous functions body */` |
|      158 |  745 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      158 |  746 | `				 if( rc != SXRET_OK ){` |
|       25 |  747 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  748 | `					 return rc;` |
|        - |  749 | `				 }` |
|      134 |  750 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  751 | `			  }` |
|    12138 |  752 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  753 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       74 |  754 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       74 |  755 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       38 |  756 | `		 }else{` |
|        - |  757 | `			 /* Assume a literal */` |
|    12000 |  758 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    12000 |  759 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  760 | `		 }` |
|  1084438 |  761 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  762 | `		 /* Constants,function name,namespace path,class name... */` |
|   388962 |  763 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   388962 |  764 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   194482 |  765 | `	 }else{` |
|   679276 |  766 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  767 | `			 /* Point to the code generator routine */` |
|   139072 |  768 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   139072 |  769 | `			 if( pNode->xCode == 0 ){` |
|        3 |  770 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  771 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  772 | `					 rc = SXERR_SYNTAX;` |
|        1 |  773 | `				 }` |
|        3 |  774 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  775 | `				 return rc;` |
|        - |  776 | `			 }` |
|    69534 |  777 | `		 }` |
|        - |  778 | `		/* Advance the stream cursor */` |
|   679274 |  779 | `		pCur++;` |
|        - |  780 | `	 }` |
|        - |  781 | `	/* Point to the end of the token stream */` |
|  2267644 |  782 | `	pNode->pEnd = pCur;` |
|        - |  783 | `	/* Save the node for later processing */` |
|  2267644 |  784 | `	*ppNode = pNode;` |
|        - |  785 | `	/* Synchronize cursors */` |
|  2267644 |  786 | `	pGen->pIn = pCur;` |
|  2267644 |  787 | `	return SXRET_OK;` |
|  1133840 |  788 |  |
|        - |  789 | `/*` |
|        - |  790 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  791 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  792 | ` * level is zero.` |
|        - |  793 | ` */` |
|    51746 |  794 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  795 |  |
|    51748 |  796 | `	SyToken *pCur = pStart;` |
|    51748 |  797 | `	sxi32 iNest = 0;` |
|    51748 |  798 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  799 | `		/* Last expression */` |
|    29526 |  800 | `		return SXERR_EOF;` |
|        - |  801 | `	}` |
|    93914 |  802 | `	while( pCur < pEnd ){` |
|    85538 |  803 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    13848 |  804 | `			break;` |
|        - |  805 | `		}` |
|    71692 |  806 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     4196 |  807 | `			iNest++;` |
|    69595 |  808 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     4198 |  809 | `			iNest--;` |
|     2098 |  810 | `		}` |
|    71692 |  811 | `		pCur++;` |
|        2 |  812 | `	}` |
|    22224 |  813 | `	*ppNext = pCur;` |
|    22224 |  814 | `	return SXRET_OK;` |
|    25875 |  815 |  |
|        - |  816 | `/*` |
|        - |  817 | ` * Free an expression tree.` |
|        - |  818 | ` */` |
|  1963732 |  819 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  820 |  |
|  1963734 |  821 | `	if( pNode->pLeft ){` |
|        - |  822 | `		/* Release the left tree */` |
|   743378 |  823 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   371688 |  824 | `	}` |
|  1963734 |  825 | `	if( pNode->pRight ){` |
|        - |  826 | `		/* Release the right tree */` |
|   411866 |  827 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   205932 |  828 | `	}` |
|  1963734 |  829 | `	if( pNode->pCond ){` |
|        - |  830 | `		/* Release the conditional tree used by the ternary operator */` |
|     1728 |  831 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      863 |  832 | `	}` |
|  1963734 |  833 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  834 | `		ph7_expr_node **apArg;` |
|        - |  835 | `		sxu32 n;` |
|        - |  836 | `		/* Release node arguments */` |
|   238656 |  837 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   500238 |  838 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   261584 |  839 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   130793 |  840 | `		}` |
|   238656 |  841 | `		SySetRelease(&pNode->aNodeArgs);` |
|   119327 |  842 | `	}` |
|        - |  843 | `	/* Finally,release this node */` |
|  1963734 |  844 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  1963734 |  845 |  |
|        - |  846 | `/*` |
|        - |  847 | ` * Free an expression tree.` |
|        - |  848 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  849 | ` */` |
|   506008 |  850 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  851 |  |
|        - |  852 | `	ph7_expr_node **apNode;` |
|        - |  853 | `	sxu32 n;` |
|   506010 |  854 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  2773652 |  855 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2267644 |  856 | `		if( apNode[n] ){` |
|   506314 |  857 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   253156 |  858 | `		}` |
|  1133823 |  859 | `	}` |
|   506010 |  860 | `	return SXRET_OK;` |
|        2 |  861 |  |
|        - |  862 | `/*` |
|        - |  863 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  864 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  865 | ` */` |
|   179444 |  866 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  867 |  |
|        - |  868 | `	sxi32 iExprOp;` |
|   179446 |  869 | `	if( pNode->pOp == 0 ){` |
|   116872 |  870 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  871 | `	}` |
|    62576 |  872 | `	iExprOp = pNode->pOp->iOp;` |
|    62576 |  873 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    39236 |  874 | `			return TRUE;` |
|        - |  875 | `	}` |
|    23342 |  876 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    23338 |  877 | `		if( pNode->pLeft->pOp ) {` |
|        2 |  878 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  879 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  880 | `				return FALSE;` |
|        1 |  881 | `			}` |
|    23337 |  882 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  883 | `			return FALSE;` |
|        - |  884 | `		}` |
|    23338 |  885 | `		return TRUE;` |
|        - |  886 | `	}` |
|        5 |  887 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  888 | `		return TRUE;` |
|        - |  889 | `	}` |
|        - |  890 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  891 | `	return FALSE;` |
|    89724 |  892 |  |
|        - |  893 | `/* Forward declaration */` |
|        - |  894 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  895 | `/* Macro to check if the given node is a terminal.` |
|        - |  896 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  897 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  898 | ` * linked ternary/elvis node). */` |
|        - |  899 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  900 | `/*` |
|        - |  901 | ` * Buid an expression tree for each given function argument.` |
|        - |  902 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  903 | ` */` |
|   191232 |  904 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  905 |  |
|        - |  906 | `	sxi32 iNest,iCur,iNode;` |
|        - |  907 | `	sxi32 rc;` |
|        - |  908 | `	/* Process function arguments from left to right */` |
|   191234 |  909 | `	iCur = 0;` |
|   202695 |  910 | `	for(;;){` |
|   405392 |  911 | `		if( iCur >= nToken ){` |
|        - |  912 | `			/* No more arguments to process */` |
|   191232 |  913 | `			break;` |
|        - |  914 | `		}` |
|   214162 |  915 | `		iNode = iCur;` |
|   214162 |  916 | `		iNest = 0;` |
|   555638 |  917 | `		while( iCur < nToken ){` |
|   364408 |  918 | `			if( apNode[iCur] ){` |
|   355192 |  919 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    11467 |  920 | `					break;` |
|   332262 |  921 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    19166 |  922 | `					iNest++;` |
|   322680 |  923 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    19166 |  924 | `					iNest--;` |
|     9582 |  925 | `				}` |
|   166130 |  926 | `			}` |
|   341478 |  927 | `			iCur++;` |
|        2 |  928 | `		}` |
|   214162 |  929 | `		if( iCur > iNode ){` |
|   214158 |  930 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  931 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  932 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  933 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  934 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  935 | `					apNode[iNode] = 0;` |
|      ! 0 |  936 | `			}` |
|   214160 |  937 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   214160 |  938 | `			if( apNode[iNode] ){` |
|        - |  939 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   214160 |  940 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   107081 |  941 | `			}else{` |
|        - |  942 | `				/* Empty function argument */` |
|      ! 0 |  943 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 |  944 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  945 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  946 | `				}` |
|      ! 0 |  947 | `				return rc;` |
|        - |  948 | `			}` |
|   107081 |  949 | `		}else{` |
|        3 |  950 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 |  951 | `			if( rc != SXERR_ABORT ){` |
|        3 |  952 | `				rc = SXERR_SYNTAX;` |
|        1 |  953 | `			}` |
|        3 |  954 | `			return rc;` |
|        - |  955 | `		}` |
|        - |  956 | `		/* Jump trailing comma */` |
|   214160 |  957 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    22930 |  958 | `			iCur++;` |
|    22930 |  959 | `			if( iCur >= nToken ){` |
|        - |  960 | `				/* missing function argument */` |
|      ! 0 |  961 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 |  962 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  963 | `					rc = SXERR_SYNTAX;` |
|      ! 0 |  964 | `				}` |
|      ! 0 |  965 | `				return rc;` |
|        - |  966 | `			}` |
|    11464 |  967 | `		}` |
|        2 |  968 | `	}` |
|   191232 |  969 | `	return SXRET_OK;` |
|    95618 |  970 |  |
|        - |  971 | ` /*` |
|        - |  972 | `  * Create an expression tree from an array of tokens.` |
|        - |  973 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - |  974 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  975 | `  */` |
|   792832 |  976 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  977 | ` {` |
|        - |  978 | `	 sxi32 i,iLeft,iRight;` |
|        - |  979 | `	 ph7_expr_node *pNode;` |
|        - |  980 | `	 sxi32 iCur;` |
|        - |  981 | `	 sxi32 rc;` |
|   792834 |  982 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - |  983 | `		 /* TICKET 1433-17: self evaluating node */` |
|   354234 |  984 | `		 return SXRET_OK;` |
|        - |  985 | `	 }` |
|        - |  986 | `	 /* Process expressions enclosed in parenthesis first */` |
|  2725304 |  987 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - |  988 | `		 sxi32 iNest;` |
|        - |  989 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - |  990 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - |  991 | `		  */` |
|  2286706 |  992 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2267268 |  993 | `			 continue;` |
|        - |  994 | `		 }` |
|    19440 |  995 | `		 iNest = 1;` |
|    19440 |  996 | `		 iLeft = iCur;` |
|        - |  997 | `		 /* Find the closing parenthesis */` |
|    19440 |  998 | `		 iCur++;` |
|   129576 |  999 | `		 while( iCur < nToken ){` |
|   129576 | 1000 | `			 if( apNode[iCur] ){` |
|   129576 | 1001 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1002 | `					 /* Decrement nesting level */` |
|    33752 | 1003 | `					 iNest--;` |
|    33752 | 1004 | `					 if( iNest <= 0 ){` |
|    19440 | 1005 | `						 break;` |
|        2 | 1006 | `					 }` |
|   102982 | 1007 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1008 | `					 /* Increment nesting level */` |
|    14314 | 1009 | `					 iNest++;` |
|     7156 | 1010 | `				 }` |
|    55068 | 1011 | `			 }` |
|   110138 | 1012 | `			 iCur++;` |
|        2 | 1013 | `		 }` |
|    19440 | 1014 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1015 | `			 /* Recurse and process this expression */` |
|    19440 | 1016 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    19440 | 1017 | `			 if( rc != SXRET_OK ){` |
|        3 | 1018 | `				 return rc;` |
|        - | 1019 | `			 }` |
|     9718 | 1020 | `		 }` |
|        - | 1021 | `		 /* Free the left and right nodes */` |
|    19438 | 1022 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    19438 | 1023 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    19438 | 1024 | `		 apNode[iLeft] = 0;` |
|    19438 | 1025 | `		 apNode[iCur] = 0;` |
|     9720 | 1026 | `	 }` |
|        - | 1027 | `	  /* Process expressions enclosed in braces */` |
|  2849862 | 1028 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1029 | `		 sxi32 iNest;` |
|        - | 1030 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1031 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1032 | `		  */` |
|  2416262 | 1033 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  2416260 | 1034 | `			 continue;` |
|        - | 1035 | `		 }` |
|        3 | 1036 | `		 iNest = 1;` |
|        3 | 1037 | `		 iLeft = iCur;` |
|        - | 1038 | `		 /* Find the closing parenthesis */` |
|        3 | 1039 | `		 iCur++;` |
|        7 | 1040 | `		 while( iCur < nToken ){` |
|        7 | 1041 | `			 if( apNode[iCur] ){` |
|        7 | 1042 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1043 | `					 /* Decrement nesting level */` |
|        3 | 1044 | `					 iNest--;` |
|        3 | 1045 | `					 if( iNest <= 0 ){` |
|        3 | 1046 | `						 break;` |
|      ! 0 | 1047 | `					 }` |
|        5 | 1048 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1049 | `					 /* Increment nesting level */` |
|      ! 0 | 1050 | `					 iNest++;` |
|      ! 0 | 1051 | `				 }` |
|        2 | 1052 | `			 }` |
|        5 | 1053 | `			 iCur++;` |
|        1 | 1054 | `		 }` |
|        3 | 1055 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1056 | `			 /* Recurse and process this expression */` |
|        3 | 1057 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        3 | 1058 | `			 if( rc != SXRET_OK ){` |
|        3 | 1059 | `				 return rc;` |
|        - | 1060 | `			 }` |
|      ! 0 | 1061 | `		 }` |
|        - | 1062 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1063 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1064 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1065 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1066 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1067 | `	 }` |
|        - | 1068 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   433602 | 1069 | `	 iLeft = -1;` |
|  2849834 | 1070 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2416244 | 1071 | `		 if( apNode[iCur] == 0 ){` |
|   909304 | 1072 | `			 continue;` |
|        - | 1073 | `		 }` |
|  1506942 | 1074 | `		 pNode = apNode[iCur];` |
|  1506942 | 1075 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   367260 | 1076 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1077 | `				 /* Collect function arguments */` |
|   220266 | 1078 | `				 sxi32 iPtr = 0;` |
|   220266 | 1079 | `				 sxi32 nFuncTok = 0;` |
|   804938 | 1080 | `				 while( nFuncTok + iCur < nToken ){` |
|   804938 | 1081 | `					 if( apNode[nFuncTok+iCur] ){` |
|   795722 | 1082 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   227584 | 1083 | `							 iPtr++;` |
|   681931 | 1084 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   227584 | 1085 | `							 iPtr--;` |
|   227584 | 1086 | `							 if( iPtr <= 0 ){` |
|   220266 | 1087 | `								 break;` |
|        - | 1088 | `							 }` |
|     3659 | 1089 | `						 }` |
|   287728 | 1090 | `					 }` |
|   584674 | 1091 | `					 nFuncTok++;` |
|        2 | 1092 | `				 }` |
|   220266 | 1093 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1094 | `					 /* Syntax error */` |
|      ! 0 | 1095 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1096 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1097 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1098 | `					 }` |
|      ! 0 | 1099 | `					 return rc;` |
|        - | 1100 | `				 }` |
|   220266 | 1101 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1102 | `					 /* Syntax error */` |
|      ! 0 | 1103 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1104 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1105 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1106 | `					 }` |
|      ! 0 | 1107 | `					 return rc;` |
|        - | 1108 | `				 }` |
|   220266 | 1109 | `				 if( nFuncTok > 1 ){` |
|        - | 1110 | `					 /* Process function arguments */` |
|   191234 | 1111 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   191234 | 1112 | `					 if( rc != SXRET_OK ){` |
|        3 | 1113 | `						 return rc;` |
|        - | 1114 | `					 }` |
|    95615 | 1115 | `				 }` |
|        - | 1116 | `				 /* Link the node to the tree */` |
|   220264 | 1117 | `				 pNode->pLeft = apNode[iLeft];` |
|   220264 | 1118 | `				 apNode[iLeft] = 0;` |
|   804930 | 1119 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   584668 | 1120 | `					 apNode[iCur+iPtr] = 0;` |
|   292335 | 1121 | `				 }` |
|   257127 | 1122 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1123 | `				 /* Subscripting */` |
|    58998 | 1124 | `				 sxi32 iArrTok = iCur + 1;` |
|    58998 | 1125 | `				 sxi32 iNest = 1;` |
|    59028 | 1126 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        2 | 1127 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString ) ) \|\|` |
|    58996 | 1128 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1129 | `						 /* Syntax error */` |
|        5 | 1130 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|        5 | 1131 | `						 if( rc != SXERR_ABORT ){` |
|        5 | 1132 | `							 rc = SXERR_SYNTAX;` |
|        2 | 1133 | `						 }` |
|        5 | 1134 | `						 return rc;` |
|        - | 1135 | `				 }` |
|        - | 1136 | `				 /* Collect index tokens */` |
|   106524 | 1137 | `				 while( iArrTok < nToken ){` |
|   106524 | 1138 | `					 if( apNode[iArrTok] ){` |
|   106492 | 1139 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1140 | `							 /* Increment nesting level */` |
|      ! 0 | 1141 | `							 iNest++;` |
|   106492 | 1142 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1143 | `							 /* Decrement nesting level */` |
|    58994 | 1144 | `							 iNest--;` |
|    58994 | 1145 | `							 if( iNest <= 0 ){` |
|    58994 | 1146 | `								 break;` |
|        - | 1147 | `							 }` |
|      ! 0 | 1148 | `						 }` |
|    23749 | 1149 | `					 }` |
|    47532 | 1150 | `					 ++iArrTok;` |
|        2 | 1151 | `				 }` |
|    58994 | 1152 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1153 | `					 /* Recurse and process this expression */` |
|    47426 | 1154 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    47426 | 1155 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1156 | `						 return rc;` |
|        - | 1157 | `					 }` |
|        - | 1158 | `					 /* Link the node to it's index */` |
|    47426 | 1159 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    23712 | 1160 | `				 }` |
|        - | 1161 | `				 /* Link the node to the tree */` |
|    58994 | 1162 | `				 pNode->pLeft = apNode[iLeft];` |
|    58994 | 1163 | `				 pNode->pRight = 0;` |
|    58994 | 1164 | `				 apNode[iLeft] = 0;` |
|   165516 | 1165 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   106524 | 1166 | `					 apNode[iNest] = 0;` |
|    53263 | 1167 | `				 }` |
|    29498 | 1168 | `			 }else{` |
|        - | 1169 | `				 /* Member access operators [i.e: '->','::'] */` |
|    88000 | 1170 | `				  iRight = iCur + 1;` |
|    88000 | 1171 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1172 | `					 iRight++;` |
|      ! 0 | 1173 | `				 }` |
|    88000 | 1174 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1175 | `					 /* Syntax error */` |
|        5 | 1176 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1177 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1178 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1179 | `					 }` |
|        5 | 1180 | `					 return rc;` |
|        - | 1181 | `				 }` |
|        - | 1182 | `				 /* Link the node to the tree */` |
|    87996 | 1183 | `				 pNode->pLeft = apNode[iLeft];` |
|    87996 | 1184 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|    87936 | 1185 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1186 | `						 /* Syntax error */` |
|      ! 0 | 1187 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1188 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1189 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1190 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1191 | `						 }` |
|      ! 0 | 1192 | `						 return rc;` |
|        - | 1193 | `				 }` |
|    87996 | 1194 | `				 pNode->pRight = apNode[iRight];` |
|    87996 | 1195 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1196 | `			 }` |
|   183624 | 1197 | `		 }` |
|  1506932 | 1198 | `		 iLeft = iCur;` |
|   753467 | 1199 | `	 }` |
|        - | 1200 | `	 /* Handle left associative (new, clone) operators */` |
|  2849802 | 1201 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2416212 | 1202 | `		 if( apNode[iCur] == 0 ){` |
|  1288364 | 1203 | `			 continue;` |
|        - | 1204 | `		 }` |
|  1127850 | 1205 | `		 pNode = apNode[iCur];` |
|  1127850 | 1206 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1207 | `			 SyToken *pToken;` |
|        - | 1208 | `			 /* Get the left node */` |
|    11814 | 1209 | `			 iLeft = iCur + 1;` |
|    23600 | 1210 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    11788 | 1211 | `				 iLeft++;` |
|        2 | 1212 | `			 }` |
|    11814 | 1213 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1214 | `				  /* Syntax error */` |
|      ! 0 | 1215 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1216 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1217 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1218 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1219 | `				 }` |
|      ! 0 | 1220 | `				 return rc;` |
|        - | 1221 | `			 }` |
|        - | 1222 | `			 /* Make sure the operand are of a valid type */` |
|    11814 | 1223 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1224 | `				 /* Clone:` |
|        - | 1225 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1226 | `				  *  ++ function call (including annonymous)` |
|        - | 1227 | `				  *  ++ array member` |
|        - | 1228 | `				  *  ++ 'new' operator` |
|        - | 1229 | `				  * Example:` |
|        - | 1230 | `				  *   clone $pObj;` |
|        - | 1231 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1232 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1233 | `				  */` |
|       18 | 1234 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1235 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1236 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1237 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1238 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1239 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1240 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1241 | `						 }` |
|      ! 0 | 1242 | `						 return rc;` |
|        - | 1243 | `					 }` |
|        7 | 1244 | `				 }` |
|       10 | 1245 | `			 }else{` |
|        - | 1246 | `				 /* New */` |
|    11798 | 1247 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       14 | 1248 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       14 | 1249 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1250 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1251 | `						 /* Syntax error */` |
|      ! 0 | 1252 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1253 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1254 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1255 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1256 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1257 | `						 }` |
|      ! 0 | 1258 | `						 return rc;` |
|        - | 1259 | `					 }` |
|        6 | 1260 | `				 }` |
|        - | 1261 | `			 }` |
|        - | 1262 | `			  /* Link the node to the tree */` |
|    11814 | 1263 | `			 pNode->pLeft = apNode[iLeft];` |
|    11814 | 1264 | `			 apNode[iLeft] = 0;` |
|    11814 | 1265 | `			 pNode->pRight = 0; /* Paranoid */` |
|     5906 | 1266 | `		 }` |
|   563926 | 1267 | `	 }` |
|        - | 1268 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   433592 | 1269 | `	 iLeft = -1;` |
|  2849802 | 1270 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2413714 | 1271 | `		 if( apNode[iCur] == 0 ){` |
|  1288364 | 1272 | `			 continue;` |
|        - | 1273 | `		 }` |
|  1125352 | 1274 | `		 pNode = apNode[iCur];` |
|  1125352 | 1275 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     6926 | 1276 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2510 | 1277 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1278 | `					 /* Link the node to the tree */` |
|     2506 | 1279 | `					 pNode->pLeft = apNode[iLeft];` |
|     2506 | 1280 | `					 apNode[iLeft] = 0;` |
|     1252 | 1281 | `			 }` |
|     4711 | 1282 | `		  }` |
|  1127850 | 1283 | `		 iLeft = iCur;` |
|   563926 | 1284 | `	  }` |
|   436090 | 1285 | `	 iLeft = -1;` |
|  2852300 | 1286 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2416212 | 1287 | `		 if( apNode[iCur] == 0 ){` |
|  1290868 | 1288 | `			 continue;` |
|        - | 1289 | `		 }` |
|  1125346 | 1290 | `		 pNode = apNode[iCur];` |
|  1125346 | 1291 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     6918 | 1292 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     6920 | 1293 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1294 | `					 /* Syntax error */` |
|      ! 0 | 1295 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1296 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1297 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1298 | `					 }` |
|      ! 0 | 1299 | `					 return rc;` |
|        - | 1300 | `			 }` |
|        - | 1301 | `			 /* Link the node to the tree */` |
|     6920 | 1302 | `			 pNode->pLeft = apNode[iLeft];` |
|     6920 | 1303 | `			 apNode[iLeft] = 0;` |
|        - | 1304 | `			 /* Mark as pre-increment/decrement node */` |
|     6920 | 1305 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3459 | 1306 | `		  }` |
|  1125346 | 1307 | `		 iLeft = iCur;` |
|   562674 | 1308 | `	 }` |
|        - | 1309 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   436090 | 1310 | `	  iLeft = 0;` |
|  2852294 | 1311 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2416208 | 1312 | `		  if( apNode[iCur] ){` |
|  1118424 | 1313 | `			  pNode = apNode[iCur];` |
|  1118424 | 1314 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    31096 | 1315 | `				  if( iLeft > 0 ){` |
|        - | 1316 | `					  /* Link the node to the tree */` |
|    31094 | 1317 | `					  pNode->pLeft = apNode[iLeft];` |
|    31094 | 1318 | `					  apNode[iLeft] = 0;` |
|    31094 | 1319 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       10 | 1320 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1321 | `							   /* Syntax error */` |
|      ! 0 | 1322 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1323 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1324 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1325 | `							  }` |
|      ! 0 | 1326 | `							  return rc;` |
|        - | 1327 | `						  }` |
|        4 | 1328 | `					  }` |
|    15548 | 1329 | `				  }else{` |
|        - | 1330 | `					  /* Syntax error */` |
|        3 | 1331 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1332 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1333 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1334 | `					  }` |
|        3 | 1335 | `					  return rc;` |
|        - | 1336 | `				  }` |
|    15546 | 1337 | `			  }` |
|        - | 1338 | `			  /* Save terminal position */` |
|  1118422 | 1339 | `			  iLeft = iCur;` |
|   559210 | 1340 | `		  }` |
|  1208104 | 1341 | `	  }` |
|        - | 1342 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  4796872 | 1343 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  4360794 | 1344 | `		 iLeft = -1;` |
| 28522588 | 1345 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 24161804 | 1346 | `			 if( apNode[iCur] == 0 ){` |
| 15147420 | 1347 | `				 continue;` |
|        - | 1348 | `			 }` |
|  9014386 | 1349 | `			 pNode = apNode[iCur];` |
|  9014386 | 1350 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1351 | `				 /* Get the right node */` |
|   142712 | 1352 | `				 iRight = iCur + 1;` |
|   202852 | 1353 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    60142 | 1354 | `					 iRight++;` |
|        2 | 1355 | `				 }` |
|   142712 | 1356 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1357 | `					 /* Syntax error */` |
|        9 | 1358 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1359 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1360 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1361 | `					 }` |
|        9 | 1362 | `					 return rc;` |
|        - | 1363 | `				 }` |
|   142704 | 1364 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1365 | `					 sxi32  iTmp;` |
|        - | 1366 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       44 | 1367 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1368 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1369 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1370 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1371 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1372 | `						 }` |
|      ! 0 | 1373 | `						 return rc;` |
|        - | 1374 | `					 }` |
|       44 | 1375 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       30 | 1376 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1377 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1378 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1379 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1380 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1381 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1382 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1383 | `									 }` |
|      ! 0 | 1384 | `									 return rc;` |
|        - | 1385 | `							 }` |
|      ! 0 | 1386 | `						 }` |
|       14 | 1387 | `					 }` |
|        - | 1388 | `					 /* Swap operands */` |
|       44 | 1389 | `					 iTmp = iRight;` |
|       44 | 1390 | `					 iRight = iLeft;` |
|       44 | 1391 | `					 iLeft = iTmp;` |
|       21 | 1392 | `				 }` |
|        - | 1393 | `				 /* Link the node to the tree */` |
|   142704 | 1394 | `				 pNode->pLeft = apNode[iLeft];` |
|   142704 | 1395 | `				 pNode->pRight = apNode[iRight];` |
|   142704 | 1396 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    71351 | 1397 | `			 }` |
|  9014378 | 1398 | `			 iLeft = iCur;` |
|  4507190 | 1399 | `		 }` |
|  2180394 | 1400 | `	 }` |
|        - | 1401 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1402 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1403 | `	  * we are dealing with a single operator.` |
|        - | 1404 | `	  */` |
|   436080 | 1405 | `	  iLeft = -1;` |
|  2844842 | 1406 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2410492 | 1407 | `		  if( apNode[iCur] == 0 ){` |
|  1613730 | 1408 | `			  continue;` |
|        - | 1409 | `		  }` |
|   796764 | 1410 | `		  pNode = apNode[iCur];` |
|   796764 | 1411 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1730 | 1412 | `			  sxi32 iNest = 1;` |
|     1730 | 1413 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1414 | `				  /* Missing condition */` |
|        3 | 1415 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1416 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1417 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1418 | `				  }` |
|        3 | 1419 | `				  return rc;` |
|        - | 1420 | `			  }` |
|        - | 1421 | `			  /* Get the right node */` |
|     1728 | 1422 | `			  iRight = iCur + 1;` |
|     3672 | 1423 | `			  while( iRight < nToken  ){` |
|     3672 | 1424 | `				  if( apNode[iRight] ){` |
|     3386 | 1425 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1426 | `						  /* Increment nesting level */` |
|      ! 0 | 1427 | `						  ++iNest;` |
|     3386 | 1428 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1429 | `						  /* Decrement nesting level */` |
|     1728 | 1430 | `						  --iNest;` |
|     1728 | 1431 | `						  if( iNest <= 0 ){` |
|     1728 | 1432 | `							  break;` |
|        - | 1433 | `						  }` |
|      ! 0 | 1434 | `					  }` |
|      829 | 1435 | `				  }` |
|     1946 | 1436 | `				  iRight++;` |
|        2 | 1437 | `			  }` |
|     1728 | 1438 | `			  if( iRight > iCur + 1 ){` |
|        - | 1439 | `				  /* Recurse and process the then expression */` |
|     1660 | 1440 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1660 | 1441 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1442 | `					  return rc;` |
|        - | 1443 | `				  }` |
|        - | 1444 | `				  /* Link the node to the tree */` |
|     1660 | 1445 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      829 | 1446 | `			  }else{` |
|        - | 1447 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1448 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1449 | `			  }` |
|     1728 | 1450 | `			  apNode[iCur + 1] = 0;` |
|     1728 | 1451 | `			  if( iRight + 1 < nToken ){` |
|        - | 1452 | `				  /* Recurse and process the else expression */` |
|     1728 | 1453 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1728 | 1454 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1455 | `					  return rc;` |
|        - | 1456 | `				  }` |
|        - | 1457 | `				  /* Link the node to the tree */` |
|     1728 | 1458 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1728 | 1459 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      865 | 1460 | `			  }else{` |
|      ! 0 | 1461 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1462 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1463 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1464 | `				 }` |
|      ! 0 | 1465 | `				 return rc;` |
|        - | 1466 | `			  }` |
|        - | 1467 | `			  /* Point to the condition */` |
|     1728 | 1468 | `			  pNode->pCond  = apNode[iLeft];` |
|     1728 | 1469 | `			  apNode[iLeft] = 0;` |
|     1728 | 1470 | `			  break;` |
|        - | 1471 | `		  }` |
|   795036 | 1472 | `		  iLeft = iCur;` |
|   397519 | 1473 | `	  }` |
|        - | 1474 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1475 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1476 | `	  * so there is no need for a precedence loop here.` |
|        - | 1477 | `	  */` |
|   436078 | 1478 | `	 iRight = -1;` |
|  2852160 | 1479 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  2416124 | 1480 | `		 if( apNode[iCur] == 0 ){` |
|  1800508 | 1481 | `			 continue;` |
|        - | 1482 | `		 }` |
|   615618 | 1483 | `		 pNode = apNode[iCur];` |
|   615618 | 1484 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1485 | `			 /* Get the left node */` |
|   179414 | 1486 | `			 iLeft = iCur - 1;` |
|   253812 | 1487 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    74400 | 1488 | `				 iLeft--;` |
|        2 | 1489 | `			 }` |
|   179414 | 1490 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1491 | `				 /* Syntax error */` |
|       39 | 1492 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1493 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1494 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1495 | `				 }` |
|       39 | 1496 | `				 return rc;` |
|        - | 1497 | `			 }` |
|   179376 | 1498 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
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
|   179374 | 1510 | `			 pNode->pLeft = apNode[iRight];` |
|   179374 | 1511 | `			 pNode->pRight = apNode[iLeft];` |
|   179374 | 1512 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    89686 | 1513 | `		 }` |
|   615578 | 1514 | `		 iRight = iCur;` |
|   307790 | 1515 | `	 }` |
|        - | 1516 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2180182 | 1517 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  1744146 | 1518 | `		 iLeft = -1;` |
| 11408466 | 1519 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  9664322 | 1520 | `			 if( apNode[iCur] == 0 ){` |
|  7919756 | 1521 | `				 continue;` |
|        - | 1522 | `			 }` |
|  1744568 | 1523 | `			 pNode = apNode[iCur];` |
|  1744568 | 1524 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
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
|  1744568 | 1543 | `			 iLeft = iCur;` |
|   872285 | 1544 | `		 }` |
|   872074 | 1545 | `	 }` |
|        - | 1546 | `	 /* Point to the root of the expression tree */` |
|  2416050 | 1547 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  1980036 | 1548 | `		 if( apNode[iCur] ){` |
|   389292 | 1549 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       24 | 1550 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       24 | 1551 | `				  if( rc != SXERR_ABORT ){` |
|       24 | 1552 | `					  rc = SXERR_SYNTAX;` |
|       11 | 1553 | `				  }` |
|       24 | 1554 | `				  return rc;` |
|        - | 1555 | `			 }` |
|   389270 | 1556 | `			 apNode[0] = apNode[iCur];` |
|   389270 | 1557 | `			 apNode[iCur] = 0;` |
|   194634 | 1558 | `		 }` |
|   990008 | 1559 | `	 }` |
|   436016 | 1560 | `	 return SXRET_OK;` |
|   395169 | 1561 | ` }` |
|        - | 1562 | ` /*` |
|        - | 1563 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1564 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1565 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1566 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1567 | `  */` |
|   506008 | 1568 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1569 |  |
|        - | 1570 | `	ph7_expr_node **apNode;` |
|        - | 1571 | `	ph7_expr_node *pNode;` |
|        - | 1572 | `	sxi32 rc;` |
|        - | 1573 | `	/* Reset node container */` |
|   506010 | 1574 | `	SySetReset(pExprNode);` |
|   506010 | 1575 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1576 | `	/* Extract nodes one after one until we hit the end of the input */` |
|  2773652 | 1577 | `	while( pGen->pIn < pGen->pEnd ){` |
|  2267678 | 1578 | `		rc = ExprExtractNode(&(*pGen),&pNode);` |
|  2267678 | 1579 | `		if( rc != SXRET_OK ){` |
|       35 | 1580 | `			return rc;` |
|        - | 1581 | `		}` |
|        - | 1582 | `		/* Save the extracted node */` |
|  2267644 | 1583 | `		SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1584 | `	}` |
|   505976 | 1585 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1586 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1587 | `		*ppRoot = 0;` |
|      ! 0 | 1588 | `		return SXRET_OK;` |
|        - | 1589 | `	}` |
|   505976 | 1590 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1591 | `	/* Make sure we are dealing with valid nodes */` |
|   505976 | 1592 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   505976 | 1593 | `	if( rc != SXRET_OK ){` |
|        - | 1594 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1595 | `		 * cleanup the mess left behind.` |
|        - | 1596 | `		 */` |
|       47 | 1597 | `		*ppRoot = 0;` |
|       47 | 1598 | `		return rc;` |
|        - | 1599 | `	}` |
|        - | 1600 | `	/* Build the tree */` |
|   505930 | 1601 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   505930 | 1602 | `	if( rc != SXRET_OK ){` |
|        - | 1603 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       84 | 1604 | `		*ppRoot = 0;` |
|       84 | 1605 | `		return rc;` |
|        - | 1606 | `	}` |
|        - | 1607 | `	/* Point to the root of the tree */` |
|   505848 | 1608 | `	*ppRoot = apNode[0];` |
|   505848 | 1609 | `	return SXRET_OK;` |
|   253006 | 1610 |  |
|        - | 1611 |  |
