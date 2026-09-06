# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1170/1344 lines (87.05%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `/*` |
|         - |    8 | ` * This file implement a hand-coded, thread-safe, full-reentrant and highly-efficient` |
|         - |    9 | ` * expression parser for the PH7 engine.` |
|         - |   10 | ` * Besides from the one introudced by PHP (Over 60), the PH7 engine have introduced three new` |
|         - |   11 | ` * operators. These are 'eq', 'ne' and the comma operator ','.` |
|         - |   12 | ` * The eq and ne operators are borrowed from the Perl world. They are used for strict` |
|         - |   13 | ` * string comparison. The reason why they have been implemented in the PH7 engine` |
|         - |   14 | ` * and introduced as an extension to the PHP programming language is due to the confusion` |
|         - |   15 | ` * introduced by the standard PHP comparison operators ('==' or '===') especially if you` |
|         - |   16 | ` * are comparing strings with numbers.` |
|         - |   17 | ` * Take the following example:` |
|         - |   18 | ` * var_dump( 0xFF == '255' ); // bool(true) ???` |
|         - |   19 | ` * // use the type equal operator by adding a single space to one of the operand` |
|         - |   20 | ` * var_dump( '255  ' === '255' ); //bool(true) depending on the PHP version` |
|         - |   21 | ` * That is, if one of the operand looks like a number (either integer or float) then PHP` |
|         - |   22 | ` * will internally convert the two operands to numbers and then a numeric comparison is performed.` |
|         - |   23 | ` * This is what the PHP language reference manual says:` |
|         - |   24 | ` * If you compare a number with a string or the comparison involves numerical strings, then each` |
|         - |   25 | ` * string is converted to a number and the comparison performed numerically.` |
|         - |   26 | ` * Bummer, if you ask me,this is broken, badly broken. I mean,the programmer cannot dictate` |
|         - |   27 | ` * it's comparison rule, it's the underlying engine who decides in it's place and perform` |
|         - |   28 | ` * the internal conversion. In most cases,PHP developers wants simple string comparison and they` |
|         - |   29 | ` * are stuck to use the ugly and inefficient strcmp() function and it's variants instead.` |
|         - |   30 | ` * This is the big reason why we have introduced these two operators.` |
|         - |   31 | ` * The eq operator is used to compare two strings byte per byte. If you came from the C/C++ world` |
|         - |   32 | ` * think of this operator as a barebone implementation of the memcmp() C standard library function.` |
|         - |   33 | ` * Keep in mind that if you are comparing two ASCII strings then the capital letters and their lowercase` |
|         - |   34 | ` * letters are completely different and so this example will output false.` |
|         - |   35 | ` * var_dump('allo' eq 'Allo'); //bool(FALSE)` |
|         - |   36 | ` * The ne operator perform the opposite operation of the eq operator and is used to test for string` |
|         - |   37 | ` * inequality. This example will output true` |
|         - |   38 | ` * var_dump('allo' ne 'Allo'); //bool(TRUE) unequal strings` |
|         - |   39 | ` * The eq operator return a Boolean true if and only if the two strings are identical while the` |
|         - |   40 | ` * ne operator return a Boolean true if and only if the two strings are different. Otherwise` |
|         - |   41 | ` * a Boolean false is returned (equal strings).` |
|         - |   42 | ` * Note that the comparison is performed only if the two strings are of the same length.` |
|         - |   43 | ` * Otherwise the eq and ne operators return a Boolean false without performing any comparison` |
|         - |   44 | ` * and avoid us wasting CPU time for nothing.` |
|         - |   45 | ` * Again remember that we talk about a low level byte per byte comparison and nothing else.` |
|         - |   46 | ` * Also remember that zero length strings are always equal.` |
|         - |   47 | ` *` |
|         - |   48 | ` * Again, another powerful mechanism borrowed from the C/C++ world and introduced as an extension` |
|         - |   49 | ` * to the PHP programming language.` |
|         - |   50 | ` * A comma expression contains two operands of any type separated by a comma and has left-to-right` |
|         - |   51 | ` * associativity. The left operand is fully evaluated, possibly producing side effects, and its` |
|         - |   52 | ` * value, if there is one, is discarded. The right operand is then evaluated. The type and value` |
|         - |   53 | ` * of the result of a comma expression are those of its right operand, after the usual unary conversions.` |
|         - |   54 | ` * Any number of expressions separated by commas can form a single expression because the comma operator` |
|         - |   55 | ` * is associative. The use of the comma operator guarantees that the sub-expressions will be evaluated` |
|         - |   56 | ` * in left-to-right order, and the value of the last becomes the value of the entire expression.` |
|         - |   57 | ` * The following example assign the value 25 to the variable $a, multiply the value of $a with 2` |
|         - |   58 | ` * and assign the result to variable $b and finally we call a test function to output the value` |
|         - |   59 | ` * of $a and $b. Keep-in mind that all theses operations are done in a single expression using` |
|         - |   60 | ` * the comma operator to create side effect.` |
|         - |   61 | ` * $a = 25,$b = $a << 1 ,test();` |
|         - |   62 | ` * //Output the value of $a and $b` |
|         - |   63 | ` * function test(){` |
|         - |   64 | ` *	 global $a,$b;` |
|         - |   65 | ` *	 echo "\$a = $a \$b= $b\n"; // You should see: $a = 25 $b = 50` |
|         - |   66 | ` * }` |
|         - |   67 | ` *` |
|         - |   68 | ` * For a full discussions on these extensions, please refer to  offical` |
|         - |   69 | ` * documentation(http://ph7.symisc.net/features.html) or visit the offical forums` |
|         - |   70 | ` * (http://forums.symisc.net/) if you want to share your point of view.` |
|         - |   71 | ` *` |
|         - |   72 | ` * Exprressions: According to the PHP language reference manual` |
|         - |   73 | ` *` |
|         - |   74 | ` * Expressions are the most important building stones of PHP. In PHP, almost anything you write is an expression.` |
|         - |   75 | ` * The simplest yet most accurate way to define an expression is "anything that has a value".` |
|         - |   76 | ` * The most basic forms of expressions are constants and variables. When you type "$a = 5", you're assigning` |
|         - |   77 | ` * '5' into $a. '5', obviously, has the value 5, or in other words '5' is an expression with the value of 5` |
|         - |   78 | ` * (in this case, '5' is an integer constant).` |
|         - |   79 | ` * After this assignment, you'd expect $a's value to be 5 as well, so if you wrote $b = $a, you'd expect` |
|         - |   80 | ` * it to behave just as if you wrote $b = 5. In other words, $a is an expression with the value of 5 as well.` |
|         - |   81 | ` * If everything works right, this is exactly what will happen.` |
|         - |   82 | ` * Slightly more complex examples for expressions are functions. For instance, consider the following function:` |
|         - |   83 | ` * <?php` |
|         - |   84 | ` * function foo ()` |
|         - |   85 | ` * {` |
|         - |   86 | ` *   return 5;` |
|         - |   87 | ` * }` |
|         - |   88 | ` * ?>` |
|         - |   89 | ` * Assuming you're familiar with the concept of functions (if you're not, take a look at the chapter about functions)` |
|         - |   90 | ` * you'd assume that typing $c = foo() is essentially just like writing $c = 5, and you're right.` |
|         - |   91 | ` * Functions are expressions with the value of their return value. Since foo() returns 5, the value of the expression` |
|         - |   92 | ` * 'foo()' is 5. Usually functions don't just return a static value but compute something.` |
|         - |   93 | ` * Of course, values in PHP don't have to be integers, and very often they aren't.` |
|         - |   94 | ` * PHP supports four scalar value types: integer values, floating point values (float), string values and boolean values` |
|         - |   95 | ` * (scalar values are values that you can't 'break' into smaller pieces, unlike arrays, for instance).` |
|         - |   96 | ` * PHP also supports two composite (non-scalar) types: arrays and objects. Each of these value types can be assigned` |
|         - |   97 | ` * into variables or returned from functions.` |
|         - |   98 | ` * PHP takes expressions much further, in the same way many other languages do. PHP is an expression-oriented language` |
|         - |   99 | ` * in the sense that almost everything is an expression. Consider the example we've already dealt with, '$a = 5'.` |
|         - |  100 | ` * It's easy to see that there are two values involved here, the value of the integer constant '5', and the value` |
|         - |  101 | ` * of $a which is being updated to 5 as well. But the truth is that there's one additional value involved here` |
|         - |  102 | ` * and that's the value of the assignment itself. The assignment itself evaluates to the assigned value, in this case 5.` |
|         - |  103 | ` * In practice, it means that '$a = 5', regardless of what it does, is an expression with the value 5. Thus, writing` |
|         - |  104 | ` * something like '$b = ($a = 5)' is like writing '$a = 5; $b = 5;' (a semicolon marks the end of a statement).` |
|         - |  105 | ` * Since assignments are parsed in a right to left order, you can also write '$b = $a = 5'.` |
|         - |  106 | ` * Another good example of expression orientation is pre- and post-increment and decrement.` |
|         - |  107 | ` * Users of PHP and many other languages may be familiar with the notation of variable++ and variable--.` |
|         - |  108 | ` * These are increment and decrement operators. In PHP, like in C, there are two types of increment - pre-increment` |
|         - |  109 | ` * and post-increment. Both pre-increment and post-increment essentially increment the variable, and the effect` |
|         - |  110 | ` * on the variable is identical. The difference is with the value of the increment expression. Pre-increment, which is written` |
|         - |  111 | ` * '++$variable', evaluates to the incremented value (PHP increments the variable before reading its value, thus the name 'pre-increment').` |
|         - |  112 | ` * Post-increment, which is written '$variable++' evaluates to the original value of $variable, before it was incremented` |
|         - |  113 | ` * (PHP increments the variable after reading its value, thus the name 'post-increment').` |
|         - |  114 | ` * A very common type of expressions are comparison expressions. These expressions evaluate to either FALSE or TRUE.` |
|         - |  115 | ` * PHP supports > (bigger than), >= (bigger than or equal to), == (equal), != (not equal), < (smaller than) and <= (smaller than or equal to).` |
|         - |  116 | ` * The language also supports a set of strict equivalence operators: === (equal to and same type) and !== (not equal to or not same type).` |
|         - |  117 | ` * These expressions are most commonly used inside conditional execution, such as if statements.` |
|         - |  118 | ` * The last example of expressions we'll deal with here is combined operator-assignment expressions.` |
|         - |  119 | ` * You already know that if you want to increment $a by 1, you can simply write '$a++' or '++$a'.` |
|         - |  120 | ` * But what if you want to add more than one to it, for instance 3? You could write '$a++' multiple times, but this is obviously not a very` |
|         - |  121 | ` * efficient or comfortable way. A much more common practice is to write '$a = $a + 3'. '$a + 3' evaluates to the value of $a plus 3` |
|         - |  122 | ` * and is assigned back into $a, which results in incrementing $a by 3. In PHP, as in several other languages like C, you can write` |
|         - |  123 | ` * this in a shorter way, which with time would become clearer and quicker to understand as well. Adding 3 to the current value of $a` |
|         - |  124 | ` * can be written '$a += 3'. This means exactly "take the value of $a, add 3 to it, and assign it back into $a".` |
|         - |  125 | ` * In addition to being shorter and clearer, this also results in faster execution. The value of '$a += 3', like the value of a regular` |
|         - |  126 | ` * assignment, is the assigned value. Notice that it is NOT 3, but the combined value of $a plus 3 (this is the value that's assigned into $a).` |
|         - |  127 | ` * Any two-place operator can be used in this operator-assignment mode, for example '$a -= 5' (subtract 5 from the value of $a), '$b *= 7'` |
|         - |  128 | ` * (multiply the value of $b by 7), etc.` |
|         - |  129 | ` * There is one more expression that may seem odd if you haven't seen it in other languages, the ternary conditional operator:` |
|         - |  130 | ` * <?php` |
|         - |  131 | ` * $first ? $second : $third` |
|         - |  132 | ` * ?>` |
|         - |  133 | ` * If the value of the first subexpression is TRUE (non-zero), then the second subexpression is evaluated, and that is the result` |
|         - |  134 | ` * of the conditional expression. Otherwise, the third subexpression is evaluated, and that is the value.` |
|         - |  135 | ` */` |
|         - |  136 | `/* Operators associativity */` |
|         - |  137 | `#define EXPR_OP_ASSOC_LEFT   0x01 /* Left associative operator */` |
|         - |  138 | `#define EXPR_OP_ASSOC_RIGHT  0x02 /* Right associative operator */` |
|         - |  139 | `#define EXPR_OP_NON_ASSOC    0x04 /* Non-associative operator */` |
|         - |  140 | `/*` |
|         - |  141 | ` * Operators table` |
|         - |  142 | ` * This table is sorted by operators priority (highest to lowest) according` |
|         - |  143 | ` * the PHP language reference manual.` |
|         - |  144 | ` * PH7 implements all the 60 PHP operators and have introduced the eq and ne operators.` |
|         - |  145 | ` * The operators precedence table have been improved dramatically so that you can do same` |
|         - |  146 | ` * amazing things now such as array dereferencing,on the fly function call,anonymous function` |
|         - |  147 | ` * as array values,class member access on instantiation and so on.` |
|         - |  148 | ` * Refer to the following page for a full discussion on these improvements:` |
|         - |  149 | ` * http://ph7.symisc.net/features.html#improved_precedence` |
|         - |  150 | ` */` |
|         - |  151 | `static const ph7_expr_op aOpTable[] = {` |
|         - |  152 | `	/* Precedence 1: non-associative */` |
|         - |  153 | `	{ {"new",sizeof("new")-1},     EXPR_OP_NEW,   1, EXPR_OP_NON_ASSOC, PH7_OP_NEW  },` |
|         - |  154 | `	{ {"clone",sizeof("clone")-1}, EXPR_OP_CLONE, 1, EXPR_OP_NON_ASSOC, PH7_OP_CLONE},` |
|         - |  155 | `	                              /* Postfix operators */` |
|         - |  156 | `	/* Precedence 2(Highest),left-associative */` |
|         - |  157 | `	{ {"->",sizeof(char)*2}, EXPR_OP_ARROW,     2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|         - |  158 | `	{ {"?->",sizeof(char)*3},EXPR_OP_NULLSAFE_ARROW, 2, EXPR_OP_ASSOC_LEFT, PH7_OP_MEMBER},` |
|         - |  159 | `	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|         - |  160 | `	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},` |
|         - |  161 | `	/* Precedence 3,non-associative  */` |
|         - |  162 | `	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},` |
|         - |  163 | `	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},` |
|         - |  164 | `	                              /* Unary operators */` |
|         - |  165 | `	/* Precedence 4,right-associative  */` |
|         - |  166 | `	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },` |
|         - |  167 | `	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },` |
|         - |  168 | `	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },` |
|         - |  169 | `	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },` |
|         - |  170 | `	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},` |
|         - |  171 | `	                             /* Cast operators */` |
|         - |  172 | `	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },` |
|         - |  173 | `	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },` |
|         - |  174 | `	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },` |
|         - |  175 | `	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|         - |  176 | `	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},` |
|         - |  177 | `	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },` |
|         - |  178 | `	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },` |
|         - |  179 | `	                           /* Binary operators */` |
|         - |  180 | `	/* Precedence 5,right-associative: exponentiation (PHP 5.6) */` |
|         - |  181 | `	{ {"**",sizeof(char)*2}, EXPR_OP_POW, 5, EXPR_OP_ASSOC_RIGHT, PH7_OP_POW},` |
|         - |  182 | `	/* Precedence 7,left-associative */` |
|         - |  183 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|         - |  184 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|         - |  185 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|         - |  186 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|         - |  187 | `	/* Precedence 8,left-associative */` |
|         - |  188 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|         - |  189 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|         - |  190 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|         - |  191 | `	/* Precedence 9,left-associative */` |
|         - |  192 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|         - |  193 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|         - |  194 | ``	/* PHP 8.5 pipe operator: `$x \|> f(...)` desugars to `f($x)`. It binds`` |
|         - |  195 | `	 * looser than shift/arithmetic and tighter than comparison — PHP places it` |
|         - |  196 | `	 * between precedence 9 and 10. We share level 9 (left-associative) so the` |
|         - |  197 | `	 * generic binary tree-builder links it correctly; the actual codegen is` |
|         - |  198 | `	 * custom (a one-argument call of the RHS callable), handled in` |
|         - |  199 | `	 * GenStateEmitExprCode. iVmOp is 0 like the other codegen-only operators. */` |
|         - |  200 | `	{ {"\|>",sizeof(char)*2}, EXPR_OP_PIPE, 9, EXPR_OP_ASSOC_LEFT, 0},` |
|         - |  201 | `	/* Precedence 10,non-associative */` |
|         - |  202 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|         - |  203 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|         - |  204 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|         - |  205 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|         - |  206 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|         - |  207 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|         - |  208 | `	/* Precedence 11,non-associative */` |
|         - |  209 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|         - |  210 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|         - |  211 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|         - |  212 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|         - |  213 | `	/* Precedence 12,left-associative */` |
|         - |  214 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|         - |  215 | `	/* Precedence 12,left-associative */` |
|         - |  216 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|         - |  217 | `	                         /* Binary operators */` |
|         - |  218 | `	/* Precedence 13,left-associative */` |
|         - |  219 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|         - |  220 | `	/* Precedence 14,left-associative */` |
|         - |  221 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|         - |  222 | `	/* Precedence 15,left-associative */` |
|         - |  223 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|         - |  224 | `	/* Precedence 16,left-associative */` |
|         - |  225 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|         - |  226 | `	                      /* Null coalescing operator */` |
|         - |  227 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|         - |  228 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|         - |  229 | `	                      /* Ternary operator */` |
|         - |  230 | `	/* Precedence 17,left-associative */` |
|         - |  231 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|         - |  232 | `	                     /* Combined binary operators */` |
|         - |  233 | `	/* Precedence 18,right-associative */` |
|         - |  234 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|         - |  235 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|         - |  236 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|         - |  237 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|         - |  238 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|         - |  239 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|         - |  240 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|         - |  241 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|         - |  242 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|         - |  243 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|         - |  244 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|         - |  245 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|         - |  246 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|         - |  247 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|         - |  248 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|         - |  249 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|         - |  250 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|         - |  251 | `	 * in this file: keep one of the question marks escaped. */` |
|         - |  252 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|         - |  253 | `	/* Precedence 19,left-associative */` |
|         - |  254 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|         - |  255 | `	/* Precedence 20,left-associative */` |
|         - |  256 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|         - |  257 | `	/* Precedence 21,left-associative */` |
|         - |  258 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|         - |  259 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|         - |  260 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|         - |  261 | `};` |
|         - |  262 | `/* Function call operator need special handling */` |
|         - |  263 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|         - |  264 | `/*` |
|         - |  265 | ` * Check if the given token is a potential operator or not.` |
|         - |  266 | ` * This function is called by the lexer each time it extract a token that may` |
|         - |  267 | ` * look like an operator.` |
|         - |  268 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|         - |  269 | ` * Otherwise NULL.` |
|         - |  270 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|         - |  271 | ` * a binary minus or unary minus.]` |
|         - |  272 | ` */` |
|  21887394 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  21887399 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 327271621 |  278 | `	for(;;){` |
| 654543247 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 654543247 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  63820839 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  31910422 |  285 | `		}else{` |
| 590722413 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 654543247 |  288 | `		if( rc == 0 ){` |
|  22134061 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  21809991 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|    324075 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|     23521 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|    300559 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|     53905 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     53905 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|     53897 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|    123331 |  307 | `		}` |
| 632655853 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|  10943702 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   6143256 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   6143261 |  320 | `	SyToken *pCur = pIn;` |
|   6143261 |  321 | `	sxi32 iNest = 1;` |
|  68873719 |  322 | `	for(;;){` |
| 137747443 |  323 | `		if( pCur >= pEnd ){` |
|     15895 |  324 | `			break;` |
|         - |  325 | `		}` |
| 137731553 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   5357489 |  328 | `			iNest++;` |
| 135052811 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|  11484855 |  331 | `			iNest--;` |
|  11484855 |  332 | `			if( iNest <= 0 ){` |
|   6127371 |  333 | `				break;` |
|         - |  334 | `			}` |
|   2678742 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
| 131604187 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   6143261 |  340 | `	*ppEnd = pCur;` |
|   6143261 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    419850 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    419850 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    419752 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       167 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    419693 |  358 | `	if( bCheckFunc ){` |
|     38820 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|     38813 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|     38794 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        51 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|     19387 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    419647 |  366 | `	return FALSE;` |
|    209930 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|  12703652 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|  12703657 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|  12703657 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  81241477 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  68537859 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|    217019 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  68320845 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   5775929 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    266198 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   5451239 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   5451239 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   5451239 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   5451239 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   2725617 |  401 | `					}` |
|   2725617 |  402 | `			}` |
|   5775929 |  403 | `			iParen++;` |
|  65432883 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   5775929 |  405 | `			if( iParen <= 0 ){` |
|        16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        16 |  407 | `				if( rc != SXERR_ABORT ){` |
|        16 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        16 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   5775917 |  412 | `			iParen--;` |
|  59656953 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   2525989 |  414 | `			iSquare++;` |
|  55506005 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   2526003 |  416 | `			if( iSquare <= 0 ){` |
|         8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         8 |  418 | `				if( rc != SXERR_ABORT ){` |
|         8 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         8 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   2525997 |  423 | `			iSquare--;` |
|  52980011 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|      3863 |  425 | `			iBraces++;` |
|      3863 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  427 | `				const ph7_expr_op *pOp,*pEnd;` |
|        11 |  428 | `				int iNest = 1;` |
|        11 |  429 | `				sxi32 j=i+1;` |
|         - |  430 | `				/*` |
|         - |  431 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|         - |  432 | `				 */` |
|        11 |  433 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|        11 |  434 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|        11 |  435 | `				pOp = aOpTable;` |
|        11 |  436 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|        61 |  437 | `				while( pOp < pEnd ){` |
|        61 |  438 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        11 |  439 | `						break;` |
|         - |  440 | `					}` |
|        51 |  441 | `					pOp++;` |
|         1 |  442 | `				}` |
|        11 |  443 | `				if( pOp >= pEnd ){` |
|       ! 0 |  444 | `					pOp = 0;` |
|       ! 0 |  445 | `				}` |
|        11 |  446 | `				if( pOp ){` |
|        11 |  447 | `					apNode[i]->pOp = pOp;` |
|        11 |  448 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|         5 |  449 | `				}` |
|        11 |  450 | `				iBraces--;` |
|        11 |  451 | `				iSquare++;` |
|        21 |  452 | `				while( j < nNode ){` |
|        21 |  453 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|         - |  454 | `						/* Increment nesting level */` |
|       ! 0 |  455 | `						iNest++;` |
|        21 |  456 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|         - |  457 | `						/* Decrement nesting level */` |
|        11 |  458 | `						iNest--;` |
|        11 |  459 | `						if( iNest < 1 ){` |
|        11 |  460 | `							break;` |
|         - |  461 | `						}` |
|       ! 0 |  462 | `					}` |
|        11 |  463 | `					j++;` |
|         1 |  464 | `				}` |
|        11 |  465 | `				if( j < nNode ){` |
|        11 |  466 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|        11 |  467 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|         5 |  468 | `				}` |
|         - |  469 |  |
|        10 |  470 | `			}` |
|  51715086 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|      3865 |  472 | `			if( iBraces <= 0 ){` |
|        15 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        15 |  474 | `				if( rc != SXERR_ABORT ){` |
|        15 |  475 | `					rc = SXERR_SYNTAX;` |
|         6 |  476 | `				}` |
|        15 |  477 | `				return rc;` |
|         - |  478 | `			}` |
|      3853 |  479 | `			iBraces--;` |
|  51711221 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    372227 |  481 | `			if( iQuesty > 0 ){` |
|    371937 |  482 | `				iQuesty--;` |
|    186261 |  483 | `			}else if( iParen <= 0 ){` |
|         - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  488 | `				if( rc != SXERR_ABORT ){` |
|         6 |  489 | `					rc = SXERR_SYNTAX;` |
|         2 |  490 | `				}` |
|         6 |  491 | `				return rc;` |
|         5 |  492 | `			}` |
|  51523184 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|  16907805 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|  16907805 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    371939 |  496 | `				iQuesty++;` |
|  16721838 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     65817 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|         9 |  499 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|         9 |  500 | `					sxu32 n = 0;` |
|         9 |  501 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|         5 |  502 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|         2 |  503 | `					}` |
|         - |  504 | `					/*` |
|         - |  505 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|         - |  506 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|         - |  507 | `					 */` |
|       213 |  508 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|       205 |  509 | `						++n;` |
|         1 |  510 | `					}` |
|         9 |  511 | `					pOp = &aOpTable[n];` |
|         - |  512 | `					/* Mark as binary '+' or '-',not an unary */` |
|         9 |  513 | `					apNode[i]->pOp = pOp;` |
|         9 |  514 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|         4 |  515 | `				}` |
|     32906 |  516 | `			}` |
|   8453900 |  517 | `		}` |
|  34160408 |  518 | `	}` |
|  12703623 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        19 |  520 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        19 |  521 | `		if( rc != SXERR_ABORT ){` |
|        19 |  522 | `			rc = SXERR_SYNTAX;` |
|         8 |  523 | `		}` |
|        19 |  524 | `		return rc;` |
|         - |  525 | `	}` |
|  12703607 |  526 | `	return SXRET_OK;` |
|   6351831 |  527 | `}` |
|         - |  528 | `/*` |
|         - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  531 | ` */` |
|  10901730 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  533 | `{` |
|  10901735 |  534 | `	SyToken *pIn = *ppCur;` |
|         - |  535 | `	/* Jump the first literal seen */` |
|  10901735 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|  10897847 |  537 | `		pIn++;` |
|   5448921 |  538 | `	}` |
|   5452836 |  539 | `	for(;;){` |
|  10905677 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      3947 |  541 | `			pIn++;` |
|      3947 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3945 |  543 | `				pIn++;` |
|      1970 |  544 | `			}` |
|      1976 |  545 | `		}else{` |
|   5450870 |  546 | `			break;` |
|         - |  547 | `		}` |
|         5 |  548 | `	}` |
|         - |  549 | `	/* Synchronize pointers */` |
|  10901735 |  550 | `	*ppCur = pIn;` |
|  10901735 |  551 | `}` |
|         - |  552 | `/*` |
|         - |  553 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|         - |  554 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  555 | ` * Note on annonymous functions.` |
|         - |  556 | ` *  According to the PHP language reference manual:` |
|         - |  557 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  558 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  559 | ` *  parameters, but they have many other uses.` |
|         - |  560 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|         - |  561 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|         - |  562 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|         - |  563 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|         - |  564 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|         - |  565 | ` *` |
|         - |  566 | ` * Some example:` |
|         - |  567 | ` *  $greet = function($name)` |
|         - |  568 | ` * {` |
|         - |  569 | ` *   printf("Hello %s\r\n", $name);` |
|         - |  570 | ` * };` |
|         - |  571 | ` *  $greet('World');` |
|         - |  572 | ` *  $greet('PHP');` |
|         - |  573 | ` *` |
|         - |  574 | ` * $double = function($a) {` |
|         - |  575 | ` *   return $a * 2;` |
|         - |  576 | ` * };` |
|         - |  577 | ` * // This is our range of numbers` |
|         - |  578 | ` * $numbers = range(1, 5);` |
|         - |  579 | ` * // Use the Annonymous function as a callback here to` |
|         - |  580 | ` * // double the size of each element in our` |
|         - |  581 | ` * // range` |
|         - |  582 | ` * $new_numbers = array_map($double, $numbers);` |
|         - |  583 | ` * print implode(' ', $new_numbers);` |
|         - |  584 | ` */` |
|         - |  585 | `/*` |
|         - |  586 | ` * Skip an optional return-type declaration at *ppIn:` |
|         - |  587 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|         - |  588 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|         - |  589 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|         - |  590 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|         - |  591 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|         - |  592 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|         - |  593 | ` * authoritative type parse, so this must accept every shape it does` |
|         - |  594 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|         - |  595 | ` */` |
|       854 |  596 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  597 | `{` |
|       859 |  598 | `	SyToken *pIn = *ppIn;` |
|       859 |  599 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        25 |  600 | `		pIn++; /* Skip ':' */` |
|        11 |  601 | `		for(;;){` |
|         - |  602 | `			/* Optional '?' nullable prefix */` |
|        29 |  603 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|         6 |  604 | `				pIn++;` |
|         2 |  605 | `			}` |
|        29 |  606 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  607 | `				/* Parenthesized DNF group '(A&B)' */` |
|       ! 0 |  608 | `				pIn++;` |
|       ! 0 |  609 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       ! 0 |  610 | `				if( pIn < pEnd ){` |
|       ! 0 |  611 | `					pIn++; /* ')' */` |
|       ! 0 |  612 | `				}` |
|        26 |  613 | `			}else if( pIn < pEnd` |
|        29 |  614 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  615 | `				/* ['\']Name('\'Name)* */` |
|        29 |  616 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|        29 |  617 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        29 |  618 | `					pIn++;` |
|        29 |  619 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  620 | `						pIn += 2;` |
|       ! 0 |  621 | `					}` |
|        13 |  622 | `				}` |
|        16 |  623 | `			}else{` |
|         - |  624 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  625 | `				break;` |
|         - |  626 | `			}` |
|         - |  627 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|        26 |  628 | `			if( pIn < pEnd` |
|        29 |  629 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|        26 |  630 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|         5 |  631 | `				pIn++;` |
|         5 |  632 | `				continue;` |
|         - |  633 | `			}` |
|        25 |  634 | `			break;` |
|       ! 0 |  635 | `		}` |
|        11 |  636 | `	}` |
|       859 |  637 | `	*ppIn = pIn;` |
|       859 |  638 | `}` |
|       490 |  639 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  640 | `{` |
|       495 |  641 | `	SyToken *pIn = *ppCur;` |
|         - |  642 | `	sxi32 rc;` |
|         - |  643 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  644 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  645 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  646 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  647 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|       495 |  648 | `	pIn++;` |
|       490 |  649 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       255 |  650 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|         9 |  651 | `		pIn++;` |
|         4 |  652 | `	}` |
|       495 |  653 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  654 | `		/* Syntax error */` |
|         6 |  655 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  656 | `		if( rc != SXERR_ABORT ){` |
|         6 |  657 | `			rc = SXERR_SYNTAX;` |
|         2 |  658 | `		}` |
|         6 |  659 | `		goto Synchronize;` |
|         - |  660 | `	}` |
|       491 |  661 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       491 |  662 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       491 |  663 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  664 | `		/* Nothing follows the parameter list inside our slice: the body is missing.` |
|         - |  665 | `		 * php names the token that actually comes next (it lives just past the` |
|         - |  666 | `		 * expression slice, still in the raw stream) and says it wanted the '{'. */` |
|         6 |  667 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         6 |  668 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         6 |  669 | `		if( rc != SXERR_ABORT ){` |
|         6 |  670 | `			rc = SXERR_SYNTAX;` |
|         2 |  671 | `		}` |
|         6 |  672 | `		goto Synchronize;` |
|         - |  673 | `	}` |
|       487 |  674 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  675 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       487 |  676 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       487 |  677 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       107 |  678 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  679 | `		/* Check if we are dealing with a closure */` |
|       107 |  680 | `		if( nKey == PH7_TKWRD_USE ){` |
|        99 |  681 | `			pIn++; /* Jump the 'use' keyword */` |
|        99 |  682 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  683 | `				/* Syntax error */` |
|         6 |  684 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  685 | `				if( rc != SXERR_ABORT ){` |
|         6 |  686 | `					rc = SXERR_SYNTAX;` |
|         2 |  687 | `				}` |
|         6 |  688 | `				goto Synchronize;` |
|         - |  689 | `			}` |
|        95 |  690 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|        95 |  691 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        95 |  692 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  693 | `				/* Syntax error */` |
|         6 |  694 | `				rc = PH7_GenSyntaxError(&(*pGen),0 /* ran off the end */,0);` |
|         6 |  695 | `				if( rc != SXERR_ABORT ){` |
|         6 |  696 | `					rc = SXERR_SYNTAX;` |
|         2 |  697 | `				}` |
|         6 |  698 | `				goto Synchronize;` |
|         - |  699 | `			}` |
|        91 |  700 | `			pIn++;` |
|         - |  701 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  702 | ``			 * `function (...) use (...) : int {` */`` |
|        91 |  703 | `			ExprSkipReturnType(&pIn,pEnd);` |
|        48 |  704 | `		}else{` |
|         - |  705 | `			/* Syntax error */` |
|        11 |  706 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        11 |  707 | `			if( rc != SXERR_ABORT ){` |
|        11 |  708 | `				rc = SXERR_SYNTAX;` |
|         4 |  709 | `			}` |
|        11 |  710 | `			goto Synchronize;` |
|         - |  711 | `		}` |
|        43 |  712 | `	}` |
|         - |  713 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  714 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  715 | `	 * the type), and pEnd is one past the last token. */` |
|       471 |  716 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       471 |  717 | `		pIn++; /* Jump the leading curly '{' */` |
|       471 |  718 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       471 |  719 | `		if( pIn < pEnd ){` |
|       471 |  720 | `			pIn++;` |
|       233 |  721 | `		}` |
|       238 |  722 | `	}else{` |
|         - |  723 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|         - |  724 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|         - |  725 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|         - |  726 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|       ! 0 |  727 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|       ! 0 |  728 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|       ! 0 |  729 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  730 | `			return SXERR_ABORT;` |
|         - |  731 | `		}` |
|         - |  732 | `	}` |
|       471 |  733 | `	rc = SXRET_OK;` |
|       245 |  734 | `Synchronize:` |
|         - |  735 | `	/* Synchronize pointers */` |
|       495 |  736 | `	*ppCur = pIn;` |
|       495 |  737 | `	return rc;` |
|       250 |  738 | `}` |
|         - |  739 | `/*` |
|         - |  740 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  741 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  742 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  743 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  744 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  745 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  746 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  747 | ` */` |
|        28 |  748 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         4 |  749 | `{` |
|        32 |  750 | `	SyToken *pIn = *ppCur;` |
|        32 |  751 | `	sxu32 nLine = pIn->nLine;` |
|         - |  752 | `	sxi32 rc;` |
|        32 |  753 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  754 | `	/* Optional constructor argument list */` |
|        32 |  755 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         7 |  756 | `		pIn++; /* Jump '(' */` |
|         7 |  757 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         7 |  758 | `		if( pIn < pEnd ){` |
|         7 |  759 | `			pIn++; /* Jump ')' */` |
|         3 |  760 | `		}` |
|         3 |  761 | `	}` |
|         - |  762 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  763 | `	 * (no braces appear between ')' and the class body). */` |
|        60 |  764 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|        32 |  765 | `		pIn++;` |
|         4 |  766 | `	}` |
|        32 |  767 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  768 | `		/* Syntax error: missing class body */` |
|       ! 0 |  769 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  770 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  771 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  772 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  773 | `		}` |
|       ! 0 |  774 | `		*ppCur = pIn;` |
|       ! 0 |  775 | `		return rc;` |
|         - |  776 | `	}` |
|        32 |  777 | `	pIn++; /* Jump the leading '{' */` |
|        32 |  778 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        32 |  779 | `	if( pIn < pEnd ){` |
|        32 |  780 | `		pIn++; /* Jump the trailing '}' */` |
|        14 |  781 | `	}` |
|        32 |  782 | `	*ppCur = pIn;` |
|        32 |  783 | `	return SXRET_OK;` |
|        18 |  784 | `}` |
|         - |  785 | `/*` |
|         - |  786 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  787 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  788 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  789 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  790 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  791 | ` */` |
|       286 |  792 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  793 | `{` |
|       291 |  794 | `	SyToken *pIn = *ppCur;` |
|         - |  795 | `	sxu32 nLine;` |
|         - |  796 | `	sxi32 rc;` |
|         - |  797 | `	int iNest;` |
|       291 |  798 | `	nLine = pIn->nLine;` |
|         - |  799 | `	/* Optional 'static' prefix */` |
|       286 |  800 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  801 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  802 | `		pIn++;` |
|         3 |  803 | `	}` |
|         - |  804 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       286 |  805 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  806 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  807 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  808 | `		goto Synchronize;` |
|         - |  809 | `	}` |
|       291 |  810 | `	pIn++; /* Jump 'fn' */` |
|       143 |  811 | `	SXUNUSED(nLine);` |
|       143 |  812 | `	SXUNUSED(pGen);` |
|         - |  813 | `	/* Optional '&' for return-by-reference */` |
|       291 |  814 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  815 | `		pIn++;` |
|       ! 0 |  816 | `	}` |
|         - |  817 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  818 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  819 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  820 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       291 |  821 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       289 |  822 | `		pIn++; /* '(' */` |
|       289 |  823 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       289 |  824 | `		if( pIn < pEnd ){` |
|       287 |  825 | `			pIn++; /* ')' */` |
|       141 |  826 | `		}` |
|       142 |  827 | `	}` |
|         - |  828 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       291 |  829 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  830 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       291 |  831 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       285 |  832 | `		pIn++;` |
|       140 |  833 | `	}` |
|         - |  834 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       291 |  835 | `	iNest = 0;` |
|      2007 |  836 | `	while( pIn < pEnd ){` |
|      1895 |  837 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  838 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       175 |  839 | `			break;` |
|         - |  840 | `		}` |
|      1721 |  841 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       196 |  842 | `			iNest++;` |
|      1625 |  843 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       196 |  844 | `			iNest--;` |
|        96 |  845 | `		}` |
|      1721 |  846 | `		pIn++;` |
|         5 |  847 | `	}` |
|       291 |  848 | `	rc = SXRET_OK;` |
|       143 |  849 | `Synchronize:` |
|       291 |  850 | `	*ppCur = pIn;` |
|       291 |  851 | `	return rc;` |
|         5 |  852 | `}` |
|         - |  853 | `/*` |
|         - |  854 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - |  855 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - |  856 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - |  857 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - |  858 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - |  859 | ` */` |
|        72 |  860 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  861 | `{` |
|        77 |  862 | `	SyToken *pIn = *ppCur;` |
|         - |  863 | `	sxi32 rc;` |
|        36 |  864 | `	SXUNUSED(pGen);` |
|         - |  865 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|        72 |  866 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        77 |  867 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 |  868 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  869 | `		goto Synchronize;` |
|         - |  870 | `	}` |
|        77 |  871 | `	pIn++; /* Jump 'match' */` |
|         - |  872 | `	/* Optional '(' subject ')' */` |
|        77 |  873 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        77 |  874 | `		pIn++;` |
|        77 |  875 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        77 |  876 | `		if( pIn < pEnd ){` |
|        77 |  877 | `			pIn++; /* ')' */` |
|        36 |  878 | `		}` |
|        36 |  879 | `	}` |
|         - |  880 | `	/* Optional '{' arms '}' */` |
|        77 |  881 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|        77 |  882 | `		pIn++;` |
|        77 |  883 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|        77 |  884 | `		if( pIn < pEnd ){` |
|        77 |  885 | `			pIn++; /* '}' */` |
|        36 |  886 | `		}` |
|        36 |  887 | `	}` |
|        77 |  888 | `	rc = SXRET_OK;` |
|        36 |  889 | `Synchronize:` |
|        77 |  890 | `	*ppCur = pIn;` |
|        77 |  891 | `	return rc;` |
|         5 |  892 | `}` |
|         - |  893 | `/*` |
|         - |  894 | ` * Extract a single expression node from the input.` |
|         - |  895 | ` * On success store the freshly extractd node in ppNode.` |
|         - |  896 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  897 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - |  898 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - |  899 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - |  900 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - |  901 | ` */` |
|  68542094 |  902 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_expr_node *pNode;` |
|         - |  905 | `	SyToken *pCur;` |
|         - |  906 | `	sxi32 rc;` |
|         - |  907 | `	/* Allocate a new node */` |
|  68542099 |  908 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  68542099 |  909 | `	if( pNode == 0 ){` |
|         - |  910 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  911 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  912 | `		 */` |
|       ! 0 |  913 | `		return SXERR_MEM;` |
|         - |  914 | `	}` |
|         - |  915 | `	/* Zero the structure */` |
|  68542099 |  916 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  68542099 |  917 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  918 | `	/* Point to the head of the token stream */` |
|  68542099 |  919 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  920 | `	/* Start collecting tokens */` |
|  68542099 |  921 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4175 |  922 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - |  923 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - |  924 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - |  925 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - |  926 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|        81 |  927 | `			pNode->pEnd = pCur;` |
|        81 |  928 | `			pCur++;` |
|        81 |  929 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|        81 |  930 | `			pNode->xCode = PH7_CompileFccMarker;` |
|        81 |  931 | `			pGen->pIn = pCur;` |
|        81 |  932 | `			*ppNode = pNode;` |
|        81 |  933 | `			return SXRET_OK;` |
|         - |  934 | `		}` |
|         - |  935 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - |  936 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|      4095 |  937 | `		pCur++;` |
|      4095 |  938 | `		pGen->pIn = pCur;` |
|      4095 |  939 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      4095 |  940 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      4095 |  941 | `		if( rc == SXRET_OK && *ppNode ){` |
|      4095 |  942 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      2045 |  943 | `		}` |
|      4095 |  944 | `		return rc;` |
|         - |  945 | `	}` |
|  68537929 |  946 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  947 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  948 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  949 | `		 */` |
|    217021 |  950 | `		pCur++; /* Skip the opening '[' */` |
|    217021 |  951 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|    217021 |  952 | `		if( pCur < pGen->pEnd ){` |
|    217021 |  953 | `			pCur++; /* Skip past the closing ']' */` |
|    108513 |  954 | `		}else{` |
|       ! 0 |  955 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - |  956 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 |  957 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 |  958 | `				rc = SXERR_SYNTAX;` |
|       ! 0 |  959 | `			}` |
|       ! 0 |  960 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 |  961 | `			return rc;` |
|         - |  962 | `		}` |
|         - |  963 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - |  964 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - |  965 | `		 */` |
|    217191 |  966 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       342 |  967 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       342 |  968 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        56 |  969 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  970 | `			}else{` |
|       287 |  971 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  972 | `			}` |
|       172 |  973 | `		}else{` |
|    216681 |  974 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  975 | `		}` |
|  68429421 |  976 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - |  977 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - |  978 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - |  979 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - |  980 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - |  981 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - |  982 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - |  983 | `		 * call, not the clone() intrinsic. */` |
|        17 |  984 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        17 |  985 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        17 |  986 | `		pNode->xCode = PH7_CompileLiteral;` |
|  68320901 |  987 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|  19433840 |  988 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   9743836 |  989 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|         - |  990 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - |  991 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|         - |  992 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|         - |  993 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|         - |  994 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|         - |  995 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|         - |  996 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|         - |  997 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|         - |  998 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|         - |  999 | `		 * operator (which would dereference the NULL pOp). */` |
|        24 | 1000 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        24 | 1001 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|        24 | 1002 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        24 | 1003 | `		if( pCur < pGen->pEnd ){` |
|        24 | 1004 | `			pCur++; /* skip the closing ')' */` |
|        13 | 1005 | `		}else{` |
|       ! 0 | 1006 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1007 | `				"clone: Missing closing parenthesis ')'");` |
|       ! 0 | 1008 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 1009 | `				rc = SXERR_SYNTAX;` |
|       ! 0 | 1010 | `			}` |
|       ! 0 | 1011 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1012 | `			return rc;` |
|         - | 1013 | `		}` |
|        24 | 1014 | `		pNode->xCode = PH7_CompileCloneCall;` |
|  68320886 | 1015 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1016 | `		/* Point to the instance that describe this operator */` |
|  19433823 | 1017 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1018 | `		/* Advance the stream cursor */` |
|  19433823 | 1019 | `		pCur++;` |
|  58603966 | 1020 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1021 | `		/* Isolate variable */` |
|  32168305 | 1022 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  16084161 | 1023 | `			pCur++; /* Variable variable */` |
|         5 | 1024 | `		}` |
|  16084149 | 1025 | `		if( pCur < pGen->pEnd ){` |
|  16084149 | 1026 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1027 | `				/* Variable name */` |
|  16084119 | 1028 | `				pCur++;` |
|   8042092 | 1029 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        26 | 1030 | `				pCur++;` |
|         - | 1031 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        26 | 1032 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        26 | 1033 | `				if( pCur < pGen->pEnd ){` |
|        21 | 1034 | `					pCur++;` |
|        12 | 1035 | `				}else{` |
|         5 | 1036 | `					rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         5 | 1037 | `					if( rc != SXERR_ABORT ){` |
|         5 | 1038 | `						rc = SXERR_SYNTAX;` |
|         2 | 1039 | `					}` |
|         5 | 1040 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         5 | 1041 | `					return rc;` |
|         - | 1042 | `				}` |
|         9 | 1043 | `			}` |
|   8042070 | 1044 | `		}` |
|  16084145 | 1045 | `		pNode->xCode = PH7_CompileVariable;` |
|  40844983 | 1046 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    810597 | 1047 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    810597 | 1048 | `		 if( bAfterMemberOp ){` |
|         - | 1049 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1050 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1051 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1052 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    126933 | 1053 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    126933 | 1054 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    747133 | 1055 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1056 | `			 /* List/Array node */` |
|    286053 | 1057 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1058 | `				 /* Assume a literal */` |
|       ! 0 | 1059 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1060 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1061 | `			 }else{` |
|    286053 | 1062 | `				 pCur += 2;` |
|         - | 1063 | `				 /* Collect array/list tokens */` |
|    286053 | 1064 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    286053 | 1065 | `				 if( pCur < pGen->pEnd ){` |
|    286051 | 1066 | `					 pCur++;` |
|    143028 | 1067 | `				 }else{` |
|         - | 1068 | `					 /* Syntax error */` |
|         4 | 1069 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1070 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1071 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1072 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1073 | `					 }` |
|         3 | 1074 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1075 | `					 return rc;` |
|         - | 1076 | `				 }` |
|    286051 | 1077 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    286051 | 1078 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        39 | 1079 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        39 | 1080 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1081 | `						 /* Syntax error */` |
|         3 | 1082 | `						 rc = PH7_GenSyntaxError(pGen,pNode->pStart,"\"=\"");` |
|         3 | 1083 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1084 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1085 | `						 }` |
|         3 | 1086 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1087 | `						 return rc;` |
|         - | 1088 | `					 }` |
|        16 | 1089 | `				 }` |
|         5 | 1090 | `			 }` |
|    540643 | 1091 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1092 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|     15749 | 1093 | `			 pCur++; /* Skip 'yield' keyword */` |
|     15749 | 1094 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1095 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1096 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|     15749 | 1097 | `			 pNode->xCode = PH7_CompileYield;` |
|    389749 | 1098 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|    381643 | 1099 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        54 | 1100 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        33 | 1101 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1102 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|       495 | 1103 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1104 | `				 /* Assume a literal */` |
|       ! 0 | 1105 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1106 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1107 | `			 }else{` |
|         - | 1108 | `				 /* Assemble annonymous functions body */` |
|       495 | 1109 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       495 | 1110 | `				 if( rc != SXRET_OK ){` |
|        28 | 1111 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1112 | `					 return rc;` |
|         - | 1113 | `				 }` |
|       471 | 1114 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1115 | `			  }` |
|    381620 | 1116 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|        41 | 1117 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        23 | 1118 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        12 | 1119 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|         9 | 1120 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1121 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1122 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1123 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1124 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|        32 | 1125 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|        32 | 1126 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1127 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1128 | `				 return rc;` |
|         - | 1129 | `			 }` |
|        32 | 1130 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    381372 | 1131 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    381222 | 1132 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        46 | 1133 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        25 | 1134 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1135 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       291 | 1136 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       291 | 1137 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1138 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1139 | `				 return rc;` |
|         - | 1140 | `			 }` |
|       291 | 1141 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    381216 | 1142 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1143 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        77 | 1144 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        77 | 1145 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1146 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1147 | `				 return rc;` |
|         - | 1148 | `			 }` |
|        77 | 1149 | `			 pNode->xCode = PH7_CompileMatch;` |
|    381037 | 1150 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1151 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1152 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1153 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1154 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1155 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1156 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1157 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1158 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    380983 | 1159 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1160 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        93 | 1161 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        93 | 1162 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        49 | 1163 | `		 }else{` |
|         - | 1164 | `			 /* Assume a literal */` |
|    380877 | 1165 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    380877 | 1166 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1167 | `		 }` |
|  32397603 | 1168 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1169 | `		 /* Constants,function name,namespace path,class name... */` |
|  10393919 | 1170 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|  10393919 | 1171 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   5196962 | 1172 | `	 }else{` |
|  21598407 | 1173 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1174 | `			 /* Point to the code generator routine */` |
|   7140599 | 1175 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   7140599 | 1176 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1177 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 1178 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1179 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1180 | `				 }` |
|         3 | 1181 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1182 | `				 return rc;` |
|         - | 1183 | `			 }` |
|   3570296 | 1184 | `		 }` |
|         - | 1185 | `		/* Advance the stream cursor */` |
|  21598405 | 1186 | `		pCur++;` |
|         - | 1187 | `	 }` |
|         - | 1188 | `	/* Point to the end of the token stream */` |
|  68537895 | 1189 | `	pNode->pEnd = pCur;` |
|         - | 1190 | `	/* Save the node for later processing */` |
|  68537895 | 1191 | `	*ppNode = pNode;` |
|         - | 1192 | `	/* Synchronize cursors */` |
|  68537895 | 1193 | `	pGen->pIn = pCur;` |
|  68537895 | 1194 | `	return SXRET_OK;` |
|  34271052 | 1195 | `}` |
|         - | 1196 | `/*` |
|         - | 1197 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1198 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1199 | ` * level is zero.` |
|         - | 1200 | ` */` |
|   1322524 | 1201 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1202 | `{` |
|   1322529 | 1203 | `	SyToken *pCur = pStart;` |
|   1322529 | 1204 | `	sxi32 iNest = 0;` |
|   1322529 | 1205 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1206 | `		/* Last expression */` |
|    546979 | 1207 | `		return SXERR_EOF;` |
|         - | 1208 | `	}` |
|   3391907 | 1209 | `	while( pCur < pEnd ){` |
|   3147381 | 1210 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    531029 | 1211 | `			break;` |
|         - | 1212 | `		}` |
|   2616357 | 1213 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    214697 | 1214 | `			iNest++;` |
|   2509011 | 1215 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    214699 | 1216 | `			iNest--;` |
|    107347 | 1217 | `		}` |
|   2616357 | 1218 | `		pCur++;` |
|         5 | 1219 | `	}` |
|    775555 | 1220 | `	*ppNext = pCur;` |
|    775555 | 1221 | `	return SXRET_OK;` |
|    661267 | 1222 | `}` |
|         - | 1223 | `/*` |
|         - | 1224 | ` * Free an expression tree.` |
|         - | 1225 | ` */` |
|  58559452 | 1226 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1227 | `{` |
|  58559457 | 1228 | `	if( pNode->pLeft ){` |
|         - | 1229 | `		/* Release the left tree */` |
|  23251627 | 1230 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|  11625811 | 1231 | `	}` |
|  58559457 | 1232 | `	if( pNode->pRight ){` |
|         - | 1233 | `		/* Release the right tree */` |
|  13525805 | 1234 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   6762900 | 1235 | `	}` |
|  58559457 | 1236 | `	if( pNode->pCond ){` |
|         - | 1237 | `		/* Release the conditional tree used by the ternary operator */` |
|    371935 | 1238 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|    185965 | 1239 | `	}` |
|  58559457 | 1240 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1241 | `		ph7_expr_node **apArg;` |
|         - | 1242 | `		sxu32 n;` |
|         - | 1243 | `		/* Release node arguments */` |
|   6419129 | 1244 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  14467589 | 1245 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   8048465 | 1246 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   4024235 | 1247 | `		}` |
|   6419129 | 1248 | `		SySetRelease(&pNode->aNodeArgs);` |
|   3209562 | 1249 | `	}` |
|         - | 1250 | `	/* Finally,release this node */` |
|  58559457 | 1251 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  58559457 | 1252 | `}` |
|         - | 1253 | `/*` |
|         - | 1254 | ` * Free an expression tree.` |
|         - | 1255 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1256 | ` */` |
|  12703682 | 1257 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1258 | `{` |
|         - | 1259 | `	ph7_expr_node **apNode;` |
|         - | 1260 | `	sxu32 n;` |
|  12703687 | 1261 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  81241627 | 1262 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  68537945 | 1263 | `		if( apNode[n] ){` |
|  12704021 | 1264 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   6352008 | 1265 | `		}` |
|  34268975 | 1266 | `	}` |
|  12703687 | 1267 | `	return SXRET_OK;` |
|         5 | 1268 | `}` |
|         - | 1269 | `/*` |
|         - | 1270 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1271 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1272 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1273 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1274 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1275 | ` */` |
|  16654914 | 1276 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1277 | `{` |
|  16654919 | 1278 | `	if( pNode == 0 ){` |
|  10339527 | 1279 | `		return 0;` |
|         - | 1280 | `	}` |
|   6315397 | 1281 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1282 | `		return 1;` |
|         - | 1283 | `	}` |
|   6315385 | 1284 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1285 | `		return 1;` |
|         - | 1286 | `	}` |
|   6315381 | 1287 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1288 | `		return 1;` |
|         - | 1289 | `	}` |
|   6315381 | 1290 | `	return 0;` |
|   8327462 | 1291 | `}` |
|         - | 1292 | `/*` |
|         - | 1293 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1294 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1295 | ` */` |
|   3993940 | 1296 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1297 | `{` |
|         - | 1298 | `	sxi32 iExprOp;` |
|   3993945 | 1299 | `	if( pNode->pOp == 0 ){` |
|   2832797 | 1300 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1301 | `	}` |
|   1161153 | 1302 | `	iExprOp = pNode->pOp->iOp;` |
|   1161153 | 1303 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    811009 | 1304 | `			return TRUE;` |
|         - | 1305 | `	}` |
|    350149 | 1306 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    350143 | 1307 | `		if( pNode->pLeft->pOp ) {` |
|    119122 | 1308 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|     49956 | 1309 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1310 | `				return FALSE;` |
|         5 | 1311 | `			}` |
|    290582 | 1312 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1313 | `			return FALSE;` |
|         - | 1314 | `		}` |
|    350143 | 1315 | `		return TRUE;` |
|         - | 1316 | `	}` |
|         8 | 1317 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1318 | `		return TRUE;` |
|         - | 1319 | `	}` |
|         - | 1320 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1321 | `	return FALSE;` |
|   1996975 | 1322 | `}` |
|         - | 1323 | `/* Forward declaration */` |
|         - | 1324 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1325 | `/* Macro to check if the given node is a terminal.` |
|         - | 1326 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1327 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1328 | ` * linked ternary/elvis node). */` |
|         - | 1329 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1330 | `/*` |
|         - | 1331 | ` * Buid an expression tree for each given function argument.` |
|         - | 1332 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1333 | ` */` |
|   4050794 | 1334 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1335 | `{` |
|         - | 1336 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1337 | `	sxi32 rc;` |
|         - | 1338 | `	/* Process function arguments from left to right */` |
|   4050799 | 1339 | `	iCur = 0;` |
|   4865450 | 1340 | `	for(;;){` |
|   9730905 | 1341 | `		if( iCur >= nToken ){` |
|         - | 1342 | `			/* No more arguments to process */` |
|   4050773 | 1343 | `			break;` |
|         - | 1344 | `		}` |
|   5680137 | 1345 | `		iNode = iCur;` |
|   5680137 | 1346 | `		iNest = 0;` |
|  18998125 | 1347 | `		while( iCur < nToken ){` |
|  14947355 | 1348 | `			if( apNode[iCur] ){` |
|  14901097 | 1349 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    814686 | 1350 | `					break;` |
|  13271730 | 1351 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   7154336 | 1352 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|   1034494 | 1353 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1354 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1355 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1356 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1357 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1358 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1359 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|   1032041 | 1360 | `					iNest++;` |
|  12755717 | 1361 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|   1032041 | 1362 | `					iNest--;` |
|    516018 | 1363 | `				}` |
|   6635865 | 1364 | `			}` |
|  13317993 | 1365 | `			iCur++;` |
|         5 | 1366 | `		}` |
|   5680137 | 1367 | `		if( iCur > iNode ){` |
|   5680131 | 1368 | `			SyString sArgName = {0, 0};` |
|         - | 1369 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1370 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1371 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   5680126 | 1372 | `			if( (iCur - iNode) >= 2` |
|   3928979 | 1373 | `				&& apNode[iNode]` |
|   2177818 | 1374 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|   1155461 | 1375 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|    132864 | 1376 | `				&& apNode[iNode+1]` |
|    132615 | 1377 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1378 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       291 | 1379 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       291 | 1380 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       291 | 1381 | `				apNode[iNode] = 0;` |
|       291 | 1382 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       291 | 1383 | `				apNode[iNode+1] = 0;` |
|       291 | 1384 | `				iNode += 2;` |
|         - | 1385 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1386 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       291 | 1387 | `				if( iNode >= iCur ){` |
|         4 | 1388 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1389 | `						pOp->pStart->nLine,` |
|         - | 1390 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1391 | `						&sArgName);` |
|         3 | 1392 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1393 | `						rc = SXERR_SYNTAX;` |
|         1 | 1394 | `					}` |
|         3 | 1395 | `					return rc;` |
|         - | 1396 | `				}` |
|       142 | 1397 | `			}` |
|   5680124 | 1398 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1399 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1400 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1401 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1402 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1403 | `					apNode[iNode] = 0;` |
|       ! 0 | 1404 | `			}` |
|         - | 1405 | `			{` |
|         - | 1406 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1407 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1408 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1409 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1410 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1411 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1412 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1413 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1414 | `				 * NULL slots ahead of the flagged subtree. */` |
|   5680129 | 1415 | `				int bSpreadArg = 0;` |
|         - | 1416 | `				sxi32 iScan;` |
|   5680157 | 1417 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   5680157 | 1418 | `					if( apNode[iScan] ){` |
|   5680129 | 1419 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   5680129 | 1420 | `						break;` |
|         - | 1421 | `					}` |
|        15 | 1422 | `				}` |
|   5680129 | 1423 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   5680129 | 1424 | `				if( bSpreadArg && apNode[iNode] ){` |
|      4029 | 1425 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|      2012 | 1426 | `				}` |
|         - | 1427 | `			}` |
|   5680129 | 1428 | `			if( apNode[iNode] ){` |
|   5680129 | 1429 | `				if( sArgName.nByte > 0 ){` |
|       289 | 1430 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       289 | 1431 | `					apNode[iNode]->sArgName = sArgName;` |
|       142 | 1432 | `				}` |
|         - | 1433 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   5680129 | 1434 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   2840067 | 1435 | `			}else{` |
|         - | 1436 | `				/* No expression before comma */` |
|       ! 0 | 1437 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1438 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1439 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1440 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1441 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1442 | `				}` |
|       ! 0 | 1443 | `				return rc;` |
|         - | 1444 | `			}` |
|   2840067 | 1445 | `		}else{` |
|         - | 1446 | `			/* Comma with no preceding argument */` |
|         8 | 1447 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1448 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1449 | `				rc = SXERR_SYNTAX;` |
|         3 | 1450 | `			}` |
|         8 | 1451 | `			return rc;` |
|         - | 1452 | `		}` |
|         - | 1453 | `		/* Jump trailing comma */` |
|   5680129 | 1454 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|   1629361 | 1455 | `			iCur++;` |
|   1629361 | 1456 | `			if( iCur >= nToken ){` |
|         - | 1457 | `				/* Trailing comma after last argument */` |
|        19 | 1458 | `				break;` |
|         - | 1459 | `			}` |
|    814669 | 1460 | `		}` |
|         5 | 1461 | `	}` |
|   4050791 | 1462 | `	return SXRET_OK;` |
|   2025402 | 1463 | `}` |
|         - | 1464 | ` /*` |
|         - | 1465 | `  * Create an expression tree from an array of tokens.` |
|         - | 1466 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1467 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1468 | `  */` |
|  21939972 | 1469 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1470 | ` {` |
|         - | 1471 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1472 | `	 ph7_expr_node *pNode;` |
|         - | 1473 | `	 ph7_expr_node *pSuppress;` |
|         - | 1474 | `	 sxi32 iCur;` |
|         - | 1475 | `	 sxi32 rc;` |
|  21939977 | 1476 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1477 | `		 /* TICKET 1433-17: self evaluating node */` |
|   9532595 | 1478 | `		 return SXRET_OK;` |
|         - | 1479 | `	 }` |
|         - | 1480 | `	 /* Process expressions enclosed in parenthesis first */` |
|  89484659 | 1481 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1482 | `		 sxi32 iNest;` |
|         - | 1483 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1484 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1485 | `		  */` |
|  77077279 | 1486 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  76752599 | 1487 | `			 continue;` |
|         - | 1488 | `		 }` |
|    324685 | 1489 | `		 iNest = 1;` |
|    324685 | 1490 | `		 iLeft = iCur;` |
|         - | 1491 | `		 /* Find the closing parenthesis */` |
|    324685 | 1492 | `		 iCur++;` |
|   2883725 | 1493 | `		 while( iCur < nToken ){` |
|   2883725 | 1494 | `			 if( apNode[iCur] ){` |
|   2883725 | 1495 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1496 | `					 /* Decrement nesting level */` |
|    452587 | 1497 | `					 iNest--;` |
|    452587 | 1498 | `					 if( iNest <= 0 ){` |
|    324685 | 1499 | `						 break;` |
|         5 | 1500 | `					 }` |
|   2495094 | 1501 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1502 | `					 /* Increment nesting level */` |
|    127907 | 1503 | `					 iNest++;` |
|     63951 | 1504 | `				 }` |
|   1279520 | 1505 | `			 }` |
|   2559045 | 1506 | `			 iCur++;` |
|         5 | 1507 | `		 }` |
|    324685 | 1508 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1509 | `			 sxi32 j;` |
|         - | 1510 | `			 /* Recurse and process this expression */` |
|    324685 | 1511 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    324685 | 1512 | `			 if( rc != SXRET_OK ){` |
|         3 | 1513 | `				 return rc;` |
|         - | 1514 | `			 }` |
|         - | 1515 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1516 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1517 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1518 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1519 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1520 | `			  * group's free below silently drops the unpacking. */` |
|    324683 | 1521 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    324683 | 1522 | `				 if( apNode[j] ){` |
|    324683 | 1523 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    324678 | 1524 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    324683 | 1525 | `					 break;` |
|         - | 1526 | `				 }` |
|       ! 0 | 1527 | `			 }` |
|    162339 | 1528 | `		 }` |
|         - | 1529 | `		 /* Free the left and right nodes */` |
|    324683 | 1530 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    324683 | 1531 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    324683 | 1532 | `		 apNode[iLeft] = 0;` |
|    324683 | 1533 | `		 apNode[iCur] = 0;` |
|    162344 | 1534 | `	 }` |
|         - | 1535 | `	  /* Process expressions enclosed in braces */` |
|  92121809 | 1536 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1537 | `		 sxi32 iNest;` |
|         - | 1538 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1539 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1540 | `		  */` |
|  79953293 | 1541 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  79949445 | 1542 | `			 continue;` |
|         - | 1543 | `		 }` |
|      3853 | 1544 | `		 iNest = 1;` |
|      3853 | 1545 | `		 iLeft = iCur;` |
|         - | 1546 | `		 /* Find the closing parenthesis */` |
|      3853 | 1547 | `		 iCur++;` |
|      7699 | 1548 | `		 while( iCur < nToken ){` |
|      7699 | 1549 | `			 if( apNode[iCur] ){` |
|      7699 | 1550 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1551 | `					 /* Decrement nesting level */` |
|      3853 | 1552 | `					 iNest--;` |
|      3853 | 1553 | `					 if( iNest <= 0 ){` |
|      3853 | 1554 | `						 break;` |
|       ! 0 | 1555 | `					 }` |
|      3851 | 1556 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1557 | `					 /* Increment nesting level */` |
|       ! 0 | 1558 | `					 iNest++;` |
|       ! 0 | 1559 | `				 }` |
|      1923 | 1560 | `			 }` |
|      3851 | 1561 | `			 iCur++;` |
|         5 | 1562 | `		 }` |
|      3853 | 1563 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1564 | `			 /* Recurse and process this expression */` |
|      3851 | 1565 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      3851 | 1566 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1567 | `				 return rc;` |
|         - | 1568 | `			 }` |
|      1923 | 1569 | `		 }` |
|         - | 1570 | `		 /* Free the left and right nodes */` |
|      3853 | 1571 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      3853 | 1572 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      3853 | 1573 | `		 apNode[iLeft] = 0;` |
|      3853 | 1574 | `		 apNode[iCur] = 0;` |
|      1929 | 1575 | `	 }` |
|         - | 1576 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|  12168521 | 1577 | `	 iLeft = -1;` |
|  92129467 | 1578 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79960963 | 1579 | `		 if( apNode[iCur] == 0 ){` |
|  35066071 | 1580 | `			 continue;` |
|         - | 1581 | `		 }` |
|  44894897 | 1582 | `		 pNode = apNode[iCur];` |
|  44894897 | 1583 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|  12891753 | 1584 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1585 | `				 /* Collect function arguments */` |
|   5451235 | 1586 | `				 sxi32 iPtr = 0;` |
|   5451235 | 1587 | `				 sxi32 nFuncTok = 0;` |
|  25849817 | 1588 | `				 while( nFuncTok + iCur < nToken ){` |
|  25849817 | 1589 | `					 if( apNode[nFuncTok+iCur] ){` |
|  25803559 | 1590 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   5764681 | 1591 | `							 iPtr++;` |
|  22921221 | 1592 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   5764681 | 1593 | `							 iPtr--;` |
|   5764681 | 1594 | `							 if( iPtr <= 0 ){` |
|   5451235 | 1595 | `								 break;` |
|         - | 1596 | `							 }` |
|    156723 | 1597 | `						 }` |
|  10176162 | 1598 | `					 }` |
|  20398587 | 1599 | `					 nFuncTok++;` |
|         5 | 1600 | `				 }` |
|   5451235 | 1601 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1602 | `					 /* Syntax error */` |
|       ! 0 | 1603 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1604 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1605 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1606 | `					 }` |
|       ! 0 | 1607 | `					 return rc;` |
|         - | 1608 | `				 }` |
|   5451235 | 1609 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1610 | `					 /* Syntax error */` |
|       ! 0 | 1611 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1612 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1613 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1614 | `					 }` |
|       ! 0 | 1615 | `					 return rc;` |
|         - | 1616 | `				 }` |
|   5451235 | 1617 | `				 if( nFuncTok > 1 ){` |
|         - | 1618 | `					 /* Process function arguments */` |
|   4050799 | 1619 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   4050799 | 1620 | `					 if( rc != SXRET_OK ){` |
|        10 | 1621 | `						 return rc;` |
|         - | 1622 | `					 }` |
|   2025393 | 1623 | `				 }` |
|         - | 1624 | `				 /* Link the node to the tree */` |
|   5451227 | 1625 | `				 pNode->pLeft = apNode[iLeft];` |
|   5451227 | 1626 | `				 apNode[iLeft] = 0;` |
|  25849785 | 1627 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  20398563 | 1628 | `					 apNode[iCur+iPtr] = 0;` |
|  10199284 | 1629 | `				 }` |
|         - | 1630 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1631 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1632 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1633 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1634 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1635 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1636 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1637 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1638 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1639 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1640 | `				 {` |
|   5451227 | 1641 | `					 sxi32 iNew = iLeft - 1;` |
|   7281489 | 1642 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|   1830267 | 1643 | `						 iNew--;` |
|         5 | 1644 | `					 }` |
|   5451222 | 1645 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   3233990 | 1646 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   1971555 | 1647 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    716809 | 1648 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    716809 | 1649 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    716809 | 1650 | `						 apNode[iNew] = 0;` |
|    716809 | 1651 | `						 pNode = apNode[iCur];` |
|    358407 | 1652 | `					 }` |
|         - | 1653 | `				 }` |
|  10166134 | 1654 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1655 | `				 /* Subscripting */` |
|   2525997 | 1656 | `				 sxi32 iArrTok = iCur + 1;` |
|   2525997 | 1657 | `				 sxi32 iNest = 1;` |
|   2525992 | 1658 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1659 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1660 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1661 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   2525992 | 1662 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1663 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1664 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|    301689 | 1665 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1666 | `						 /* Syntax error */` |
|       ! 0 | 1667 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1668 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1669 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1670 | `						 }` |
|       ! 0 | 1671 | `						 return rc;` |
|         - | 1672 | `				 }` |
|         - | 1673 | `				 /* Collect index tokens */` |
|   5339919 | 1674 | `				 while( iArrTok < nToken ){` |
|   5339919 | 1675 | `					 if( apNode[iArrTok] ){` |
|   5339887 | 1676 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1677 | `							 /* Increment nesting level */` |
|     19205 | 1678 | `							 iNest++;` |
|   5330287 | 1679 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1680 | `							 /* Decrement nesting level */` |
|   2545197 | 1681 | `							 iNest--;` |
|   2545197 | 1682 | `							 if( iNest <= 0 ){` |
|   2525997 | 1683 | `								 break;` |
|         - | 1684 | `							 }` |
|      9600 | 1685 | `						 }` |
|   1406945 | 1686 | `					 }` |
|   2813927 | 1687 | `					 ++iArrTok;` |
|         5 | 1688 | `				 }` |
|   2525997 | 1689 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1690 | `					 /* Recurse and process this expression */` |
|   2368341 | 1691 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   2368341 | 1692 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1693 | `						 return rc;` |
|         - | 1694 | `					 }` |
|         - | 1695 | `					 /* Link the node to it's index */` |
|   2368341 | 1696 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|   1184168 | 1697 | `				 }` |
|         - | 1698 | `				 /* Link the node to the tree */` |
|   2525997 | 1699 | `				 pNode->pLeft = apNode[iLeft];` |
|   2525997 | 1700 | `				 pNode->pRight = 0;` |
|   2525997 | 1701 | `				 apNode[iLeft] = 0;` |
|   7865911 | 1702 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   5339919 | 1703 | `					 apNode[iNest] = 0;` |
|   2669962 | 1704 | `				 }` |
|   1263001 | 1705 | `			 }else{` |
|         - | 1706 | `				 /* Member access operators [i.e: '->','::'] */` |
|   4914531 | 1707 | `				  iRight = iCur + 1;` |
|   4918377 | 1708 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      3851 | 1709 | `					 iRight++;` |
|         5 | 1710 | `				 }` |
|   4914531 | 1711 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1712 | `					 /* Syntax error */` |
|         5 | 1713 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1714 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1715 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1716 | `					 }` |
|         5 | 1717 | `					 return rc;` |
|         - | 1718 | `				 }` |
|         - | 1719 | `				 /* Link the node to the tree */` |
|   4914527 | 1720 | `				 pNode->pLeft = apNode[iLeft];` |
|   4914522 | 1721 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   4733601 | 1722 | `					 && pNode->pLeft->pOp == 0 &&` |
|   4475339 | 1723 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|         - | 1724 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|         - | 1725 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|         - | 1726 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|         - | 1727 | `					  * accepts. */` |
|         4 | 1728 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|         - | 1729 | `						 /* Syntax error */` |
|       ! 0 | 1730 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1731 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|       ! 0 | 1732 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1733 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1734 | `						 }` |
|       ! 0 | 1735 | `						 return rc;` |
|         - | 1736 | `				 }` |
|   4914527 | 1737 | `				 pNode->pRight = apNode[iRight];` |
|   4914527 | 1738 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1739 | `			 }` |
|   6445868 | 1740 | `		 }` |
|  44894885 | 1741 | `		 iLeft = iCur;` |
|  22447445 | 1742 | `	 }` |
|         - | 1743 | `	 /* Handle left associative (new, clone) operators */` |
|  92129435 | 1744 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79960931 | 1745 | `		 if( apNode[iCur] == 0 ){` |
|  48728735 | 1746 | `			 continue;` |
|         - | 1747 | `		 }` |
|  31232201 | 1748 | `		 pNode = apNode[iCur];` |
|  31232201 | 1749 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1750 | `			 SyToken *pToken;` |
|         - | 1751 | `			 /* Get the left node */` |
|     54129 | 1752 | `			 iLeft = iCur + 1;` |
|     54137 | 1753 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         9 | 1754 | `				 iLeft++;` |
|         1 | 1755 | `			 }` |
|     54129 | 1756 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1757 | `				  /* Syntax error */` |
|       ! 0 | 1758 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1759 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1760 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1761 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1762 | `				 }` |
|       ! 0 | 1763 | `				 return rc;` |
|         - | 1764 | `			 }` |
|         - | 1765 | `			 /* Make sure the operand are of a valid type */` |
|     54129 | 1766 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1767 | `				 /* Clone:` |
|         - | 1768 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|         - | 1769 | `				  *  ++ function call (including annonymous)` |
|         - | 1770 | `				  *  ++ array member` |
|         - | 1771 | `				  *  ++ 'new' operator` |
|         - | 1772 | `				  * Example:` |
|         - | 1773 | `				  *   clone $pObj;` |
|         - | 1774 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|         - | 1775 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|         - | 1776 | `				  */` |
|     53805 | 1777 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|     53799 | 1778 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1779 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1780 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1781 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1782 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1783 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1784 | `						 }` |
|       ! 0 | 1785 | `						 return rc;` |
|         - | 1786 | `					 }` |
|     26897 | 1787 | `				 }` |
|     26905 | 1788 | `			 }else{` |
|         - | 1789 | `				 /* New */` |
|       324 | 1790 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 1791 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 1792 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 1793 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 1794 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 1795 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 1796 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 1797 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 1798 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1799 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1800 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1801 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1802 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1803 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1804 | `					 }` |
|       ! 0 | 1805 | `					 return rc;` |
|         - | 1806 | `				 }` |
|       329 | 1807 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       329 | 1808 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       324 | 1809 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        33 | 1810 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 1811 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 1812 | `						 /* Syntax error */` |
|       ! 0 | 1813 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1814 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1815 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1816 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1817 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1818 | `						 }` |
|       ! 0 | 1819 | `						 return rc;` |
|         - | 1820 | `					 }` |
|       162 | 1821 | `				 }` |
|         - | 1822 | `			 }` |
|         - | 1823 | `			  /* Link the node to the tree */` |
|     54129 | 1824 | `			 pNode->pLeft = apNode[iLeft];` |
|     54129 | 1825 | `			 apNode[iLeft] = 0;` |
|     54129 | 1826 | `			 pNode->pRight = 0; /* Paranoid */` |
|     27062 | 1827 | `		 }` |
|  15616103 | 1828 | `	 }` |
|         - | 1829 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|  12168509 | 1830 | `	 iLeft = -1;` |
|  92248867 | 1831 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79960931 | 1832 | `		 if( apNode[iCur] == 0 ){` |
|  48728735 | 1833 | `			 continue;` |
|         - | 1834 | `		 }` |
|  31232201 | 1835 | `		 pNode = apNode[iCur];` |
|  31232201 | 1836 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    146421 | 1837 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|    127163 | 1838 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1839 | `					 /* Link the node to the tree */` |
|    134869 | 1840 | `					 pNode->pLeft = apNode[iLeft];` |
|    134869 | 1841 | `					 apNode[iLeft] = 0;` |
|     67432 | 1842 | `			 }` |
|    192640 | 1843 | `		  }` |
|  31351633 | 1844 | `		 iLeft = iCur;` |
|  15735535 | 1845 | `	  }` |
|  12287941 | 1846 | `	 iLeft = -1;` |
|  92248867 | 1847 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  79960931 | 1848 | `		 if( apNode[iCur] == 0 ){` |
|  48863599 | 1849 | `			 continue;` |
|         - | 1850 | `		 }` |
|  31097337 | 1851 | `		 pNode = apNode[iCur];` |
|  31097337 | 1852 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     11552 | 1853 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     11557 | 1854 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1855 | `					 /* Syntax error */` |
|       ! 0 | 1856 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1857 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1858 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1859 | `					 }` |
|       ! 0 | 1860 | `					 return rc;` |
|         - | 1861 | `			 }` |
|         - | 1862 | `			 /* Link the node to the tree */` |
|     11557 | 1863 | `			 pNode->pLeft = apNode[iLeft];` |
|     11557 | 1864 | `			 apNode[iLeft] = 0;` |
|         - | 1865 | `			 /* Mark as pre-increment/decrement node */` |
|     11557 | 1866 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      5776 | 1867 | `		  }` |
|  31097337 | 1868 | `		 iLeft = iCur;` |
|  15548671 | 1869 | `	 }` |
|         - | 1870 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|  12287941 | 1871 | `	  iLeft = 0;` |
|  92248861 | 1872 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  79960927 | 1873 | `		  if( apNode[iCur] ){` |
|  31085781 | 1874 | `			  pNode = apNode[iCur];` |
|  31085781 | 1875 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    835181 | 1876 | `				  if( iLeft > 0 ){` |
|         - | 1877 | `					  /* Link the node to the tree */` |
|    835179 | 1878 | `					  pNode->pLeft = apNode[iLeft];` |
|    835179 | 1879 | `					  apNode[iLeft] = 0;` |
|    835179 | 1880 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|     53853 | 1881 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1882 | `							   /* Syntax error */` |
|       ! 0 | 1883 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 1884 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1885 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1886 | `							  }` |
|       ! 0 | 1887 | `							  return rc;` |
|         - | 1888 | `						  }` |
|     26924 | 1889 | `					  }` |
|    417592 | 1890 | `				  }else{` |
|         - | 1891 | `					  /* Syntax error */` |
|         3 | 1892 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 1893 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1894 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1895 | `					  }` |
|         3 | 1896 | `					  return rc;` |
|         - | 1897 | `				  }` |
|    417587 | 1898 | `			  }` |
|         - | 1899 | `			  /* Save terminal position */` |
|  31085779 | 1900 | `			  iLeft = iCur;` |
|  15542887 | 1901 | `		  }` |
|  39980465 | 1902 | `	  }` |
|         - | 1903 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1904 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1905 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1906 | `	  * yielding a right-leaning tree. */` |
|  92248859 | 1907 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  79960925 | 1908 | `		 if( apNode[iCur] == 0 ){` |
|  49710437 | 1909 | `			 continue;` |
|         - | 1910 | `		 }` |
|  30250493 | 1911 | `		 pNode = apNode[iCur];` |
|  30250493 | 1912 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 1913 | `			 sxi32 iL, iR;` |
|         - | 1914 | `			 /* Find the right operand */` |
|       113 | 1915 | `			 iR = -1;` |
|         - | 1916 | `			 {` |
|         - | 1917 | `				 sxi32 j;` |
|       125 | 1918 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       125 | 1919 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 1920 | `				 }` |
|         - | 1921 | `			 }` |
|         - | 1922 | `			 /* Find the left operand */` |
|       113 | 1923 | `			 iL = -1;` |
|         - | 1924 | `			 {` |
|         - | 1925 | `				 sxi32 j;` |
|       181 | 1926 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       181 | 1927 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 1928 | `				 }` |
|         - | 1929 | `			 }` |
|       113 | 1930 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 1931 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 1932 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1933 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1934 | `				 }` |
|       ! 0 | 1935 | `				 return rc;` |
|         - | 1936 | `			 }` |
|       113 | 1937 | `			 pNode->pLeft  = apNode[iL];` |
|       113 | 1938 | `			 pNode->pRight = apNode[iR];` |
|       113 | 1939 | `			 apNode[iL] = 0;` |
|       113 | 1940 | `			 apNode[iR] = 0;` |
|         - | 1941 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 1942 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 1943 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 1944 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 1945 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 1946 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 1947 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 1948 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 1949 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 1950 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 1951 | `			  * operands are respected. */` |
|       112 | 1952 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        74 | 1953 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 1954 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 1955 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 1956 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 1957 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 1958 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 1959 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 1960 | `				 while( pTail->pLeft` |
|        34 | 1961 | `					 && pTail->pLeft->pOp` |
|        23 | 1962 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 1963 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 1964 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 1965 | `					 pTail = pTail->pLeft;` |
|         1 | 1966 | `				 }` |
|         - | 1967 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 1968 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 1969 | `				 pTail->pLeft = pNode;` |
|        27 | 1970 | `				 apNode[iCur] = pHead;` |
|        13 | 1971 | `			 }` |
|        56 | 1972 | `		 }` |
|  15125249 | 1973 | `	 }` |
|         - | 1974 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
| 135167193 | 1975 | `	 for( i = 7 ; i < 17 ; i++ ){` |
| 122879269 | 1976 | `		 iLeft = -1;` |
| 922488175 | 1977 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 799608921 | 1978 | `			 if( apNode[iCur] == 0 ){` |
| 549204543 | 1979 | `				 continue;` |
|         - | 1980 | `			 }` |
| 250404383 | 1981 | `			 pNode = apNode[iCur];` |
| 250404383 | 1982 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1983 | `				 /* Get the right node */` |
|   4245377 | 1984 | `				 iRight = iCur + 1;` |
|   6610939 | 1985 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   2365567 | 1986 | `					 iRight++;` |
|         5 | 1987 | `				 }` |
|   4245377 | 1988 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1989 | `					 /* Syntax error */` |
|        11 | 1990 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        11 | 1991 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1992 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1993 | `					 }` |
|        11 | 1994 | `					 return rc;` |
|         - | 1995 | `				 }` |
|   4245369 | 1996 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1997 | `					 sxi32  iTmp;` |
|         - | 1998 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1999 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 2000 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 2001 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 2002 | `					  * is swapped below. */` |
|        65 | 2003 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 2004 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2005 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 2006 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2007 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2008 | `						 }` |
|         3 | 2009 | `						 return rc;` |
|         - | 2010 | `					 }` |
|        62 | 2011 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|         - | 2012 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 2013 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 2014 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2015 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2016 | `						 }` |
|       ! 0 | 2017 | `						 return rc;` |
|         - | 2018 | `					 }` |
|        62 | 2019 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|        44 | 2020 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 2021 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 2022 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 2023 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2024 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 2025 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2026 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 2027 | `									 }` |
|       ! 0 | 2028 | `									 return rc;` |
|         - | 2029 | `							 }` |
|       ! 0 | 2030 | `						 }` |
|        21 | 2031 | `					 }` |
|         - | 2032 | `					 /* Swap operands */` |
|        62 | 2033 | `					 iTmp = iRight;` |
|        62 | 2034 | `					 iRight = iLeft;` |
|        62 | 2035 | `					 iLeft = iTmp;` |
|        30 | 2036 | `				 }` |
|         - | 2037 | `				 /* Link the node to the tree */` |
|   4245367 | 2038 | `				 pNode->pLeft = apNode[iLeft];` |
|   4245367 | 2039 | `				 pNode->pRight = apNode[iRight];` |
|   4245367 | 2040 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   2122681 | 2041 | `			 }` |
| 250404373 | 2042 | `			 iLeft = iCur;` |
| 125202189 | 2043 | `		 }` |
|  61439632 | 2044 | `	 }` |
|         - | 2045 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2046 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2047 | `	  * we are dealing with a single operator.` |
|         - | 2048 | `	  */` |
|  12287929 | 2049 | `	  iLeft = -1;` |
|  89243311 | 2050 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  77327319 | 2051 | `		  if( apNode[iCur] == 0 ){` |
|  56679607 | 2052 | `			  continue;` |
|         - | 2053 | `		  }` |
|  20647717 | 2054 | `		  pNode = apNode[iCur];` |
|  20647717 | 2055 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    371937 | 2056 | `			  sxi32 iNest = 1;` |
|    371937 | 2057 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2058 | `				  /* Missing condition */` |
|         3 | 2059 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 2060 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2061 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2062 | `				  }` |
|         3 | 2063 | `				  return rc;` |
|         - | 2064 | `			  }` |
|         - | 2065 | `			  /* Get the right node */` |
|    371935 | 2066 | `			  iRight = iCur + 1;` |
|   1458407 | 2067 | `			  while( iRight < nToken  ){` |
|   1458407 | 2068 | `				  if( apNode[iRight] ){` |
|    739957 | 2069 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2070 | `						  /* Increment nesting level */` |
|       ! 0 | 2071 | `						  ++iNest;` |
|    739957 | 2072 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2073 | `						  /* Decrement nesting level */` |
|    371935 | 2074 | `						  --iNest;` |
|    371935 | 2075 | `						  if( iNest <= 0 ){` |
|    371935 | 2076 | `							  break;` |
|         - | 2077 | `						  }` |
|       ! 0 | 2078 | `					  }` |
|    184011 | 2079 | `				  }` |
|   1086477 | 2080 | `				  iRight++;` |
|         5 | 2081 | `			  }` |
|    371935 | 2082 | `			  if( iRight > iCur + 1 ){` |
|         - | 2083 | `				  /* Recurse and process the then expression */` |
|    368027 | 2084 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    368027 | 2085 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2086 | `					  return rc;` |
|         - | 2087 | `				  }` |
|         - | 2088 | `				  /* Link the node to the tree */` |
|    368027 | 2089 | `				  pNode->pLeft = apNode[iCur + 1];` |
|    184011 | 2090 | `			  }else{` |
|         - | 2091 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2092 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2093 | `			  }` |
|    371935 | 2094 | `			  apNode[iCur + 1] = 0;` |
|    371935 | 2095 | `			  if( iRight + 1 < nToken ){` |
|         - | 2096 | `				  /* Recurse and process the else expression */` |
|    371935 | 2097 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    371935 | 2098 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2099 | `					  return rc;` |
|         - | 2100 | `				  }` |
|         - | 2101 | `				  /* Link the node to the tree */` |
|    371935 | 2102 | `				  pNode->pRight = apNode[iRight + 1];` |
|    371935 | 2103 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|    185970 | 2104 | `			  }else{` |
|       ! 0 | 2105 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2106 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2107 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2108 | `				 }` |
|       ! 0 | 2109 | `				 return rc;` |
|         - | 2110 | `			  }` |
|         - | 2111 | `			  /* Point to the condition */` |
|    371935 | 2112 | `			  pNode->pCond  = apNode[iLeft];` |
|    371935 | 2113 | `			  apNode[iLeft] = 0;` |
|    371935 | 2114 | `			  break;` |
|         - | 2115 | `		  }` |
|  20275785 | 2116 | `		  iLeft = iCur;` |
|  10137895 | 2117 | `	  }` |
|         - | 2118 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2119 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2120 | `	  * so there is no need for a precedence loop here.` |
|         - | 2121 | `	  */` |
|  12287927 | 2122 | `	 iRight = -1;` |
|  92248663 | 2123 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  79960795 | 2124 | `		 if( apNode[iCur] == 0 ){` |
|  63678907 | 2125 | `			 continue;` |
|         - | 2126 | `		 }` |
|  16281893 | 2127 | `		 pNode = apNode[iCur];` |
|  16281893 | 2128 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2129 | `			 /* Get the left node */` |
|   3993893 | 2130 | `			 iLeft = iCur - 1;` |
|   5474315 | 2131 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   1480427 | 2132 | `				 iLeft--;` |
|         5 | 2133 | `			 }` |
|   3993893 | 2134 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2135 | `				 /* Syntax error */` |
|        44 | 2136 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2137 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2138 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2139 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2140 | `				 }else{` |
|        40 | 2141 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2142 | `				 }` |
|        44 | 2143 | `				 if( rc != SXERR_ABORT ){` |
|        42 | 2144 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2145 | `				 }` |
|        44 | 2146 | `				 return rc;` |
|         - | 2147 | `			 }` |
|         - | 2148 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2149 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2150 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2151 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2152 | `			  * a write. */` |
|   3993851 | 2153 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2154 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2155 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2156 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2157 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2158 | `				 }` |
|        11 | 2159 | `				 return rc;` |
|         - | 2160 | `			 }` |
|         - | 2161 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|         - | 2162 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|         - | 2163 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|         - | 2164 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|         - | 2165 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|   3993843 | 2166 | `			 pSuppress = 0;` |
|   3993838 | 2167 | `			 if( apNode[iLeft]->pOp` |
|   2577479 | 2168 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|    580560 | 2169 | `				 && apNode[iLeft]->pLeft != 0` |
|         5 | 2170 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2171 | `				 pSuppress = apNode[iLeft];` |
|       ! 0 | 2172 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|       ! 0 | 2173 | `			 }` |
|   3993843 | 2174 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       123 | 2175 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        88 | 2176 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2177 | `					 /* Left operand must be a modifiable l-value */` |
|         6 | 2178 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2179 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2180 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2181 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2182 | `					 }else{` |
|         4 | 2183 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2184 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2185 | `					 }` |
|         6 | 2186 | `					 if( rc != SXERR_ABORT ){` |
|         6 | 2187 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2188 | `					 }` |
|         6 | 2189 | `					 return rc;` |
|         - | 2190 | `				 }` |
|        43 | 2191 | `			 }` |
|         - | 2192 | `			 /* Link the node to the tree (Reverse) */` |
|   3993839 | 2193 | `			 pNode->pLeft = apNode[iRight];` |
|   3993839 | 2194 | `			 pNode->pRight = apNode[iLeft];` |
|   3993839 | 2195 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   3993839 | 2196 | `			 if( pSuppress ){` |
|         - | 2197 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|       ! 0 | 2198 | `				 pSuppress->pLeft = pNode;` |
|       ! 0 | 2199 | `				 apNode[iCur] = pSuppress;` |
|       ! 0 | 2200 | `			 }` |
|   1996917 | 2201 | `		 }` |
|  16281839 | 2202 | `		 iRight = iCur;` |
|   8140922 | 2203 | `	 }` |
|         - | 2204 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  61439345 | 2205 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  49151477 | 2206 | `		 iLeft = -1;` |
| 368994365 | 2207 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 319842893 | 2208 | `			 if( apNode[iCur] == 0 ){` |
| 270691169 | 2209 | `				 continue;` |
|         - | 2210 | `			 }` |
|  49151729 | 2211 | `			 pNode = apNode[iCur];` |
|  49151729 | 2212 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2213 | `				 /* Get the right node */` |
|        51 | 2214 | `				 iRight = iCur + 1;` |
|        63 | 2215 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        13 | 2216 | `					 iRight++;` |
|         1 | 2217 | `				 }` |
|        51 | 2218 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2219 | `					 /* Syntax error */` |
|       ! 0 | 2220 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2221 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2222 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2223 | `					 }` |
|       ! 0 | 2224 | `					 return rc;` |
|         - | 2225 | `				 }` |
|         - | 2226 | `				 /* Link the node to the tree */` |
|        51 | 2227 | `				 pNode->pLeft = apNode[iLeft];` |
|        51 | 2228 | `				 pNode->pRight = apNode[iRight];` |
|        51 | 2229 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        24 | 2230 | `			 }` |
|  49151729 | 2231 | `			 iLeft = iCur;` |
|  24575867 | 2232 | `		 }` |
|  24575741 | 2233 | `	 }` |
|         - | 2234 | `	 /* Point to the root of the expression tree */` |
|  79960699 | 2235 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  67672849 | 2236 | `		 if( apNode[iCur] ){` |
|  11802635 | 2237 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        22 | 2238 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|        22 | 2239 | `				  if( rc != SXERR_ABORT ){` |
|        22 | 2240 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2241 | `				  }` |
|        22 | 2242 | `				  return rc;` |
|         - | 2243 | `			 }` |
|  11802617 | 2244 | `			 apNode[0] = apNode[iCur];` |
|  11802617 | 2245 | `			 apNode[iCur] = 0;` |
|   5901306 | 2246 | `		 }` |
|  33836418 | 2247 | `	 }` |
|  12287855 | 2248 | `	 return SXRET_OK;` |
|  10910275 | 2249 | ` }` |
|         - | 2250 | ` /*` |
|         - | 2251 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2252 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2253 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2254 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2255 | `  */` |
|  12703686 | 2256 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2257 | `{` |
|         - | 2258 | `	ph7_expr_node **apNode;` |
|         - | 2259 | `	ph7_expr_node *pNode;` |
|         - | 2260 | `	sxi32 rc;` |
|         - | 2261 | `	/* Reset node container */` |
|  12703691 | 2262 | `	SySetReset(pExprNode);` |
|  12703691 | 2263 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2264 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2265 | `	{` |
|  12703691 | 2266 | `		int iLastWasTerm = 0;` |
|  12703691 | 2267 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  81241661 | 2268 | `		while( pGen->pIn < pGen->pEnd ){` |
|  68538009 | 2269 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  68538009 | 2270 | `			if( rc != SXRET_OK ){` |
|        38 | 2271 | `				return rc;` |
|         - | 2272 | `			}` |
|         - | 2273 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  68537975 | 2274 | `			if( pNode->xCode ){` |
|         - | 2275 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  34646349 | 2276 | `				iLastWasTerm = 1;` |
|  51214803 | 2277 | `			}else if( pNode->pOp ){` |
|         - | 2278 | `				/* Operator node */` |
|  19433823 | 2279 | `				iLastWasTerm = 0;` |
|   9716914 | 2280 | `			}else{` |
|         - | 2281 | `				/* Delimiter: ')' and ']' end terms */` |
|  14457813 | 2282 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2283 | `			}` |
|         - | 2284 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2285 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2286 | `			 * node kind, so this single test covers all branches. */` |
|  68537975 | 2287 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2288 | `			/* Save the extracted node */` |
|  68537975 | 2289 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2290 | `		}` |
|         - | 2291 | `	}` |
|  12703657 | 2292 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2293 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2294 | `		*ppRoot = 0;` |
|       ! 0 | 2295 | `		return SXRET_OK;` |
|         - | 2296 | `	}` |
|  12703657 | 2297 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2298 | `	/* Make sure we are dealing with valid nodes */` |
|  12703657 | 2299 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  12703657 | 2300 | `	if( rc != SXRET_OK ){` |
|         - | 2301 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2302 | `		 * cleanup the mess left behind.` |
|         - | 2303 | `		 */` |
|        54 | 2304 | `		*ppRoot = 0;` |
|        54 | 2305 | `		return rc;` |
|         - | 2306 | `	}` |
|         - | 2307 | `	/* Build the tree */` |
|  12703607 | 2308 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  12703607 | 2309 | `	if( rc != SXRET_OK ){` |
|         - | 2310 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2311 | `		*ppRoot = 0;` |
|       103 | 2312 | `		return rc;` |
|         - | 2313 | `	}` |
|         - | 2314 | `	/* Point to the root of the tree */` |
|  12703509 | 2315 | `	*ppRoot = apNode[0];` |
|  12703509 | 2316 | `	return SXRET_OK;` |
|   6351848 | 2317 | `}` |
|         - | 2318 |  |
