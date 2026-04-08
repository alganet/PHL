# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 834/984 lines (84.76%)

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
|        - |  217 | `	                      /* Null coalescing operator */` |
|        - |  218 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|        - |  219 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|        - |  220 | `	                      /* Ternary operator */` |
|        - |  221 | `	/* Precedence 17,left-associative */` |
|        - |  222 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  223 | `	                     /* Combined binary operators */` |
|        - |  224 | `	/* Precedence 18,right-associative */` |
|        - |  225 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  226 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  227 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  228 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  229 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  230 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  231 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  232 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  233 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  234 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  235 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  236 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  237 | `	/* Precedence 19,left-associative */` |
|        - |  238 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  239 | `	/* Precedence 20,left-associative */` |
|        - |  240 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  241 | `	/* Precedence 21,left-associative */` |
|        - |  242 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  243 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  244 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  245 | `};` |
|        - |  246 | `/* Function call operator need special handling */` |
|        - |  247 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  248 | `/*` |
|        - |  249 | ` * Check if the given token is a potential operator or not.` |
|        - |  250 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  251 | ` * look like an operator.` |
|        - |  252 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  253 | ` * Otherwise NULL.` |
|        - |  254 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  255 | ` * a binary minus or unary minus.]` |
|        - |  256 | ` */` |
|   707008 |  257 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        2 |  258 |  |
|   707010 |  259 | `	sxu32 n = 0;` |
|        - |  260 | `	sxi32 rc;` |
|        - |  261 | `	/* Do a linear lookup on the operators table */` |
| 11370329 |  262 | `	for(;;){` |
| 22740660 |  263 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  264 | `			break;` |
|        - |  265 | `		}` |
| 22740660 |  266 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  267 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  2820608 |  268 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1410305 |  269 | `		}else{` |
| 19920054 |  270 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  271 | `		}` |
| 22740660 |  272 | `		if( rc == 0 ){` |
|   710114 |  273 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  274 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   706692 |  275 | `				return &aOpTable[n];` |
|        - |  276 | `			}` |
|        - |  277 | `			/* Handle ambiguity */` |
|     3424 |  278 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  279 | `				/* Unary opertors have prcedence here over binary operators */` |
|      210 |  280 | `				return &aOpTable[n];` |
|        - |  281 | `			}` |
|     3216 |  282 | `			if( pLast->nType & PH7_TK_OP ){` |
|      120 |  283 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  284 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      120 |  285 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  286 | `					/* Unary opertors have prcedence here over binary operators */` |
|      112 |  287 | `					return &aOpTable[n];` |
|        - |  288 | `				}` |
|        - |  289 |  |
|        4 |  290 | `			}` |
|     1552 |  291 | `		}` |
| 22033652 |  292 | `		++n; /* Next operator in the table */` |
|        2 |  293 | `	}` |
|        - |  294 | `	/* No such operator */` |
|      ! 0 |  295 | `	return 0;` |
|   353506 |  296 |  |
|        - |  297 | `/*` |
|        - |  298 | ` * Delimit a set of token stream.` |
|        - |  299 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  300 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  301 | ` */` |
|   358822 |  302 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        2 |  303 |  |
|   358824 |  304 | `	SyToken *pCur = pIn;` |
|   358824 |  305 | `	sxi32 iNest = 1;` |
|  2065796 |  306 | `	for(;;){` |
|  4131594 |  307 | `		if( pCur >= pEnd ){` |
|      124 |  308 | `			break;` |
|        - |  309 | `		}` |
|  4131472 |  310 | `		if( pCur->nType & nTokStart ){` |
|        - |  311 | `			/* Increment nesting level */` |
|   228890 |  312 | `			iNest++;` |
|  4017028 |  313 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  314 | `			/* Decrement nesting level */` |
|   587590 |  315 | `			iNest--;` |
|   587590 |  316 | `			if( iNest <= 0 ){` |
|   358702 |  317 | `				break;` |
|        - |  318 | `			}` |
|   114444 |  319 | `		}` |
|        - |  320 | `		/* Advance cursor */` |
|  3772772 |  321 | `		pCur++;` |
|        2 |  322 | `	}` |
|        - |  323 | `	/* Point to the end of the chunk */` |
|   358824 |  324 | `	*ppEnd = pCur;` |
|   358824 |  325 |  |
|        - |  326 | `/*` |
|        - |  327 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  328 | ` * Note on reserved keywords.` |
|        - |  329 | ` *  According to the PHP language reference manual:` |
|        - |  330 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  331 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  332 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  333 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  334 | ` */` |
|    10888 |  335 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        2 |  336 |  |
|    16268 |  337 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    10799 |  338 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  339 | `		){` |
|      142 |  340 | `			return TRUE;` |
|        - |  341 | `	}` |
|    10750 |  342 | `	if( bCheckFunc ){` |
|       92 |  343 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       68 |  344 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       53 |  345 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       34 |  346 | `				return TRUE;` |
|        - |  347 | `		}` |
|       20 |  348 | `	}` |
|        - |  349 | `	/* Not a language construct */` |
|    10718 |  350 | `	return FALSE;` |
|     5446 |  351 |  |
|        - |  352 | `/*` |
|        - |  353 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  354 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  355 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  356 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  357 | ` */` |
|   623168 |  358 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        2 |  359 |  |
|        - |  360 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  361 | `	sxi32 i,rc;` |
|        - |  362 |  |
|   623170 |  363 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  364 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       14 |  365 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       14 |  366 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        6 |  367 | `	}` |
|   623170 |  368 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  3370290 |  369 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  2747152 |  370 | `		if( apNode[i]->xCode == PH7_CompileShortArray ){` |
|        - |  371 | `			/* Short array literal: brackets are self-contained, skip */` |
|      176 |  372 | `			continue;` |
|        - |  373 | `		}` |
|  2746978 |  374 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   315634 |  375 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    16144 |  376 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  377 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   293982 |  378 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  379 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  380 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  381 | `						 */` |
|   293982 |  382 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   293982 |  383 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   293982 |  384 | `						apNode[i]->pOp = &sFCallOp;` |
|   146990 |  385 | `					}` |
|   146990 |  386 | `			}` |
|   315634 |  387 | `			iParen++;` |
|  2589162 |  388 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   315630 |  389 | `			if( iParen <= 0 ){` |
|        9 |  390 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        9 |  391 | `				if( rc != SXERR_ABORT ){` |
|        9 |  392 | `					rc = SXERR_SYNTAX;` |
|        4 |  393 | `				}` |
|        9 |  394 | `				return rc;` |
|        - |  395 | `			}` |
|   315622 |  396 | `			iParen--;` |
|  2273528 |  397 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    65850 |  398 | `			iSquare++;` |
|  2082794 |  399 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    65864 |  400 | `			if( iSquare <= 0 ){` |
|        7 |  401 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        7 |  402 | `				if( rc != SXERR_ABORT ){` |
|        7 |  403 | `					rc = SXERR_SYNTAX;` |
|        3 |  404 | `				}` |
|        7 |  405 | `				return rc;` |
|        - |  406 | `			}` |
|    65858 |  407 | `			iSquare--;` |
|  2016936 |  408 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       11 |  409 | `			iBraces++;` |
|       11 |  410 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  411 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  412 | `				int iNest = 1;` |
|       11 |  413 | `				sxi32 j=i+1;` |
|        - |  414 | `				/*` |
|        - |  415 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  416 | `				 */` |
|       11 |  417 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  418 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  419 | `				pOp = aOpTable;` |
|       11 |  420 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       51 |  421 | `				while( pOp < pEnd ){` |
|       51 |  422 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  423 | `						break;` |
|        - |  424 | `					}` |
|       41 |  425 | `					pOp++;` |
|        1 |  426 | `				}` |
|       11 |  427 | `				if( pOp >= pEnd ){` |
|      ! 0 |  428 | `					pOp = 0;` |
|      ! 0 |  429 | `				}` |
|       11 |  430 | `				if( pOp ){` |
|       11 |  431 | `					apNode[i]->pOp = pOp;` |
|       11 |  432 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  433 | `				}` |
|       11 |  434 | `				iBraces--;` |
|       11 |  435 | `				iSquare++;` |
|       21 |  436 | `				while( j < nNode ){` |
|       21 |  437 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  438 | `						/* Increment nesting level */` |
|      ! 0 |  439 | `						iNest++;` |
|       21 |  440 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  441 | `						/* Decrement nesting level */` |
|       11 |  442 | `						iNest--;` |
|       11 |  443 | `						if( iNest < 1 ){` |
|       11 |  444 | `							break;` |
|        - |  445 | `						}` |
|      ! 0 |  446 | `					}` |
|       11 |  447 | `					j++;` |
|        1 |  448 | `				}` |
|       11 |  449 | `				if( j < nNode ){` |
|       11 |  450 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  451 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  452 | `				}` |
|        - |  453 |  |
|        6 |  454 | `			}` |
|  1984003 |  455 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       13 |  456 | `			if( iBraces <= 0 ){` |
|       13 |  457 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       13 |  458 | `				if( rc != SXERR_ABORT ){` |
|       13 |  459 | `					rc = SXERR_SYNTAX;` |
|        6 |  460 | `				}` |
|       13 |  461 | `				return rc;` |
|        - |  462 | `			}` |
|      ! 0 |  463 | `			iBraces--;` |
|  1983986 |  464 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     1868 |  465 | `			if( iQuesty <= 0 ){` |
|        5 |  466 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        5 |  467 | `				if( rc != SXERR_ABORT ){` |
|        5 |  468 | `					rc = SXERR_SYNTAX;` |
|        2 |  469 | `				}` |
|        5 |  470 | `				return rc;` |
|        - |  471 | `			}` |
|     1864 |  472 | `			iQuesty--;` |
|  1983051 |  473 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   551918 |  474 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   551918 |  475 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     1866 |  476 | `				iQuesty++;` |
|   550986 |  477 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      306 |  478 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|       11 |  479 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|       11 |  480 | `					sxu32 n = 0;` |
|       11 |  481 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        7 |  482 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        3 |  483 | `					}` |
|        - |  484 | `					/*` |
|        - |  485 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  486 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  487 | `					 */` |
|      245 |  488 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      235 |  489 | `						++n;` |
|        1 |  490 | `					}` |
|       11 |  491 | `					pOp = &aOpTable[n];` |
|        - |  492 | `					/* Mark as binary '+' or '-',not an unary */` |
|       11 |  493 | `					apNode[i]->pOp = pOp;` |
|       11 |  494 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        5 |  495 | `				}` |
|      152 |  496 | `			}` |
|   275958 |  497 | `		}` |
|  1373475 |  498 | `	}` |
|   623140 |  499 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       17 |  500 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       17 |  501 | `		if( rc != SXERR_ABORT ){` |
|       17 |  502 | `			rc = SXERR_SYNTAX;` |
|        8 |  503 | `		}` |
|       17 |  504 | `		return rc;` |
|        - |  505 | `	}` |
|   623124 |  506 | `	return SXRET_OK;` |
|   311586 |  507 |  |
|        - |  508 | `/*` |
|        - |  509 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  510 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  511 | ` */` |
|   502688 |  512 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        2 |  513 |  |
|   502690 |  514 | `	SyToken *pIn = *ppCur;` |
|        - |  515 | `	/* Jump the first literal seen */` |
|   502690 |  516 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   502672 |  517 | `		pIn++;` |
|   251335 |  518 | `	}` |
|   251368 |  519 | `	for(;;){` |
|   502738 |  520 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       50 |  521 | `			pIn++;` |
|       50 |  522 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       50 |  523 | `				pIn++;` |
|       24 |  524 | `			}` |
|       26 |  525 | `		}else{` |
|   251346 |  526 | `			break;` |
|        - |  527 | `		}` |
|        2 |  528 | `	}` |
|        - |  529 | `	/* Synchronize pointers */` |
|   502690 |  530 | `	*ppCur = pIn;` |
|   502690 |  531 |  |
|        - |  532 | `/*` |
|        - |  533 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  534 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  535 | ` * Note on annonymous functions.` |
|        - |  536 | ` *  According to the PHP language reference manual:` |
|        - |  537 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  538 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  539 | ` *  parameters, but they have many other uses.` |
|        - |  540 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  541 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  542 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  543 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  544 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  545 | ` *` |
|        - |  546 | ` * Some example:` |
|        - |  547 | ` *  $greet = function($name)` |
|        - |  548 | ` * {` |
|        - |  549 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  550 | ` * };` |
|        - |  551 | ` *  $greet('World');` |
|        - |  552 | ` *  $greet('PHP');` |
|        - |  553 | ` *` |
|        - |  554 | ` * $double = function($a) {` |
|        - |  555 | ` *   return $a * 2;` |
|        - |  556 | ` * };` |
|        - |  557 | ` * // This is our range of numbers` |
|        - |  558 | ` * $numbers = range(1, 5);` |
|        - |  559 | ` * // Use the Annonymous function as a callback here to` |
|        - |  560 | ` * // double the size of each element in our` |
|        - |  561 | ` * // range` |
|        - |  562 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  563 | ` * print implode(' ', $new_numbers);` |
|        - |  564 | ` */` |
|      192 |  565 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        2 |  566 |  |
|      194 |  567 | `	SyToken *pIn = *ppCur;` |
|        - |  568 | `	sxu32 nLine;` |
|        - |  569 | `	sxi32 rc;` |
|        - |  570 | `	/* Jump the 'function' keyword */` |
|      194 |  571 | `	nLine = pIn->nLine;` |
|      194 |  572 | `	pIn++;` |
|      194 |  573 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  574 | `		pIn++;` |
|        1 |  575 | `	}` |
|      194 |  576 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  577 | `		/* Syntax error */` |
|        5 |  578 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        5 |  579 | `		if( rc != SXERR_ABORT ){` |
|        5 |  580 | `			rc = SXERR_SYNTAX;` |
|        2 |  581 | `		}` |
|        5 |  582 | `		goto Synchronize;` |
|        - |  583 | `	}` |
|      190 |  584 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      190 |  585 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      190 |  586 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  587 | `		/* Syntax error */` |
|        5 |  588 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  589 | `		if( rc != SXERR_ABORT ){` |
|        5 |  590 | `			rc = SXERR_SYNTAX;` |
|        2 |  591 | `		}` |
|        5 |  592 | `		goto Synchronize;` |
|        - |  593 | `	}` |
|      186 |  594 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  595 | `	/* Skip optional return type declaration ': [?] type' */` |
|      186 |  596 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        5 |  597 | `		pIn++; /* Skip ':' */` |
|        - |  598 | `		/* Skip optional '?' nullable prefix */` |
|        5 |  599 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      ! 0 |  600 | `			pIn++;` |
|      ! 0 |  601 | `		}` |
|        - |  602 | `		/* Skip the type name (keyword or identifier) */` |
|        5 |  603 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        5 |  604 | `			pIn++;` |
|        2 |  605 | `		}` |
|        2 |  606 | `	}` |
|      186 |  607 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|       30 |  608 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  609 | `		/* Check if we are dealing with a closure */` |
|       30 |  610 | `		if( nKey == PH7_TKWRD_USE ){` |
|       22 |  611 | `			pIn++; /* Jump the 'use' keyword */` |
|       22 |  612 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  613 | `				/* Syntax error */` |
|        5 |  614 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  615 | `				if( rc != SXERR_ABORT ){` |
|        5 |  616 | `					rc = SXERR_SYNTAX;` |
|        2 |  617 | `				}` |
|        5 |  618 | `				goto Synchronize;` |
|        - |  619 | `			}` |
|       18 |  620 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       18 |  621 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       18 |  622 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  623 | `				/* Syntax error */` |
|        5 |  624 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        5 |  625 | `				if( rc != SXERR_ABORT ){` |
|        5 |  626 | `					rc = SXERR_SYNTAX;` |
|        2 |  627 | `				}` |
|        5 |  628 | `				goto Synchronize;` |
|        - |  629 | `			}` |
|       14 |  630 | `			pIn++;` |
|        8 |  631 | `		}else{` |
|        - |  632 | `			/* Syntax error */` |
|        9 |  633 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        9 |  634 | `			if( rc != SXERR_ABORT ){` |
|        9 |  635 | `				rc = SXERR_SYNTAX;` |
|        4 |  636 | `			}` |
|        9 |  637 | `			goto Synchronize;` |
|        - |  638 | `		}` |
|        6 |  639 | `	}` |
|      170 |  640 | `	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){` |
|      170 |  641 | `		pIn++; /* Jump the leading curly '{' */` |
|      170 |  642 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      170 |  643 | `		if( pIn < pEnd ){` |
|      170 |  644 | `			pIn++;` |
|       84 |  645 | `		}` |
|       86 |  646 | `	}else{` |
|        - |  647 | `		/* Syntax error */` |
|      ! 0 |  648 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  649 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  650 | `			return SXERR_ABORT;` |
|        - |  651 | `		}` |
|        - |  652 | `	}` |
|      170 |  653 | `	rc = SXRET_OK;` |
|       96 |  654 | `Synchronize:` |
|        - |  655 | `	/* Synchronize pointers */` |
|      194 |  656 | `	*ppCur = pIn;` |
|      194 |  657 | `	return rc;` |
|       98 |  658 |  |
|        - |  659 | `/*` |
|        - |  660 | ` * Extract a single expression node from the input.` |
|        - |  661 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  662 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  663 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  664 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  665 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  666 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  667 | ` */` |
|  2747302 |  668 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm)` |
|        2 |  669 |  |
|        - |  670 | `	ph7_expr_node *pNode;` |
|        - |  671 | `	SyToken *pCur;` |
|        - |  672 | `	sxi32 rc;` |
|        - |  673 | `	/* Allocate a new node */` |
|  2747304 |  674 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  2747304 |  675 | `	if( pNode == 0 ){` |
|        - |  676 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  677 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  678 | `		 */` |
|      ! 0 |  679 | `		return SXERR_MEM;` |
|        - |  680 | `	}` |
|        - |  681 | `	/* Zero the structure */` |
|  2747304 |  682 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  2747304 |  683 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  684 | `	/* Point to the head of the token stream */` |
|  2747304 |  685 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  686 | `	/* Start collecting tokens */` |
|  2747304 |  687 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|        - |  688 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  689 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       15 |  690 | `		pCur++;` |
|       15 |  691 | `		pGen->pIn = pCur;` |
|       15 |  692 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       15 |  693 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm);` |
|       15 |  694 | `		if( rc == SXRET_OK && *ppNode ){` |
|       15 |  695 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|        7 |  696 | `		}` |
|       15 |  697 | `		return rc;` |
|        - |  698 | `	}` |
|  2747290 |  699 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  700 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  701 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  702 | `		 */` |
|      178 |  703 | `		pCur++; /* Skip the opening '[' */` |
|      178 |  704 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      178 |  705 | `		if( pCur < pGen->pEnd ){` |
|      178 |  706 | `			pCur++; /* Skip past the closing ']' */` |
|       90 |  707 | `		}else{` |
|      ! 0 |  708 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  709 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  710 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  711 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  712 | `			}` |
|      ! 0 |  713 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  714 | `			return rc;` |
|        - |  715 | `		}` |
|      178 |  716 | `		pNode->xCode = PH7_CompileShortArray;` |
|  2747202 |  717 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - |  718 | `		/* Point to the instance that describe this operator */` |
|   617800 |  719 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - |  720 | `		/* Advance the stream cursor */` |
|   617800 |  721 | `		pCur++;` |
|  2438215 |  722 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - |  723 | `		/* Isolate variable */` |
|  1499606 |  724 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   749810 |  725 | `			pCur++; /* Variable variable */` |
|        2 |  726 | `		}` |
|   749798 |  727 | `		if( pCur < pGen->pEnd ){` |
|   749798 |  728 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - |  729 | `				/* Variable name */` |
|   749770 |  730 | `				pCur++;` |
|   374914 |  731 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       22 |  732 | `				pCur++;` |
|        - |  733 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       22 |  734 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       22 |  735 | `				if( pCur < pGen->pEnd ){` |
|       18 |  736 | `					pCur++;` |
|       10 |  737 | `				}else{` |
|        5 |  738 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        5 |  739 | `					if( rc != SXERR_ABORT ){` |
|        5 |  740 | `						rc = SXERR_SYNTAX;` |
|        2 |  741 | `					}` |
|        5 |  742 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        5 |  743 | `					return rc;` |
|        - |  744 | `				}` |
|        8 |  745 | `			}` |
|   374896 |  746 | `		}` |
|   749794 |  747 | `		pNode->xCode = PH7_CompileVariable;` |
|  1754416 |  748 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    33290 |  749 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    33290 |  750 | `		 if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - |  751 | `			 /* List/Array node */` |
|    22312 |  752 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  753 | `				 /* Assume a literal */` |
|       17 |  754 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  755 | `				 pNode->xCode = PH7_CompileLiteral;` |
|        9 |  756 | `			 }else{` |
|    22296 |  757 | `				 pCur += 2;` |
|        - |  758 | `				 /* Collect array/list tokens */` |
|    22296 |  759 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    22296 |  760 | `				 if( pCur < pGen->pEnd ){` |
|    22294 |  761 | `					 pCur++;` |
|    11148 |  762 | `				 }else{` |
|        - |  763 | `					 /* Syntax error */` |
|        4 |  764 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 |  765 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 |  766 | `					 if( rc != SXERR_ABORT ){` |
|        3 |  767 | `						 rc = SXERR_SYNTAX;` |
|        1 |  768 | `					 }` |
|        3 |  769 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  770 | `					 return rc;` |
|        - |  771 | `				 }` |
|    22294 |  772 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    22294 |  773 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       28 |  774 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       28 |  775 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - |  776 | `						 /* Syntax error */` |
|        3 |  777 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 |  778 | `						 if( rc != SXERR_ABORT ){` |
|        3 |  779 | `							 rc = SXERR_SYNTAX;` |
|        1 |  780 | `						 }` |
|        3 |  781 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  782 | `						 return rc;` |
|        - |  783 | `					 }` |
|       12 |  784 | `				 }` |
|        2 |  785 | `			 }` |
|    22133 |  786 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - |  787 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       34 |  788 | `			 pCur++; /* Skip 'yield' keyword */` |
|       34 |  789 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - |  790 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - |  791 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       34 |  792 | `			 pNode->xCode = PH7_CompileYield;` |
|    10964 |  793 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - |  794 | `			 /* Annonymous function */` |
|      194 |  795 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - |  796 | `				 /* Assume a literal */` |
|      ! 0 |  797 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 |  798 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 |  799 | `			 }else{` |
|        - |  800 | `				 /* Assemble annonymous functions body */` |
|      194 |  801 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      194 |  802 | `				 if( rc != SXRET_OK ){` |
|       25 |  803 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       25 |  804 | `					 return rc;` |
|        - |  805 | `				 }` |
|      170 |  806 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        2 |  807 | `			  }` |
|    10840 |  808 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - |  809 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       76 |  810 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       76 |  811 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       39 |  812 | `		 }else{` |
|        - |  813 | `			 /* Assume a literal */` |
|    10682 |  814 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    10682 |  815 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        2 |  816 | `		 }` |
|  1362862 |  817 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - |  818 | `		 /* Constants,function name,namespace path,class name... */` |
|   491994 |  819 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   491994 |  820 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   245998 |  821 | `	 }else{` |
|   854240 |  822 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - |  823 | `			 /* Point to the code generator routine */` |
|   155216 |  824 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   155216 |  825 | `			 if( pNode->xCode == 0 ){` |
|        3 |  826 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 |  827 | `				 if( rc != SXERR_ABORT ){` |
|        3 |  828 | `					 rc = SXERR_SYNTAX;` |
|        1 |  829 | `				 }` |
|        3 |  830 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 |  831 | `				 return rc;` |
|        - |  832 | `			 }` |
|    77606 |  833 | `		 }` |
|        - |  834 | `		/* Advance the stream cursor */` |
|   854238 |  835 | `		pCur++;` |
|        - |  836 | `	 }` |
|        - |  837 | `	/* Point to the end of the token stream */` |
|  2747256 |  838 | `	pNode->pEnd = pCur;` |
|        - |  839 | `	/* Save the node for later processing */` |
|  2747256 |  840 | `	*ppNode = pNode;` |
|        - |  841 | `	/* Synchronize cursors */` |
|  2747256 |  842 | `	pGen->pIn = pCur;` |
|  2747256 |  843 | `	return SXRET_OK;` |
|  1373653 |  844 |  |
|        - |  845 | `/*` |
|        - |  846 | ` * Point to the next expression that should be evaluated shortly.` |
|        - |  847 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - |  848 | ` * level is zero.` |
|        - |  849 | ` */` |
|    66462 |  850 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        2 |  851 |  |
|    66464 |  852 | `	SyToken *pCur = pStart;` |
|    66464 |  853 | `	sxi32 iNest = 0;` |
|    66464 |  854 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - |  855 | `		/* Last expression */` |
|    35498 |  856 | `		return SXERR_EOF;` |
|        - |  857 | `	}` |
|   124374 |  858 | `	while( pCur < pEnd ){` |
|   112486 |  859 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    19080 |  860 | `			break;` |
|        - |  861 | `		}` |
|    93408 |  862 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     5182 |  863 | `			iNest++;` |
|    90818 |  864 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     5184 |  865 | `			iNest--;` |
|     2591 |  866 | `		}` |
|    93408 |  867 | `		pCur++;` |
|        2 |  868 | `	}` |
|    30968 |  869 | `	*ppNext = pCur;` |
|    30968 |  870 | `	return SXRET_OK;` |
|    33233 |  871 |  |
|        - |  872 | `/*` |
|        - |  873 | ` * Free an expression tree.` |
|        - |  874 | ` */` |
|  2350784 |  875 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        2 |  876 |  |
|  2350786 |  877 | `	if( pNode->pLeft ){` |
|        - |  878 | `		/* Release the left tree */` |
|   876802 |  879 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   438400 |  880 | `	}` |
|  2350786 |  881 | `	if( pNode->pRight ){` |
|        - |  882 | `		/* Release the right tree */` |
|   458828 |  883 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   229413 |  884 | `	}` |
|  2350786 |  885 | `	if( pNode->pCond ){` |
|        - |  886 | `		/* Release the conditional tree used by the ternary operator */` |
|     1862 |  887 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|      930 |  888 | `	}` |
|  2350786 |  889 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - |  890 | `		ph7_expr_node **apArg;` |
|        - |  891 | `		sxu32 n;` |
|        - |  892 | `		/* Release node arguments */` |
|   311766 |  893 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   658310 |  894 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   346546 |  895 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   173274 |  896 | `		}` |
|   311766 |  897 | `		SySetRelease(&pNode->aNodeArgs);` |
|   155882 |  898 | `	}` |
|        - |  899 | `	/* Finally,release this node */` |
|  2350786 |  900 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  2350786 |  901 |  |
|        - |  902 | `/*` |
|        - |  903 | ` * Free an expression tree.` |
|        - |  904 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - |  905 | ` */` |
|   623202 |  906 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        2 |  907 |  |
|        - |  908 | `	ph7_expr_node **apNode;` |
|        - |  909 | `	sxu32 n;` |
|   623204 |  910 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  3370458 |  911 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  2747256 |  912 | `		if( apNode[n] ){` |
|   623476 |  913 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   311737 |  914 | `		}` |
|  1373629 |  915 | `	}` |
|   623204 |  916 | `	return SXRET_OK;` |
|        2 |  917 |  |
|        - |  918 | `/*` |
|        - |  919 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - |  920 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - |  921 | ` */` |
|   199678 |  922 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        2 |  923 |  |
|        - |  924 | `	sxi32 iExprOp;` |
|   199680 |  925 | `	if( pNode->pOp == 0 ){` |
|   130018 |  926 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - |  927 | `	}` |
|    69664 |  928 | `	iExprOp = pNode->pOp->iOp;` |
|    69664 |  929 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    43682 |  930 | `			return TRUE;` |
|        - |  931 | `	}` |
|    25984 |  932 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    25980 |  933 | `		if( pNode->pLeft->pOp ) {` |
|        6 |  934 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        2 |  935 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 |  936 | `				return FALSE;` |
|        1 |  937 | `			}` |
|    25977 |  938 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 |  939 | `			return FALSE;` |
|        - |  940 | `		}` |
|    25980 |  941 | `		return TRUE;` |
|        - |  942 | `	}` |
|        5 |  943 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        5 |  944 | `		return TRUE;` |
|        - |  945 | `	}` |
|        - |  946 | `	/* Not a modifiable l or r-value */` |
|      ! 0 |  947 | `	return FALSE;` |
|    99841 |  948 |  |
|        - |  949 | `/* Forward declaration */` |
|        - |  950 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - |  951 | `/* Macro to check if the given node is a terminal.` |
|        - |  952 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - |  953 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - |  954 | ` * linked ternary/elvis node). */` |
|        - |  955 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - |  956 | `/*` |
|        - |  957 | ` * Buid an expression tree for each given function argument.` |
|        - |  958 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  959 | ` */` |
|   258788 |  960 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 |  961 |  |
|        - |  962 | `	sxi32 iNest,iCur,iNode;` |
|        - |  963 | `	sxi32 rc;` |
|        - |  964 | `	/* Process function arguments from left to right */` |
|   258790 |  965 | `	iCur = 0;` |
|   276177 |  966 | `	for(;;){` |
|   552356 |  967 | `		if( iCur >= nToken ){` |
|        - |  968 | `			/* No more arguments to process */` |
|   258788 |  969 | `			break;` |
|        - |  970 | `		}` |
|   293570 |  971 | `		iNode = iCur;` |
|   293570 |  972 | `		iNest = 0;` |
|   734116 |  973 | `		while( iCur < nToken ){` |
|   475330 |  974 | `			if( apNode[iCur] ){` |
|   465074 |  975 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    17393 |  976 | `					break;` |
|   430292 |  977 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    23840 |  978 | `					iNest++;` |
|   418373 |  979 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    23828 |  980 | `					iNest--;` |
|    11913 |  981 | `				}` |
|   215145 |  982 | `			}` |
|   440548 |  983 | `			iCur++;` |
|        2 |  984 | `		}` |
|   293570 |  985 | `		if( iCur > iNode ){` |
|   293566 |  986 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        2 |  987 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 |  988 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - |  989 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 |  990 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 |  991 | `					apNode[iNode] = 0;` |
|      ! 0 |  992 | `			}` |
|   293568 |  993 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   293568 |  994 | `			if( apNode[iNode] ){` |
|        - |  995 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   293568 |  996 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   146785 |  997 | `			}else{` |
|        - |  998 | `				/* Empty function argument */` |
|      ! 0 |  999 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");` |
|      ! 0 | 1000 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1001 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1002 | `				}` |
|      ! 0 | 1003 | `				return rc;` |
|        - | 1004 | `			}` |
|   146785 | 1005 | `		}else{` |
|        3 | 1006 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|        3 | 1007 | `			if( rc != SXERR_ABORT ){` |
|        3 | 1008 | `				rc = SXERR_SYNTAX;` |
|        1 | 1009 | `			}` |
|        3 | 1010 | `			return rc;` |
|        - | 1011 | `		}` |
|        - | 1012 | `		/* Jump trailing comma */` |
|   293568 | 1013 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    34782 | 1014 | `			iCur++;` |
|    34782 | 1015 | `			if( iCur >= nToken ){` |
|        - | 1016 | `				/* missing function argument */` |
|      ! 0 | 1017 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");` |
|      ! 0 | 1018 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1019 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1020 | `				}` |
|      ! 0 | 1021 | `				return rc;` |
|        - | 1022 | `			}` |
|    17390 | 1023 | `		}` |
|        2 | 1024 | `	}` |
|   258788 | 1025 | `	return SXRET_OK;` |
|   129396 | 1026 |  |
|        - | 1027 | ` /*` |
|        - | 1028 | `  * Create an expression tree from an array of tokens.` |
|        - | 1029 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1030 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1031 | `  */` |
|   997734 | 1032 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        2 | 1033 | ` {` |
|        - | 1034 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1035 | `	 ph7_expr_node *pNode;` |
|        - | 1036 | `	 sxi32 iCur;` |
|        - | 1037 | `	 sxi32 rc;` |
|   997736 | 1038 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1039 | `		 /* TICKET 1433-17: self evaluating node */` |
|   460674 | 1040 | `		 return SXRET_OK;` |
|        - | 1041 | `	 }` |
|        - | 1042 | `	 /* Process expressions enclosed in parenthesis first */` |
|  3299646 | 1043 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1044 | `		 sxi32 iNest;` |
|        - | 1045 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1046 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1047 | `		  */` |
|  2762586 | 1048 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  2740944 | 1049 | `			 continue;` |
|        - | 1050 | `		 }` |
|    21644 | 1051 | `		 iNest = 1;` |
|    21644 | 1052 | `		 iLeft = iCur;` |
|        - | 1053 | `		 /* Find the closing parenthesis */` |
|    21644 | 1054 | `		 iCur++;` |
|   144144 | 1055 | `		 while( iCur < nToken ){` |
|   144144 | 1056 | `			 if( apNode[iCur] ){` |
|   144144 | 1057 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1058 | `					 /* Decrement nesting level */` |
|    37570 | 1059 | `					 iNest--;` |
|    37570 | 1060 | `					 if( iNest <= 0 ){` |
|    21644 | 1061 | `						 break;` |
|        2 | 1062 | `					 }` |
|   114539 | 1063 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1064 | `					 /* Increment nesting level */` |
|    15928 | 1065 | `					 iNest++;` |
|     7963 | 1066 | `				 }` |
|    61250 | 1067 | `			 }` |
|   122502 | 1068 | `			 iCur++;` |
|        2 | 1069 | `		 }` |
|    21644 | 1070 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1071 | `			 /* Recurse and process this expression */` |
|    21644 | 1072 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    21644 | 1073 | `			 if( rc != SXRET_OK ){` |
|        3 | 1074 | `				 return rc;` |
|        - | 1075 | `			 }` |
|    10820 | 1076 | `		 }` |
|        - | 1077 | `		 /* Free the left and right nodes */` |
|    21642 | 1078 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    21642 | 1079 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    21642 | 1080 | `		 apNode[iLeft] = 0;` |
|    21642 | 1081 | `		 apNode[iCur] = 0;` |
|    10822 | 1082 | `	 }` |
|        - | 1083 | `	  /* Process expressions enclosed in braces */` |
|  3438228 | 1084 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1085 | `		 sxi32 iNest;` |
|        - | 1086 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1087 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1088 | `		  */` |
|  2906716 | 1089 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  2906716 | 1090 | `			 continue;` |
|        - | 1091 | `		 }` |
|      ! 0 | 1092 | `		 iNest = 1;` |
|      ! 0 | 1093 | `		 iLeft = iCur;` |
|        - | 1094 | `		 /* Find the closing parenthesis */` |
|      ! 0 | 1095 | `		 iCur++;` |
|      ! 0 | 1096 | `		 while( iCur < nToken ){` |
|      ! 0 | 1097 | `			 if( apNode[iCur] ){` |
|      ! 0 | 1098 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1099 | `					 /* Decrement nesting level */` |
|      ! 0 | 1100 | `					 iNest--;` |
|      ! 0 | 1101 | `					 if( iNest <= 0 ){` |
|      ! 0 | 1102 | `						 break;` |
|      ! 0 | 1103 | `					 }` |
|      ! 0 | 1104 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1105 | `					 /* Increment nesting level */` |
|      ! 0 | 1106 | `					 iNest++;` |
|      ! 0 | 1107 | `				 }` |
|      ! 0 | 1108 | `			 }` |
|      ! 0 | 1109 | `			 iCur++;` |
|      ! 0 | 1110 | `		 }` |
|      ! 0 | 1111 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1112 | `			 /* Recurse and process this expression */` |
|      ! 0 | 1113 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      ! 0 | 1114 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1115 | `				 return rc;` |
|        - | 1116 | `			 }` |
|      ! 0 | 1117 | `		 }` |
|        - | 1118 | `		 /* Free the left and right nodes */` |
|      ! 0 | 1119 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      ! 0 | 1120 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      ! 0 | 1121 | `		 apNode[iLeft] = 0;` |
|      ! 0 | 1122 | `		 apNode[iCur] = 0;` |
|      ! 0 | 1123 | `	 }` |
|        - | 1124 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   531514 | 1125 | `	 iLeft = -1;` |
|  3438216 | 1126 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2906710 | 1127 | `		 if( apNode[iCur] == 0 ){` |
|  1131226 | 1128 | `			 continue;` |
|        - | 1129 | `		 }` |
|  1775486 | 1130 | `		 pNode = apNode[iCur];` |
|  1775486 | 1131 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   458118 | 1132 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1133 | `				 /* Collect function arguments */` |
|   293978 | 1134 | `				 sxi32 iPtr = 0;` |
|   293978 | 1135 | `				 sxi32 nFuncTok = 0;` |
|  1063284 | 1136 | `				 while( nFuncTok + iCur < nToken ){` |
|  1063284 | 1137 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1053028 | 1138 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   304678 | 1139 | `							 iPtr++;` |
|   900690 | 1140 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   304678 | 1141 | `							 iPtr--;` |
|   304678 | 1142 | `							 if( iPtr <= 0 ){` |
|   293978 | 1143 | `								 break;` |
|        - | 1144 | `							 }` |
|     5350 | 1145 | `						 }` |
|   379525 | 1146 | `					 }` |
|   769308 | 1147 | `					 nFuncTok++;` |
|        2 | 1148 | `				 }` |
|   293978 | 1149 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1150 | `					 /* Syntax error */` |
|      ! 0 | 1151 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1152 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1153 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1154 | `					 }` |
|      ! 0 | 1155 | `					 return rc;` |
|        - | 1156 | `				 }` |
|   293978 | 1157 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1158 | `					 /* Syntax error */` |
|      ! 0 | 1159 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1160 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1161 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1162 | `					 }` |
|      ! 0 | 1163 | `					 return rc;` |
|        - | 1164 | `				 }` |
|   293978 | 1165 | `				 if( nFuncTok > 1 ){` |
|        - | 1166 | `					 /* Process function arguments */` |
|   258790 | 1167 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   258790 | 1168 | `					 if( rc != SXRET_OK ){` |
|        3 | 1169 | `						 return rc;` |
|        - | 1170 | `					 }` |
|   129393 | 1171 | `				 }` |
|        - | 1172 | `				 /* Link the node to the tree */` |
|   293976 | 1173 | `				 pNode->pLeft = apNode[iLeft];` |
|   293976 | 1174 | `				 apNode[iLeft] = 0;` |
|  1063276 | 1175 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   769302 | 1176 | `					 apNode[iCur+iPtr] = 0;` |
|   384652 | 1177 | `				 }` |
|   311129 | 1178 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1179 | `				 /* Subscripting */` |
|    65858 | 1180 | `				 sxi32 iArrTok = iCur + 1;` |
|    65858 | 1181 | `				 sxi32 iNest = 1;` |
|    65925 | 1182 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       12 | 1183 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       10 | 1184 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    65856 | 1185 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){` |
|        - | 1186 | `						 /* Syntax error */` |
|      ! 0 | 1187 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1188 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1189 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1190 | `						 }` |
|      ! 0 | 1191 | `						 return rc;` |
|        - | 1192 | `				 }` |
|        - | 1193 | `				 /* Collect index tokens */` |
|   118946 | 1194 | `				 while( iArrTok < nToken ){` |
|   118946 | 1195 | `					 if( apNode[iArrTok] ){` |
|   118914 | 1196 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1197 | `							 /* Increment nesting level */` |
|      ! 0 | 1198 | `							 iNest++;` |
|   118914 | 1199 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1200 | `							 /* Decrement nesting level */` |
|    65858 | 1201 | `							 iNest--;` |
|    65858 | 1202 | `							 if( iNest <= 0 ){` |
|    65858 | 1203 | `								 break;` |
|        - | 1204 | `							 }` |
|      ! 0 | 1205 | `						 }` |
|    26528 | 1206 | `					 }` |
|    53090 | 1207 | `					 ++iArrTok;` |
|        2 | 1208 | `				 }` |
|    65858 | 1209 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1210 | `					 /* Recurse and process this expression */` |
|    52980 | 1211 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    52980 | 1212 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1213 | `						 return rc;` |
|        - | 1214 | `					 }` |
|        - | 1215 | `					 /* Link the node to it's index */` |
|    52980 | 1216 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    26489 | 1217 | `				 }` |
|        - | 1218 | `				 /* Link the node to the tree */` |
|    65858 | 1219 | `				 pNode->pLeft = apNode[iLeft];` |
|    65858 | 1220 | `				 pNode->pRight = 0;` |
|    65858 | 1221 | `				 apNode[iLeft] = 0;` |
|   184802 | 1222 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   118946 | 1223 | `					 apNode[iNest] = 0;` |
|    59474 | 1224 | `				 }` |
|    32930 | 1225 | `			 }else{` |
|        - | 1226 | `				 /* Member access operators [i.e: '->','::'] */` |
|    98286 | 1227 | `				  iRight = iCur + 1;` |
|    98286 | 1228 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      ! 0 | 1229 | `					 iRight++;` |
|      ! 0 | 1230 | `				 }` |
|    98286 | 1231 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1232 | `					 /* Syntax error */` |
|        5 | 1233 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1234 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1235 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1236 | `					 }` |
|        5 | 1237 | `					 return rc;` |
|        - | 1238 | `				 }` |
|        - | 1239 | `				 /* Link the node to the tree */` |
|    98282 | 1240 | `				 pNode->pLeft = apNode[iLeft];` |
|    98282 | 1241 | `				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&` |
|    98154 | 1242 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1243 | `						 /* Syntax error */` |
|      ! 0 | 1244 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1245 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1246 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1247 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1248 | `						 }` |
|      ! 0 | 1249 | `						 return rc;` |
|        - | 1250 | `				 }` |
|    98282 | 1251 | `				 pNode->pRight = apNode[iRight];` |
|    98282 | 1252 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1253 | `			 }` |
|   229055 | 1254 | `		 }` |
|  1775480 | 1255 | `		 iLeft = iCur;` |
|   887741 | 1256 | `	 }` |
|        - | 1257 | `	 /* Handle left associative (new, clone) operators */` |
|  3438196 | 1258 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2906690 | 1259 | `		 if( apNode[iCur] == 0 ){` |
|  1602548 | 1260 | `			 continue;` |
|        - | 1261 | `		 }` |
|  1304144 | 1262 | `		 pNode = apNode[iCur];` |
|  1304144 | 1263 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1264 | `			 SyToken *pToken;` |
|        - | 1265 | `			 /* Get the left node */` |
|    13214 | 1266 | `			 iLeft = iCur + 1;` |
|    26396 | 1267 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|    13184 | 1268 | `				 iLeft++;` |
|        2 | 1269 | `			 }` |
|    13214 | 1270 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1271 | `				  /* Syntax error */` |
|      ! 0 | 1272 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1273 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1274 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1275 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1276 | `				 }` |
|      ! 0 | 1277 | `				 return rc;` |
|        - | 1278 | `			 }` |
|        - | 1279 | `			 /* Make sure the operand are of a valid type */` |
|    13214 | 1280 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1281 | `				 /* Clone:` |
|        - | 1282 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1283 | `				  *  ++ function call (including annonymous)` |
|        - | 1284 | `				  *  ++ array member` |
|        - | 1285 | `				  *  ++ 'new' operator` |
|        - | 1286 | `				  * Example:` |
|        - | 1287 | `				  *   clone $pObj;` |
|        - | 1288 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1289 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1290 | `				  */` |
|       18 | 1291 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       16 | 1292 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1293 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1294 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1295 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1296 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1297 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1298 | `						 }` |
|      ! 0 | 1299 | `						 return rc;` |
|        - | 1300 | `					 }` |
|        7 | 1301 | `				 }` |
|       10 | 1302 | `			 }else{` |
|        - | 1303 | `				 /* New */` |
|    13198 | 1304 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       18 | 1305 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       18 | 1306 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){` |
|      ! 0 | 1307 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1308 | `						 /* Syntax error */` |
|      ! 0 | 1309 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1310 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1311 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1312 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1313 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1314 | `						 }` |
|      ! 0 | 1315 | `						 return rc;` |
|        - | 1316 | `					 }` |
|        8 | 1317 | `				 }` |
|        - | 1318 | `			 }` |
|        - | 1319 | `			  /* Link the node to the tree */` |
|    13214 | 1320 | `			 pNode->pLeft = apNode[iLeft];` |
|    13214 | 1321 | `			 apNode[iLeft] = 0;` |
|    13214 | 1322 | `			 pNode->pRight = 0; /* Paranoid */` |
|     6606 | 1323 | `		 }` |
|   652073 | 1324 | `	 }` |
|        - | 1325 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   531508 | 1326 | `	 iLeft = -1;` |
|  3440970 | 1327 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2906690 | 1328 | `		 if( apNode[iCur] == 0 ){` |
|  1602548 | 1329 | `			 continue;` |
|        - | 1330 | `		 }` |
|  1304144 | 1331 | `		 pNode = apNode[iCur];` |
|  1304144 | 1332 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7718 | 1333 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     2792 | 1334 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1335 | `					 /* Link the node to the tree */` |
|     2794 | 1336 | `					 pNode->pLeft = apNode[iLeft];` |
|     2794 | 1337 | `					 apNode[iLeft] = 0;` |
|     1396 | 1338 | `			 }` |
|     5245 | 1339 | `		  }` |
|  1306918 | 1340 | `		 iLeft = iCur;` |
|   654847 | 1341 | `	  }` |
|   534282 | 1342 | `	 iLeft = -1;` |
|  3440970 | 1343 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2906690 | 1344 | `		 if( apNode[iCur] == 0 ){` |
|  1605340 | 1345 | `			 continue;` |
|        - | 1346 | `		 }` |
|  1301352 | 1347 | `		 pNode = apNode[iCur];` |
|  1301352 | 1348 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     7698 | 1349 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     7700 | 1350 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1351 | `					 /* Syntax error */` |
|      ! 0 | 1352 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1353 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1354 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1355 | `					 }` |
|      ! 0 | 1356 | `					 return rc;` |
|        - | 1357 | `			 }` |
|        - | 1358 | `			 /* Link the node to the tree */` |
|     7700 | 1359 | `			 pNode->pLeft = apNode[iLeft];` |
|     7700 | 1360 | `			 apNode[iLeft] = 0;` |
|        - | 1361 | `			 /* Mark as pre-increment/decrement node */` |
|     7700 | 1362 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     3849 | 1363 | `		  }` |
|  1301352 | 1364 | `		 iLeft = iCur;` |
|   650677 | 1365 | `	 }` |
|        - | 1366 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   534282 | 1367 | `	  iLeft = 0;` |
|  3440964 | 1368 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  2906686 | 1369 | `		  if( apNode[iCur] ){` |
|  1293650 | 1370 | `			  pNode = apNode[iCur];` |
|  1293650 | 1371 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    34514 | 1372 | `				  if( iLeft > 0 ){` |
|        - | 1373 | `					  /* Link the node to the tree */` |
|    34512 | 1374 | `					  pNode->pLeft = apNode[iLeft];` |
|    34512 | 1375 | `					  apNode[iLeft] = 0;` |
|    34512 | 1376 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|        9 | 1377 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1378 | `							   /* Syntax error */` |
|      ! 0 | 1379 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1380 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1381 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1382 | `							  }` |
|      ! 0 | 1383 | `							  return rc;` |
|        - | 1384 | `						  }` |
|        4 | 1385 | `					  }` |
|    17257 | 1386 | `				  }else{` |
|        - | 1387 | `					  /* Syntax error */` |
|        3 | 1388 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1389 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1390 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1391 | `					  }` |
|        3 | 1392 | `					  return rc;` |
|        - | 1393 | `				  }` |
|    17255 | 1394 | `			  }` |
|        - | 1395 | `			  /* Save terminal position */` |
|  1293648 | 1396 | `			  iLeft = iCur;` |
|   646823 | 1397 | `		  }` |
|  1453343 | 1398 | `	  }` |
|        - | 1399 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  5876984 | 1400 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  5342714 | 1401 | `		 iLeft = -1;` |
| 34409288 | 1402 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 29066584 | 1403 | `			 if( apNode[iCur] == 0 ){` |
| 18547820 | 1404 | `				 continue;` |
|        - | 1405 | `			 }` |
| 10518766 | 1406 | `			 pNode = apNode[iCur];` |
| 10518766 | 1407 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1408 | `				 /* Get the right node */` |
|   159024 | 1409 | `				 iRight = iCur + 1;` |
|   225902 | 1410 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    66880 | 1411 | `					 iRight++;` |
|        2 | 1412 | `				 }` |
|   159024 | 1413 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1414 | `					 /* Syntax error */` |
|        9 | 1415 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        9 | 1416 | `					 if( rc != SXERR_ABORT ){` |
|        9 | 1417 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1418 | `					 }` |
|        9 | 1419 | `					 return rc;` |
|        - | 1420 | `				 }` |
|   159016 | 1421 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1422 | `					 sxi32  iTmp;` |
|        - | 1423 | `					 /* Reference operator [i.e: '&=' ]*/` |
|       46 | 1424 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1425 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1426 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1427 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1428 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1429 | `						 }` |
|      ! 0 | 1430 | `						 return rc;` |
|        - | 1431 | `					 }` |
|       46 | 1432 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       32 | 1433 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1434 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1435 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1436 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1437 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1438 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1439 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1440 | `									 }` |
|      ! 0 | 1441 | `									 return rc;` |
|        - | 1442 | `							 }` |
|      ! 0 | 1443 | `						 }` |
|       15 | 1444 | `					 }` |
|        - | 1445 | `					 /* Swap operands */` |
|       46 | 1446 | `					 iTmp = iRight;` |
|       46 | 1447 | `					 iRight = iLeft;` |
|       46 | 1448 | `					 iLeft = iTmp;` |
|       22 | 1449 | `				 }` |
|        - | 1450 | `				 /* Link the node to the tree */` |
|   159016 | 1451 | `				 pNode->pLeft = apNode[iLeft];` |
|   159016 | 1452 | `				 pNode->pRight = apNode[iRight];` |
|   159016 | 1453 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    79507 | 1454 | `			 }` |
| 10518758 | 1455 | `			 iLeft = iCur;` |
|  5259380 | 1456 | `		 }` |
|  2671354 | 1457 | `	 }` |
|        - | 1458 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 1459 | `	  * Note that we do not need a precedence loop here since` |
|        - | 1460 | `	  * we are dealing with a single operator.` |
|        - | 1461 | `	  */` |
|   534272 | 1462 | `	  iLeft = -1;` |
|  3432964 | 1463 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  2900556 | 1464 | `		  if( apNode[iCur] == 0 ){` |
|  1965012 | 1465 | `			  continue;` |
|        - | 1466 | `		  }` |
|   935546 | 1467 | `		  pNode = apNode[iCur];` |
|   935546 | 1468 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     1864 | 1469 | `			  sxi32 iNest = 1;` |
|     1864 | 1470 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1471 | `				  /* Missing condition */` |
|        3 | 1472 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 1473 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 1474 | `					  rc = SXERR_SYNTAX;` |
|        1 | 1475 | `				  }` |
|        3 | 1476 | `				  return rc;` |
|        - | 1477 | `			  }` |
|        - | 1478 | `			  /* Get the right node */` |
|     1862 | 1479 | `			  iRight = iCur + 1;` |
|     3946 | 1480 | `			  while( iRight < nToken  ){` |
|     3946 | 1481 | `				  if( apNode[iRight] ){` |
|     3654 | 1482 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 1483 | `						  /* Increment nesting level */` |
|      ! 0 | 1484 | `						  ++iNest;` |
|     3654 | 1485 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 1486 | `						  /* Decrement nesting level */` |
|     1862 | 1487 | `						  --iNest;` |
|     1862 | 1488 | `						  if( iNest <= 0 ){` |
|     1862 | 1489 | `							  break;` |
|        - | 1490 | `						  }` |
|      ! 0 | 1491 | `					  }` |
|      896 | 1492 | `				  }` |
|     2086 | 1493 | `				  iRight++;` |
|        2 | 1494 | `			  }` |
|     1862 | 1495 | `			  if( iRight > iCur + 1 ){` |
|        - | 1496 | `				  /* Recurse and process the then expression */` |
|     1794 | 1497 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     1794 | 1498 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1499 | `					  return rc;` |
|        - | 1500 | `				  }` |
|        - | 1501 | `				  /* Link the node to the tree */` |
|     1794 | 1502 | `				  pNode->pLeft = apNode[iCur + 1];` |
|      896 | 1503 | `			  }else{` |
|        - | 1504 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 1505 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 1506 | `			  }` |
|     1862 | 1507 | `			  apNode[iCur + 1] = 0;` |
|     1862 | 1508 | `			  if( iRight + 1 < nToken ){` |
|        - | 1509 | `				  /* Recurse and process the else expression */` |
|     1862 | 1510 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     1862 | 1511 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 1512 | `					  return rc;` |
|        - | 1513 | `				  }` |
|        - | 1514 | `				  /* Link the node to the tree */` |
|     1862 | 1515 | `				  pNode->pRight = apNode[iRight + 1];` |
|     1862 | 1516 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|      932 | 1517 | `			  }else{` |
|      ! 0 | 1518 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 1519 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1520 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1521 | `				 }` |
|      ! 0 | 1522 | `				 return rc;` |
|        - | 1523 | `			  }` |
|        - | 1524 | `			  /* Point to the condition */` |
|     1862 | 1525 | `			  pNode->pCond  = apNode[iLeft];` |
|     1862 | 1526 | `			  apNode[iLeft] = 0;` |
|     1862 | 1527 | `			  break;` |
|        - | 1528 | `		  }` |
|   933684 | 1529 | `		  iLeft = iCur;` |
|   466843 | 1530 | `	  }` |
|        - | 1531 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 1532 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 1533 | `	  * so there is no need for a precedence loop here.` |
|        - | 1534 | `	  */` |
|   534270 | 1535 | `	 iRight = -1;` |
|  3440830 | 1536 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  2906602 | 1537 | `		 if( apNode[iCur] == 0 ){` |
|  2172568 | 1538 | `			 continue;` |
|        - | 1539 | `		 }` |
|   734036 | 1540 | `		 pNode = apNode[iCur];` |
|   734036 | 1541 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 1542 | `			 /* Get the left node */` |
|   199644 | 1543 | `			 iLeft = iCur - 1;` |
|   282462 | 1544 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    82820 | 1545 | `				 iLeft--;` |
|        2 | 1546 | `			 }` |
|   199644 | 1547 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1548 | `				 /* Syntax error */` |
|       39 | 1549 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       39 | 1550 | `				 if( rc != SXERR_ABORT ){` |
|       37 | 1551 | `					 rc = SXERR_SYNTAX;` |
|       18 | 1552 | `				 }` |
|       39 | 1553 | `				 return rc;` |
|        - | 1554 | `			 }` |
|   199606 | 1555 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       28 | 1556 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\| apNode[iLeft]->xCode != PH7_CompileList ){` |
|        - | 1557 | `					 /* Left operand must be a modifiable l-value */` |
|        4 | 1558 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 1559 | `						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        3 | 1560 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1561 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1562 | `					 }` |
|        3 | 1563 | `					 return rc;` |
|        - | 1564 | `				 }` |
|       12 | 1565 | `			 }` |
|        - | 1566 | `			 /* Link the node to the tree (Reverse) */` |
|   199604 | 1567 | `			 pNode->pLeft = apNode[iRight];` |
|   199604 | 1568 | `			 pNode->pRight = apNode[iLeft];` |
|   199604 | 1569 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    99801 | 1570 | `		 }` |
|   733996 | 1571 | `		 iRight = iCur;` |
|   366999 | 1572 | `	 }` |
|        - | 1573 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  2671142 | 1574 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  2136914 | 1575 | `		 iLeft = -1;` |
| 13763146 | 1576 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 11626234 | 1577 | `			 if( apNode[iCur] == 0 ){` |
|  9488916 | 1578 | `				 continue;` |
|        - | 1579 | `			 }` |
|  2137320 | 1580 | `			 pNode = apNode[iCur];` |
|  2137320 | 1581 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1582 | `				 /* Get the right node */` |
|       72 | 1583 | `				 iRight = iCur + 1;` |
|      110 | 1584 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 1585 | `					 iRight++;` |
|        2 | 1586 | `				 }` |
|       72 | 1587 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1588 | `					 /* Syntax error */` |
|      ! 0 | 1589 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1590 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1591 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1592 | `					 }` |
|      ! 0 | 1593 | `					 return rc;` |
|        - | 1594 | `				 }` |
|        - | 1595 | `				 /* Link the node to the tree */` |
|       72 | 1596 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 1597 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 1598 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 1599 | `			 }` |
|  2137320 | 1600 | `			 iLeft = iCur;` |
|  1068661 | 1601 | `		 }` |
|  1068458 | 1602 | `	 }` |
|        - | 1603 | `	 /* Point to the root of the expression tree */` |
|  2906532 | 1604 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  2372322 | 1605 | `		 if( apNode[iCur] ){` |
|   482228 | 1606 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       20 | 1607 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       20 | 1608 | `				  if( rc != SXERR_ABORT ){` |
|       20 | 1609 | `					  rc = SXERR_SYNTAX;` |
|        9 | 1610 | `				  }` |
|       20 | 1611 | `				  return rc;` |
|        - | 1612 | `			 }` |
|   482210 | 1613 | `			 apNode[0] = apNode[iCur];` |
|   482210 | 1614 | `			 apNode[iCur] = 0;` |
|   241104 | 1615 | `		 }` |
|  1186153 | 1616 | `	 }` |
|   534212 | 1617 | `	 return SXRET_OK;` |
|   497482 | 1618 | ` }` |
|        - | 1619 | ` /*` |
|        - | 1620 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 1621 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 1622 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1623 | `  * This is the public interface used by the most code generator routines.` |
|        - | 1624 | `  */` |
|   623202 | 1625 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        2 | 1626 |  |
|        - | 1627 | `	ph7_expr_node **apNode;` |
|        - | 1628 | `	ph7_expr_node *pNode;` |
|        - | 1629 | `	sxi32 rc;` |
|        - | 1630 | `	/* Reset node container */` |
|   623204 | 1631 | `	SySetReset(pExprNode);` |
|   623204 | 1632 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 1633 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 1634 | `	{` |
|   623204 | 1635 | `		int iLastWasTerm = 0;` |
|  3370458 | 1636 | `		while( pGen->pIn < pGen->pEnd ){` |
|  2747290 | 1637 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm);` |
|  2747290 | 1638 | `			if( rc != SXRET_OK ){` |
|       35 | 1639 | `				return rc;` |
|        - | 1640 | `			}` |
|        - | 1641 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  2747256 | 1642 | `			if( pNode->xCode ){` |
|        - | 1643 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  1430434 | 1644 | `				iLastWasTerm = 1;` |
|  2032040 | 1645 | `			}else if( pNode->pOp ){` |
|        - | 1646 | `				/* Operator node */` |
|   617800 | 1647 | `				iLastWasTerm = 0;` |
|   308901 | 1648 | `			}else{` |
|        - | 1649 | `				/* Delimiter: ')' and ']' end terms */` |
|   699026 | 1650 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 1651 | `			}` |
|        - | 1652 | `			/* Save the extracted node */` |
|  2747256 | 1653 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        2 | 1654 | `		}` |
|        - | 1655 | `	}` |
|   623170 | 1656 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 1657 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 1658 | `		*ppRoot = 0;` |
|      ! 0 | 1659 | `		return SXRET_OK;` |
|        - | 1660 | `	}` |
|   623170 | 1661 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 1662 | `	/* Make sure we are dealing with valid nodes */` |
|   623170 | 1663 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   623170 | 1664 | `	if( rc != SXRET_OK ){` |
|        - | 1665 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 1666 | `		 * cleanup the mess left behind.` |
|        - | 1667 | `		 */` |
|       47 | 1668 | `		*ppRoot = 0;` |
|       47 | 1669 | `		return rc;` |
|        - | 1670 | `	}` |
|        - | 1671 | `	/* Build the tree */` |
|   623124 | 1672 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   623124 | 1673 | `	if( rc != SXRET_OK ){` |
|        - | 1674 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       78 | 1675 | `		*ppRoot = 0;` |
|       78 | 1676 | `		return rc;` |
|        - | 1677 | `	}` |
|        - | 1678 | `	/* Point to the root of the tree */` |
|   623048 | 1679 | `	*ppRoot = apNode[0];` |
|   623048 | 1680 | `	return SXRET_OK;` |
|   311603 | 1681 |  |
|        - | 1682 |  |
