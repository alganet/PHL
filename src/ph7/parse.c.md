# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1150/1318 lines (87.25%)

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
|  10808568 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  10808573 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 167139833 |  278 | `	for(;;){` |
| 334279671 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 334279671 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  31597455 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  15798730 |  285 | `		}else{` |
| 302682221 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 334279671 |  288 | `		if( rc == 0 ){` |
|  10874763 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  10804177 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|     70591 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|       437 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|     70159 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      3977 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      3977 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      3969 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|     33095 |  307 | `		}` |
| 323471103 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|   5404289 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   3506752 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   3506757 |  320 | `	SyToken *pCur = pIn;` |
|   3506757 |  321 | `	sxi32 iNest = 1;` |
|  35205418 |  322 | `	for(;;){` |
|  70410841 |  323 | `		if( pCur >= pEnd ){` |
|       481 |  324 | `			break;` |
|         - |  325 | `		}` |
|  70410365 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   2952721 |  328 | `			iNest++;` |
|  68934007 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|   6458997 |  331 | `			iNest--;` |
|   6458997 |  332 | `			if( iNest <= 0 ){` |
|   3506281 |  333 | `				break;` |
|         - |  334 | `			}` |
|   1476358 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
|  66904089 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   3506757 |  340 | `	*ppEnd = pCur;` |
|   3506757 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    170256 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    170256 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    170158 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       167 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    170099 |  358 | `	if( bCheckFunc ){` |
|      8052 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      8045 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      8027 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        49 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|      4004 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    170055 |  366 | `	return FALSE;` |
|     85133 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|   6994956 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|   6994961 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|   6994961 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  43418621 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  36423699 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      1447 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  36422257 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   3248389 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    135722 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   3073477 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   3073477 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   3073477 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   3073477 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   1536736 |  401 | `					}` |
|   1536736 |  402 | `			}` |
|   3248389 |  403 | `			iParen++;` |
|  34798065 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   3248389 |  405 | `			if( iParen <= 0 ){` |
|        16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        16 |  407 | `				if( rc != SXERR_ABORT ){` |
|        16 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        16 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   3248377 |  412 | `			iParen--;` |
|  31549675 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   1521185 |  414 | `			iSquare++;` |
|  29164899 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   1521199 |  416 | `			if( iSquare <= 0 ){` |
|         8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|         8 |  418 | `				if( rc != SXERR_ABORT ){` |
|         8 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         8 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   1521193 |  423 | `			iSquare--;` |
|  27643709 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|        20 |  425 | `			iBraces++;` |
|        20 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|         7 |  470 | `			}` |
|  26883106 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|        24 |  472 | `			if( iBraces <= 0 ){` |
|        15 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|        15 |  474 | `				if( rc != SXERR_ABORT ){` |
|        15 |  475 | `					rc = SXERR_SYNTAX;` |
|         6 |  476 | `				}` |
|        15 |  477 | `				return rc;` |
|         - |  478 | `			}` |
|        10 |  479 | `			iBraces--;` |
|  26883081 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    199357 |  481 | `			if( iQuesty > 0 ){` |
|    199103 |  482 | `				iQuesty--;` |
|     99808 |  483 | `			}else if( iParen <= 0 ){` |
|         - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  488 | `				if( rc != SXERR_ABORT ){` |
|         6 |  489 | `					rc = SXERR_SYNTAX;` |
|         2 |  490 | `				}` |
|         6 |  491 | `				return rc;` |
|         5 |  492 | `			}` |
|  26783399 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   8304437 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   8304437 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    199105 |  496 | `				iQuesty++;` |
|   8204887 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      4327 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      2161 |  516 | `			}` |
|   4152216 |  517 | `		}` |
|  18211114 |  518 | `	}` |
|   6994927 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        19 |  520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|        19 |  521 | `		if( rc != SXERR_ABORT ){` |
|        19 |  522 | `			rc = SXERR_SYNTAX;` |
|         8 |  523 | `		}` |
|        19 |  524 | `		return rc;` |
|         - |  525 | `	}` |
|   6994911 |  526 | `	return SXRET_OK;` |
|   3497483 |  527 | `}` |
|         - |  528 | `/*` |
|         - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  531 | ` */` |
|   5421916 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  533 | `{` |
|   5421921 |  534 | `	SyToken *pIn = *ppCur;` |
|         - |  535 | `	/* Jump the first literal seen */` |
|   5421921 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   5418029 |  537 | `		pIn++;` |
|   2709012 |  538 | `	}` |
|   2712931 |  539 | `	for(;;){` |
|   5425867 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      3951 |  541 | `			pIn++;` |
|      3951 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3949 |  543 | `				pIn++;` |
|      1972 |  544 | `			}` |
|      1978 |  545 | `		}else{` |
|   2710963 |  546 | `			break;` |
|         - |  547 | `		}` |
|         5 |  548 | `	}` |
|         - |  549 | `	/* Synchronize pointers */` |
|   5421921 |  550 | `	*ppCur = pIn;` |
|   5421921 |  551 | `}` |
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
|       608 |  596 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  597 | `{` |
|       613 |  598 | `	SyToken *pIn = *ppIn;` |
|       613 |  599 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        26 |  600 | `		pIn++; /* Skip ':' */` |
|        11 |  601 | `		for(;;){` |
|         - |  602 | `			/* Optional '?' nullable prefix */` |
|        30 |  603 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|         6 |  604 | `				pIn++;` |
|         2 |  605 | `			}` |
|        30 |  606 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  607 | `				/* Parenthesized DNF group '(A&B)' */` |
|       ! 0 |  608 | `				pIn++;` |
|       ! 0 |  609 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       ! 0 |  610 | `				if( pIn < pEnd ){` |
|       ! 0 |  611 | `					pIn++; /* ')' */` |
|       ! 0 |  612 | `				}` |
|        26 |  613 | `			}else if( pIn < pEnd` |
|        30 |  614 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  615 | `				/* ['\']Name('\'Name)* */` |
|        30 |  616 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|        30 |  617 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        30 |  618 | `					pIn++;` |
|        30 |  619 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  620 | `						pIn += 2;` |
|       ! 0 |  621 | `					}` |
|        13 |  622 | `				}` |
|        17 |  623 | `			}else{` |
|         - |  624 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  625 | `				break;` |
|         - |  626 | `			}` |
|         - |  627 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|        26 |  628 | `			if( pIn < pEnd` |
|        30 |  629 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|        26 |  630 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|         5 |  631 | `				pIn++;` |
|         5 |  632 | `				continue;` |
|         - |  633 | `			}` |
|        26 |  634 | `			break;` |
|       ! 0 |  635 | `		}` |
|        11 |  636 | `	}` |
|       613 |  637 | `	*ppIn = pIn;` |
|       613 |  638 | `}` |
|       358 |  639 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  640 | `{` |
|       363 |  641 | `	SyToken *pIn = *ppCur;` |
|         - |  642 | `	sxu32 nLine;` |
|         - |  643 | `	sxi32 rc;` |
|         - |  644 | `	/* Jump the 'function' keyword */` |
|       363 |  645 | `	nLine = pIn->nLine;` |
|       363 |  646 | `	pIn++;` |
|       363 |  647 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  648 | `		pIn++;` |
|         1 |  649 | `	}` |
|       363 |  650 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  651 | `		/* Syntax error */` |
|         6 |  652 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|         6 |  653 | `		if( rc != SXERR_ABORT ){` |
|         6 |  654 | `			rc = SXERR_SYNTAX;` |
|         2 |  655 | `		}` |
|         6 |  656 | `		goto Synchronize;` |
|         - |  657 | `	}` |
|       359 |  658 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       359 |  659 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       359 |  660 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  661 | `		/* Syntax error */` |
|         6 |  662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  663 | `		if( rc != SXERR_ABORT ){` |
|         6 |  664 | `			rc = SXERR_SYNTAX;` |
|         2 |  665 | `		}` |
|         6 |  666 | `		goto Synchronize;` |
|         - |  667 | `	}` |
|       355 |  668 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  669 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       355 |  670 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       355 |  671 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|        47 |  672 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  673 | `		/* Check if we are dealing with a closure */` |
|        47 |  674 | `		if( nKey == PH7_TKWRD_USE ){` |
|        39 |  675 | `			pIn++; /* Jump the 'use' keyword */` |
|        39 |  676 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  677 | `				/* Syntax error */` |
|         6 |  678 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  679 | `				if( rc != SXERR_ABORT ){` |
|         6 |  680 | `					rc = SXERR_SYNTAX;` |
|         2 |  681 | `				}` |
|         6 |  682 | `				goto Synchronize;` |
|         - |  683 | `			}` |
|        35 |  684 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|        35 |  685 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        35 |  686 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  687 | `				/* Syntax error */` |
|         6 |  688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  689 | `				if( rc != SXERR_ABORT ){` |
|         6 |  690 | `					rc = SXERR_SYNTAX;` |
|         2 |  691 | `				}` |
|         6 |  692 | `				goto Synchronize;` |
|         - |  693 | `			}` |
|        31 |  694 | `			pIn++;` |
|         - |  695 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  696 | ``			 * `function (...) use (...) : int {` */`` |
|        31 |  697 | `			ExprSkipReturnType(&pIn,pEnd);` |
|        18 |  698 | `		}else{` |
|         - |  699 | `			/* Syntax error */` |
|        12 |  700 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        12 |  701 | `			if( rc != SXERR_ABORT ){` |
|        12 |  702 | `				rc = SXERR_SYNTAX;` |
|         4 |  703 | `			}` |
|        12 |  704 | `			goto Synchronize;` |
|         - |  705 | `		}` |
|        13 |  706 | `	}` |
|         - |  707 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  708 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  709 | `	 * the type), and pEnd is one past the last token. */` |
|       339 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       339 |  711 | `		pIn++; /* Jump the leading curly '{' */` |
|       339 |  712 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       339 |  713 | `		if( pIn < pEnd ){` |
|       339 |  714 | `			pIn++;` |
|       167 |  715 | `		}` |
|       172 |  716 | `	}else{` |
|         - |  717 | `		/* Syntax error */` |
|       ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|       ! 0 |  719 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  720 | `			return SXERR_ABORT;` |
|         - |  721 | `		}` |
|         - |  722 | `	}` |
|       339 |  723 | `	rc = SXRET_OK;` |
|       179 |  724 | `Synchronize:` |
|         - |  725 | `	/* Synchronize pointers */` |
|       363 |  726 | `	*ppCur = pIn;` |
|       363 |  727 | `	return rc;` |
|       184 |  728 | `}` |
|         - |  729 | `/*` |
|         - |  730 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  731 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  732 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  733 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  734 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  735 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  736 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  737 | ` */` |
|        26 |  738 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         4 |  739 | `{` |
|        30 |  740 | `	SyToken *pIn = *ppCur;` |
|        30 |  741 | `	sxu32 nLine = pIn->nLine;` |
|         - |  742 | `	sxi32 rc;` |
|        30 |  743 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  744 | `	/* Optional constructor argument list */` |
|        30 |  745 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         7 |  746 | `		pIn++; /* Jump '(' */` |
|         7 |  747 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         7 |  748 | `		if( pIn < pEnd ){` |
|         7 |  749 | `			pIn++; /* Jump ')' */` |
|         3 |  750 | `		}` |
|         3 |  751 | `	}` |
|         - |  752 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  753 | `	 * (no braces appear between ')' and the class body). */` |
|        58 |  754 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|        32 |  755 | `		pIn++;` |
|         4 |  756 | `	}` |
|        30 |  757 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  758 | `		/* Syntax error: missing class body */` |
|       ! 0 |  759 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  760 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  761 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  762 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  763 | `		}` |
|       ! 0 |  764 | `		*ppCur = pIn;` |
|       ! 0 |  765 | `		return rc;` |
|         - |  766 | `	}` |
|        30 |  767 | `	pIn++; /* Jump the leading '{' */` |
|        30 |  768 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        30 |  769 | `	if( pIn < pEnd ){` |
|        30 |  770 | `		pIn++; /* Jump the trailing '}' */` |
|        13 |  771 | `	}` |
|        30 |  772 | `	*ppCur = pIn;` |
|        30 |  773 | `	return SXRET_OK;` |
|        17 |  774 | `}` |
|         - |  775 | `/*` |
|         - |  776 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  777 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  778 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  779 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  780 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  781 | ` */` |
|       232 |  782 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  783 | `{` |
|       237 |  784 | `	SyToken *pIn = *ppCur;` |
|         - |  785 | `	sxu32 nLine;` |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	int iNest;` |
|       237 |  788 | `	nLine = pIn->nLine;` |
|         - |  789 | `	/* Optional 'static' prefix */` |
|       232 |  790 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       237 |  791 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         3 |  792 | `		pIn++;` |
|         1 |  793 | `	}` |
|         - |  794 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       232 |  795 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       237 |  796 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  797 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  798 | `		goto Synchronize;` |
|         - |  799 | `	}` |
|       237 |  800 | `	pIn++; /* Jump 'fn' */` |
|       116 |  801 | `	SXUNUSED(nLine);` |
|       116 |  802 | `	SXUNUSED(pGen);` |
|         - |  803 | `	/* Optional '&' for return-by-reference */` |
|       237 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  805 | `		pIn++;` |
|       ! 0 |  806 | `	}` |
|         - |  807 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  808 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  809 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  810 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       237 |  811 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       235 |  812 | `		pIn++; /* '(' */` |
|       235 |  813 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       235 |  814 | `		if( pIn < pEnd ){` |
|       233 |  815 | `			pIn++; /* ')' */` |
|       114 |  816 | `		}` |
|       115 |  817 | `	}` |
|         - |  818 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       237 |  819 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  820 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       237 |  821 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       230 |  822 | `		pIn++;` |
|       113 |  823 | `	}` |
|         - |  824 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       237 |  825 | `	iNest = 0;` |
|      1527 |  826 | `	while( pIn < pEnd ){` |
|      1423 |  827 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  828 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       129 |  829 | `			break;` |
|         - |  830 | `		}` |
|      1295 |  831 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       133 |  832 | `			iNest++;` |
|      1230 |  833 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       133 |  834 | `			iNest--;` |
|        65 |  835 | `		}` |
|      1295 |  836 | `		pIn++;` |
|         5 |  837 | `	}` |
|       237 |  838 | `	rc = SXRET_OK;` |
|       116 |  839 | `Synchronize:` |
|       237 |  840 | `	*ppCur = pIn;` |
|       237 |  841 | `	return rc;` |
|         5 |  842 | `}` |
|         - |  843 | `/*` |
|         - |  844 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - |  845 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - |  846 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - |  847 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - |  848 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - |  849 | ` */` |
|        70 |  850 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  851 | `{` |
|        75 |  852 | `	SyToken *pIn = *ppCur;` |
|         - |  853 | `	sxi32 rc;` |
|        35 |  854 | `	SXUNUSED(pGen);` |
|         - |  855 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|        70 |  856 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        75 |  857 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 |  858 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  859 | `		goto Synchronize;` |
|         - |  860 | `	}` |
|        75 |  861 | `	pIn++; /* Jump 'match' */` |
|         - |  862 | `	/* Optional '(' subject ')' */` |
|        75 |  863 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        75 |  864 | `		pIn++;` |
|        75 |  865 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        75 |  866 | `		if( pIn < pEnd ){` |
|        75 |  867 | `			pIn++; /* ')' */` |
|        35 |  868 | `		}` |
|        35 |  869 | `	}` |
|         - |  870 | `	/* Optional '{' arms '}' */` |
|        75 |  871 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|        75 |  872 | `		pIn++;` |
|        75 |  873 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|        75 |  874 | `		if( pIn < pEnd ){` |
|        75 |  875 | `			pIn++; /* '}' */` |
|        35 |  876 | `		}` |
|        35 |  877 | `	}` |
|        75 |  878 | `	rc = SXRET_OK;` |
|        35 |  879 | `Synchronize:` |
|        75 |  880 | `	*ppCur = pIn;` |
|        75 |  881 | `	return rc;` |
|         5 |  882 | `}` |
|         - |  883 | `/*` |
|         - |  884 | ` * Extract a single expression node from the input.` |
|         - |  885 | ` * On success store the freshly extractd node in ppNode.` |
|         - |  886 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  887 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - |  888 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - |  889 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - |  890 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - |  891 | ` */` |
|  36427778 |  892 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  893 | `{` |
|         - |  894 | `	ph7_expr_node *pNode;` |
|         - |  895 | `	SyToken *pCur;` |
|         - |  896 | `	sxi32 rc;` |
|         - |  897 | `	/* Allocate a new node */` |
|  36427783 |  898 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  36427783 |  899 | `	if( pNode == 0 ){` |
|         - |  900 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  901 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  902 | `		 */` |
|       ! 0 |  903 | `		return SXERR_MEM;` |
|         - |  904 | `	}` |
|         - |  905 | `	/* Zero the structure */` |
|  36427783 |  906 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  36427783 |  907 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  908 | `	/* Point to the head of the token stream */` |
|  36427783 |  909 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  910 | `	/* Start collecting tokens */` |
|  36427783 |  911 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4015 |  912 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - |  913 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - |  914 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - |  915 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - |  916 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|        77 |  917 | `			pNode->pEnd = pCur;` |
|        77 |  918 | `			pCur++;` |
|        77 |  919 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|        77 |  920 | `			pNode->xCode = PH7_CompileFccMarker;` |
|        77 |  921 | `			pGen->pIn = pCur;` |
|        77 |  922 | `			*ppNode = pNode;` |
|        77 |  923 | `			return SXRET_OK;` |
|         - |  924 | `		}` |
|         - |  925 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - |  926 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|      3939 |  927 | `		pCur++;` |
|      3939 |  928 | `		pGen->pIn = pCur;` |
|      3939 |  929 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      3939 |  930 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      3939 |  931 | `		if( rc == SXRET_OK && *ppNode ){` |
|      3939 |  932 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      1967 |  933 | `		}` |
|      3939 |  934 | `		return rc;` |
|         - |  935 | `	}` |
|  36423773 |  936 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  937 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  938 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  939 | `		 */` |
|      1449 |  940 | `		pCur++; /* Skip the opening '[' */` |
|      1449 |  941 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      1449 |  942 | `		if( pCur < pGen->pEnd ){` |
|      1449 |  943 | `			pCur++; /* Skip past the closing ']' */` |
|       727 |  944 | `		}else{` |
|       ! 0 |  945 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - |  946 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 |  947 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 |  948 | `				rc = SXERR_SYNTAX;` |
|       ! 0 |  949 | `			}` |
|       ! 0 |  950 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 |  951 | `			return rc;` |
|         - |  952 | `		}` |
|         - |  953 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - |  954 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - |  955 | `		 */` |
|      1562 |  956 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       230 |  957 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       230 |  958 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        54 |  959 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  960 | `			}else{` |
|       177 |  961 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  962 | `			}` |
|       117 |  963 | `		}else{` |
|      1223 |  964 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  965 | `		}` |
|  36423051 |  966 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - |  967 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - |  968 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - |  969 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - |  970 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - |  971 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - |  972 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - |  973 | `		 * call, not the clone() intrinsic. */` |
|        17 |  974 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        17 |  975 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        17 |  976 | `		pNode->xCode = PH7_CompileLiteral;` |
|  36422317 |  977 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   9825668 |  978 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   4912868 |  979 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|         - |  980 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - |  981 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|         - |  982 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|         - |  983 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|         - |  984 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|         - |  985 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|         - |  986 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|         - |  987 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|         - |  988 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|         - |  989 | `		 * operator (which would dereference the NULL pOp). */` |
|        24 |  990 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        24 |  991 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|        24 |  992 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        24 |  993 | `		if( pCur < pGen->pEnd ){` |
|        24 |  994 | `			pCur++; /* skip the closing ')' */` |
|        13 |  995 | `		}else{` |
|       ! 0 |  996 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - |  997 | `				"clone: Missing closing parenthesis ')'");` |
|       ! 0 |  998 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 |  999 | `				rc = SXERR_SYNTAX;` |
|       ! 0 | 1000 | `			}` |
|       ! 0 | 1001 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1002 | `			return rc;` |
|         - | 1003 | `		}` |
|        24 | 1004 | `		pNode->xCode = PH7_CompileCloneCall;` |
|  36422302 | 1005 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1006 | `		/* Point to the instance that describe this operator */` |
|   9825651 | 1007 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1008 | `		/* Advance the stream cursor */` |
|   9825651 | 1009 | `		pCur++;` |
|  31509468 | 1010 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1011 | `		/* Isolate variable */` |
|  16984465 | 1012 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   8492241 | 1013 | `			pCur++; /* Variable variable */` |
|         5 | 1014 | `		}` |
|   8492229 | 1015 | `		if( pCur < pGen->pEnd ){` |
|   8492229 | 1016 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1017 | `				/* Variable name */` |
|   8492199 | 1018 | `				pCur++;` |
|   4246132 | 1019 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        27 | 1020 | `				pCur++;` |
|         - | 1021 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        27 | 1022 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        27 | 1023 | `				if( pCur < pGen->pEnd ){` |
|        21 | 1024 | `					pCur++;` |
|        12 | 1025 | `				}else{` |
|         6 | 1026 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|         6 | 1027 | `					if( rc != SXERR_ABORT ){` |
|         6 | 1028 | `						rc = SXERR_SYNTAX;` |
|         2 | 1029 | `					}` |
|         6 | 1030 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         6 | 1031 | `					return rc;` |
|         - | 1032 | `				}` |
|         9 | 1033 | `			}` |
|   4246110 | 1034 | `		}` |
|   8492225 | 1035 | `		pNode->xCode = PH7_CompileVariable;` |
|  22350531 | 1036 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    565053 | 1037 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    565053 | 1038 | `		 if( bAfterMemberOp ){` |
|         - | 1039 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1040 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1041 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1042 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    119365 | 1043 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    119365 | 1044 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    505373 | 1045 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1046 | `			 /* List/Array node */` |
|    282499 | 1047 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1048 | `				 /* Assume a literal */` |
|       ! 0 | 1049 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1050 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1051 | `			 }else{` |
|    282499 | 1052 | `				 pCur += 2;` |
|         - | 1053 | `				 /* Collect array/list tokens */` |
|    282499 | 1054 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    282499 | 1055 | `				 if( pCur < pGen->pEnd ){` |
|    282497 | 1056 | `					 pCur++;` |
|    141251 | 1057 | `				 }else{` |
|         - | 1058 | `					 /* Syntax error */` |
|         4 | 1059 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1060 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1061 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1062 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1063 | `					 }` |
|         3 | 1064 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1065 | `					 return rc;` |
|         - | 1066 | `				 }` |
|    282497 | 1067 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    282497 | 1068 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        37 | 1069 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        37 | 1070 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1071 | `						 /* Syntax error */` |
|         3 | 1072 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|         3 | 1073 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1074 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1075 | `						 }` |
|         3 | 1076 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1077 | `						 return rc;` |
|         - | 1078 | `					 }` |
|        15 | 1079 | `				 }` |
|         5 | 1080 | `			 }` |
|    304444 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1082 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       343 | 1083 | `			 pCur++; /* Skip 'yield' keyword */` |
|       343 | 1084 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1085 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1086 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       343 | 1087 | `			 pNode->xCode = PH7_CompileYield;` |
|    163030 | 1088 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|         - | 1089 | `			 /* Annonymous function */` |
|       363 | 1090 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1091 | `				 /* Assume a literal */` |
|       ! 0 | 1092 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1093 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1094 | `			 }else{` |
|         - | 1095 | `				 /* Assemble annonymous functions body */` |
|       363 | 1096 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       363 | 1097 | `				 if( rc != SXRET_OK ){` |
|        28 | 1098 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1099 | `					 return rc;` |
|         - | 1100 | `				 }` |
|       339 | 1101 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1102 | `			  }` |
|    162670 | 1103 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|        39 | 1104 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        22 | 1105 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        12 | 1106 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|         9 | 1107 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1108 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1109 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1110 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1111 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|        30 | 1112 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|        30 | 1113 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1114 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1115 | `				 return rc;` |
|         - | 1116 | `			 }` |
|        30 | 1117 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    162489 | 1118 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    162363 | 1119 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        30 | 1120 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        15 | 1121 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1122 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       237 | 1123 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       237 | 1124 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1125 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1126 | `				 return rc;` |
|         - | 1127 | `			 }` |
|       237 | 1128 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    162361 | 1129 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1130 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        75 | 1131 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        75 | 1132 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1133 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1134 | `				 return rc;` |
|         - | 1135 | `			 }` |
|        75 | 1136 | `			 pNode->xCode = PH7_CompileMatch;` |
|    162210 | 1137 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1138 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1139 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1140 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1141 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1142 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1143 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1144 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1145 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    162157 | 1146 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1147 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        93 | 1148 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        93 | 1149 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        49 | 1150 | `		 }else{` |
|         - | 1151 | `			 /* Assume a literal */` |
|    162051 | 1152 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    162051 | 1153 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1154 | `		 }` |
|  17821883 | 1155 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1156 | `		 /* Constants,function name,namespace path,class name... */` |
|   5140499 | 1157 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   5140499 | 1158 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   2570252 | 1159 | `	 }else{` |
|  12398879 | 1160 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1161 | `			 /* Point to the code generator routine */` |
|   4181505 | 1162 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   4181505 | 1163 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1164 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|         3 | 1165 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1166 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1167 | `				 }` |
|         3 | 1168 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1169 | `				 return rc;` |
|         - | 1170 | `			 }` |
|   2090749 | 1171 | `		 }` |
|         - | 1172 | `		/* Advance the stream cursor */` |
|  12398877 | 1173 | `		pCur++;` |
|         - | 1174 | `	 }` |
|         - | 1175 | `	/* Point to the end of the token stream */` |
|  36423739 | 1176 | `	pNode->pEnd = pCur;` |
|         - | 1177 | `	/* Save the node for later processing */` |
|  36423739 | 1178 | `	*ppNode = pNode;` |
|         - | 1179 | `	/* Synchronize cursors */` |
|  36423739 | 1180 | `	pGen->pIn = pCur;` |
|  36423739 | 1181 | `	return SXRET_OK;` |
|  18213894 | 1182 | `}` |
|         - | 1183 | `/*` |
|         - | 1184 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1185 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1186 | ` * level is zero.` |
|         - | 1187 | ` */` |
|    704192 | 1188 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1189 | `{` |
|    704197 | 1190 | `	SyToken *pCur = pStart;` |
|    704197 | 1191 | `	sxi32 iNest = 0;` |
|    704197 | 1192 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1193 | `		/* Last expression */` |
|    302949 | 1194 | `		return SXERR_EOF;` |
|         - | 1195 | `	}` |
|   1641559 | 1196 | `	while( pCur < pEnd ){` |
|   1543405 | 1197 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    303099 | 1198 | `			break;` |
|         - | 1199 | `		}` |
|   1240311 | 1200 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     73887 | 1201 | `			iNest++;` |
|   1203370 | 1202 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     73889 | 1203 | `			iNest--;` |
|     36942 | 1204 | `		}` |
|   1240311 | 1205 | `		pCur++;` |
|         5 | 1206 | `	}` |
|    401253 | 1207 | `	*ppNext = pCur;` |
|    401253 | 1208 | `	return SXRET_OK;` |
|    352101 | 1209 | `}` |
|         - | 1210 | `/*` |
|         - | 1211 | ` * Free an expression tree.` |
|         - | 1212 | ` */` |
|  30861572 | 1213 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1214 | `{` |
|  30861577 | 1215 | `	if( pNode->pLeft ){` |
|         - | 1216 | `		/* Release the left tree */` |
|  12130401 | 1217 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   6065198 | 1218 | `	}` |
|  30861577 | 1219 | `	if( pNode->pRight ){` |
|         - | 1220 | `		/* Release the right tree */` |
|   6743477 | 1221 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   3371736 | 1222 | `	}` |
|  30861577 | 1223 | `	if( pNode->pCond ){` |
|         - | 1224 | `		/* Release the conditional tree used by the ternary operator */` |
|    199101 | 1225 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     99548 | 1226 | `	}` |
|  30861577 | 1227 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1228 | `		ph7_expr_node **apArg;` |
|         - | 1229 | `		sxu32 n;` |
|         - | 1230 | `		/* Release node arguments */` |
|   3674495 | 1231 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   8117463 | 1232 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   4442973 | 1233 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   2221489 | 1234 | `		}` |
|   3674495 | 1235 | `		SySetRelease(&pNode->aNodeArgs);` |
|   1837245 | 1236 | `	}` |
|         - | 1237 | `	/* Finally,release this node */` |
|  30861577 | 1238 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  30861577 | 1239 | `}` |
|         - | 1240 | `/*` |
|         - | 1241 | ` * Free an expression tree.` |
|         - | 1242 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1243 | ` */` |
|   6994990 | 1244 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1245 | `{` |
|         - | 1246 | `	ph7_expr_node **apNode;` |
|         - | 1247 | `	sxu32 n;` |
|   6994995 | 1248 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  43418805 | 1249 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  36423815 | 1250 | `		if( apNode[n] ){` |
|   6995329 | 1251 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   3497662 | 1252 | `		}` |
|  18211910 | 1253 | `	}` |
|   6994995 | 1254 | `	return SXRET_OK;` |
|         5 | 1255 | `}` |
|         - | 1256 | `/*` |
|         - | 1257 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1258 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1259 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1260 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1261 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1262 | ` */` |
|   8108914 | 1263 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1264 | `{` |
|   8108919 | 1265 | `	if( pNode == 0 ){` |
|   5179777 | 1266 | `		return 0;` |
|         - | 1267 | `	}` |
|   2929147 | 1268 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1269 | `		return 1;` |
|         - | 1270 | `	}` |
|   2929135 | 1271 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1272 | `		return 1;` |
|         - | 1273 | `	}` |
|   2929131 | 1274 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1275 | `		return 1;` |
|         - | 1276 | `	}` |
|   2929131 | 1277 | `	return 0;` |
|   4054462 | 1278 | `}` |
|         - | 1279 | `/*` |
|         - | 1280 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1281 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1282 | ` */` |
|   2243970 | 1283 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1284 | `{` |
|         - | 1285 | `	sxi32 iExprOp;` |
|   2243975 | 1286 | `	if( pNode->pOp == 0 ){` |
|   1812359 | 1287 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1288 | `	}` |
|    431621 | 1289 | `	iExprOp = pNode->pOp->iOp;` |
|    431621 | 1290 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    246571 | 1291 | `			return TRUE;` |
|         - | 1292 | `	}` |
|    185055 | 1293 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    185049 | 1294 | `		if( pNode->pLeft->pOp ) {` |
|        70 | 1295 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        31 | 1296 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1297 | `				return FALSE;` |
|         5 | 1298 | `			}` |
|    185014 | 1299 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1300 | `			return FALSE;` |
|         - | 1301 | `		}` |
|    185049 | 1302 | `		return TRUE;` |
|         - | 1303 | `	}` |
|         8 | 1304 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1305 | `		return TRUE;` |
|         - | 1306 | `	}` |
|         - | 1307 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1308 | `	return FALSE;` |
|   1121990 | 1309 | `}` |
|         - | 1310 | `/* Forward declaration */` |
|         - | 1311 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1312 | `/* Macro to check if the given node is a terminal.` |
|         - | 1313 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1314 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1315 | ` * linked ternary/elvis node). */` |
|         - | 1316 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1317 | `/*` |
|         - | 1318 | ` * Buid an expression tree for each given function argument.` |
|         - | 1319 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1320 | ` */` |
|   2276428 | 1321 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1322 | `{` |
|         - | 1323 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	/* Process function arguments from left to right */` |
|   2276433 | 1326 | `	iCur = 0;` |
|   2660655 | 1327 | `	for(;;){` |
|   5321315 | 1328 | `		if( iCur >= nToken ){` |
|         - | 1329 | `			/* No more arguments to process */` |
|   2276407 | 1330 | `			break;` |
|         - | 1331 | `		}` |
|   3044913 | 1332 | `		iNode = iCur;` |
|   3044913 | 1333 | `		iNest = 0;` |
|   9926127 | 1334 | `		while( iCur < nToken ){` |
|   7649723 | 1335 | `			if( apNode[iCur] ){` |
|   7634291 | 1336 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    384257 | 1337 | `					break;` |
|   6865782 | 1338 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   3705259 | 1339 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    544462 | 1340 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1341 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1342 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1343 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1344 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1345 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1346 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    544183 | 1347 | `					iNest++;` |
|   6593698 | 1348 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    544183 | 1349 | `					iNest--;` |
|    272089 | 1350 | `				}` |
|   3432891 | 1351 | `			}` |
|   6881219 | 1352 | `			iCur++;` |
|         5 | 1353 | `		}` |
|   3044913 | 1354 | `		if( iCur > iNode ){` |
|   3044907 | 1355 | `			SyString sArgName = {0, 0};` |
|         - | 1356 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1357 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1358 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   3044902 | 1359 | `			if( (iCur - iNode) >= 2` |
|   2029615 | 1360 | `				&& apNode[iNode]` |
|   1014324 | 1361 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    531136 | 1362 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     47801 | 1363 | `				&& apNode[iNode+1]` |
|     47655 | 1364 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1365 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       255 | 1366 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       255 | 1367 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       255 | 1368 | `				apNode[iNode] = 0;` |
|       255 | 1369 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       255 | 1370 | `				apNode[iNode+1] = 0;` |
|       255 | 1371 | `				iNode += 2;` |
|         - | 1372 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1373 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       255 | 1374 | `				if( iNode >= iCur ){` |
|         4 | 1375 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1376 | `						pOp->pStart->nLine,` |
|         - | 1377 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1378 | `						&sArgName);` |
|         3 | 1379 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1380 | `						rc = SXERR_SYNTAX;` |
|         1 | 1381 | `					}` |
|         3 | 1382 | `					return rc;` |
|         - | 1383 | `				}` |
|       124 | 1384 | `			}` |
|   3044900 | 1385 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1386 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1387 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1388 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1389 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1390 | `					apNode[iNode] = 0;` |
|       ! 0 | 1391 | `			}` |
|   3044905 | 1392 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   3044905 | 1393 | `			if( apNode[iNode] ){` |
|   3044905 | 1394 | `				if( sArgName.nByte > 0 ){` |
|       253 | 1395 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       253 | 1396 | `					apNode[iNode]->sArgName = sArgName;` |
|       124 | 1397 | `				}` |
|         - | 1398 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   3044905 | 1399 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   1522455 | 1400 | `			}else{` |
|         - | 1401 | `				/* No expression before comma */` |
|       ! 0 | 1402 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1403 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1404 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1405 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1406 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1407 | `				}` |
|       ! 0 | 1408 | `				return rc;` |
|         - | 1409 | `			}` |
|   1522455 | 1410 | `		}else{` |
|         - | 1411 | `			/* Comma with no preceding argument */` |
|         8 | 1412 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1413 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1414 | `				rc = SXERR_SYNTAX;` |
|         3 | 1415 | `			}` |
|         8 | 1416 | `			return rc;` |
|         - | 1417 | `		}` |
|         - | 1418 | `		/* Jump trailing comma */` |
|   3044905 | 1419 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    768503 | 1420 | `			iCur++;` |
|    768503 | 1421 | `			if( iCur >= nToken ){` |
|         - | 1422 | `				/* Trailing comma after last argument */` |
|        19 | 1423 | `				break;` |
|         - | 1424 | `			}` |
|    384240 | 1425 | `		}` |
|         5 | 1426 | `	}` |
|   2276425 | 1427 | `	return SXRET_OK;` |
|   1138219 | 1428 | `}` |
|         - | 1429 | ` /*` |
|         - | 1430 | `  * Create an expression tree from an array of tokens.` |
|         - | 1431 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1432 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1433 | `  */` |
|  12068920 | 1434 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1435 | ` {` |
|         - | 1436 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1437 | `	 ph7_expr_node *pNode;` |
|         - | 1438 | `	 sxi32 iCur;` |
|         - | 1439 | `	 sxi32 rc;` |
|  12068925 | 1440 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1441 | `		 /* TICKET 1433-17: self evaluating node */` |
|   5466507 | 1442 | `		 return SXRET_OK;` |
|         - | 1443 | `	 }` |
|         - | 1444 | `	 /* Process expressions enclosed in parenthesis first */` |
|  46973655 | 1445 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1446 | `		 sxi32 iNest;` |
|         - | 1447 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1448 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1449 | `		  */` |
|  40371239 | 1450 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  40196337 | 1451 | `			 continue;` |
|         - | 1452 | `		 }` |
|    174907 | 1453 | `		 iNest = 1;` |
|    174907 | 1454 | `		 iLeft = iCur;` |
|         - | 1455 | `		 /* Find the closing parenthesis */` |
|    174907 | 1456 | `		 iCur++;` |
|   1886259 | 1457 | `		 while( iCur < nToken ){` |
|   1886259 | 1458 | `			 if( apNode[iCur] ){` |
|   1886259 | 1459 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1460 | `					 /* Decrement nesting level */` |
|    272043 | 1461 | `					 iNest--;` |
|    272043 | 1462 | `					 if( iNest <= 0 ){` |
|    174907 | 1463 | `						 break;` |
|         5 | 1464 | `					 }` |
|   1662789 | 1465 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1466 | `					 /* Increment nesting level */` |
|     97141 | 1467 | `					 iNest++;` |
|     48568 | 1468 | `				 }` |
|    855676 | 1469 | `			 }` |
|   1711357 | 1470 | `			 iCur++;` |
|         5 | 1471 | `		 }` |
|    174907 | 1472 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1473 | `			 sxi32 j;` |
|         - | 1474 | `			 /* Recurse and process this expression */` |
|    174907 | 1475 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    174907 | 1476 | `			 if( rc != SXRET_OK ){` |
|         3 | 1477 | `				 return rc;` |
|         - | 1478 | `			 }` |
|         - | 1479 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1480 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1481 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    174905 | 1482 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    174905 | 1483 | `				 if( apNode[j] ){` |
|    174905 | 1484 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    174905 | 1485 | `					 break;` |
|         - | 1486 | `				 }` |
|       ! 0 | 1487 | `			 }` |
|     87450 | 1488 | `		 }` |
|         - | 1489 | `		 /* Free the left and right nodes */` |
|    174905 | 1490 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    174905 | 1491 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    174905 | 1492 | `		 apNode[iLeft] = 0;` |
|    174905 | 1493 | `		 apNode[iCur] = 0;` |
|     87455 | 1494 | `	 }` |
|         - | 1495 | `	  /* Process expressions enclosed in braces */` |
|  48743855 | 1496 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1497 | `		 sxi32 iNest;` |
|         - | 1498 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1499 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1500 | `		  */` |
|  42257467 | 1501 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  42257459 | 1502 | `			 continue;` |
|         - | 1503 | `		 }` |
|        10 | 1504 | `		 iNest = 1;` |
|        10 | 1505 | `		 iLeft = iCur;` |
|         - | 1506 | `		 /* Find the closing parenthesis */` |
|        10 | 1507 | `		 iCur++;` |
|        16 | 1508 | `		 while( iCur < nToken ){` |
|        16 | 1509 | `			 if( apNode[iCur] ){` |
|        16 | 1510 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1511 | `					 /* Decrement nesting level */` |
|        10 | 1512 | `					 iNest--;` |
|        10 | 1513 | `					 if( iNest <= 0 ){` |
|        10 | 1514 | `						 break;` |
|       ! 0 | 1515 | `					 }` |
|         7 | 1516 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1517 | `					 /* Increment nesting level */` |
|       ! 0 | 1518 | `					 iNest++;` |
|       ! 0 | 1519 | `				 }` |
|         3 | 1520 | `			 }` |
|         7 | 1521 | `			 iCur++;` |
|         1 | 1522 | `		 }` |
|        10 | 1523 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1524 | `			 /* Recurse and process this expression */` |
|         7 | 1525 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|         7 | 1526 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1527 | `				 return rc;` |
|         - | 1528 | `			 }` |
|         3 | 1529 | `		 }` |
|         - | 1530 | `		 /* Free the left and right nodes */` |
|        10 | 1531 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        10 | 1532 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        10 | 1533 | `		 apNode[iLeft] = 0;` |
|        10 | 1534 | `		 apNode[iCur] = 0;` |
|         6 | 1535 | `	 }` |
|         - | 1536 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   6486393 | 1537 | `	 iLeft = -1;` |
|  48743833 | 1538 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  42257457 | 1539 | `		 if( apNode[iCur] == 0 ){` |
|  18336995 | 1540 | `			 continue;` |
|         - | 1541 | `		 }` |
|  23920467 | 1542 | `		 pNode = apNode[iCur];` |
|  23920467 | 1543 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   6490831 | 1544 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1545 | `				 /* Collect function arguments */` |
|   3073473 | 1546 | `				 sxi32 iPtr = 0;` |
|   3073473 | 1547 | `				 sxi32 nFuncTok = 0;` |
|  13796661 | 1548 | `				 while( nFuncTok + iCur < nToken ){` |
|  13796661 | 1549 | `					 if( apNode[nFuncTok+iCur] ){` |
|  13781229 | 1550 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   3221247 | 1551 | `							 iPtr++;` |
|  12170608 | 1552 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   3221247 | 1553 | `							 iPtr--;` |
|   3221247 | 1554 | `							 if( iPtr <= 0 ){` |
|   3073473 | 1555 | `								 break;` |
|         - | 1556 | `							 }` |
|     73887 | 1557 | `						 }` |
|   5353878 | 1558 | `					 }` |
|  10723193 | 1559 | `					 nFuncTok++;` |
|         5 | 1560 | `				 }` |
|   3073473 | 1561 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1562 | `					 /* Syntax error */` |
|       ! 0 | 1563 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1564 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1565 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1566 | `					 }` |
|       ! 0 | 1567 | `					 return rc;` |
|         - | 1568 | `				 }` |
|   3073473 | 1569 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1570 | `					 /* Syntax error */` |
|       ! 0 | 1571 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1572 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1573 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1574 | `					 }` |
|       ! 0 | 1575 | `					 return rc;` |
|         - | 1576 | `				 }` |
|   3073473 | 1577 | `				 if( nFuncTok > 1 ){` |
|         - | 1578 | `					 /* Process function arguments */` |
|   2276433 | 1579 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   2276433 | 1580 | `					 if( rc != SXRET_OK ){` |
|        10 | 1581 | `						 return rc;` |
|         - | 1582 | `					 }` |
|   1138210 | 1583 | `				 }` |
|         - | 1584 | `				 /* Link the node to the tree */` |
|   3073465 | 1585 | `				 pNode->pLeft = apNode[iLeft];` |
|   3073465 | 1586 | `				 apNode[iLeft] = 0;` |
|  13796629 | 1587 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  10723169 | 1588 | `					 apNode[iCur+iPtr] = 0;` |
|   5361587 | 1589 | `				 }` |
|         - | 1590 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1591 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1592 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1593 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1594 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1595 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1596 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1597 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1598 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1599 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1600 | `				 {` |
|   3073465 | 1601 | `					 sxi32 iNew = iLeft - 1;` |
|   3854005 | 1602 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    780545 | 1603 | `						 iNew--;` |
|         5 | 1604 | `					 }` |
|   3073460 | 1605 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   2004263 | 1606 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   1228882 | 1607 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    455429 | 1608 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    455429 | 1609 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    455429 | 1610 | `						 apNode[iNew] = 0;` |
|    455429 | 1611 | `						 pNode = apNode[iCur];` |
|    227717 | 1612 | `					 }` |
|         - | 1613 | `				 }` |
|   4954093 | 1614 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1615 | `				 /* Subscripting */` |
|   1521193 | 1616 | `				 sxi32 iArrTok = iCur + 1;` |
|   1521193 | 1617 | `				 sxi32 iNest = 1;` |
|   1521188 | 1618 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1619 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1620 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1621 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   1521188 | 1622 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1623 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1624 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     57893 | 1625 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1626 | `						 /* Syntax error */` |
|       ! 0 | 1627 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1628 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1629 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1630 | `						 }` |
|       ! 0 | 1631 | `						 return rc;` |
|         - | 1632 | `				 }` |
|         - | 1633 | `				 /* Collect index tokens */` |
|   3015497 | 1634 | `				 while( iArrTok < nToken ){` |
|   3015497 | 1635 | `					 if( apNode[iArrTok] ){` |
|   3015465 | 1636 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1637 | `							 /* Increment nesting level */` |
|      3849 | 1638 | `							 iNest++;` |
|   3013543 | 1639 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1640 | `							 /* Decrement nesting level */` |
|   1525037 | 1641 | `							 iNest--;` |
|   1525037 | 1642 | `							 if( iNest <= 0 ){` |
|   1521193 | 1643 | `								 break;` |
|         - | 1644 | `							 }` |
|      1922 | 1645 | `						 }` |
|    747136 | 1646 | `					 }` |
|   1494309 | 1647 | `					 ++iArrTok;` |
|         5 | 1648 | `				 }` |
|   1521193 | 1649 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1650 | `					 /* Recurse and process this expression */` |
|   1398073 | 1651 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   1398073 | 1652 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1653 | `						 return rc;` |
|         - | 1654 | `					 }` |
|         - | 1655 | `					 /* Link the node to it's index */` |
|   1398073 | 1656 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    699034 | 1657 | `				 }` |
|         - | 1658 | `				 /* Link the node to the tree */` |
|   1521193 | 1659 | `				 pNode->pLeft = apNode[iLeft];` |
|   1521193 | 1660 | `				 pNode->pRight = 0;` |
|   1521193 | 1661 | `				 apNode[iLeft] = 0;` |
|   4536685 | 1662 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   3015497 | 1663 | `					 apNode[iNest] = 0;` |
|   1507751 | 1664 | `				 }` |
|    760599 | 1665 | `			 }else{` |
|         - | 1666 | `				 /* Member access operators [i.e: '->','::'] */` |
|   1896175 | 1667 | `				  iRight = iCur + 1;` |
|   1896181 | 1668 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         7 | 1669 | `					 iRight++;` |
|         1 | 1670 | `				 }` |
|   1896175 | 1671 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1672 | `					 /* Syntax error */` |
|         5 | 1673 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1674 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1675 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1676 | `					 }` |
|         5 | 1677 | `					 return rc;` |
|         - | 1678 | `				 }` |
|         - | 1679 | `				 /* Link the node to the tree */` |
|   1896171 | 1680 | `				 pNode->pLeft = apNode[iLeft];` |
|   1896166 | 1681 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   1882490 | 1682 | `					 && pNode->pLeft->pOp == 0 &&` |
|   1860837 | 1683 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|         - | 1684 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|         - | 1685 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|         - | 1686 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|         - | 1687 | `					  * accepts. */` |
|         4 | 1688 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|         - | 1689 | `						 /* Syntax error */` |
|       ! 0 | 1690 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1691 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|       ! 0 | 1692 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1693 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1694 | `						 }` |
|       ! 0 | 1695 | `						 return rc;` |
|         - | 1696 | `				 }` |
|   1896171 | 1697 | `				 pNode->pRight = apNode[iRight];` |
|   1896171 | 1698 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1699 | `			 }` |
|   3245407 | 1700 | `		 }` |
|  23920455 | 1701 | `		 iLeft = iCur;` |
|  11960230 | 1702 | `	 }` |
|         - | 1703 | `	 /* Handle left associative (new, clone) operators */` |
|  48743801 | 1704 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  42257425 | 1705 | `		 if( apNode[iCur] == 0 ){` |
|  25283511 | 1706 | `			 continue;` |
|         - | 1707 | `		 }` |
|  16973919 | 1708 | `		 pNode = apNode[iCur];` |
|  16973919 | 1709 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1710 | `			 SyToken *pToken;` |
|         - | 1711 | `			 /* Get the left node */` |
|       283 | 1712 | `			 iLeft = iCur + 1;` |
|       285 | 1713 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         3 | 1714 | `				 iLeft++;` |
|         1 | 1715 | `			 }` |
|       283 | 1716 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1717 | `				  /* Syntax error */` |
|       ! 0 | 1718 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1719 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1720 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1721 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1722 | `				 }` |
|       ! 0 | 1723 | `				 return rc;` |
|         - | 1724 | `			 }` |
|         - | 1725 | `			 /* Make sure the operand are of a valid type */` |
|       283 | 1726 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1727 | `				 /* Clone:` |
|         - | 1728 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|         - | 1729 | `				  *  ++ function call (including annonymous)` |
|         - | 1730 | `				  *  ++ array member` |
|         - | 1731 | `				  *  ++ 'new' operator` |
|         - | 1732 | `				  * Example:` |
|         - | 1733 | `				  *   clone $pObj;` |
|         - | 1734 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|         - | 1735 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|         - | 1736 | `				  */` |
|        40 | 1737 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|        38 | 1738 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1739 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1740 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1741 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1742 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1743 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1744 | `						 }` |
|       ! 0 | 1745 | `						 return rc;` |
|         - | 1746 | `					 }` |
|        17 | 1747 | `				 }` |
|        22 | 1748 | `			 }else{` |
|         - | 1749 | `				 /* New */` |
|       242 | 1750 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 1751 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 1752 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 1753 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 1754 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 1755 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 1756 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 1757 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 1758 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1759 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1760 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1761 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1762 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1763 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1764 | `					 }` |
|       ! 0 | 1765 | `					 return rc;` |
|         - | 1766 | `				 }` |
|       247 | 1767 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       247 | 1768 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       242 | 1769 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        31 | 1770 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 1771 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 1772 | `						 /* Syntax error */` |
|       ! 0 | 1773 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1774 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1775 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1776 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1777 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1778 | `						 }` |
|       ! 0 | 1779 | `						 return rc;` |
|         - | 1780 | `					 }` |
|       121 | 1781 | `				 }` |
|         - | 1782 | `			 }` |
|         - | 1783 | `			  /* Link the node to the tree */` |
|       283 | 1784 | `			 pNode->pLeft = apNode[iLeft];` |
|       283 | 1785 | `			 apNode[iLeft] = 0;` |
|       283 | 1786 | `			 pNode->pRight = 0; /* Paranoid */` |
|       139 | 1787 | `		 }` |
|   8486962 | 1788 | `	 }` |
|         - | 1789 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   6486381 | 1790 | `	 iLeft = -1;` |
|  48801815 | 1791 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  42257425 | 1792 | `		 if( apNode[iCur] == 0 ){` |
|  25283511 | 1793 | `			 continue;` |
|         - | 1794 | `		 }` |
|  16973919 | 1795 | `		 pNode = apNode[iCur];` |
|  16973919 | 1796 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     69615 | 1797 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     58051 | 1798 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1799 | `					 /* Link the node to the tree */` |
|     58063 | 1800 | `					 pNode->pLeft = apNode[iLeft];` |
|     58063 | 1801 | `					 apNode[iLeft] = 0;` |
|     29029 | 1802 | `			 }` |
|     92819 | 1803 | `		  }` |
|  17031933 | 1804 | `		 iLeft = iCur;` |
|   8544976 | 1805 | `	  }` |
|   6544395 | 1806 | `	 iLeft = -1;` |
|  48801815 | 1807 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  42257425 | 1808 | `		 if( apNode[iCur] == 0 ){` |
|  25341569 | 1809 | `			 continue;` |
|         - | 1810 | `		 }` |
|  16915861 | 1811 | `		 pNode = apNode[iCur];` |
|  16915861 | 1812 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     11552 | 1813 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     11557 | 1814 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1815 | `					 /* Syntax error */` |
|       ! 0 | 1816 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1817 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1818 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1819 | `					 }` |
|       ! 0 | 1820 | `					 return rc;` |
|         - | 1821 | `			 }` |
|         - | 1822 | `			 /* Link the node to the tree */` |
|     11557 | 1823 | `			 pNode->pLeft = apNode[iLeft];` |
|     11557 | 1824 | `			 apNode[iLeft] = 0;` |
|         - | 1825 | `			 /* Mark as pre-increment/decrement node */` |
|     11557 | 1826 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      5776 | 1827 | `		  }` |
|  16915861 | 1828 | `		 iLeft = iCur;` |
|   8457933 | 1829 | `	 }` |
|         - | 1830 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   6544395 | 1831 | `	  iLeft = 0;` |
|  48801809 | 1832 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  42257421 | 1833 | `		  if( apNode[iCur] ){` |
|  16904305 | 1834 | `			  pNode = apNode[iCur];` |
|  16904305 | 1835 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    267039 | 1836 | `				  if( iLeft > 0 ){` |
|         - | 1837 | `					  /* Link the node to the tree */` |
|    267037 | 1838 | `					  pNode->pLeft = apNode[iLeft];` |
|    267037 | 1839 | `					  apNode[iLeft] = 0;` |
|    267037 | 1840 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      7781 | 1841 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1842 | `							   /* Syntax error */` |
|       ! 0 | 1843 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|       ! 0 | 1844 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1845 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1846 | `							  }` |
|       ! 0 | 1847 | `							  return rc;` |
|         - | 1848 | `						  }` |
|      3888 | 1849 | `					  }` |
|    133521 | 1850 | `				  }else{` |
|         - | 1851 | `					  /* Syntax error */` |
|         3 | 1852 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|         3 | 1853 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1854 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1855 | `					  }` |
|         3 | 1856 | `					  return rc;` |
|         - | 1857 | `				  }` |
|    133516 | 1858 | `			  }` |
|         - | 1859 | `			  /* Save terminal position */` |
|  16904303 | 1860 | `			  iLeft = iCur;` |
|   8452149 | 1861 | `		  }` |
|  21128712 | 1862 | `	  }` |
|         - | 1863 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1864 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1865 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1866 | `	  * yielding a right-leaning tree. */` |
|  48801807 | 1867 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  42257419 | 1868 | `		 if( apNode[iCur] == 0 ){` |
|  25620265 | 1869 | `			 continue;` |
|         - | 1870 | `		 }` |
|  16637159 | 1871 | `		 pNode = apNode[iCur];` |
|  16637159 | 1872 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 1873 | `			 sxi32 iL, iR;` |
|         - | 1874 | `			 /* Find the right operand */` |
|       113 | 1875 | `			 iR = -1;` |
|         - | 1876 | `			 {` |
|         - | 1877 | `				 sxi32 j;` |
|       125 | 1878 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       125 | 1879 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 1880 | `				 }` |
|         - | 1881 | `			 }` |
|         - | 1882 | `			 /* Find the left operand */` |
|       113 | 1883 | `			 iL = -1;` |
|         - | 1884 | `			 {` |
|         - | 1885 | `				 sxi32 j;` |
|       181 | 1886 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       181 | 1887 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 1888 | `				 }` |
|         - | 1889 | `			 }` |
|       113 | 1890 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 1891 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1892 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 1893 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1894 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1895 | `				 }` |
|       ! 0 | 1896 | `				 return rc;` |
|         - | 1897 | `			 }` |
|       113 | 1898 | `			 pNode->pLeft  = apNode[iL];` |
|       113 | 1899 | `			 pNode->pRight = apNode[iR];` |
|       113 | 1900 | `			 apNode[iL] = 0;` |
|       113 | 1901 | `			 apNode[iR] = 0;` |
|         - | 1902 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 1903 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 1904 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 1905 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 1906 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 1907 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 1908 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 1909 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 1910 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 1911 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 1912 | `			  * operands are respected. */` |
|       112 | 1913 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        74 | 1914 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 1915 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 1916 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 1917 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 1918 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 1919 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 1920 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 1921 | `				 while( pTail->pLeft` |
|        34 | 1922 | `					 && pTail->pLeft->pOp` |
|        23 | 1923 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 1924 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 1925 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 1926 | `					 pTail = pTail->pLeft;` |
|         1 | 1927 | `				 }` |
|         - | 1928 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 1929 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 1930 | `				 pTail->pLeft = pNode;` |
|        27 | 1931 | `				 apNode[iCur] = pHead;` |
|        13 | 1932 | `			 }` |
|        56 | 1933 | `		 }` |
|   8318582 | 1934 | `	 }` |
|         - | 1935 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  71988187 | 1936 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  65443809 | 1937 | `		 iLeft = -1;` |
| 488017655 | 1938 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 422573861 | 1939 | `			 if( apNode[iCur] == 0 ){` |
| 287737709 | 1940 | `				 continue;` |
|         - | 1941 | `			 }` |
| 134836157 | 1942 | `			 pNode = apNode[iCur];` |
| 134836157 | 1943 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1944 | `				 /* Get the right node */` |
|   2404179 | 1945 | `				 iRight = iCur + 1;` |
|   3580445 | 1946 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   1176271 | 1947 | `					 iRight++;` |
|         5 | 1948 | `				 }` |
|   2404179 | 1949 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1950 | `					 /* Syntax error */` |
|        11 | 1951 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        11 | 1952 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1953 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1954 | `					 }` |
|        11 | 1955 | `					 return rc;` |
|         - | 1956 | `				 }` |
|   2404171 | 1957 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1958 | `					 sxi32  iTmp;` |
|         - | 1959 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1960 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 1961 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 1962 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 1963 | `					  * is swapped below. */` |
|        66 | 1964 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 1965 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 1966 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 1967 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1968 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1969 | `						 }` |
|         3 | 1970 | `						 return rc;` |
|         - | 1971 | `					 }` |
|        63 | 1972 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|         - | 1973 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 1974 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 1975 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1976 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1977 | `						 }` |
|       ! 0 | 1978 | `						 return rc;` |
|         - | 1979 | `					 }` |
|        63 | 1980 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|        45 | 1981 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 1982 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 1983 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 1984 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1985 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 1986 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1987 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 1988 | `									 }` |
|       ! 0 | 1989 | `									 return rc;` |
|         - | 1990 | `							 }` |
|       ! 0 | 1991 | `						 }` |
|        21 | 1992 | `					 }` |
|         - | 1993 | `					 /* Swap operands */` |
|        63 | 1994 | `					 iTmp = iRight;` |
|        63 | 1995 | `					 iRight = iLeft;` |
|        63 | 1996 | `					 iLeft = iTmp;` |
|        30 | 1997 | `				 }` |
|         - | 1998 | `				 /* Link the node to the tree */` |
|   2404169 | 1999 | `				 pNode->pLeft = apNode[iLeft];` |
|   2404169 | 2000 | `				 pNode->pRight = apNode[iRight];` |
|   2404169 | 2001 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   1202082 | 2002 | `			 }` |
| 134836147 | 2003 | `			 iLeft = iCur;` |
|  67418076 | 2004 | `		 }` |
|  32721902 | 2005 | `	 }` |
|         - | 2006 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2007 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2008 | `	  * we are dealing with a single operator.` |
|         - | 2009 | `	  */` |
|   6544383 | 2010 | `	  iLeft = -1;` |
|  47189513 | 2011 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  40844233 | 2012 | `		  if( apNode[iCur] == 0 ){` |
|  29612797 | 2013 | `			  continue;` |
|         - | 2014 | `		  }` |
|  11231441 | 2015 | `		  pNode = apNode[iCur];` |
|  11231441 | 2016 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    199103 | 2017 | `			  sxi32 iNest = 1;` |
|    199103 | 2018 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2019 | `				  /* Missing condition */` |
|         3 | 2020 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|         3 | 2021 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2022 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2023 | `				  }` |
|         3 | 2024 | `				  return rc;` |
|         - | 2025 | `			  }` |
|         - | 2026 | `			  /* Get the right node */` |
|    199101 | 2027 | `			  iRight = iCur + 1;` |
|    613779 | 2028 | `			  while( iRight < nToken  ){` |
|    613779 | 2029 | `				  if( apNode[iRight] ){` |
|    398129 | 2030 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2031 | `						  /* Increment nesting level */` |
|       ! 0 | 2032 | `						  ++iNest;` |
|    398129 | 2033 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2034 | `						  /* Decrement nesting level */` |
|    199101 | 2035 | `						  --iNest;` |
|    199101 | 2036 | `						  if( iNest <= 0 ){` |
|    199101 | 2037 | `							  break;` |
|         - | 2038 | `						  }` |
|       ! 0 | 2039 | `					  }` |
|     99514 | 2040 | `				  }` |
|    414683 | 2041 | `				  iRight++;` |
|         5 | 2042 | `			  }` |
|    199101 | 2043 | `			  if( iRight > iCur + 1 ){` |
|         - | 2044 | `				  /* Recurse and process the then expression */` |
|    199033 | 2045 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    199033 | 2046 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2047 | `					  return rc;` |
|         - | 2048 | `				  }` |
|         - | 2049 | `				  /* Link the node to the tree */` |
|    199033 | 2050 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     99514 | 2051 | `			  }else{` |
|         - | 2052 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2053 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2054 | `			  }` |
|    199101 | 2055 | `			  apNode[iCur + 1] = 0;` |
|    199101 | 2056 | `			  if( iRight + 1 < nToken ){` |
|         - | 2057 | `				  /* Recurse and process the else expression */` |
|    199101 | 2058 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    199101 | 2059 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2060 | `					  return rc;` |
|         - | 2061 | `				  }` |
|         - | 2062 | `				  /* Link the node to the tree */` |
|    199101 | 2063 | `				  pNode->pRight = apNode[iRight + 1];` |
|    199101 | 2064 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     99553 | 2065 | `			  }else{` |
|       ! 0 | 2066 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2067 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2068 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2069 | `				 }` |
|       ! 0 | 2070 | `				 return rc;` |
|         - | 2071 | `			  }` |
|         - | 2072 | `			  /* Point to the condition */` |
|    199101 | 2073 | `			  pNode->pCond  = apNode[iLeft];` |
|    199101 | 2074 | `			  apNode[iLeft] = 0;` |
|    199101 | 2075 | `			  break;` |
|         - | 2076 | `		  }` |
|  11032343 | 2077 | `		  iLeft = iCur;` |
|   5516174 | 2078 | `	  }` |
|         - | 2079 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2080 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2081 | `	  * so there is no need for a precedence loop here.` |
|         - | 2082 | `	  */` |
|   6544381 | 2083 | `	 iRight = -1;` |
|  48801611 | 2084 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  42257289 | 2085 | `		 if( apNode[iCur] == 0 ){` |
|  33468873 | 2086 | `			 continue;` |
|         - | 2087 | `		 }` |
|   8788421 | 2088 | `		 pNode = apNode[iCur];` |
|   8788421 | 2089 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2090 | `			 /* Get the left node */` |
|   2243923 | 2091 | `			 iLeft = iCur - 1;` |
|   2737531 | 2092 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    493613 | 2093 | `				 iLeft--;` |
|         5 | 2094 | `			 }` |
|   2243923 | 2095 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2096 | `				 /* Syntax error */` |
|        44 | 2097 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2098 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2099 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2100 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2101 | `				 }else{` |
|        40 | 2102 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|         - | 2103 | `				 }` |
|        44 | 2104 | `				 if( rc != SXERR_ABORT ){` |
|        42 | 2105 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2106 | `				 }` |
|        44 | 2107 | `				 return rc;` |
|         - | 2108 | `			 }` |
|         - | 2109 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2110 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2111 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2112 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2113 | `			  * a write. */` |
|   2243881 | 2114 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2115 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2116 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2117 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2118 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2119 | `				 }` |
|        11 | 2120 | `				 return rc;` |
|         - | 2121 | `			 }` |
|   2243873 | 2122 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       115 | 2123 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        82 | 2124 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2125 | `					 /* Left operand must be a modifiable l-value */` |
|         6 | 2126 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2127 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2128 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2129 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2130 | `					 }else{` |
|         4 | 2131 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2132 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2133 | `					 }` |
|         6 | 2134 | `					 if( rc != SXERR_ABORT ){` |
|         6 | 2135 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2136 | `					 }` |
|         6 | 2137 | `					 return rc;` |
|         - | 2138 | `				 }` |
|        40 | 2139 | `			 }` |
|         - | 2140 | `			 /* Link the node to the tree (Reverse) */` |
|   2243869 | 2141 | `			 pNode->pLeft = apNode[iRight];` |
|   2243869 | 2142 | `			 pNode->pRight = apNode[iLeft];` |
|   2243869 | 2143 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   1121932 | 2144 | `		 }` |
|   8788367 | 2145 | `		 iRight = iCur;` |
|   4394186 | 2146 | `	 }` |
|         - | 2147 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  32721615 | 2148 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  26177293 | 2149 | `		 iLeft = -1;` |
| 195206157 | 2150 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 169028869 | 2151 | `			 if( apNode[iCur] == 0 ){` |
| 142851175 | 2152 | `				 continue;` |
|         - | 2153 | `			 }` |
|  26177699 | 2154 | `			 pNode = apNode[iCur];` |
|  26177699 | 2155 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2156 | `				 /* Get the right node */` |
|        72 | 2157 | `				 iRight = iCur + 1;` |
|       110 | 2158 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        40 | 2159 | `					 iRight++;` |
|         2 | 2160 | `				 }` |
|        72 | 2161 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2162 | `					 /* Syntax error */` |
|       ! 0 | 2163 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 2164 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2165 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2166 | `					 }` |
|       ! 0 | 2167 | `					 return rc;` |
|         - | 2168 | `				 }` |
|         - | 2169 | `				 /* Link the node to the tree */` |
|        72 | 2170 | `				 pNode->pLeft = apNode[iLeft];` |
|        72 | 2171 | `				 pNode->pRight = apNode[iRight];` |
|        72 | 2172 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        35 | 2173 | `			 }` |
|  26177699 | 2174 | `			 iLeft = iCur;` |
|  13088852 | 2175 | `		 }` |
|  13088649 | 2176 | `	 }` |
|         - | 2177 | `	 /* Point to the root of the expression tree */` |
|  42257193 | 2178 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  35712889 | 2179 | `		 if( apNode[iCur] ){` |
|   6354753 | 2180 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        23 | 2181 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|        23 | 2182 | `				  if( rc != SXERR_ABORT ){` |
|        23 | 2183 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2184 | `				  }` |
|        23 | 2185 | `				  return rc;` |
|         - | 2186 | `			 }` |
|   6354735 | 2187 | `			 apNode[0] = apNode[iCur];` |
|   6354735 | 2188 | `			 apNode[iCur] = 0;` |
|   3177365 | 2189 | `		 }` |
|  17856438 | 2190 | `	 }` |
|   6544309 | 2191 | `	 return SXRET_OK;` |
|   6005458 | 2192 | ` }` |
|         - | 2193 | ` /*` |
|         - | 2194 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2195 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2196 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2197 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2198 | `  */` |
|   6994990 | 2199 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2200 | `{` |
|         - | 2201 | `	ph7_expr_node **apNode;` |
|         - | 2202 | `	ph7_expr_node *pNode;` |
|         - | 2203 | `	sxi32 rc;` |
|         - | 2204 | `	/* Reset node container */` |
|   6994995 | 2205 | `	SySetReset(pExprNode);` |
|   6994995 | 2206 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2207 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2208 | `	{` |
|   6994995 | 2209 | `		int iLastWasTerm = 0;` |
|   6994995 | 2210 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  43418805 | 2211 | `		while( pGen->pIn < pGen->pEnd ){` |
|  36423849 | 2212 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  36423849 | 2213 | `			if( rc != SXRET_OK ){` |
|        38 | 2214 | `				return rc;` |
|         - | 2215 | `			}` |
|         - | 2216 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  36423815 | 2217 | `			if( pNode->xCode ){` |
|         - | 2218 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  18380795 | 2219 | `				iLastWasTerm = 1;` |
|  27233420 | 2220 | `			}else if( pNode->pOp ){` |
|         - | 2221 | `				/* Operator node */` |
|   9825651 | 2222 | `				iLastWasTerm = 0;` |
|   4912828 | 2223 | `			}else{` |
|         - | 2224 | `				/* Delimiter: ')' and ']' end terms */` |
|   8217379 | 2225 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2226 | `			}` |
|         - | 2227 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2228 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2229 | `			 * node kind, so this single test covers all branches. */` |
|  36423815 | 2230 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2231 | `			/* Save the extracted node */` |
|  36423815 | 2232 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2233 | `		}` |
|         - | 2234 | `	}` |
|   6994961 | 2235 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2236 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2237 | `		*ppRoot = 0;` |
|       ! 0 | 2238 | `		return SXRET_OK;` |
|         - | 2239 | `	}` |
|   6994961 | 2240 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2241 | `	/* Make sure we are dealing with valid nodes */` |
|   6994961 | 2242 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   6994961 | 2243 | `	if( rc != SXRET_OK ){` |
|         - | 2244 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2245 | `		 * cleanup the mess left behind.` |
|         - | 2246 | `		 */` |
|        54 | 2247 | `		*ppRoot = 0;` |
|        54 | 2248 | `		return rc;` |
|         - | 2249 | `	}` |
|         - | 2250 | `	/* Build the tree */` |
|   6994911 | 2251 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   6994911 | 2252 | `	if( rc != SXRET_OK ){` |
|         - | 2253 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2254 | `		*ppRoot = 0;` |
|       103 | 2255 | `		return rc;` |
|         - | 2256 | `	}` |
|         - | 2257 | `	/* Point to the root of the tree */` |
|   6994813 | 2258 | `	*ppRoot = apNode[0];` |
|   6994813 | 2259 | `	return SXRET_OK;` |
|   3497500 | 2260 | `}` |
|         - | 2261 |  |
