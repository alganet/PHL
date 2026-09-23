# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1317/1489 lines (88.45%)

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
|   2732756 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|   2732761 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
|  54782080 |  278 | `	for(;;){` |
| 109564165 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 109564165 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|   9149295 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   4574650 |  285 | `		}else{` |
| 100414875 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 109564165 |  288 | `		if( rc == 0 ){` |
|   2808785 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   2703785 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|    105005 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      5455 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|     99555 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|     23539 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     23539 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|     23531 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|     38012 |  307 | `		}` |
| 106831409 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|   1366383 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|    676776 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|    676781 |  320 | `	SyToken *pCur = pIn;` |
|    676781 |  321 | `	sxi32 iNest = 1;` |
|   2864682 |  322 | `	for(;;){` |
|   5729369 |  323 | `		if( pCur >= pEnd ){` |
|       657 |  324 | `			break;` |
|         - |  325 | `		}` |
|   5728717 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|    273395 |  328 | `			iNest++;` |
|   5592022 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|    949519 |  331 | `			iNest--;` |
|    949519 |  332 | `			if( iNest <= 0 ){` |
|    676129 |  333 | `				break;` |
|         - |  334 | `			}` |
|    136695 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
|   5052593 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|    676781 |  340 | `	*ppEnd = pCur;` |
|    676781 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|     11456 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|     11456 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     11402 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       223 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|     11243 |  358 | `	if( bCheckFunc ){` |
|       588 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       555 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       506 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       111 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|       241 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|     11137 |  366 | `	return FALSE;` |
|      5733 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|   1811818 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|   1811823 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       112 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       112 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        54 |  383 | `	}` |
|   1811823 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  10243377 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|   8431597 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      7585 |  388 | `			continue;` |
|         - |  389 | `		}` |
|   8424017 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|         - |  391 | `			/* A short-array literal is a SELF-CONTAINED node whose start token is '['` |
|         - |  392 | `			 * (its ']' was consumed), so the raw-token CSB test below can never see it —` |
|         - |  393 | ``			 * `[$obj, 'm']()` parsed the '(' as a grouping paren and silently DROPPED`` |
|         - |  394 | `			 * the call (the expression evaluated to the array). php invokes the literal` |
|         - |  395 | `			 * array callable exactly like the variable-held form. */` |
|    777920 |  396 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    104772 |  397 | `				apNode[i-1]->xCode == PH7_CompileShortArray \|\|` |
|    104754 |  398 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  399 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis.` |
|         - |  400 | `					 * A self-contained short-array node is exempt: its start token is '[',` |
|         - |  401 | `					 * which carries PH7_TK_OP as the subscript operator, but the node is a` |
|         - |  402 | ``					 * complete array-literal TERM — `[$obj, 'm'](...)` is a call. */`` |
|    600704 |  403 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0` |
|    300372 |  404 | `					 \|\| apNode[i-1]->xCode == PH7_CompileShortArray ){` |
|         - |  405 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  406 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  407 | `						 */` |
|    600697 |  408 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    600697 |  409 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    600697 |  410 | `						apNode[i]->pOp = &sFCallOp;` |
|    300346 |  411 | `					}` |
|    300352 |  412 | `			}` |
|    725543 |  413 | `			iParen++;` |
|   8061248 |  414 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|    725543 |  415 | `			if( iParen <= 0 ){` |
|        17 |  416 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        17 |  417 | `				if( rc != SXERR_ABORT ){` |
|        17 |  418 | `					rc = SXERR_SYNTAX;` |
|         7 |  419 | `				}` |
|        17 |  420 | `				return rc;` |
|         - |  421 | `			}` |
|    725529 |  422 | `			iParen--;` |
|   7335703 |  423 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    187717 |  424 | `			iSquare++;` |
|   6879085 |  425 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    187721 |  426 | `			if( iSquare <= 0 ){` |
|         8 |  427 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         8 |  428 | `				if( rc != SXERR_ABORT ){` |
|         8 |  429 | `					rc = SXERR_SYNTAX;` |
|         3 |  430 | `				}` |
|         8 |  431 | `				return rc;` |
|         - |  432 | `			}` |
|    187715 |  433 | `			iSquare--;` |
|   6691368 |  434 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|        72 |  435 | `			iBraces++;` |
|        72 |  436 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  437 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  438 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  439 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  440 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  441 | `				if( rc != SXERR_ABORT ){` |
|         3 |  442 | `					rc = SXERR_SYNTAX;` |
|         1 |  443 | `				}` |
|         3 |  444 | `				return rc;` |
|         2 |  445 | `			}` |
|   6597477 |  446 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|        84 |  447 | `			if( iBraces <= 0 ){` |
|        15 |  448 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        15 |  449 | `				if( rc != SXERR_ABORT ){` |
|        15 |  450 | `					rc = SXERR_SYNTAX;` |
|         6 |  451 | `				}` |
|        15 |  452 | `				return rc;` |
|         - |  453 | `			}` |
|        70 |  454 | `			iBraces--;` |
|   6597397 |  455 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     32075 |  456 | `			if( iQuesty > 0 ){` |
|     31485 |  457 | `				iQuesty--;` |
|     16335 |  458 | `			}else if( iParen <= 0 ){` |
|         - |  459 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  460 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  461 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  462 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  463 | `				if( rc != SXERR_ABORT ){` |
|         6 |  464 | `					rc = SXERR_SYNTAX;` |
|         2 |  465 | `				}` |
|         6 |  466 | `				return rc;` |
|         5 |  467 | `			}` |
|   6581326 |  468 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   2174233 |  469 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   2174233 |  470 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     31487 |  471 | `				iQuesty++;` |
|   2158492 |  472 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     19503 |  473 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|         9 |  474 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|         9 |  475 | `					sxu32 n = 0;` |
|         9 |  476 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|         5 |  477 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|         2 |  478 | `					}` |
|         - |  479 | `					/*` |
|         - |  480 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|         - |  481 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|         - |  482 | `					 */` |
|       213 |  483 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|       205 |  484 | `						++n;` |
|         1 |  485 | `					}` |
|         9 |  486 | `					pOp = &aOpTable[n];` |
|         - |  487 | `					/* Mark as binary '+' or '-',not an unary */` |
|         9 |  488 | `					apNode[i]->pOp = pOp;` |
|         9 |  489 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|         4 |  490 | `				}` |
|      9749 |  491 | `			}` |
|   1087114 |  492 | `		}` |
|   4211992 |  493 | `	}` |
|   1811785 |  494 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        18 |  495 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        18 |  496 | `		if( rc != SXERR_ABORT ){` |
|        18 |  497 | `			rc = SXERR_SYNTAX;` |
|         7 |  498 | `		}` |
|        18 |  499 | `		return rc;` |
|         - |  500 | `	}` |
|   1811771 |  501 | `	return SXRET_OK;` |
|    905914 |  502 | `}` |
|         - |  503 | `/*` |
|         - |  504 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  505 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  506 | ` */` |
|    916498 |  507 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  508 | `{` |
|    916503 |  509 | `	SyToken *pIn = *ppCur;` |
|         - |  510 | `	/* Jump the first literal seen */` |
|    916503 |  511 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|    916387 |  512 | `		pIn++;` |
|    458191 |  513 | `	}` |
|    458395 |  514 | `	for(;;){` |
|    916795 |  515 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       297 |  516 | `			pIn++;` |
|       297 |  517 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       295 |  518 | `				pIn++;` |
|       145 |  519 | `			}` |
|       151 |  520 | `		}else{` |
|    458254 |  521 | `			break;` |
|         - |  522 | `		}` |
|         5 |  523 | `	}` |
|         - |  524 | `	/* Synchronize pointers */` |
|    916503 |  525 | `	*ppCur = pIn;` |
|    916503 |  526 | `}` |
|         - |  527 | `/*` |
|         - |  528 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|         - |  529 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  530 | ` * Note on annonymous functions.` |
|         - |  531 | ` *  According to the PHP language reference manual:` |
|         - |  532 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  533 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  534 | ` *  parameters, but they have many other uses.` |
|         - |  535 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|         - |  536 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|         - |  537 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|         - |  538 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|         - |  539 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|         - |  540 | ` *` |
|         - |  541 | ` * Some example:` |
|         - |  542 | ` *  $greet = function($name)` |
|         - |  543 | ` * {` |
|         - |  544 | ` *   printf("Hello %s\r\n", $name);` |
|         - |  545 | ` * };` |
|         - |  546 | ` *  $greet('World');` |
|         - |  547 | ` *  $greet('PHP');` |
|         - |  548 | ` *` |
|         - |  549 | ` * $double = function($a) {` |
|         - |  550 | ` *   return $a * 2;` |
|         - |  551 | ` * };` |
|         - |  552 | ` * // This is our range of numbers` |
|         - |  553 | ` * $numbers = range(1, 5);` |
|         - |  554 | ` * // Use the Annonymous function as a callback here to` |
|         - |  555 | ` * // double the size of each element in our` |
|         - |  556 | ` * // range` |
|         - |  557 | ` * $new_numbers = array_map($double, $numbers);` |
|         - |  558 | ` * print implode(' ', $new_numbers);` |
|         - |  559 | ` */` |
|         - |  560 | `/*` |
|         - |  561 | ` * Skip an optional return-type declaration at *ppIn:` |
|         - |  562 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|         - |  563 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|         - |  564 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|         - |  565 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|         - |  566 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|         - |  567 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|         - |  568 | ` * authoritative type parse, so this must accept every shape it does` |
|         - |  569 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|         - |  570 | ` */` |
|      6522 |  571 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  572 | `{` |
|      6527 |  573 | `	SyToken *pIn = *ppIn;` |
|      6527 |  574 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        35 |  575 | `		pIn++; /* Skip ':' */` |
|        16 |  576 | `		for(;;){` |
|         - |  577 | `			/* Optional '?' nullable prefix */` |
|        39 |  578 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|         6 |  579 | `				pIn++;` |
|         2 |  580 | `			}` |
|        39 |  581 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  582 | `				/* Parenthesized DNF group '(A&B)' */` |
|       ! 0 |  583 | `				pIn++;` |
|       ! 0 |  584 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       ! 0 |  585 | `				if( pIn < pEnd ){` |
|       ! 0 |  586 | `					pIn++; /* ')' */` |
|       ! 0 |  587 | `				}` |
|        36 |  588 | `			}else if( pIn < pEnd` |
|        39 |  589 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  590 | `				/* ['\']Name('\'Name)* */` |
|        39 |  591 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|        39 |  592 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        39 |  593 | `					pIn++;` |
|        39 |  594 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  595 | `						pIn += 2;` |
|       ! 0 |  596 | `					}` |
|        18 |  597 | `				}` |
|        21 |  598 | `			}else{` |
|         - |  599 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  600 | `				break;` |
|         - |  601 | `			}` |
|         - |  602 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|        36 |  603 | `			if( pIn < pEnd` |
|        39 |  604 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|        36 |  605 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|         5 |  606 | `				pIn++;` |
|         5 |  607 | `				continue;` |
|         - |  608 | `			}` |
|        35 |  609 | `			break;` |
|       ! 0 |  610 | `		}` |
|        16 |  611 | `	}` |
|      6527 |  612 | `	*ppIn = pIn;` |
|      6527 |  613 | `}` |
|      2628 |  614 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  615 | `{` |
|      2633 |  616 | `	SyToken *pIn = *ppCur;` |
|         - |  617 | `	sxi32 rc;` |
|         - |  618 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  619 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  620 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  621 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  622 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|      2633 |  623 | `	pIn++;` |
|      2628 |  624 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      1343 |  625 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|        51 |  626 | `		pIn++;` |
|        23 |  627 | `	}` |
|      2633 |  628 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  629 | `		/* Syntax error */` |
|         6 |  630 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  631 | `		if( rc != SXERR_ABORT ){` |
|         6 |  632 | `			rc = SXERR_SYNTAX;` |
|         2 |  633 | `		}` |
|         6 |  634 | `		goto Synchronize;` |
|         - |  635 | `	}` |
|      2629 |  636 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      2629 |  637 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      2629 |  638 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  639 | `		/* Two different failures used to share this arm and both claimed the body was` |
|         - |  640 | `		 * missing. They are distinguishable: the delimiter search leaves pIn ON the` |
|         - |  641 | `		 * ')' when it found one, and AT pEnd when it did not.` |
|         - |  642 | ``		 *   pIn >= pEnd      the parameter list never closed (`function($x {`)`` |
|         - |  643 | `		 *                    -> php expects ')'` |
|         - |  644 | `		 *   &pIn[1] >= pEnd  ')' closed it but nothing follows -> php expects '{'` |
|         - |  645 | `		 * php names the token that actually comes next, which lives just past the` |
|         - |  646 | `		 * expression slice, still in the raw stream. */` |
|         6 |  647 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         6 |  648 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,pIn >= pEnd ? "\")\"" : "\"{\"");` |
|         6 |  649 | `		if( rc != SXERR_ABORT ){` |
|         6 |  650 | `			rc = SXERR_SYNTAX;` |
|         2 |  651 | `		}` |
|         6 |  652 | `		goto Synchronize;` |
|         - |  653 | `	}` |
|      2625 |  654 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  655 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      2625 |  656 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      2625 |  657 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       399 |  658 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  659 | `		/* Check if we are dealing with a closure */` |
|       399 |  660 | `		if( nKey == PH7_TKWRD_USE ){` |
|       391 |  661 | `			pIn++; /* Jump the 'use' keyword */` |
|       391 |  662 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  663 | `				/* Syntax error */` |
|         6 |  664 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  665 | `				if( rc != SXERR_ABORT ){` |
|         6 |  666 | `					rc = SXERR_SYNTAX;` |
|         2 |  667 | `				}` |
|         6 |  668 | `				goto Synchronize;` |
|         - |  669 | `			}` |
|       387 |  670 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|         - |  671 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|         - |  672 | `			 * has no nested structure to balance here, so the first token that is not` |
|         - |  673 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|         - |  674 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|         - |  675 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|         - |  676 | `			 * the first foreign token. */` |
|         - |  677 | `			{` |
|       387 |  678 | `				SyToken *pUse = pIn;` |
|       387 |  679 | `				int bClosed = 0;` |
|      1415 |  680 | `				while( pUse < pEnd ){` |
|      1415 |  681 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|      1035 |  682 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      1033 |  683 | `						pUse++;` |
|      1033 |  684 | `						continue;` |
|         - |  685 | `					}` |
|         3 |  686 | `					break; /* foreign token: php names this one */` |
|       ! 0 |  687 | `				}` |
|       387 |  688 | `				if( !bClosed ){` |
|         - |  689 | `					/* php names the offending token and expects ')'; if the list simply` |
|         - |  690 | `					 * ran off the end of the slice, that token sits just past it. */` |
|         3 |  691 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|         3 |  692 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|         3 |  693 | `					if( rc != SXERR_ABORT ){` |
|         3 |  694 | `						rc = SXERR_SYNTAX;` |
|         1 |  695 | `					}` |
|         3 |  696 | `					goto Synchronize;` |
|         - |  697 | `				}` |
|       385 |  698 | `				pIn = pUse; /* on the ')' */` |
|         - |  699 | `			}` |
|       385 |  700 | `			if( &pIn[1] >= pEnd ){` |
|         - |  701 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|         3 |  702 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         3 |  703 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         3 |  704 | `				if( rc != SXERR_ABORT ){` |
|         3 |  705 | `					rc = SXERR_SYNTAX;` |
|         1 |  706 | `				}` |
|         3 |  707 | `				goto Synchronize;` |
|         - |  708 | `			}` |
|       383 |  709 | `			pIn++;` |
|         - |  710 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  711 | ``			 * `function (...) use (...) : int {` */`` |
|       383 |  712 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       194 |  713 | `		}else{` |
|         - |  714 | `			/* Syntax error */` |
|        11 |  715 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        11 |  716 | `			if( rc != SXERR_ABORT ){` |
|        11 |  717 | `				rc = SXERR_SYNTAX;` |
|         4 |  718 | `			}` |
|        11 |  719 | `			goto Synchronize;` |
|         - |  720 | `		}` |
|       189 |  721 | `	}` |
|         - |  722 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  723 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  724 | `	 * the type), and pEnd is one past the last token. */` |
|      2609 |  725 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      2609 |  726 | `		pIn++; /* Jump the leading curly '{' */` |
|      2609 |  727 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      2609 |  728 | `		if( pIn < pEnd ){` |
|      2609 |  729 | `			pIn++;` |
|      1302 |  730 | `		}` |
|      1307 |  731 | `	}else{` |
|         - |  732 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|         - |  733 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|         - |  734 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|         - |  735 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|       ! 0 |  736 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|       ! 0 |  737 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|       ! 0 |  738 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  739 | `			return SXERR_ABORT;` |
|         - |  740 | `		}` |
|         - |  741 | `	}` |
|      2609 |  742 | `	rc = SXRET_OK;` |
|      1314 |  743 | `Synchronize:` |
|         - |  744 | `	/* Synchronize pointers */` |
|      2633 |  745 | `	*ppCur = pIn;` |
|      2633 |  746 | `	return rc;` |
|      1319 |  747 | `}` |
|         - |  748 | `/*` |
|         - |  749 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  750 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  751 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  752 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  753 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  754 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  755 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  756 | ` */` |
|        74 |  757 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  758 | `{` |
|        79 |  759 | `	SyToken *pIn = *ppCur;` |
|        79 |  760 | `	sxu32 nLine = pIn->nLine;` |
|         - |  761 | `	sxi32 rc;` |
|        79 |  762 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  763 | `	/* Optional constructor argument list */` |
|        79 |  764 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        16 |  765 | `		pIn++; /* Jump '(' */` |
|        16 |  766 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        16 |  767 | `		if( pIn < pEnd ){` |
|        16 |  768 | `			pIn++; /* Jump ')' */` |
|         7 |  769 | `		}` |
|         7 |  770 | `	}` |
|         - |  771 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  772 | `	 * (no braces appear between ')' and the class body). */` |
|       201 |  773 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       127 |  774 | `		pIn++;` |
|         5 |  775 | `	}` |
|        79 |  776 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  777 | `		/* Syntax error: missing class body */` |
|       ! 0 |  778 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  779 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  780 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  781 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  782 | `		}` |
|       ! 0 |  783 | `		*ppCur = pIn;` |
|       ! 0 |  784 | `		return rc;` |
|         - |  785 | `	}` |
|        79 |  786 | `	pIn++; /* Jump the leading '{' */` |
|        79 |  787 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        79 |  788 | `	if( pIn < pEnd ){` |
|        79 |  789 | `		pIn++; /* Jump the trailing '}' */` |
|        37 |  790 | `	}` |
|        79 |  791 | `	*ppCur = pIn;` |
|        79 |  792 | `	return SXRET_OK;` |
|        42 |  793 | `}` |
|         - |  794 | `/*` |
|         - |  795 | ` * TRUE when a KEYWORD token actually OPENS an arrow function.` |
|         - |  796 | ` *` |
|         - |  797 | `` * `fn` is reserved, but it only ever introduces `[static] fn[&](…) => expr`.`` |
|         - |  798 | ``  * Everywhere php expects a NAME the same word is an ordinary identifier: `$fn` `` |
|         - |  799 | ` * (the lexer emits '$' plus the keyword, so the keyword IS the variable name),` |
|         - |  800 | ``  * `$fn(…)` calling that variable, `C::fn`, `$o->fn`, `\A\fn`, and the `fn:` `` |
|         - |  801 | ` * named-argument label. Every raw-token lookahead that steps over an arrow` |
|         - |  802 | ` * function has to make that distinction or it swallows a plain name and loses` |
|         - |  803 | `` * the '=>' that follows it (`[$fn => 1]` became `syntax error, unexpected token`` |
|         - |  804 | `` * "=>"`).`` |
|         - |  805 | ` *` |
|         - |  806 | `` * The test is POSITIONAL, never "is it well formed": a malformed `fn` (`fn $x`` |
|         - |  807 | `` * => $x`, or a bare `fn` used as a key) must still reach the arrow parser,`` |
|         - |  808 | `` * which is what reports php's `expecting "("`. Two name positions:`` |
|         - |  809 | ` *   - member/variable/namespace: '$', '->', '?->', '::' or '\' immediately` |
|         - |  810 | ` *     before the word;` |
|         - |  811 | ``  *   - a named-argument LABEL: a bare `fn` directly before ':' (`static fn:` `` |
|         - |  812 | `` *     and `fn&:` cannot be labels, so they stay the arrow parser's business).`` |
|         - |  813 | ` *     The argument list is re-parsed from the argument's own first token, so` |
|         - |  814 | ` *     there is no '(' to look back at — the label test cannot be scoped to` |
|         - |  815 | `` *     call context, and the degenerate `true ? fn : 0` (only reachable through`` |
|         - |  816 | ` *     define('fn',…), which php itself cannot parse) is accepted as a` |
|         - |  817 | ` *     constant instead of rejected. A recorded divergence; rejecting it` |
|         - |  818 | `` *     would cost the real `f(fn: 1)` spelling.`` |
|         - |  819 | `` * pStart bounds the look-back; pTok may point at `static`, which must then be`` |
|         - |  820 | `` * followed by `fn`.`` |
|         - |  821 | ` */` |
|      4050 |  822 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd)` |
|         5 |  823 | `{` |
|      4055 |  824 | `	int bStatic = FALSE;` |
|      4055 |  825 | `	if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  826 | `		return FALSE;` |
|         - |  827 | `	}` |
|      4055 |  828 | `	if( pTok > pStart ){` |
|       248 |  829 | `		SyToken *pPrev = &pTok[-1];` |
|       248 |  830 | `		if( pPrev->nType & (PH7_TK_DOLLAR\|PH7_TK_NSSEP) ){` |
|        53 |  831 | `			return FALSE; /* $fn / \A\fn — the keyword IS the name */` |
|         - |  832 | `		}` |
|       198 |  833 | `		if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData ){` |
|       104 |  834 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)pPrev->pUserData;` |
|       100 |  835 | `			if( pOp->iOp == EXPR_OP_ARROW \|\| pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|        97 |  836 | `			 \|\| pOp->iOp == EXPR_OP_DC ){` |
|        48 |  837 | `				return FALSE; /* $o->fn, $o?->fn, C::fn — a member name */` |
|         - |  838 | `			}` |
|        28 |  839 | `		}` |
|        75 |  840 | `	}` |
|      3961 |  841 | `	if( SX_PTR_TO_INT(pTok->pUserData) == PH7_TKWRD_STATIC ){` |
|       134 |  842 | `		bStatic = TRUE;` |
|       134 |  843 | `		pTok++;` |
|       134 |  844 | `		if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|        96 |  845 | `			return FALSE;` |
|         - |  846 | `		}` |
|        19 |  847 | `	}` |
|      3869 |  848 | `	if( SX_PTR_TO_INT(pTok->pUserData) != PH7_TKWRD_FN ){` |
|       216 |  849 | `		return FALSE;` |
|         - |  850 | `	}` |
|      3657 |  851 | `	if( !bStatic && &pTok[1] < pEnd && (pTok[1].nType & PH7_TK_COLON) ){` |
|         3 |  852 | `		return FALSE; /* f(fn: 1) — a named-argument label */` |
|         - |  853 | `	}` |
|      3655 |  854 | `	return TRUE;` |
|      2030 |  855 | `}` |
|         - |  856 | `/*` |
|         - |  857 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  858 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  859 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  860 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  861 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  862 | ` */` |
|      3524 |  863 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  864 | `{` |
|      3529 |  865 | `	SyToken *pIn = *ppCur;` |
|         - |  866 | `	sxu32 nLine;` |
|         - |  867 | `	sxi32 rc;` |
|         - |  868 | `	int iNest;` |
|      3529 |  869 | `	nLine = pIn->nLine;` |
|         - |  870 | `	/* Optional 'static' prefix */` |
|      3524 |  871 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      3529 |  872 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        37 |  873 | `		pIn++;` |
|        18 |  874 | `	}` |
|         - |  875 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      3524 |  876 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      3529 |  877 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  878 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  879 | `		goto Synchronize;` |
|         - |  880 | `	}` |
|      3529 |  881 | `	pIn++; /* Jump 'fn' */` |
|      1762 |  882 | `	SXUNUSED(nLine);` |
|      1762 |  883 | `	SXUNUSED(pGen);` |
|         - |  884 | `	/* Optional '&' for return-by-reference */` |
|      3529 |  885 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  886 | `		pIn++;` |
|       ! 0 |  887 | `	}` |
|         - |  888 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  889 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  890 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  891 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      3529 |  892 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      3527 |  893 | `		pIn++; /* '(' */` |
|      3527 |  894 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      3527 |  895 | `		if( pIn < pEnd ){` |
|      3525 |  896 | `			pIn++; /* ')' */` |
|      1760 |  897 | `		}` |
|      1761 |  898 | `	}` |
|         - |  899 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      3529 |  900 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  901 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      3529 |  902 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      3523 |  903 | `		pIn++;` |
|      1759 |  904 | `	}` |
|         - |  905 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      3529 |  906 | `	iNest = 0;` |
|     35231 |  907 | `	while( pIn < pEnd ){` |
|     34823 |  908 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  909 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      3121 |  910 | `			break;` |
|         - |  911 | `		}` |
|     31707 |  912 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      6063 |  913 | `			iNest++;` |
|     28678 |  914 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      6063 |  915 | `			iNest--;` |
|      3029 |  916 | `		}` |
|     31707 |  917 | `		pIn++;` |
|         5 |  918 | `	}` |
|      3529 |  919 | `	rc = SXRET_OK;` |
|      1762 |  920 | `Synchronize:` |
|      3529 |  921 | `	*ppCur = pIn;` |
|      3529 |  922 | `	return rc;` |
|         5 |  923 | `}` |
|         - |  924 | `/*` |
|         - |  925 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - |  926 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - |  927 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - |  928 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - |  929 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - |  930 | ` */` |
|       112 |  931 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  932 | `{` |
|       117 |  933 | `	SyToken *pIn = *ppCur;` |
|         - |  934 | `	sxi32 rc;` |
|        56 |  935 | `	SXUNUSED(pGen);` |
|         - |  936 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       112 |  937 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       117 |  938 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 |  939 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  940 | `		goto Synchronize;` |
|         - |  941 | `	}` |
|       117 |  942 | `	pIn++; /* Jump 'match' */` |
|         - |  943 | `	/* Optional '(' subject ')' */` |
|       117 |  944 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       117 |  945 | `		pIn++;` |
|       117 |  946 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       117 |  947 | `		if( pIn < pEnd ){` |
|       117 |  948 | `			pIn++; /* ')' */` |
|        56 |  949 | `		}` |
|        56 |  950 | `	}` |
|         - |  951 | `	/* Optional '{' arms '}' */` |
|       117 |  952 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       117 |  953 | `		pIn++;` |
|       117 |  954 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       117 |  955 | `		if( pIn < pEnd ){` |
|       117 |  956 | `			pIn++; /* '}' */` |
|        56 |  957 | `		}` |
|        56 |  958 | `	}` |
|       117 |  959 | `	rc = SXRET_OK;` |
|        56 |  960 | `Synchronize:` |
|       117 |  961 | `	*ppCur = pIn;` |
|       117 |  962 | `	return rc;` |
|         5 |  963 | `}` |
|         - |  964 | `/*` |
|         - |  965 | `` * PHP 8.5 `clone (`: tell the clone() CALL form from the clone OPERATOR applied to a`` |
|         - |  966 | ` * parenthesised operand. php's grammar has both, and its parser resolves the conflict` |
|         - |  967 | ` * by continuing the parenthesised expression whenever the token after the ')' can` |
|         - |  968 | ``  * dereference it — so `clone ($a)->b()` clones what `b()` returns, `clone ($c)[0]` `` |
|         - |  969 | `` * clones the ELEMENT and `clone ($f)()` clones the call's result, while a plain`` |
|         - |  970 | `` * `clone ($a)` (nothing dereferencing) is the one-argument call, which means the same`` |
|         - |  971 | `` * thing either way. PHL took the call form for every `clone (`, so the receiver was`` |
|         - |  972 | ` * cloned and the member access ran on the ORIGINAL — a silent wrong answer with no` |
|         - |  973 | `` * diagnostic, out of `clone (new A)->b()`.`` |
|         - |  974 | ` *` |
|         - |  975 | ` * pClone points at the 'clone' token and pClone[1] at its '('. Returns TRUE when the` |
|         - |  976 | ` * call-form branch should take the tokens (including the unterminated case, which that` |
|         - |  977 | ` * branch reports), FALSE to leave them to the precedence-1 operator path.` |
|         - |  978 | ` */` |
|        66 |  979 | `static int CloneCallFormFollows(SyToken *pClone,SyToken *pEnd)` |
|         2 |  980 | `{` |
|        68 |  981 | `	SyToken *pNext = &pClone[2]; /* first token inside the '(' */` |
|        68 |  982 | `	PH7_DelimitNestedTokens(pNext,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pNext);` |
|        68 |  983 | `	if( pNext >= pEnd ){` |
|       ! 0 |  984 | `		return TRUE; /* unterminated '(' — the call-form branch raises php's ')' error */` |
|         - |  985 | `	}` |
|        68 |  986 | `	pNext++; /* step past the matching ')' */` |
|        68 |  987 | `	if( pNext >= pEnd ){` |
|        50 |  988 | `		return TRUE;` |
|         - |  989 | `	}` |
|        19 |  990 | `	if( pNext->nType & (PH7_TK_OSB /*'['*/\|PH7_TK_LPAREN /*'('*/) ){` |
|         5 |  991 | `		return FALSE;` |
|         - |  992 | `	}` |
|        15 |  993 | `	if( (pNext->nType & PH7_TK_OP) && pNext->pUserData ){` |
|         9 |  994 | `		sxi32 iOp = ((const ph7_expr_op *)pNext->pUserData)->iOp;` |
|         9 |  995 | `		if( iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW \|\| iOp == EXPR_OP_DC ){` |
|         9 |  996 | `			return FALSE;` |
|         - |  997 | `		}` |
|       ! 0 |  998 | `	}` |
|         7 |  999 | `	return TRUE;` |
|        35 | 1000 | `}` |
|         - | 1001 | `/*` |
|         - | 1002 | ` * Extract a single expression node from the input.` |
|         - | 1003 | ` * On success store the freshly extractd node in ppNode.` |
|         - | 1004 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1005 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - | 1006 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - | 1007 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - | 1008 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - | 1009 | ` */` |
|   8432096 | 1010 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 | 1011 | `{` |
|         - | 1012 | `	ph7_expr_node *pNode;` |
|         - | 1013 | `	SyToken *pCur;` |
|         - | 1014 | `	sxi32 rc;` |
|         - | 1015 | `	/* Allocate a new node */` |
|   8432101 | 1016 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|   8432101 | 1017 | `	if( pNode == 0 ){` |
|         - | 1018 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1019 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1020 | `		 */` |
|       ! 0 | 1021 | `		return SXERR_MEM;` |
|         - | 1022 | `	}` |
|         - | 1023 | `	/* Zero the structure */` |
|   8432101 | 1024 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|   8432101 | 1025 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - | 1026 | `	/* Point to the head of the token stream */` |
|   8432101 | 1027 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - | 1028 | `	/* Start collecting tokens */` |
|   8432101 | 1029 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       583 | 1030 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - | 1031 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - | 1032 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - | 1033 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - | 1034 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       247 | 1035 | `			pNode->pEnd = pCur;` |
|       247 | 1036 | `			pCur++;` |
|       247 | 1037 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       247 | 1038 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       247 | 1039 | `			pGen->pIn = pCur;` |
|       247 | 1040 | `			*ppNode = pNode;` |
|       247 | 1041 | `			return SXRET_OK;` |
|         - | 1042 | `		}` |
|         - | 1043 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - | 1044 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       339 | 1045 | `		pCur++;` |
|       339 | 1046 | `		pGen->pIn = pCur;` |
|       339 | 1047 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       339 | 1048 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       339 | 1049 | `		if( rc == SXRET_OK && *ppNode ){` |
|       339 | 1050 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       168 | 1051 | `		}` |
|       339 | 1052 | `		return rc;` |
|         - | 1053 | `	}` |
|   8431523 | 1054 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - | 1055 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - | 1056 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - | 1057 | `		 */` |
|      7587 | 1058 | `		pCur++; /* Skip the opening '[' */` |
|      7587 | 1059 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      7587 | 1060 | `		if( pCur < pGen->pEnd ){` |
|      7587 | 1061 | `			pCur++; /* Skip past the closing ']' */` |
|      3796 | 1062 | `		}else{` |
|       ! 0 | 1063 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1064 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 | 1065 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 1066 | `				rc = SXERR_SYNTAX;` |
|       ! 0 | 1067 | `			}` |
|       ! 0 | 1068 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1069 | `			return rc;` |
|         - | 1070 | `		}` |
|         - | 1071 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - | 1072 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - | 1073 | `		 */` |
|      8171 | 1074 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      1173 | 1075 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      1173 | 1076 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       122 | 1077 | `				pNode->xCode = PH7_CompileShortList;` |
|        63 | 1078 | `			}else{` |
|      1055 | 1079 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - | 1080 | `			}` |
|       589 | 1081 | `		}else{` |
|      6419 | 1082 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 | 1083 | `		}` |
|   8427732 | 1084 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - | 1085 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - | 1086 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - | 1087 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - | 1088 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - | 1089 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - | 1090 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - | 1091 | `		 * call, not the clone() intrinsic. */` |
|        25 | 1092 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        25 | 1093 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        25 | 1094 | `		pNode->xCode = PH7_CompileLiteral;` |
|   8423925 | 1095 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   2362032 | 1096 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   1181121 | 1097 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN)` |
|       143 | 1098 | `		&& CloneCallFormFollows(pCur,pGen->pEnd) ){` |
|         - | 1099 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - | 1100 | ``		 * `clone` is a real internal FUNCTION in php 8.5, so this spelling is an`` |
|         - | 1101 | `		 * ordinary call — and every property the call machinery owns comes with` |
|         - | 1102 | ``		 * it: named arguments, spread, the first-class-callable `clone(...)`, and`` |
|         - | 1103 | `		 * the runtime ArgumentCountError/TypeError php raises for a degenerate` |
|         - | 1104 | ``		 * argument list (PHL used to refuse `clone()` and a three-argument call at`` |
|         - | 1105 | ``		 * COMPILE time, and had no FCC form at all). `clone` is an alpha-stream`` |
|         - | 1106 | `` 		 * operator token, so `clone(` is not auto-marked as a call the way `foo(` `` |
|         - | 1107 | `		 * is: clear PH7_TK_OP and leave a plain name TERM behind, and the postfix` |
|         - | 1108 | `		 * pass then binds the '(' to it. The bare operator/statement form` |
|         - | 1109 | ``		 * `clone $obj` (no immediately-following '(') keeps the precedence-1`` |
|         - | 1110 | `		 * operator path below. */` |
|        56 | 1111 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        56 | 1112 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        56 | 1113 | `		pNode->xCode = PH7_CompileLiteral;` |
|   8423890 | 1114 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1115 | `		/* Point to the instance that describe this operator */` |
|   2361983 | 1116 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1117 | `		/* Advance the stream cursor */` |
|   2361983 | 1118 | `		pCur++;` |
|   7242874 | 1119 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1120 | `		/* Isolate variable */` |
|   4712735 | 1121 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   2356389 | 1122 | `			pCur++; /* Variable variable */` |
|         5 | 1123 | `		}` |
|   2356351 | 1124 | `		if( pCur < pGen->pEnd ){` |
|   2356351 | 1125 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1126 | `				/* Variable name */` |
|   2356313 | 1127 | `				pCur++;` |
|   1178197 | 1128 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        35 | 1129 | `				pCur++;` |
|         - | 1130 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        35 | 1131 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        35 | 1132 | `				if( pCur < pGen->pEnd ){` |
|        33 | 1133 | `					pCur++;` |
|        19 | 1134 | `				}else{` |
|         - | 1135 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|         - | 1136 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|         - | 1137 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|         - | 1138 | `					 * delimiter search stops at the slice end, so the token php names` |
|         - | 1139 | `					 * usually sits just past it, still inside the chunk stream. */` |
|         - | 1140 | `					{` |
|         3 | 1141 | `						SyToken *pBad = 0;` |
|         3 | 1142 | `						if( pGen->pTokenSet ){` |
|         3 | 1143 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         3 | 1144 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         3 | 1145 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|       ! 0 | 1146 | `								pBad = pCur;` |
|       ! 0 | 1147 | `							}` |
|         1 | 1148 | `						}` |
|         3 | 1149 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|         - | 1150 | `					}` |
|         3 | 1151 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1152 | `						rc = SXERR_SYNTAX;` |
|         1 | 1153 | `					}` |
|         3 | 1154 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1155 | `					return rc;` |
|         - | 1156 | `				}` |
|        19 | 1157 | `			}else{` |
|         - | 1158 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|         - | 1159 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|         - | 1160 | `				 * covered only the '$' and the offending token drifted into a later` |
|         - | 1161 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|         - | 1162 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|        11 | 1163 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|        11 | 1164 | `				if( rc != SXERR_ABORT ){` |
|        11 | 1165 | `					rc = SXERR_SYNTAX;` |
|         4 | 1166 | `				}` |
|        11 | 1167 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        11 | 1168 | `				return rc;` |
|         - | 1169 | `			}` |
|   1178168 | 1170 | `		}` |
|   2356341 | 1171 | `		pNode->xCode = PH7_CompileVariable;` |
|   4883707 | 1172 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|     80709 | 1173 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     80709 | 1174 | `		 if( bAfterMemberOp ){` |
|         - | 1175 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1176 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1177 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1178 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|         - | 1179 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|         - | 1180 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|         - | 1181 | `			  * the word itself. */` |
|       471 | 1182 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|       471 | 1183 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       471 | 1184 | `			 pNode->xCode = PH7_CompileLiteral;` |
|     80476 | 1185 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1186 | `			 /* List/Array node */` |
|     62611 | 1187 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1188 | `				 /* Assume a literal */` |
|         9 | 1189 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|         9 | 1190 | `				 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1191 | `			 }else{` |
|     62603 | 1192 | `				 pCur += 2;` |
|         - | 1193 | `				 /* Collect array/list tokens */` |
|     62603 | 1194 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     62603 | 1195 | `				 if( pCur < pGen->pEnd ){` |
|     62601 | 1196 | `					 pCur++;` |
|     31303 | 1197 | `				 }else{` |
|         - | 1198 | `					 /* Syntax error */` |
|         - | 1199 | `					 /* php names the token it stopped on and says it expected ")". */` |
|         3 | 1200 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|         3 | 1201 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1202 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1203 | `					 }` |
|         3 | 1204 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1205 | `					 return rc;` |
|         - | 1206 | `				 }` |
|     62601 | 1207 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     62601 | 1208 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        47 | 1209 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        47 | 1210 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1211 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|         - | 1212 | ``						  * not the `list` the construct started at. */`` |
|         3 | 1213 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|         3 | 1214 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1215 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1216 | `						 }` |
|         3 | 1217 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1218 | `						 return rc;` |
|         - | 1219 | `					 }` |
|        20 | 1220 | `				 }` |
|         5 | 1221 | `			 }` |
|     48938 | 1222 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1223 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       481 | 1224 | `			 pCur++; /* Skip 'yield' keyword */` |
|       481 | 1225 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1226 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1227 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       481 | 1228 | `			 pNode->xCode = PH7_CompileYield;` |
|     17399 | 1229 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     15911 | 1230 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       166 | 1231 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       123 | 1232 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1233 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|      2633 | 1234 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1235 | `				 /* Assume a literal */` |
|       ! 0 | 1236 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1237 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1238 | `			 }else{` |
|         - | 1239 | `				 /* Assemble annonymous functions body */` |
|      2633 | 1240 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      2633 | 1241 | `				 if( rc != SXRET_OK ){` |
|        28 | 1242 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1243 | `					 return rc;` |
|         - | 1244 | `				 }` |
|      2609 | 1245 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1246 | `			  }` |
|     15835 | 1247 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|        90 | 1248 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        58 | 1249 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        36 | 1250 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        24 | 1251 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1252 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1253 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1254 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1255 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|        79 | 1256 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|        79 | 1257 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1258 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1259 | `				 return rc;` |
|         - | 1260 | `			 }` |
|        79 | 1261 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     14496 | 1262 | `		 }else if( (nKeyword == PH7_TKWRD_FN \|\| nKeyword == PH7_TKWRD_STATIC)` |
|      9038 | 1263 | `			&& PH7_TokenOpensArrowFunc(pGen->pIn,pCur,pGen->pEnd) ){` |
|         - | 1264 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      3529 | 1265 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      3529 | 1266 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1267 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1268 | `				 return rc;` |
|         - | 1269 | `			 }` |
|      3529 | 1270 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     12697 | 1271 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1272 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       117 | 1273 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       117 | 1274 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1275 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1276 | `				 return rc;` |
|         - | 1277 | `			 }` |
|       117 | 1278 | `			 pNode->xCode = PH7_CompileMatch;` |
|     10879 | 1279 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1280 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1281 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1282 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        43 | 1283 | `			 pCur++; /* Skip 'throw' */` |
|        43 | 1284 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1285 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1286 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        43 | 1287 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     10803 | 1288 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1289 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       129 | 1290 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       129 | 1291 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        67 | 1292 | `		 }else{` |
|         - | 1293 | `			 /* Assume a literal */` |
|     10659 | 1294 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     10659 | 1295 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1296 | `		 }` |
|   3665173 | 1297 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1298 | `		 /* Constants,function name,namespace path,class name... */` |
|    905297 | 1299 | `		 if( bAfterMemberOp ){` |
|         - | 1300 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|         - | 1301 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|         - | 1302 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|         - | 1303 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|     28815 | 1304 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     14405 | 1305 | `		 }` |
|    905297 | 1306 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    905297 | 1307 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    452651 | 1308 | `	 }else{` |
|   2719543 | 1309 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1310 | `			 /* Point to the code generator routine */` |
|   1048507 | 1311 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   1048507 | 1312 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1313 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 1314 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1315 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1316 | `				 }` |
|         3 | 1317 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1318 | `				 return rc;` |
|         - | 1319 | `			 }` |
|    524250 | 1320 | `		 }` |
|         - | 1321 | `		/* Advance the stream cursor */` |
|   2719541 | 1322 | `		pCur++;` |
|         - | 1323 | `	 }` |
|         - | 1324 | `	/* Point to the end of the token stream */` |
|   8431483 | 1325 | `	pNode->pEnd = pCur;` |
|         - | 1326 | `	/* Save the node for later processing */` |
|   8431483 | 1327 | `	*ppNode = pNode;` |
|         - | 1328 | `	/* Synchronize cursors */` |
|   8431483 | 1329 | `	pGen->pIn = pCur;` |
|   8431483 | 1330 | `	return SXRET_OK;` |
|   4216053 | 1331 | `}` |
|         - | 1332 | `/*` |
|         - | 1333 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1334 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1335 | ` * level is zero.` |
|         - | 1336 | ` */` |
|    283402 | 1337 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1338 | `{` |
|    283407 | 1339 | `	SyToken *pCur = pStart;` |
|    283407 | 1340 | `	sxi32 iNest = 0;` |
|    283407 | 1341 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1342 | `		/* Last expression */` |
|     96763 | 1343 | `		return SXERR_EOF;` |
|         - | 1344 | `	}` |
|    539835 | 1345 | `	while( pCur < pEnd ){` |
|    519083 | 1346 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    165897 | 1347 | `			break;` |
|         - | 1348 | `		}` |
|    353191 | 1349 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     25535 | 1350 | `			iNest++;` |
|    340426 | 1351 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     25539 | 1352 | `			iNest--;` |
|     12767 | 1353 | `		}` |
|    353191 | 1354 | `		pCur++;` |
|         5 | 1355 | `	}` |
|    186649 | 1356 | `	*ppNext = pCur;` |
|    186649 | 1357 | `	return SXRET_OK;` |
|    141706 | 1358 | `}` |
|         - | 1359 | `/*` |
|         - | 1360 | ` * Free an expression tree.` |
|         - | 1361 | ` */` |
|   7352206 | 1362 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1363 | `{` |
|   7352211 | 1364 | `	if( pNode->pLeft ){` |
|         - | 1365 | `		/* Release the left tree */` |
|   2702799 | 1366 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   1351397 | 1367 | `	}` |
|   7352211 | 1368 | `	if( pNode->pRight ){` |
|         - | 1369 | `		/* Release the right tree */` |
|   1567731 | 1370 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|    783863 | 1371 | `	}` |
|   7352211 | 1372 | `	if( pNode->pCond ){` |
|         - | 1373 | `		/* Release the conditional tree used by the ternary operator */` |
|     31483 | 1374 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     15739 | 1375 | `	}` |
|   7352211 | 1376 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1377 | `		ph7_expr_node **apArg;` |
|         - | 1378 | `		sxu32 n;` |
|         - | 1379 | `		/* Release node arguments */` |
|    727445 | 1380 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   1714491 | 1381 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|    987051 | 1382 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|    493528 | 1383 | `		}` |
|    727445 | 1384 | `		SySetRelease(&pNode->aNodeArgs);` |
|    363720 | 1385 | `	}` |
|         - | 1386 | `	/* Finally,release this node */` |
|   7352211 | 1387 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|   7352211 | 1388 | `}` |
|         - | 1389 | `/*` |
|         - | 1390 | ` * Free an expression tree.` |
|         - | 1391 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1392 | ` */` |
|   1811854 | 1393 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1394 | `{` |
|         - | 1395 | `	ph7_expr_node **apNode;` |
|         - | 1396 | `	sxu32 n;` |
|   1811859 | 1397 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  10243549 | 1398 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|   8431695 | 1399 | `		if( apNode[n] ){` |
|   1812195 | 1400 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|    906095 | 1401 | `		}` |
|   4215850 | 1402 | `	}` |
|   1811859 | 1403 | `	return SXRET_OK;` |
|         5 | 1404 | `}` |
|         - | 1405 | `/*` |
|         - | 1406 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1407 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1408 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1409 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1410 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1411 | ` */` |
|   2132258 | 1412 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1413 | `{` |
|   2132263 | 1414 | `	if( pNode == 0 ){` |
|   1384081 | 1415 | `		return 0;` |
|         - | 1416 | `	}` |
|    748187 | 1417 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1418 | `		return 1;` |
|         - | 1419 | `	}` |
|    748175 | 1420 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1421 | `		return 1;` |
|         - | 1422 | `	}` |
|    748171 | 1423 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1424 | `		return 1;` |
|         - | 1425 | `	}` |
|    748171 | 1426 | `	return 0;` |
|   1066134 | 1427 | `}` |
|         - | 1428 | `/*` |
|         - | 1429 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1430 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1431 | ` */` |
|    635834 | 1432 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1433 | `{` |
|         - | 1434 | `	sxi32 iExprOp;` |
|    635839 | 1435 | `	if( pNode->pOp == 0 ){` |
|    525483 | 1436 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1437 | `	}` |
|    110361 | 1438 | `	iExprOp = pNode->pOp->iOp;` |
|    110361 | 1439 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|      1267 | 1440 | `			return TRUE;` |
|         - | 1441 | `	}` |
|    109099 | 1442 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    109083 | 1443 | `		if( pNode->pLeft->pOp ) {` |
|       218 | 1444 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        72 | 1445 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1446 | `				return FALSE;` |
|         5 | 1447 | `			}` |
|    108974 | 1448 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1449 | `			return FALSE;` |
|         - | 1450 | `		}` |
|    109083 | 1451 | `		return TRUE;` |
|         - | 1452 | `	}` |
|        19 | 1453 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        11 | 1454 | `		return TRUE;` |
|         - | 1455 | `	}` |
|         - | 1456 | `	/* Not a modifiable l or r-value */` |
|         9 | 1457 | `	return FALSE;` |
|    317922 | 1458 | `}` |
|         - | 1459 | `/* Forward declaration */` |
|         - | 1460 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1461 | `/* Macro to check if the given node is a terminal.` |
|         - | 1462 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1463 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1464 | ` * linked ternary/elvis node). */` |
|         - | 1465 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1466 | `/*` |
|         - | 1467 | ` * Buid an expression tree for each given function argument.` |
|         - | 1468 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1469 | ` */` |
|    568444 | 1470 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1471 | `{` |
|         - | 1472 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1473 | `	sxi32 rc;` |
|         - | 1474 | ``	/* php: a stray token in a call argument is `... expecting ")"`. Each arg's`` |
|         - | 1475 | `	 * tree is built by the shared ExprMakeTree below, whose leftover-node error` |
|         - | 1476 | `	 * reads this. Saved/restored so a nested call or array element inside an arg` |
|         - | 1477 | `	 * gets its own closer. */` |
|    568449 | 1478 | `	const char *zSaveArg = pGen->zClauseCloser;` |
|    568449 | 1479 | `	pGen->zClauseCloser = "\")\"";` |
|         - | 1480 | `	/* Process function arguments from left to right */` |
|    568449 | 1481 | `	iCur = 0;` |
|    698234 | 1482 | `	for(;;){` |
|   1396473 | 1483 | `		if( iCur >= nToken ){` |
|         - | 1484 | `			/* No more arguments to process */` |
|    568421 | 1485 | `			break;` |
|         - | 1486 | `		}` |
|    828057 | 1487 | `		iNode = iCur;` |
|    828057 | 1488 | `		iNest = 0;` |
|   2135733 | 1489 | `		while( iCur < nToken ){` |
|   1567315 | 1490 | `			if( apNode[iCur] ){` |
|   1565289 | 1491 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    129822 | 1492 | `					break;` |
|   1305650 | 1493 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|    692327 | 1494 | `					&& apNode[iCur]->pLeft == 0` |
|     78995 | 1495 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|     77017 | 1496 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1497 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1498 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1499 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1500 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1501 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1502 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|         - | 1503 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|         - | 1504 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|         - | 1505 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|         - | 1506 | `					 * and swallow the following argument separator. */` |
|     75043 | 1507 | `					iNest++;` |
|   1268136 | 1508 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|    652830 | 1509 | `					&& apNode[iCur]->pLeft == 0 ){` |
|     75043 | 1510 | `					iNest--;` |
|     37519 | 1511 | `				}` |
|    652825 | 1512 | `			}` |
|   1307681 | 1513 | `			iCur++;` |
|         5 | 1514 | `		}` |
|    828057 | 1515 | `		if( iCur > iNode ){` |
|    828051 | 1516 | `			SyString sArgName = {0, 0};` |
|         - | 1517 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1518 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1519 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|    828046 | 1520 | `			if( (iCur - iNode) >= 2` |
|    483247 | 1521 | `				&& apNode[iNode]` |
|    138295 | 1522 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     93846 | 1523 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     48893 | 1524 | `				&& apNode[iNode+1]` |
|     48241 | 1525 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1526 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       591 | 1527 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       591 | 1528 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       591 | 1529 | `				apNode[iNode] = 0;` |
|       591 | 1530 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       591 | 1531 | `				apNode[iNode+1] = 0;` |
|       591 | 1532 | `				iNode += 2;` |
|         - | 1533 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1534 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       591 | 1535 | `				if( iNode >= iCur ){` |
|         4 | 1536 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1537 | `						pOp->pStart->nLine,` |
|         - | 1538 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1539 | `						&sArgName);` |
|         3 | 1540 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1541 | `						rc = SXERR_SYNTAX;` |
|         1 | 1542 | `					}` |
|         3 | 1543 | `					pGen->zClauseCloser = zSaveArg;` |
|         3 | 1544 | `					return rc;` |
|         - | 1545 | `				}` |
|       292 | 1546 | `			}` |
|    828044 | 1547 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1548 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1549 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1550 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1551 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1552 | `					apNode[iNode] = 0;` |
|       ! 0 | 1553 | `			}` |
|         - | 1554 | `			{` |
|         - | 1555 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1556 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1557 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1558 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1559 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1560 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1561 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1562 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1563 | `				 * NULL slots ahead of the flagged subtree. */` |
|    828049 | 1564 | `				int bSpreadArg = 0;` |
|         - | 1565 | `				sxi32 iScan;` |
|    828355 | 1566 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|    828355 | 1567 | `					if( apNode[iScan] ){` |
|    828049 | 1568 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|    828049 | 1569 | `						break;` |
|         - | 1570 | `					}` |
|       158 | 1571 | `				}` |
|    828049 | 1572 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|    828049 | 1573 | `				if( bSpreadArg && apNode[iNode] ){` |
|       265 | 1574 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       131 | 1575 | `				}` |
|         - | 1576 | `			}` |
|    828049 | 1577 | `			if( apNode[iNode] ){` |
|    828049 | 1578 | `				if( sArgName.nByte > 0 ){` |
|       589 | 1579 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       589 | 1580 | `					apNode[iNode]->sArgName = sArgName;` |
|       292 | 1581 | `				}` |
|         - | 1582 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|    828049 | 1583 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    414027 | 1584 | `			}else{` |
|         - | 1585 | `				/* No expression before comma */` |
|       ! 0 | 1586 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1587 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1588 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1589 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1590 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1591 | `				}` |
|       ! 0 | 1592 | `				pGen->zClauseCloser = zSaveArg;` |
|       ! 0 | 1593 | `				return rc;` |
|         - | 1594 | `			}` |
|    414027 | 1595 | `		}else{` |
|         - | 1596 | `			/* Comma with no preceding argument */` |
|         9 | 1597 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         9 | 1598 | `			if( rc != SXERR_ABORT ){` |
|         9 | 1599 | `				rc = SXERR_SYNTAX;` |
|         3 | 1600 | `			}` |
|         9 | 1601 | `			pGen->zClauseCloser = zSaveArg;` |
|         9 | 1602 | `			return rc;` |
|         - | 1603 | `		}` |
|         - | 1604 | `		/* Jump trailing comma */` |
|    828049 | 1605 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    259633 | 1606 | `			iCur++;` |
|    259633 | 1607 | `			if( iCur >= nToken ){` |
|         - | 1608 | `				/* Trailing comma after last argument */` |
|        21 | 1609 | `				break;` |
|         - | 1610 | `			}` |
|    129804 | 1611 | `		}` |
|         5 | 1612 | `	}` |
|    568441 | 1613 | `	pGen->zClauseCloser = zSaveArg;` |
|    568441 | 1614 | `	return SXRET_OK;` |
|    284227 | 1615 | `}` |
|         - | 1616 | ` /*` |
|         - | 1617 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|         - | 1618 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|         - | 1619 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|         - | 1620 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|         - | 1621 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|         - | 1622 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|         - | 1623 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|         - | 1624 | `  */` |
|        74 | 1625 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|         4 | 1626 | ` {` |
|         - | 1627 | `	 SyToken *pMin;` |
|         - | 1628 | `	 SyToken *pChild;` |
|        78 | 1629 | `	 if( pNode == 0 ){` |
|        50 | 1630 | `		 return 0;` |
|         - | 1631 | `	 }` |
|        32 | 1632 | `	 pMin = pNode->pStart;` |
|        32 | 1633 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|        32 | 1634 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|         6 | 1635 | `		 pMin = pChild;` |
|         2 | 1636 | `	 }` |
|        32 | 1637 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|        32 | 1638 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|       ! 0 | 1639 | `		 pMin = pChild;` |
|       ! 0 | 1640 | `	 }` |
|        32 | 1641 | `	 return pMin;` |
|        41 | 1642 | ` }` |
|         - | 1643 | `/*` |
|         - | 1644 | ` * The full RAW token extent of a linked subtree: minimum pStart / maximum pEnd` |
|         - | 1645 | ` * over every node (pLeft/pRight/pCond and postfix aNodeArgs children), widened` |
|         - | 1646 | ` * by one token on each side for a node whose group parens were consumed by` |
|         - | 1647 | ` * ExprMakeTree's paren pass (EXPR_NODE_PARENS — its '('/')' slots were nulled,` |
|         - | 1648 | ` * but token contiguity guarantees they sit exactly one token outside the inner` |
|         - | 1649 | ` * extent). Tokens live in one contiguous set, so pointer min/max IS source` |
|         - | 1650 | ` * order. Consumed by the assert() source-text capture, which` |
|         - | 1651 | ` * needs the argument's whole source span where a root's own pStart/pEnd name` |
|         - | 1652 | `` * only the operator token. Note: nested redundant groups `((x))` share the`` |
|         - | 1653 | ` * single PARENS bit, so only one paren layer is recovered — the renderer` |
|         - | 1654 | ` * strips redundant outermost parens anyway, matching php's export.` |
|         - | 1655 | ` */` |
|       536 | 1656 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax)` |
|         5 | 1657 | `{` |
|         - | 1658 | `	SyToken *pMin;` |
|         - | 1659 | `	SyToken *pMax;` |
|       541 | 1660 | `	SyToken *pCMin = 0;` |
|       541 | 1661 | `	SyToken *pCMax = 0;` |
|         - | 1662 | `	ph7_expr_node **apArg;` |
|         - | 1663 | `	sxu32 n;` |
|       541 | 1664 | `	if( pNode == 0 ){` |
|       385 | 1665 | `		return;` |
|         - | 1666 | `	}` |
|       161 | 1667 | `	pMin = pNode->pStart;` |
|       161 | 1668 | `	pMax = pNode->pEnd;` |
|       161 | 1669 | `	PH7_ExprSubtreeSpan(pNode->pLeft,&pCMin,&pCMax);` |
|       161 | 1670 | `	PH7_ExprSubtreeSpan(pNode->pRight,&pCMin,&pCMax);` |
|       161 | 1671 | `	PH7_ExprSubtreeSpan(pNode->pCond,&pCMin,&pCMax);` |
|       161 | 1672 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       167 | 1673 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|         7 | 1674 | `		PH7_ExprSubtreeSpan(apArg[n],&pCMin,&pCMax);` |
|         4 | 1675 | `	}` |
|       161 | 1676 | `	if( pCMin && (pMin == 0 \|\| pCMin < pMin) ){` |
|        46 | 1677 | `		pMin = pCMin;` |
|        22 | 1678 | `	}` |
|       161 | 1679 | `	if( pCMax && (pMax == 0 \|\| pCMax > pMax) ){` |
|        50 | 1680 | `		pMax = pCMax;` |
|        24 | 1681 | `	}` |
|       161 | 1682 | `	if( (pNode->iFlags & EXPR_NODE_PARENS) && pMin && pMax ){` |
|         5 | 1683 | `		pMin--;` |
|         5 | 1684 | `		pMax++;` |
|         2 | 1685 | `	}` |
|       156 | 1686 | `	if( pNode->pOp && pMax` |
|        53 | 1687 | `	 && (pNode->pOp->iOp == EXPR_OP_FUNC_CALL \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT) ){` |
|         - | 1688 | `		/* A postfix call/subscript's extent stops AT its closing ')' / ']' (the` |
|         - | 1689 | `		 * closer's node was consumed building the postfix op); token contiguity` |
|         - | 1690 | `		 * puts the closer exactly at the extent, so widen one token past it. */` |
|         5 | 1691 | `		pMax++;` |
|         2 | 1692 | `	}` |
|       161 | 1693 | `	if( pMin && (*ppMin == 0 \|\| pMin < *ppMin) ){` |
|       117 | 1694 | `		*ppMin = pMin;` |
|        56 | 1695 | `	}` |
|       161 | 1696 | `	if( pMax && (*ppMax == 0 \|\| pMax > *ppMax) ){` |
|       159 | 1697 | `		*ppMax = pMax;` |
|        77 | 1698 | `	}` |
|       273 | 1699 | `}` |
|         - | 1700 | ` /*` |
|         - | 1701 | `  * Create an expression tree from an array of tokens.` |
|         - | 1702 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1703 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1704 | `  */` |
|   3019740 | 1705 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1706 | ` {` |
|         - | 1707 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1708 | `	 ph7_expr_node *pNode;` |
|         - | 1709 | `	 ph7_expr_node *pSuppress;` |
|         - | 1710 | `	 sxi32 iCur;` |
|         - | 1711 | `	 sxi32 rc;` |
|   3019745 | 1712 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1713 | `		 /* TICKET 1433-17: self evaluating node */` |
|   1401639 | 1714 | `		 return SXRET_OK;` |
|         - | 1715 | `	 }` |
|         - | 1716 | `	 /* Process expressions enclosed in parenthesis first */` |
|  10161101 | 1717 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1718 | `		 sxi32 iNest;` |
|         - | 1719 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1720 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1721 | `		  */` |
|   8542997 | 1722 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|   8418163 | 1723 | `			 continue;` |
|         - | 1724 | `		 }` |
|    124839 | 1725 | `		 iNest = 1;` |
|    124839 | 1726 | `		 iLeft = iCur;` |
|         - | 1727 | `		 /* Find the closing parenthesis */` |
|    124839 | 1728 | `		 iCur++;` |
|   1031695 | 1729 | `		 while( iCur < nToken ){` |
|   1031695 | 1730 | `			 if( apNode[iCur] ){` |
|   1031695 | 1731 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1732 | `					 /* Decrement nesting level */` |
|    215737 | 1733 | `					 iNest--;` |
|    215737 | 1734 | `					 if( iNest <= 0 ){` |
|    124839 | 1735 | `						 break;` |
|         5 | 1736 | `					 }` |
|    861412 | 1737 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1738 | `					 /* Increment nesting level */` |
|     90903 | 1739 | `					 iNest++;` |
|     45449 | 1740 | `				 }` |
|    453428 | 1741 | `			 }` |
|    906861 | 1742 | `			 iCur++;` |
|         5 | 1743 | `		 }` |
|    124839 | 1744 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1745 | `			 sxi32 j;` |
|         - | 1746 | `			 /* Recurse and process this expression */` |
|    124839 | 1747 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    124839 | 1748 | `			 if( rc != SXRET_OK ){` |
|         3 | 1749 | `				 return rc;` |
|         - | 1750 | `			 }` |
|         - | 1751 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1752 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1753 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1754 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1755 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1756 | `			  * group's free below silently drops the unpacking. */` |
|    124837 | 1757 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    124837 | 1758 | `				 if( apNode[j] ){` |
|    124837 | 1759 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    124832 | 1760 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    124837 | 1761 | `					 break;` |
|         - | 1762 | `				 }` |
|       ! 0 | 1763 | `			 }` |
|     62416 | 1764 | `		 }` |
|         - | 1765 | `		 /* Free the left and right nodes */` |
|    124837 | 1766 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    124837 | 1767 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    124837 | 1768 | `		 apNode[iLeft] = 0;` |
|    124837 | 1769 | `		 apNode[iCur] = 0;` |
|     62421 | 1770 | `	 }` |
|         - | 1771 | `	  /* Process expressions enclosed in braces */` |
|  11126357 | 1772 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1773 | `		 sxi32 iNest;` |
|         - | 1774 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1775 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1776 | `		  */` |
|   9574541 | 1777 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|   9574473 | 1778 | `			 continue;` |
|         - | 1779 | `		 }` |
|        70 | 1780 | `		 iNest = 1;` |
|        70 | 1781 | `		 iLeft = iCur;` |
|         - | 1782 | `		 /* Find the closing parenthesis */` |
|        70 | 1783 | `		 iCur++;` |
|       136 | 1784 | `		 while( iCur < nToken ){` |
|       136 | 1785 | `			 if( apNode[iCur] ){` |
|       136 | 1786 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1787 | `					 /* Decrement nesting level */` |
|        70 | 1788 | `					 iNest--;` |
|        70 | 1789 | `					 if( iNest <= 0 ){` |
|        70 | 1790 | `						 break;` |
|       ! 0 | 1791 | `					 }` |
|        67 | 1792 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1793 | `					 /* Increment nesting level */` |
|       ! 0 | 1794 | `					 iNest++;` |
|       ! 0 | 1795 | `				 }` |
|        33 | 1796 | `			 }` |
|        67 | 1797 | `			 iCur++;` |
|         1 | 1798 | `		 }` |
|        70 | 1799 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1800 | `			 /* Recurse and process this expression */` |
|        67 | 1801 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        67 | 1802 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1803 | `				 return rc;` |
|         - | 1804 | `			 }` |
|        33 | 1805 | `		 }` |
|         - | 1806 | `		 /* Free the left and right nodes */` |
|        70 | 1807 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        70 | 1808 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        70 | 1809 | `		 apNode[iLeft] = 0;` |
|        70 | 1810 | `		 apNode[iCur] = 0;` |
|        36 | 1811 | `	 }` |
|         - | 1812 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   1551821 | 1813 | `	 iLeft = -1;` |
|  11126451 | 1814 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   9574649 | 1815 | `		 if( apNode[iCur] == 0 ){` |
|   3685681 | 1816 | `			 continue;` |
|         - | 1817 | `		 }` |
|   5888973 | 1818 | `		 pNode = apNode[iCur];` |
|   5888973 | 1819 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|    818057 | 1820 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1821 | `				 /* Collect function arguments */` |
|    600693 | 1822 | `				 sxi32 iPtr = 0;` |
|    600693 | 1823 | `				 sxi32 nFuncTok = 0;` |
|   2768693 | 1824 | `				 while( nFuncTok + iCur < nToken ){` |
|   2768693 | 1825 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|         - | 1826 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|         - | 1827 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|         - | 1828 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|         - | 1829 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|         - | 1830 | `					  * nulled, so counting it here would over-count and never find` |
|         - | 1831 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   2768693 | 1832 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   2766333 | 1833 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|    655859 | 1834 | `							 iPtr++;` |
|   2438406 | 1835 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|    655859 | 1836 | `							 iPtr--;` |
|    655859 | 1837 | `							 if( iPtr <= 0 ){` |
|    600693 | 1838 | `								 break;` |
|         - | 1839 | `							 }` |
|     27583 | 1840 | `						 }` |
|   1082820 | 1841 | `					 }` |
|   2168005 | 1842 | `					 nFuncTok++;` |
|         5 | 1843 | `				 }` |
|    600693 | 1844 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1845 | `					 /* Syntax error */` |
|       ! 0 | 1846 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1847 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1848 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1849 | `					 }` |
|       ! 0 | 1850 | `					 return rc;` |
|         - | 1851 | `				 }` |
|    600693 | 1852 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1853 | `					 /* Syntax error */` |
|       ! 0 | 1854 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1855 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1856 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1857 | `					 }` |
|       ! 0 | 1858 | `					 return rc;` |
|         - | 1859 | `				 }` |
|    600693 | 1860 | `				 if( nFuncTok > 1 ){` |
|         - | 1861 | `					 /* Process function arguments */` |
|    568449 | 1862 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    568449 | 1863 | `					 if( rc != SXRET_OK ){` |
|        12 | 1864 | `						 return rc;` |
|         - | 1865 | `					 }` |
|    284218 | 1866 | `				 }` |
|         - | 1867 | `				 /* Link the node to the tree */` |
|    600685 | 1868 | `				 pNode->pLeft = apNode[iLeft];` |
|    600685 | 1869 | `				 apNode[iLeft] = 0;` |
|   2768661 | 1870 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   2167981 | 1871 | `					 apNode[iCur+iPtr] = 0;` |
|   1083993 | 1872 | `				 }` |
|         - | 1873 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1874 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1875 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1876 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1877 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1878 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1879 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1880 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1881 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1882 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1883 | `				 {` |
|    600685 | 1884 | `					 sxi32 iNew = iLeft - 1;` |
|    633547 | 1885 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     32867 | 1886 | `						 iNew--;` |
|         5 | 1887 | `					 }` |
|    600680 | 1888 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    375186 | 1889 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    219477 | 1890 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     66139 | 1891 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     66139 | 1892 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     66139 | 1893 | `						 apNode[iNew] = 0;` |
|     66139 | 1894 | `						 pNode = apNode[iCur];` |
|     33072 | 1895 | `					 }` |
|         - | 1896 | `				 }` |
|    517709 | 1897 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1898 | `				 /* Subscripting */` |
|    187715 | 1899 | `				 sxi32 iArrTok = iCur + 1;` |
|    187715 | 1900 | `				 sxi32 iNest = 1;` |
|    187710 | 1901 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        60 | 1902 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        54 | 1903 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         - | 1904 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|         - | 1905 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|         - | 1906 | `					  * subscripting a global constant raised "Invalid array name" while` |
|         - | 1907 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|         - | 1908 | `					  * worked. */` |
|        48 | 1909 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        40 | 1910 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    187710 | 1911 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1912 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1913 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|       497 | 1914 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1915 | `						 /* Syntax error */` |
|       ! 0 | 1916 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1917 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1918 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1919 | `						 }` |
|       ! 0 | 1920 | `						 return rc;` |
|         - | 1921 | `				 }` |
|         - | 1922 | `				 /* Collect index tokens */` |
|    365673 | 1923 | `				 while( iArrTok < nToken ){` |
|    365673 | 1924 | `					 if( apNode[iArrTok] ){` |
|    365641 | 1925 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1926 | `							 /* Increment nesting level */` |
|       ! 0 | 1927 | `							 iNest++;` |
|    365641 | 1928 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1929 | `							 /* Decrement nesting level */` |
|    187715 | 1930 | `							 iNest--;` |
|    187715 | 1931 | `							 if( iNest <= 0 ){` |
|    187715 | 1932 | `								 break;` |
|         - | 1933 | `							 }` |
|       ! 0 | 1934 | `						 }` |
|     88963 | 1935 | `					 }` |
|    177963 | 1936 | `					 ++iArrTok;` |
|         5 | 1937 | `				 }` |
|    187715 | 1938 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1939 | ``					 /* php: a stray token in a subscript index is `... expecting "]"`. */`` |
|    159007 | 1940 | `					 const char *zSaveIdx = pGen->zClauseCloser;` |
|    159007 | 1941 | `					 pGen->zClauseCloser = "\"]\"";` |
|         - | 1942 | `					 /* Recurse and process this expression */` |
|    159007 | 1943 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    159007 | 1944 | `					 pGen->zClauseCloser = zSaveIdx;` |
|    159007 | 1945 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1946 | `						 return rc;` |
|         - | 1947 | `					 }` |
|         - | 1948 | `					 /* Link the node to it's index */` |
|    159007 | 1949 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|     79501 | 1950 | `				 }` |
|         - | 1951 | `				 /* Link the node to the tree */` |
|    187715 | 1952 | `				 pNode->pLeft = apNode[iLeft];` |
|    187715 | 1953 | `				 pNode->pRight = 0;` |
|    187715 | 1954 | `				 apNode[iLeft] = 0;` |
|    553383 | 1955 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    365673 | 1956 | `					 apNode[iNest] = 0;` |
|    182839 | 1957 | `				 }` |
|     93860 | 1958 | `			 }else{` |
|         - | 1959 | `				 /* Member access operators [i.e: '->','::'] */` |
|     29659 | 1960 | `				  iRight = iCur + 1;` |
|     29725 | 1961 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        67 | 1962 | `					 iRight++;` |
|         1 | 1963 | `				 }` |
|     29659 | 1964 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1965 | `					 /* Syntax error */` |
|         5 | 1966 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1967 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1968 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1969 | `					 }` |
|         5 | 1970 | `					 return rc;` |
|         - | 1971 | `				 }` |
|         - | 1972 | `				 /* Validate the left operand BEFORE linking it. A node that is both` |
|         - | 1973 | `				  * still in apNode[] and already reachable as pNode->pLeft is freed` |
|         - | 1974 | `				  * TWICE by PH7_ExprFreeTree on the error path — a heap-use-after-free` |
|         - | 1975 | ``				  * that `1->x;`, `"s"->x;` and `[1]->x;` all reached. Ownership moves`` |
|         - | 1976 | `				  * out of the set only once the link is certain. */` |
|     29650 | 1977 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|     28511 | 1978 | `					 && apNode[iLeft]->pOp == 0 &&` |
|     25428 | 1979 | `					 apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         - | 1980 | ``					 /* A PARENTHESISED group is php's `( expr )` dereferencable: whatever it`` |
|         - | 1981 | ``					  * evaluates to may be reached through `->`, which is how the closure`` |
|         - | 1982 | ``					  * idioms are written — `(function(){ … })->bindTo($o)`,`` |
|         - | 1983 | ``					  * `(fn() => …)->call($o)`, `(match($k){ … })->m()`. PHL refused all of`` |
|         - | 1984 | `					  * them as "Expecting a variable as left operand", a compile fatal on` |
|         - | 1985 | `					  * valid php, because a literal TERM carries no operator. */` |
|        34 | 1986 | `					 (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 &&` |
|         - | 1987 | ``					 /* php's `dereferencable` also covers the SCALAR forms — a quoted`` |
|         - | 1988 | `					  * string (interpolated or not) and an array literal — plus a` |
|         - | 1989 | ``					  * CONSTANT, and it runs them: `"s"->p` warns `Attempt to read`` |
|         - | 1990 | ``					  * property "p" on string` and yields null, `"s"->m()` is the`` |
|         - | 1991 | `					  * member-function Error. Refusing them at COMPILE time killed the` |
|         - | 1992 | `					  * whole file instead. A NUMBER literal and a heredoc stay refused` |
|         - | 1993 | ``					  * — those are php's own parse error for `1->x`. */`` |
|        22 | 1994 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString &&` |
|        20 | 1995 | `					 apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1996 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        11 | 1997 | `					 apNode[iLeft]->xCode != PH7_CompileArray &&` |
|         4 | 1998 | `					 apNode[iLeft]->xCode != PH7_CompileShortArray ){` |
|         - | 1999 | `						 /* Syntax error */` |
|         4 | 2000 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2001 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|         3 | 2002 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2003 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2004 | `						 }` |
|         3 | 2005 | `						 return rc;` |
|         - | 2006 | `				 }` |
|         - | 2007 | `				 /* Link the node to the tree */` |
|     29653 | 2008 | `				 pNode->pLeft = apNode[iLeft];` |
|     29653 | 2009 | `				 pNode->pRight = apNode[iRight];` |
|     29653 | 2010 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 2011 | `			 }` |
|    409019 | 2012 | `		 }` |
|   5888959 | 2013 | `		 iLeft = iCur;` |
|   2944482 | 2014 | `	 }` |
|         - | 2015 | `	 /* Handle the prefix (new, clone) operators. Walk RIGHT to LEFT: both take the` |
|         - | 2016 | `	  * operand on their right, so a nested one has to be linked before the outer` |
|         - | 2017 | ``	  * sees it. Left-to-right, `clone new Q` reached the still-unlinked `new` node —`` |
|         - | 2018 | `	  * not a term yet — and answered php's own valid source with the compile fatal` |
|         - | 2019 | ``	  * "'clone': Expecting class constructor call". `clone new Q()` worked only`` |
|         - | 2020 | `	  * because the postfix pass folds a constructor CALL into its new-node early. */` |
|  11126415 | 2021 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|   9574613 | 2022 | `		 if( apNode[iCur] == 0 ){` |
|   4569853 | 2023 | `			 continue;` |
|         - | 2024 | `		 }` |
|   5004765 | 2025 | `		 pNode = apNode[iCur];` |
|   5004765 | 2026 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 2027 | `			 SyToken *pToken;` |
|         - | 2028 | `			 /* Get the left node */` |
|      1667 | 2029 | `			 iLeft = iCur + 1;` |
|      1765 | 2030 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|        99 | 2031 | `				 iLeft++;` |
|         1 | 2032 | `			 }` |
|      1667 | 2033 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2034 | `				  /* Syntax error */` |
|       ! 0 | 2035 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 2036 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 2037 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2038 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2039 | `				 }` |
|       ! 0 | 2040 | `				 return rc;` |
|         - | 2041 | `			 }` |
|         - | 2042 | `			 /* Make sure the operand are of a valid type. CLONE takes ANY expression —` |
|         - | 2043 | ``			  * php's grammar is `clone expr`, and what that expression evaluates to is a`` |
|         - | 2044 | `			  * RUNTIME question: a non-object operand is php's catchable` |
|         - | 2045 | ``			  * `clone(): Argument #1 ($object) must be of type object, %s given`, which`` |
|         - | 2046 | `			  * OP_CLONE already raises. The whitelist that used to sit here (a variable,` |
|         - | 2047 | ``			  * or any operator node) refused `clone 5`, `clone []`, `clone null` and`` |
|         - | 2048 | ``			  * `clone match(…){…}` at COMPILE time — the first three with a diagnostic php`` |
|         - | 2049 | `			  * never prints, the last on source php runs. NEW keeps its own, because its` |
|         - | 2050 | `			  * operand is a class-name REFERENCE, not a value. */` |
|      1667 | 2051 | `			 if( pNode->pOp->iOp != EXPR_OP_CLONE ){` |
|         - | 2052 | `				 /* New */` |
|      1506 | 2053 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 2054 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 2055 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 2056 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 2057 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 2058 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 2059 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 2060 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 2061 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 2062 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2063 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2064 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2065 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2066 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2067 | `					 }` |
|       ! 0 | 2068 | `					 return rc;` |
|         - | 2069 | `				 }` |
|      1511 | 2070 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      1511 | 2071 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      1506 | 2072 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        79 | 2073 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 2074 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 2075 | `						 /* Syntax error */` |
|       ! 0 | 2076 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2077 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2078 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2079 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2080 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2081 | `						 }` |
|       ! 0 | 2082 | `						 return rc;` |
|         - | 2083 | `					 }` |
|       753 | 2084 | `				 }` |
|       753 | 2085 | `			 }` |
|         - | 2086 | `			  /* Link the node to the tree */` |
|      1667 | 2087 | `			 pNode->pLeft = apNode[iLeft];` |
|      1667 | 2088 | `			 apNode[iLeft] = 0;` |
|      1667 | 2089 | `			 pNode->pRight = 0; /* Paranoid */` |
|       831 | 2090 | `		 }` |
|   2502385 | 2091 | `	 }` |
|         - | 2092 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   1551807 | 2093 | `	 iLeft = -1;` |
|  11126415 | 2094 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   9541469 | 2095 | `		 if( apNode[iCur] == 0 ){` |
|   4571515 | 2096 | `			 continue;` |
|         - | 2097 | `		 }` |
|   4969959 | 2098 | `		 pNode = apNode[iCur];` |
|   4969959 | 2099 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     75877 | 2100 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     37942 | 2101 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 2102 | `					 /* Link the node to the tree */` |
|     38009 | 2103 | `					 pNode->pLeft = apNode[iLeft];` |
|     38009 | 2104 | `					 apNode[iLeft] = 0;` |
|     19002 | 2105 | `			 }` |
|     92370 | 2106 | `		  }` |
|   5003103 | 2107 | `		 iLeft = iCur;` |
|   2501554 | 2108 | `	  }` |
|   1584951 | 2109 | `	 iLeft = -1;` |
|  11159559 | 2110 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|   9574613 | 2111 | `		 if( apNode[iCur] == 0 ){` |
|   4609519 | 2112 | `			 continue;` |
|         - | 2113 | `		 }` |
|   4965099 | 2114 | `		 pNode = apNode[iCur];` |
|   4965099 | 2115 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      4724 | 2116 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      4729 | 2117 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 2118 | `					 /* Syntax error */` |
|       ! 0 | 2119 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 2120 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2121 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2122 | `					 }` |
|       ! 0 | 2123 | `					 return rc;` |
|         - | 2124 | `			 }` |
|         - | 2125 | `			 /* Link the node to the tree */` |
|      4729 | 2126 | `			 pNode->pLeft = apNode[iLeft];` |
|      4729 | 2127 | `			 apNode[iLeft] = 0;` |
|         - | 2128 | `			 /* Mark as pre-increment/decrement node */` |
|      4729 | 2129 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      2362 | 2130 | `		  }` |
|   4965099 | 2131 | `		 iLeft = iCur;` |
|   2482552 | 2132 | `	 }` |
|         - | 2133 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|         - | 2134 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|         - | 2135 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|         - | 2136 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|         - | 2137 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|         - | 2138 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|         - | 2139 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|         - | 2140 | `	  * pass below skips it (pLeft != 0). */` |
|   1584951 | 2141 | `	 iLeft = -1;` |
|  11159559 | 2142 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   9574613 | 2143 | `		 if( apNode[iCur] == 0 ){` |
|   4623983 | 2144 | `			 continue;` |
|         - | 2145 | `		 }` |
|   4950635 | 2146 | `		 pNode = apNode[iCur];` |
|   4950635 | 2147 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|      9745 | 2148 | `			 iRight = iCur + 1;` |
|      9745 | 2149 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       ! 0 | 2150 | `				 iRight++;` |
|       ! 0 | 2151 | `			 }` |
|      9745 | 2152 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|       ! 0 | 2153 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2154 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2155 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2156 | `				 }` |
|       ! 0 | 2157 | `				 return rc;` |
|         - | 2158 | `			 }` |
|      9745 | 2159 | `			 pNode->pLeft = apNode[iLeft];` |
|      9745 | 2160 | `			 pNode->pRight = apNode[iRight];` |
|      9745 | 2161 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      4870 | 2162 | `		 }` |
|   4950635 | 2163 | `		 iLeft = iCur;` |
|   2475320 | 2164 | `	 }` |
|         - | 2165 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   1584951 | 2166 | `	  iLeft = 0;` |
|  11159553 | 2167 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|   9574609 | 2168 | `		  if( apNode[iCur] ){` |
|   4940891 | 2169 | `			  pNode = apNode[iCur];` |
|   4940891 | 2170 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    236235 | 2171 | `				  if( iLeft > 0 ){` |
|         - | 2172 | `					  /* Link the node to the tree */` |
|    236233 | 2173 | `					  pNode->pLeft = apNode[iLeft];` |
|    236233 | 2174 | `					  apNode[iLeft] = 0;` |
|    236233 | 2175 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       123 | 2176 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 2177 | `							   /* Syntax error */` |
|       ! 0 | 2178 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2179 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2180 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 2181 | `							  }` |
|       ! 0 | 2182 | `							  return rc;` |
|         - | 2183 | `						  }` |
|        59 | 2184 | `					  }` |
|    118119 | 2185 | `				  }else{` |
|         - | 2186 | `					  /* Syntax error */` |
|         3 | 2187 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 2188 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 2189 | `						  rc = SXERR_SYNTAX;` |
|         1 | 2190 | `					  }` |
|         3 | 2191 | `					  return rc;` |
|         - | 2192 | `				  }` |
|    118114 | 2193 | `			  }` |
|         - | 2194 | `			  /* Save terminal position */` |
|   4940889 | 2195 | `			  iLeft = iCur;` |
|   2470442 | 2196 | `		  }` |
|   4787306 | 2197 | `	  }` |
|         - | 2198 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 2199 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 2200 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 2201 | `	  * yielding a right-leaning tree. */` |
|  11159551 | 2202 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|   9574607 | 2203 | `		 if( apNode[iCur] == 0 ){` |
|   4870075 | 2204 | `			 continue;` |
|         - | 2205 | `		 }` |
|   4704537 | 2206 | `		 pNode = apNode[iCur];` |
|   4704537 | 2207 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 2208 | `			 sxi32 iL, iR;` |
|         - | 2209 | `			 /* Find the right operand */` |
|       125 | 2210 | `			 iR = -1;` |
|         - | 2211 | `			 {` |
|         - | 2212 | `				 sxi32 j;` |
|       137 | 2213 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       137 | 2214 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 2215 | `				 }` |
|         - | 2216 | `			 }` |
|         - | 2217 | `			 /* Find the left operand */` |
|       125 | 2218 | `			 iL = -1;` |
|         - | 2219 | `			 {` |
|         - | 2220 | `				 sxi32 j;` |
|       193 | 2221 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       193 | 2222 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 2223 | `				 }` |
|         - | 2224 | `			 }` |
|       125 | 2225 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 2226 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2227 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2228 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2229 | `				 }` |
|       ! 0 | 2230 | `				 return rc;` |
|         - | 2231 | `			 }` |
|       125 | 2232 | `			 pNode->pLeft  = apNode[iL];` |
|       125 | 2233 | `			 pNode->pRight = apNode[iR];` |
|       125 | 2234 | `			 apNode[iL] = 0;` |
|       125 | 2235 | `			 apNode[iR] = 0;` |
|         - | 2236 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 2237 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 2238 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 2239 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 2240 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 2241 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 2242 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 2243 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 2244 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 2245 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 2246 | `			  * operands are respected. */` |
|       124 | 2247 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        80 | 2248 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 2249 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 2250 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 2251 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 2252 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 2253 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 2254 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 2255 | `				 while( pTail->pLeft` |
|        34 | 2256 | `					 && pTail->pLeft->pOp` |
|        23 | 2257 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 2258 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 2259 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 2260 | `					 pTail = pTail->pLeft;` |
|         1 | 2261 | `				 }` |
|         - | 2262 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 2263 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 2264 | `				 pTail->pLeft = pNode;` |
|        27 | 2265 | `				 apNode[iCur] = pHead;` |
|        13 | 2266 | `			 }` |
|        62 | 2267 | `		 }` |
|   2352271 | 2268 | `	 }` |
|         - | 2269 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  17434303 | 2270 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  15849369 | 2271 | `		 iLeft = -1;` |
| 111595095 | 2272 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  95745741 | 2273 | `			 if( apNode[iCur] == 0 ){` |
|  58728207 | 2274 | `				 continue;` |
|         - | 2275 | `			 }` |
|  37017539 | 2276 | `			 pNode = apNode[iCur];` |
|  37017539 | 2277 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2278 | `				 /* Get the right node */` |
|    861233 | 2279 | `				 iRight = iCur + 1;` |
|   1109451 | 2280 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    248223 | 2281 | `					 iRight++;` |
|         5 | 2282 | `				 }` |
|    861233 | 2283 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2284 | `					 /* Syntax error */` |
|        10 | 2285 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        10 | 2286 | `					 if( rc != SXERR_ABORT ){` |
|        10 | 2287 | `						 rc = SXERR_SYNTAX;` |
|         4 | 2288 | `					 }` |
|        10 | 2289 | `					 return rc;` |
|         - | 2290 | `				 }` |
|    861225 | 2291 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2292 | `					 sxi32  iTmp;` |
|         - | 2293 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 2294 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 2295 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 2296 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 2297 | `					  * is swapped below. */` |
|       207 | 2298 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 2299 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2300 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 2301 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2302 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2303 | `						 }` |
|         3 | 2304 | `						 return rc;` |
|         - | 2305 | `					 }` |
|         - | 2306 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|         - | 2307 | `					  * reference target — ExprIsModifiableValue already accepts` |
|         - | 2308 | ``					  * EXPR_OP_ARROW (`->`) and EXPR_OP_DC (`::`) and rejects the`` |
|         - | 2309 | ``					  * nullsafe `?->` form (not in its l-value list), so no extra`` |
|         - | 2310 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|         - | 2311 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|       205 | 2312 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|         - | 2313 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 2314 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 2315 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2316 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2317 | `						 }` |
|       ! 0 | 2318 | `						 return rc;` |
|         - | 2319 | `					 }` |
|       205 | 2320 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       153 | 2321 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 2322 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 2323 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 2324 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2325 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 2326 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2327 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 2328 | `									 }` |
|       ! 0 | 2329 | `									 return rc;` |
|         - | 2330 | `							 }` |
|       ! 0 | 2331 | `						 }` |
|        74 | 2332 | `					 }` |
|         - | 2333 | `					 /* Swap operands */` |
|       205 | 2334 | `					 iTmp = iRight;` |
|       205 | 2335 | `					 iRight = iLeft;` |
|       205 | 2336 | `					 iLeft = iTmp;` |
|       100 | 2337 | `				 }` |
|         - | 2338 | `				 /* Link the node to the tree */` |
|    861223 | 2339 | `				 pNode->pLeft = apNode[iLeft];` |
|    861223 | 2340 | `				 pNode->pRight = apNode[iRight];` |
|    861223 | 2341 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|    430609 | 2342 | `			 }` |
|  37017529 | 2343 | `			 iLeft = iCur;` |
|  18508767 | 2344 | `		 }` |
|   7924682 | 2345 | `	 }` |
|         - | 2346 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2347 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2348 | `	  * we are dealing with a single operator.` |
|         - | 2349 | `	  */` |
|   1584939 | 2350 | `	  iLeft = -1;` |
|  10943073 | 2351 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|   9389619 | 2352 | `		  if( apNode[iCur] == 0 ){` |
|   6502067 | 2353 | `			  continue;` |
|         - | 2354 | `		  }` |
|   2887557 | 2355 | `		  pNode = apNode[iCur];` |
|   2887557 | 2356 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     31485 | 2357 | `			  sxi32 iNest = 1;` |
|     31485 | 2358 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2359 | `				  /* Missing condition */` |
|         3 | 2360 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 2361 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2362 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2363 | `				  }` |
|         3 | 2364 | `				  return rc;` |
|         - | 2365 | `			  }` |
|         - | 2366 | `			  /* Get the right node */` |
|     31483 | 2367 | `			  iRight = iCur + 1;` |
|    119785 | 2368 | `			  while( iRight < nToken  ){` |
|    119785 | 2369 | `				  if( apNode[iRight] ){` |
|     62889 | 2370 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2371 | `						  /* Increment nesting level */` |
|       ! 0 | 2372 | `						  ++iNest;` |
|     62889 | 2373 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2374 | `						  /* Decrement nesting level */` |
|     31483 | 2375 | `						  --iNest;` |
|     31483 | 2376 | `						  if( iNest <= 0 ){` |
|     31483 | 2377 | `							  break;` |
|         - | 2378 | `						  }` |
|       ! 0 | 2379 | `					  }` |
|     15703 | 2380 | `				  }` |
|     88307 | 2381 | `				  iRight++;` |
|         5 | 2382 | `			  }` |
|     31483 | 2383 | `			  if( iRight > iCur + 1 ){` |
|         - | 2384 | `				  /* Recurse and process the then expression */` |
|     31411 | 2385 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     31411 | 2386 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2387 | `					  return rc;` |
|         - | 2388 | `				  }` |
|         - | 2389 | `				  /* Link the node to the tree */` |
|     31411 | 2390 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     15703 | 2391 | `			  }else{` |
|         - | 2392 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2393 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2394 | `			  }` |
|     31483 | 2395 | `			  apNode[iCur + 1] = 0;` |
|     31483 | 2396 | `			  if( iRight + 1 < nToken ){` |
|         - | 2397 | `				  /* Recurse and process the else expression */` |
|     31483 | 2398 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     31483 | 2399 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2400 | `					  return rc;` |
|         - | 2401 | `				  }` |
|         - | 2402 | `				  /* Link the node to the tree */` |
|     31483 | 2403 | `				  pNode->pRight = apNode[iRight + 1];` |
|     31483 | 2404 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     15744 | 2405 | `			  }else{` |
|       ! 0 | 2406 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2407 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2408 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2409 | `				 }` |
|       ! 0 | 2410 | `				 return rc;` |
|         - | 2411 | `			  }` |
|         - | 2412 | `			  /* Point to the condition */` |
|     31483 | 2413 | `			  pNode->pCond  = apNode[iLeft];` |
|     31483 | 2414 | `			  apNode[iLeft] = 0;` |
|     31483 | 2415 | `			  break;` |
|         - | 2416 | `		  }` |
|   2856077 | 2417 | `		  iLeft = iCur;` |
|   1428041 | 2418 | `	  }` |
|         - | 2419 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2420 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2421 | `	  * so there is no need for a precedence loop here.` |
|         - | 2422 | `	  */` |
|   1584937 | 2423 | `	 iRight = -1;` |
|  11159361 | 2424 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|   9574481 | 2425 | `		 if( apNode[iCur] == 0 ){` |
|   7353937 | 2426 | `			 continue;` |
|         - | 2427 | `		 }` |
|   2220549 | 2428 | `		 pNode = apNode[iCur];` |
|   2220549 | 2429 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2430 | `			 /* Get the left node */` |
|    635531 | 2431 | `			 iLeft = iCur - 1;` |
|    826253 | 2432 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    190727 | 2433 | `				 iLeft--;` |
|         5 | 2434 | `			 }` |
|    635531 | 2435 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2436 | `				 /* Syntax error */` |
|        46 | 2437 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2438 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2439 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2440 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2441 | `				 }else{` |
|        41 | 2442 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2443 | `				 }` |
|        46 | 2444 | `				 if( rc != SXERR_ABORT ){` |
|        44 | 2445 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2446 | `				 }` |
|        46 | 2447 | `				 return rc;` |
|         - | 2448 | `			 }` |
|         - | 2449 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2450 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2451 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2452 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2453 | `			  * a write. */` |
|    635489 | 2454 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2455 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2456 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2457 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2458 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2459 | `				 }` |
|        11 | 2460 | `				 return rc;` |
|         - | 2461 | `			 }` |
|         - | 2462 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|         - | 2463 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|         - | 2464 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|         - | 2465 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|         - | 2466 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    635481 | 2467 | `			 pSuppress = 0;` |
|    635476 | 2468 | `			 if( apNode[iLeft]->pOp` |
|    372831 | 2469 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|     55093 | 2470 | `				 && apNode[iLeft]->pLeft != 0` |
|         5 | 2471 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2472 | `				 pSuppress = apNode[iLeft];` |
|       ! 0 | 2473 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|       ! 0 | 2474 | `			 }` |
|    635481 | 2475 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|         - | 2476 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|         - | 2477 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|         - | 2478 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|         - | 2479 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|         - | 2480 | `				  * assignment there, leaving the binary operator as the outer node.` |
|         - | 2481 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|       173 | 2482 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|         9 | 2483 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|         9 | 2484 | `					 ph7_expr_node *pParent = pHost;` |
|        13 | 2485 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|         7 | 2486 | `						 && ExprIsModifiableValue(pParent->pRight,FALSE) == FALSE ){` |
|       ! 0 | 2487 | `						 pParent = pParent->pRight;` |
|       ! 0 | 2488 | `					 }` |
|         8 | 2489 | `					 if( pParent->pRight && ExprIsModifiableValue(pParent->pRight,FALSE)` |
|         9 | 2490 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|         9 | 2491 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|         9 | 2492 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|         9 | 2493 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|         9 | 2494 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|         9 | 2495 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|         9 | 2496 | `						 iRight = iCur;` |
|         9 | 2497 | `						 continue;` |
|         - | 2498 | `					 }` |
|       ! 0 | 2499 | `				 }` |
|       224 | 2500 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       158 | 2501 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2502 | `					 /* Left operand must be a modifiable l-value */` |
|         3 | 2503 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2504 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2505 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2506 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2507 | `					 }else{` |
|       ! 0 | 2508 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 2509 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2510 | `					 }` |
|         3 | 2511 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 2512 | `						 rc = SXERR_SYNTAX;` |
|         1 | 2513 | `					 }` |
|         3 | 2514 | `					 return rc;` |
|         - | 2515 | `				 }` |
|        79 | 2516 | `			 }` |
|         - | 2517 | `			 /* Link the node to the tree (Reverse) */` |
|    635471 | 2518 | `			 pNode->pLeft = apNode[iRight];` |
|    635471 | 2519 | `			 pNode->pRight = apNode[iLeft];` |
|    635471 | 2520 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    635471 | 2521 | `			 if( pSuppress ){` |
|         - | 2522 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|       ! 0 | 2523 | `				 pSuppress->pLeft = pNode;` |
|       ! 0 | 2524 | `				 apNode[iCur] = pSuppress;` |
|       ! 0 | 2525 | `			 }` |
|    317733 | 2526 | `		 }` |
|   2220489 | 2527 | `		 iRight = iCur;` |
|   1110247 | 2528 | `	 }` |
|         - | 2529 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|   7924405 | 2530 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   6339525 | 2531 | `		 iLeft = -1;` |
|  44637173 | 2532 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  38297653 | 2533 | `			 if( apNode[iCur] == 0 ){` |
|  31957873 | 2534 | `				 continue;` |
|         - | 2535 | `			 }` |
|   6339785 | 2536 | `			 pNode = apNode[iCur];` |
|   6339785 | 2537 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2538 | `				 /* Get the right node */` |
|        56 | 2539 | `				 iRight = iCur + 1;` |
|        68 | 2540 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        13 | 2541 | `					 iRight++;` |
|         1 | 2542 | `				 }` |
|        56 | 2543 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2544 | `					 /* Syntax error */` |
|       ! 0 | 2545 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2546 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2547 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2548 | `					 }` |
|       ! 0 | 2549 | `					 return rc;` |
|         - | 2550 | `				 }` |
|         - | 2551 | `				 /* Link the node to the tree */` |
|        56 | 2552 | `				 pNode->pLeft = apNode[iLeft];` |
|        56 | 2553 | `				 pNode->pRight = apNode[iRight];` |
|        56 | 2554 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        26 | 2555 | `			 }` |
|   6339785 | 2556 | `			 iLeft = iCur;` |
|   3169895 | 2557 | `		 }` |
|   3169765 | 2558 | `	 }` |
|         - | 2559 | `	 /* Point to the root of the expression tree */` |
|   9574389 | 2560 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|   7989527 | 2561 | `		 if( apNode[iCur] ){` |
|   1497935 | 2562 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         - | 2563 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|         - | 2564 | `				  * operator sitting at its slot. The "expecting" clause is the closer` |
|         - | 2565 | ``				  * the enclosing construct set (`;` after `return`, `,`/`;` after`` |
|         - | 2566 | ``				  * `echo`, `)` for a for() post clause …); a for() clause defaults to`` |
|         - | 2567 | ``				  * `;` when nothing more specific was set. php prints no clause for a`` |
|         - | 2568 | `				  * plain expression statement, so a NULL closer stays clauseless. */` |
|        22 | 2569 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|        22 | 2570 | `				 const char *zExpect = pGen->zClauseCloser;` |
|        22 | 2571 | `				 if( zExpect == 0 && pGen->nCommaExprOk > 0 ){` |
|       ! 0 | 2572 | `					 zExpect = "\";\"";` |
|       ! 0 | 2573 | `				 }` |
|        22 | 2574 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,zExpect);` |
|        22 | 2575 | `				  if( rc != SXERR_ABORT ){` |
|        22 | 2576 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2577 | `				  }` |
|        22 | 2578 | `				  return rc;` |
|         - | 2579 | `			 }` |
|   1497917 | 2580 | `			 apNode[0] = apNode[iCur];` |
|   1497917 | 2581 | `			 apNode[iCur] = 0;` |
|    748956 | 2582 | `		 }` |
|   3994757 | 2583 | `	 }` |
|   1584867 | 2584 | `	 return SXRET_OK;` |
|   1493303 | 2585 | ` }` |
|         - | 2586 | ` /*` |
|         - | 2587 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2588 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2589 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2590 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2591 | `  */` |
|   1811858 | 2592 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2593 | `{` |
|         - | 2594 | `	ph7_expr_node **apNode;` |
|         - | 2595 | `	ph7_expr_node *pNode;` |
|         - | 2596 | `	sxi32 rc;` |
|         - | 2597 | `	/* Reset node container */` |
|   1811863 | 2598 | `	SySetReset(pExprNode);` |
|   1811863 | 2599 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2600 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2601 | `	{` |
|   1811863 | 2602 | `		int iLastWasTerm = 0;` |
|   1811863 | 2603 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  10243583 | 2604 | `		while( pGen->pIn < pGen->pEnd ){` |
|   8431765 | 2605 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|   8431765 | 2606 | `			if( rc != SXRET_OK ){` |
|        44 | 2607 | `				return rc;` |
|         - | 2608 | `			}` |
|         - | 2609 | `			/* Determine if this node is a term for short-array disambiguation */` |
|   8431725 | 2610 | `			if( pNode->xCode ){` |
|         - | 2611 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   4398711 | 2612 | `				iLastWasTerm = 1;` |
|   6232372 | 2613 | `			}else if( pNode->pOp ){` |
|         - | 2614 | `				/* Operator node */` |
|   2361983 | 2615 | `				iLastWasTerm = 0;` |
|   1180994 | 2616 | `			}else{` |
|         - | 2617 | `				/* Delimiter: ')' and ']' end terms */` |
|   1671041 | 2618 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2619 | `			}` |
|         - | 2620 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2621 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2622 | `			 * node kind, so this single test covers all branches. */` |
|   8431725 | 2623 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2624 | `			/* Save the extracted node */` |
|   8431725 | 2625 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2626 | `		}` |
|         - | 2627 | `	}` |
|   1811823 | 2628 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2629 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2630 | `		*ppRoot = 0;` |
|       ! 0 | 2631 | `		return SXRET_OK;` |
|         - | 2632 | `	}` |
|   1811823 | 2633 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2634 | `	/* Make sure we are dealing with valid nodes */` |
|   1811823 | 2635 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   1811823 | 2636 | `	if( rc != SXRET_OK ){` |
|         - | 2637 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2638 | `		 * cleanup the mess left behind.` |
|         - | 2639 | `		 */` |
|        57 | 2640 | `		*ppRoot = 0;` |
|        57 | 2641 | `		return rc;` |
|         - | 2642 | `	}` |
|         - | 2643 | `	/* Build the tree */` |
|   1811771 | 2644 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   1811771 | 2645 | `	if( rc != SXRET_OK ){` |
|         - | 2646 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2647 | `		*ppRoot = 0;` |
|       103 | 2648 | `		return rc;` |
|         - | 2649 | `	}` |
|         - | 2650 | `	/* Point to the root of the tree */` |
|   1811673 | 2651 | `	*ppRoot = apNode[0];` |
|   1811673 | 2652 | `	return SXRET_OK;` |
|    905934 | 2653 | `}` |
|         - | 2654 |  |
