# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1145/1315 lines (87.07%)

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
|  22055854 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  22055859 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 330000707 |  278 | `	for(;;){` |
| 660001419 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 660001419 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  64339149 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  32169577 |  285 | `		}else{` |
| 595662275 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 660001419 |  288 | `		if( rc == 0 ){` |
|  22299991 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  21979249 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|    320747 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|     23283 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|    297469 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|     53345 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     53345 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|     53337 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|    122066 |  307 | `		}` |
| 637945565 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|  11027932 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   6166962 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   6166967 |  320 | `	SyToken *pCur = pIn;` |
|   6166967 |  321 | `	sxi32 iNest = 1;` |
|  69412968 |  322 | `	for(;;){` |
| 138825941 |  323 | `		if( pCur >= pEnd ){` |
|     15717 |  324 | `			break;` |
|         - |  325 | `		}` |
| 138810229 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   5442081 |  328 | `			iNest++;` |
| 136089191 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|  11593331 |  331 | `			iNest--;` |
|  11593331 |  332 | `			if( iNest <= 0 ){` |
|   6151255 |  333 | `				break;` |
|         - |  334 | `			}` |
|   2721038 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
| 132658979 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   6166967 |  340 | `	*ppEnd = pCur;` |
|   6166967 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    423040 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    423040 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    422978 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       131 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    422919 |  358 | `	if( bCheckFunc ){` |
|     38420 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|     38413 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|     38394 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        51 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|     19187 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    422873 |  366 | `	return FALSE;` |
|    211525 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|  12751324 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|  12751329 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|  12751329 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  81897691 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  69146403 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|    214825 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  68931583 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   5811101 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    263436 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   5489783 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   5489783 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   5489783 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   5489783 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   2744889 |  401 | `					}` |
|   2744889 |  402 | `			}` |
|   5811101 |  403 | `			iParen++;` |
|  66026035 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   5811101 |  405 | `			if( iParen <= 0 ){` |
|        15 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        15 |  407 | `				if( rc != SXERR_ABORT ){` |
|        15 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        15 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   5811089 |  412 | `			iParen--;` |
|  60214933 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   2590919 |  414 | `			iSquare++;` |
|  56013934 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   2590923 |  416 | `			if( iSquare <= 0 ){` |
|         8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         8 |  418 | `				if( rc != SXERR_ABORT ){` |
|         8 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         8 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   2590917 |  423 | `			iSquare--;` |
|  53423015 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|      3815 |  425 | `			iBraces++;` |
|      3815 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  431 | `				if( rc != SXERR_ABORT ){` |
|         3 |  432 | `					rc = SXERR_SYNTAX;` |
|         1 |  433 | `				}` |
|         3 |  434 | `				return rc;` |
|         5 |  435 | `			}` |
|  52125653 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|      3825 |  437 | `			if( iBraces <= 0 ){` |
|        15 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        15 |  439 | `				if( rc != SXERR_ABORT ){` |
|        15 |  440 | `					rc = SXERR_SYNTAX;` |
|         6 |  441 | `				}` |
|        15 |  442 | `				return rc;` |
|         - |  443 | `			}` |
|      3813 |  444 | `			iBraces--;` |
|  52121833 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    383583 |  446 | `			if( iQuesty > 0 ){` |
|    383293 |  447 | `				iQuesty--;` |
|    191939 |  448 | `			}else if( iParen <= 0 ){` |
|         - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  453 | `				if( rc != SXERR_ABORT ){` |
|         6 |  454 | `					rc = SXERR_SYNTAX;` |
|         2 |  455 | `				}` |
|         6 |  456 | `				return rc;` |
|         5 |  457 | `			}` |
|  51928138 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|  17036455 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|  17036455 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    383295 |  461 | `				iQuesty++;` |
|  16844810 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     65139 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|         9 |  464 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|         9 |  465 | `					sxu32 n = 0;` |
|         9 |  466 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|         5 |  467 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|         2 |  468 | `					}` |
|         - |  469 | `					/*` |
|         - |  470 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|         - |  471 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|         - |  472 | `					 */` |
|       213 |  473 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|       205 |  474 | `						++n;` |
|         1 |  475 | `					}` |
|         9 |  476 | `					pOp = &aOpTable[n];` |
|         - |  477 | `					/* Mark as binary '+' or '-',not an unary */` |
|         9 |  478 | `					apNode[i]->pOp = pOp;` |
|         9 |  479 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|         4 |  480 | `				}` |
|     32567 |  481 | `			}` |
|   8518225 |  482 | `		}` |
|  34465776 |  483 | `	}` |
|  12751293 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        20 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        20 |  486 | `		if( rc != SXERR_ABORT ){` |
|        20 |  487 | `			rc = SXERR_SYNTAX;` |
|         8 |  488 | `		}` |
|        20 |  489 | `		return rc;` |
|         - |  490 | `	}` |
|  12751277 |  491 | `	return SXRET_OK;` |
|   6375667 |  492 | `}` |
|         - |  493 | `/*` |
|         - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  496 | ` */` |
|  10872252 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  498 | `{` |
|  10872257 |  499 | `	SyToken *pIn = *ppCur;` |
|         - |  500 | `	/* Jump the first literal seen */` |
|  10872257 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|  10868409 |  502 | `		pIn++;` |
|   5434202 |  503 | `	}` |
|   5438077 |  504 | `	for(;;){` |
|  10876159 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      3907 |  506 | `			pIn++;` |
|      3907 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3905 |  508 | `				pIn++;` |
|      1950 |  509 | `			}` |
|      1956 |  510 | `		}else{` |
|   5436131 |  511 | `			break;` |
|         - |  512 | `		}` |
|         5 |  513 | `	}` |
|         - |  514 | `	/* Synchronize pointers */` |
|  10872257 |  515 | `	*ppCur = pIn;` |
|  10872257 |  516 | `}` |
|         - |  517 | `/*` |
|         - |  518 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|         - |  519 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  520 | ` * Note on annonymous functions.` |
|         - |  521 | ` *  According to the PHP language reference manual:` |
|         - |  522 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  523 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  524 | ` *  parameters, but they have many other uses.` |
|         - |  525 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|         - |  526 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|         - |  527 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|         - |  528 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|         - |  529 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|         - |  530 | ` *` |
|         - |  531 | ` * Some example:` |
|         - |  532 | ` *  $greet = function($name)` |
|         - |  533 | ` * {` |
|         - |  534 | ` *   printf("Hello %s\r\n", $name);` |
|         - |  535 | ` * };` |
|         - |  536 | ` *  $greet('World');` |
|         - |  537 | ` *  $greet('PHP');` |
|         - |  538 | ` *` |
|         - |  539 | ` * $double = function($a) {` |
|         - |  540 | ` *   return $a * 2;` |
|         - |  541 | ` * };` |
|         - |  542 | ` * // This is our range of numbers` |
|         - |  543 | ` * $numbers = range(1, 5);` |
|         - |  544 | ` * // Use the Annonymous function as a callback here to` |
|         - |  545 | ` * // double the size of each element in our` |
|         - |  546 | ` * // range` |
|         - |  547 | ` * $new_numbers = array_map($double, $numbers);` |
|         - |  548 | ` * print implode(' ', $new_numbers);` |
|         - |  549 | ` */` |
|         - |  550 | `/*` |
|         - |  551 | ` * Skip an optional return-type declaration at *ppIn:` |
|         - |  552 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|         - |  553 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|         - |  554 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|         - |  555 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|         - |  556 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|         - |  557 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|         - |  558 | ` * authoritative type parse, so this must accept every shape it does` |
|         - |  559 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|         - |  560 | ` */` |
|       974 |  561 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  562 | `{` |
|       979 |  563 | `	SyToken *pIn = *ppIn;` |
|       979 |  564 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|        25 |  565 | `		pIn++; /* Skip ':' */` |
|        11 |  566 | `		for(;;){` |
|         - |  567 | `			/* Optional '?' nullable prefix */` |
|        29 |  568 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|         6 |  569 | `				pIn++;` |
|         2 |  570 | `			}` |
|        29 |  571 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  572 | `				/* Parenthesized DNF group '(A&B)' */` |
|       ! 0 |  573 | `				pIn++;` |
|       ! 0 |  574 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       ! 0 |  575 | `				if( pIn < pEnd ){` |
|       ! 0 |  576 | `					pIn++; /* ')' */` |
|       ! 0 |  577 | `				}` |
|        26 |  578 | `			}else if( pIn < pEnd` |
|        29 |  579 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  580 | `				/* ['\']Name('\'Name)* */` |
|        29 |  581 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|        29 |  582 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|        29 |  583 | `					pIn++;` |
|        29 |  584 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  585 | `						pIn += 2;` |
|       ! 0 |  586 | `					}` |
|        13 |  587 | `				}` |
|        16 |  588 | `			}else{` |
|         - |  589 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  590 | `				break;` |
|         - |  591 | `			}` |
|         - |  592 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|        26 |  593 | `			if( pIn < pEnd` |
|        29 |  594 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|        26 |  595 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|         5 |  596 | `				pIn++;` |
|         5 |  597 | `				continue;` |
|         - |  598 | `			}` |
|        25 |  599 | `			break;` |
|       ! 0 |  600 | `		}` |
|        11 |  601 | `	}` |
|       979 |  602 | `	*ppIn = pIn;` |
|       979 |  603 | `}` |
|       544 |  604 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  605 | `{` |
|       549 |  606 | `	SyToken *pIn = *ppCur;` |
|         - |  607 | `	sxi32 rc;` |
|         - |  608 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  609 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  610 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  611 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  612 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|       549 |  613 | `	pIn++;` |
|       544 |  614 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       282 |  615 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|         9 |  616 | `		pIn++;` |
|         4 |  617 | `	}` |
|       549 |  618 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  619 | `		/* Syntax error */` |
|         6 |  620 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  621 | `		if( rc != SXERR_ABORT ){` |
|         6 |  622 | `			rc = SXERR_SYNTAX;` |
|         2 |  623 | `		}` |
|         6 |  624 | `		goto Synchronize;` |
|         - |  625 | `	}` |
|       545 |  626 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       545 |  627 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       545 |  628 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  629 | `		/* Nothing follows the parameter list inside our slice: the body is missing.` |
|         - |  630 | `		 * php names the token that actually comes next (it lives just past the` |
|         - |  631 | `		 * expression slice, still in the raw stream) and says it wanted the '{'. */` |
|         6 |  632 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         6 |  633 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         6 |  634 | `		if( rc != SXERR_ABORT ){` |
|         6 |  635 | `			rc = SXERR_SYNTAX;` |
|         2 |  636 | `		}` |
|         6 |  637 | `		goto Synchronize;` |
|         - |  638 | `	}` |
|       541 |  639 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  640 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       541 |  641 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       541 |  642 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       107 |  643 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  644 | `		/* Check if we are dealing with a closure */` |
|       107 |  645 | `		if( nKey == PH7_TKWRD_USE ){` |
|        99 |  646 | `			pIn++; /* Jump the 'use' keyword */` |
|        99 |  647 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  648 | `				/* Syntax error */` |
|         6 |  649 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  650 | `				if( rc != SXERR_ABORT ){` |
|         6 |  651 | `					rc = SXERR_SYNTAX;` |
|         2 |  652 | `				}` |
|         6 |  653 | `				goto Synchronize;` |
|         - |  654 | `			}` |
|        95 |  655 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|        95 |  656 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        95 |  657 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  658 | `				/* Syntax error */` |
|         6 |  659 | `				rc = PH7_GenSyntaxError(&(*pGen),0 /* ran off the end */,0);` |
|         6 |  660 | `				if( rc != SXERR_ABORT ){` |
|         6 |  661 | `					rc = SXERR_SYNTAX;` |
|         2 |  662 | `				}` |
|         6 |  663 | `				goto Synchronize;` |
|         - |  664 | `			}` |
|        91 |  665 | `			pIn++;` |
|         - |  666 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  667 | ``			 * `function (...) use (...) : int {` */`` |
|        91 |  668 | `			ExprSkipReturnType(&pIn,pEnd);` |
|        48 |  669 | `		}else{` |
|         - |  670 | `			/* Syntax error */` |
|        11 |  671 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        11 |  672 | `			if( rc != SXERR_ABORT ){` |
|        11 |  673 | `				rc = SXERR_SYNTAX;` |
|         4 |  674 | `			}` |
|        11 |  675 | `			goto Synchronize;` |
|         - |  676 | `		}` |
|        43 |  677 | `	}` |
|         - |  678 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  679 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  680 | `	 * the type), and pEnd is one past the last token. */` |
|       525 |  681 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       525 |  682 | `		pIn++; /* Jump the leading curly '{' */` |
|       525 |  683 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       525 |  684 | `		if( pIn < pEnd ){` |
|       525 |  685 | `			pIn++;` |
|       260 |  686 | `		}` |
|       265 |  687 | `	}else{` |
|         - |  688 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|         - |  689 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|         - |  690 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|         - |  691 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|       ! 0 |  692 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|       ! 0 |  693 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|       ! 0 |  694 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  695 | `			return SXERR_ABORT;` |
|         - |  696 | `		}` |
|         - |  697 | `	}` |
|       525 |  698 | `	rc = SXRET_OK;` |
|       272 |  699 | `Synchronize:` |
|         - |  700 | `	/* Synchronize pointers */` |
|       549 |  701 | `	*ppCur = pIn;` |
|       549 |  702 | `	return rc;` |
|       277 |  703 | `}` |
|         - |  704 | `/*` |
|         - |  705 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  706 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  707 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  708 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  709 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  710 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  711 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  712 | ` */` |
|        28 |  713 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         4 |  714 | `{` |
|        32 |  715 | `	SyToken *pIn = *ppCur;` |
|        32 |  716 | `	sxu32 nLine = pIn->nLine;` |
|         - |  717 | `	sxi32 rc;` |
|        32 |  718 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  719 | `	/* Optional constructor argument list */` |
|        32 |  720 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         7 |  721 | `		pIn++; /* Jump '(' */` |
|         7 |  722 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         7 |  723 | `		if( pIn < pEnd ){` |
|         7 |  724 | `			pIn++; /* Jump ')' */` |
|         3 |  725 | `		}` |
|         3 |  726 | `	}` |
|         - |  727 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  728 | `	 * (no braces appear between ')' and the class body). */` |
|        60 |  729 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|        32 |  730 | `		pIn++;` |
|         4 |  731 | `	}` |
|        32 |  732 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  733 | `		/* Syntax error: missing class body */` |
|       ! 0 |  734 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  735 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  736 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  737 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  738 | `		}` |
|       ! 0 |  739 | `		*ppCur = pIn;` |
|       ! 0 |  740 | `		return rc;` |
|         - |  741 | `	}` |
|        32 |  742 | `	pIn++; /* Jump the leading '{' */` |
|        32 |  743 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        32 |  744 | `	if( pIn < pEnd ){` |
|        32 |  745 | `		pIn++; /* Jump the trailing '}' */` |
|        14 |  746 | `	}` |
|        32 |  747 | `	*ppCur = pIn;` |
|        32 |  748 | `	return SXRET_OK;` |
|        18 |  749 | `}` |
|         - |  750 | `/*` |
|         - |  751 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  752 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  753 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  754 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  755 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  756 | ` */` |
|       352 |  757 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  758 | `{` |
|       357 |  759 | `	SyToken *pIn = *ppCur;` |
|         - |  760 | `	sxu32 nLine;` |
|         - |  761 | `	sxi32 rc;` |
|         - |  762 | `	int iNest;` |
|       357 |  763 | `	nLine = pIn->nLine;` |
|         - |  764 | `	/* Optional 'static' prefix */` |
|       352 |  765 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       357 |  766 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  767 | `		pIn++;` |
|         3 |  768 | `	}` |
|         - |  769 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       352 |  770 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       357 |  771 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  772 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  773 | `		goto Synchronize;` |
|         - |  774 | `	}` |
|       357 |  775 | `	pIn++; /* Jump 'fn' */` |
|       176 |  776 | `	SXUNUSED(nLine);` |
|       176 |  777 | `	SXUNUSED(pGen);` |
|         - |  778 | `	/* Optional '&' for return-by-reference */` |
|       357 |  779 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  780 | `		pIn++;` |
|       ! 0 |  781 | `	}` |
|         - |  782 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  783 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  784 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  785 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       357 |  786 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       355 |  787 | `		pIn++; /* '(' */` |
|       355 |  788 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       355 |  789 | `		if( pIn < pEnd ){` |
|       353 |  790 | `			pIn++; /* ')' */` |
|       174 |  791 | `		}` |
|       175 |  792 | `	}` |
|         - |  793 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       357 |  794 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  795 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       357 |  796 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       351 |  797 | `		pIn++;` |
|       173 |  798 | `	}` |
|         - |  799 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       357 |  800 | `	iNest = 0;` |
|      2401 |  801 | `	while( pIn < pEnd ){` |
|      2289 |  802 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  803 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       241 |  804 | `			break;` |
|         - |  805 | `		}` |
|      2049 |  806 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       250 |  807 | `			iNest++;` |
|      1926 |  808 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       250 |  809 | `			iNest--;` |
|       123 |  810 | `		}` |
|      2049 |  811 | `		pIn++;` |
|         5 |  812 | `	}` |
|       357 |  813 | `	rc = SXRET_OK;` |
|       176 |  814 | `Synchronize:` |
|       357 |  815 | `	*ppCur = pIn;` |
|       357 |  816 | `	return rc;` |
|         5 |  817 | `}` |
|         - |  818 | `/*` |
|         - |  819 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - |  820 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - |  821 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - |  822 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - |  823 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - |  824 | ` */` |
|        72 |  825 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  826 | `{` |
|        77 |  827 | `	SyToken *pIn = *ppCur;` |
|         - |  828 | `	sxi32 rc;` |
|        36 |  829 | `	SXUNUSED(pGen);` |
|         - |  830 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|        72 |  831 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        77 |  832 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 |  833 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  834 | `		goto Synchronize;` |
|         - |  835 | `	}` |
|        77 |  836 | `	pIn++; /* Jump 'match' */` |
|         - |  837 | `	/* Optional '(' subject ')' */` |
|        77 |  838 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        77 |  839 | `		pIn++;` |
|        77 |  840 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        77 |  841 | `		if( pIn < pEnd ){` |
|        77 |  842 | `			pIn++; /* ')' */` |
|        36 |  843 | `		}` |
|        36 |  844 | `	}` |
|         - |  845 | `	/* Optional '{' arms '}' */` |
|        77 |  846 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|        77 |  847 | `		pIn++;` |
|        77 |  848 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|        77 |  849 | `		if( pIn < pEnd ){` |
|        77 |  850 | `			pIn++; /* '}' */` |
|        36 |  851 | `		}` |
|        36 |  852 | `	}` |
|        77 |  853 | `	rc = SXRET_OK;` |
|        36 |  854 | `Synchronize:` |
|        77 |  855 | `	*ppCur = pIn;` |
|        77 |  856 | `	return rc;` |
|         5 |  857 | `}` |
|         - |  858 | `/*` |
|         - |  859 | ` * Extract a single expression node from the input.` |
|         - |  860 | ` * On success store the freshly extractd node in ppNode.` |
|         - |  861 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  862 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - |  863 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - |  864 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - |  865 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - |  866 | ` */` |
|  69150602 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  868 | `{` |
|         - |  869 | `	ph7_expr_node *pNode;` |
|         - |  870 | `	SyToken *pCur;` |
|         - |  871 | `	sxi32 rc;` |
|         - |  872 | `	/* Allocate a new node */` |
|  69150607 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  69150607 |  874 | `	if( pNode == 0 ){` |
|         - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  877 | `		 */` |
|       ! 0 |  878 | `		return SXERR_MEM;` |
|         - |  879 | `	}` |
|         - |  880 | `	/* Zero the structure */` |
|  69150607 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  69150607 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  883 | `	/* Point to the head of the token stream */` |
|  69150607 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  885 | `	/* Start collecting tokens */` |
|  69150607 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4135 |  887 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - |  888 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - |  889 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - |  890 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - |  891 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|        81 |  892 | `			pNode->pEnd = pCur;` |
|        81 |  893 | `			pCur++;` |
|        81 |  894 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|        81 |  895 | `			pNode->xCode = PH7_CompileFccMarker;` |
|        81 |  896 | `			pGen->pIn = pCur;` |
|        81 |  897 | `			*ppNode = pNode;` |
|        81 |  898 | `			return SXRET_OK;` |
|         - |  899 | `		}` |
|         - |  900 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - |  901 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|      4055 |  902 | `		pCur++;` |
|      4055 |  903 | `		pGen->pIn = pCur;` |
|      4055 |  904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      4055 |  905 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      4055 |  906 | `		if( rc == SXRET_OK && *ppNode ){` |
|      4055 |  907 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      2025 |  908 | `		}` |
|      4055 |  909 | `		return rc;` |
|         - |  910 | `	}` |
|  69146477 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  912 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  913 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  914 | `		 */` |
|    214827 |  915 | `		pCur++; /* Skip the opening '[' */` |
|    214827 |  916 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|    214827 |  917 | `		if( pCur < pGen->pEnd ){` |
|    214827 |  918 | `			pCur++; /* Skip past the closing ']' */` |
|    107416 |  919 | `		}else{` |
|       ! 0 |  920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - |  921 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 |  922 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 |  923 | `				rc = SXERR_SYNTAX;` |
|       ! 0 |  924 | `			}` |
|       ! 0 |  925 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 |  926 | `			return rc;` |
|         - |  927 | `		}` |
|         - |  928 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - |  929 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - |  930 | `		 */` |
|    214999 |  931 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       346 |  932 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       346 |  933 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        56 |  934 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  935 | `			}else{` |
|       291 |  936 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  937 | `			}` |
|       174 |  938 | `		}else{` |
|    214483 |  939 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  940 | `		}` |
|  69039066 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - |  942 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - |  943 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - |  944 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - |  945 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - |  946 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - |  947 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - |  948 | `		 * call, not the clone() intrinsic. */` |
|        17 |  949 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        17 |  950 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        17 |  951 | `		pNode->xCode = PH7_CompileLiteral;` |
|  68931643 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|  19627420 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   9840346 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|         - |  955 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - |  956 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|         - |  957 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|         - |  958 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|         - |  959 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|         - |  960 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|         - |  961 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|         - |  962 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|         - |  963 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|         - |  964 | `		 * operator (which would dereference the NULL pOp). */` |
|        24 |  965 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        24 |  966 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|        24 |  967 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        24 |  968 | `		if( pCur < pGen->pEnd ){` |
|        24 |  969 | `			pCur++; /* skip the closing ')' */` |
|        13 |  970 | `		}else{` |
|       ! 0 |  971 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - |  972 | `				"clone: Missing closing parenthesis ')'");` |
|       ! 0 |  973 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 |  974 | `				rc = SXERR_SYNTAX;` |
|       ! 0 |  975 | `			}` |
|       ! 0 |  976 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 |  977 | `			return rc;` |
|         - |  978 | `		}` |
|        24 |  979 | `		pNode->xCode = PH7_CompileCloneCall;` |
|  68931628 |  980 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - |  981 | `		/* Point to the instance that describe this operator */` |
|  19627403 |  982 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - |  983 | `		/* Advance the stream cursor */` |
|  19627403 |  984 | `		pCur++;` |
|  59117918 |  985 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - |  986 | `		/* Isolate variable */` |
|  32420729 |  987 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  16210373 |  988 | `			pCur++; /* Variable variable */` |
|         5 |  989 | `		}` |
|  16210361 |  990 | `		if( pCur < pGen->pEnd ){` |
|  16210361 |  991 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - |  992 | `				/* Variable name */` |
|  16210333 |  993 | `				pCur++;` |
|   8105197 |  994 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        24 |  995 | `				pCur++;` |
|         - |  996 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        24 |  997 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        24 |  998 | `				if( pCur < pGen->pEnd ){` |
|        19 |  999 | `					pCur++;` |
|        11 | 1000 | `				}else{` |
|         5 | 1001 | `					rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         5 | 1002 | `					if( rc != SXERR_ABORT ){` |
|         5 | 1003 | `						rc = SXERR_SYNTAX;` |
|         2 | 1004 | `					}` |
|         5 | 1005 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         5 | 1006 | `					return rc;` |
|         - | 1007 | `				}` |
|         8 | 1008 | `			}` |
|   8105176 | 1009 | `		}` |
|  16210357 | 1010 | `		pNode->xCode = PH7_CompileVariable;` |
|  41199039 | 1011 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    817483 | 1012 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    817483 | 1013 | `		 if( bAfterMemberOp ){` |
|         - | 1014 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1015 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1016 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1017 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    125615 | 1018 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    125615 | 1019 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    754678 | 1020 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1021 | `			 /* List/Array node */` |
|    290689 | 1022 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1023 | `				 /* Assume a literal */` |
|       ! 0 | 1024 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1025 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1026 | `			 }else{` |
|    290689 | 1027 | `				 pCur += 2;` |
|         - | 1028 | `				 /* Collect array/list tokens */` |
|    290689 | 1029 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    290689 | 1030 | `				 if( pCur < pGen->pEnd ){` |
|    290687 | 1031 | `					 pCur++;` |
|    145346 | 1032 | `				 }else{` |
|         - | 1033 | `					 /* Syntax error */` |
|         4 | 1034 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1035 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1036 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1037 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1038 | `					 }` |
|         3 | 1039 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1040 | `					 return rc;` |
|         - | 1041 | `				 }` |
|    290687 | 1042 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    290687 | 1043 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        39 | 1044 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        39 | 1045 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1046 | `						 /* Syntax error */` |
|         3 | 1047 | `						 rc = PH7_GenSyntaxError(pGen,pNode->pStart,"\"=\"");` |
|         3 | 1048 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1049 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1050 | `						 }` |
|         3 | 1051 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1052 | `						 return rc;` |
|         - | 1053 | `					 }` |
|        16 | 1054 | `				 }` |
|         5 | 1055 | `			 }` |
|    546529 | 1056 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1057 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|     15589 | 1058 | `			 pCur++; /* Skip 'yield' keyword */` |
|     15589 | 1059 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1060 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1061 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|     15589 | 1062 | `			 pNode->xCode = PH7_CompileYield;` |
|    393397 | 1063 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|    385344 | 1064 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        52 | 1065 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        32 | 1066 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1067 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|       549 | 1068 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1069 | `				 /* Assume a literal */` |
|       ! 0 | 1070 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1071 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1072 | `			 }else{` |
|         - | 1073 | `				 /* Assemble annonymous functions body */` |
|       549 | 1074 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       549 | 1075 | `				 if( rc != SXRET_OK ){` |
|        28 | 1076 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1077 | `					 return rc;` |
|         - | 1078 | `				 }` |
|       525 | 1079 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1080 | `			  }` |
|    385321 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|        41 | 1082 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        23 | 1083 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        12 | 1084 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|         9 | 1085 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1086 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1087 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1088 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1089 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|        32 | 1090 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|        32 | 1091 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1092 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1093 | `				 return rc;` |
|         - | 1094 | `			 }` |
|        32 | 1095 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    385046 | 1096 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    384863 | 1097 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        44 | 1098 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        24 | 1099 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1100 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       357 | 1101 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       357 | 1102 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1103 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1104 | `				 return rc;` |
|         - | 1105 | `			 }` |
|       357 | 1106 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    384857 | 1107 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1108 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        77 | 1109 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        77 | 1110 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1111 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1112 | `				 return rc;` |
|         - | 1113 | `			 }` |
|        77 | 1114 | `			 pNode->xCode = PH7_CompileMatch;` |
|    384645 | 1115 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1116 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1117 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1118 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1119 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1120 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1121 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1122 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1123 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    384591 | 1124 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1125 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        75 | 1126 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        75 | 1127 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        40 | 1128 | `		 }else{` |
|         - | 1129 | `			 /* Assume a literal */` |
|    384503 | 1130 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    384503 | 1131 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1132 | `		 }` |
|  32685110 | 1133 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1134 | `		 /* Constants,function name,namespace path,class name... */` |
|  10362133 | 1135 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|  10362133 | 1136 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   5181069 | 1137 | `	 }else{` |
|  21914257 | 1138 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1139 | `			 /* Point to the code generator routine */` |
|   7309915 | 1140 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   7309915 | 1141 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1142 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 1143 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1144 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1145 | `				 }` |
|         3 | 1146 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1147 | `				 return rc;` |
|         - | 1148 | `			 }` |
|   3654954 | 1149 | `		 }` |
|         - | 1150 | `		/* Advance the stream cursor */` |
|  21914255 | 1151 | `		pCur++;` |
|         - | 1152 | `	 }` |
|         - | 1153 | `	/* Point to the end of the token stream */` |
|  69146443 | 1154 | `	pNode->pEnd = pCur;` |
|         - | 1155 | `	/* Save the node for later processing */` |
|  69146443 | 1156 | `	*ppNode = pNode;` |
|         - | 1157 | `	/* Synchronize cursors */` |
|  69146443 | 1158 | `	pGen->pIn = pCur;` |
|  69146443 | 1159 | `	return SXRET_OK;` |
|  34575306 | 1160 | `}` |
|         - | 1161 | `/*` |
|         - | 1162 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1163 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1164 | ` * level is zero.` |
|         - | 1165 | ` */` |
|   1316932 | 1166 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1167 | `{` |
|   1316937 | 1168 | `	SyToken *pCur = pStart;` |
|   1316937 | 1169 | `	sxi32 iNest = 0;` |
|   1316937 | 1170 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1171 | `		/* Last expression */` |
|    549023 | 1172 | `		return SXERR_EOF;` |
|         - | 1173 | `	}` |
|   3358953 | 1174 | `	while( pCur < pEnd ){` |
|   3116885 | 1175 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    525851 | 1176 | `			break;` |
|         - | 1177 | `		}` |
|   2591039 | 1178 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    212695 | 1179 | `			iNest++;` |
|   2484694 | 1180 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    212697 | 1181 | `			iNest--;` |
|    106346 | 1182 | `		}` |
|   2591039 | 1183 | `		pCur++;` |
|         5 | 1184 | `	}` |
|    767919 | 1185 | `	*ppNext = pCur;` |
|    767919 | 1186 | `	return SXRET_OK;` |
|    658471 | 1187 | `}` |
|         - | 1188 | `/*` |
|         - | 1189 | ` * Free an expression tree.` |
|         - | 1190 | ` */` |
|  59035736 | 1191 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1192 | `{` |
|  59035741 | 1193 | `	if( pNode->pLeft ){` |
|         - | 1194 | `		/* Release the left tree */` |
|  23466337 | 1195 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|  11733166 | 1196 | `	}` |
|  59035741 | 1197 | `	if( pNode->pRight ){` |
|         - | 1198 | `		/* Release the right tree */` |
|  13628603 | 1199 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   6814299 | 1200 | `	}` |
|  59035741 | 1201 | `	if( pNode->pCond ){` |
|         - | 1202 | `		/* Release the conditional tree used by the ternary operator */` |
|    383291 | 1203 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|    191643 | 1204 | `	}` |
|  59035741 | 1205 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1206 | `		ph7_expr_node **apArg;` |
|         - | 1207 | `		sxu32 n;` |
|         - | 1208 | `		/* Release node arguments */` |
|   6508251 | 1209 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  14663277 | 1210 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   8155031 | 1211 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   4077518 | 1212 | `		}` |
|   6508251 | 1213 | `		SySetRelease(&pNode->aNodeArgs);` |
|   3254123 | 1214 | `	}` |
|         - | 1215 | `	/* Finally,release this node */` |
|  59035741 | 1216 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  59035741 | 1217 | `}` |
|         - | 1218 | `/*` |
|         - | 1219 | ` * Free an expression tree.` |
|         - | 1220 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1221 | ` */` |
|  12751354 | 1222 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1223 | `{` |
|         - | 1224 | `	ph7_expr_node **apNode;` |
|         - | 1225 | `	sxu32 n;` |
|  12751359 | 1226 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  81897847 | 1227 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  69146493 | 1228 | `		if( apNode[n] ){` |
|  12751699 | 1229 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   6375847 | 1230 | `		}` |
|  34573249 | 1231 | `	}` |
|  12751359 | 1232 | `	return SXRET_OK;` |
|         5 | 1233 | `}` |
|         - | 1234 | `/*` |
|         - | 1235 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1236 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1237 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1238 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1239 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1240 | ` */` |
|  16666168 | 1241 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1242 | `{` |
|  16666173 | 1243 | `	if( pNode == 0 ){` |
|  10354973 | 1244 | `		return 0;` |
|         - | 1245 | `	}` |
|   6311205 | 1246 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1247 | `		return 1;` |
|         - | 1248 | `	}` |
|   6311193 | 1249 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1250 | `		return 1;` |
|         - | 1251 | `	}` |
|   6311189 | 1252 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1253 | `		return 1;` |
|         - | 1254 | `	}` |
|   6311189 | 1255 | `	return 0;` |
|   8333089 | 1256 | `}` |
|         - | 1257 | `/*` |
|         - | 1258 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1259 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1260 | ` */` |
|   4020910 | 1261 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1262 | `{` |
|         - | 1263 | `	sxi32 iExprOp;` |
|   4020915 | 1264 | `	if( pNode->pOp == 0 ){` |
|   2849039 | 1265 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1266 | `	}` |
|   1171881 | 1267 | `	iExprOp = pNode->pOp->iOp;` |
|   1171881 | 1268 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    779771 | 1269 | `			return TRUE;` |
|         - | 1270 | `	}` |
|    392115 | 1271 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    392109 | 1272 | `		if( pNode->pLeft->pOp ) {` |
|    117882 | 1273 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|     49436 | 1274 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1275 | `				return FALSE;` |
|         5 | 1276 | `			}` |
|    333168 | 1277 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1278 | `			return FALSE;` |
|         - | 1279 | `		}` |
|    392109 | 1280 | `		return TRUE;` |
|         - | 1281 | `	}` |
|         8 | 1282 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1283 | `		return TRUE;` |
|         - | 1284 | `	}` |
|         - | 1285 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1286 | `	return FALSE;` |
|   2010460 | 1287 | `}` |
|         - | 1288 | `/* Forward declaration */` |
|         - | 1289 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1290 | `/* Macro to check if the given node is a terminal.` |
|         - | 1291 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1292 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1293 | ` * linked ternary/elvis node). */` |
|         - | 1294 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1295 | `/*` |
|         - | 1296 | ` * Buid an expression tree for each given function argument.` |
|         - | 1297 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1298 | ` */` |
|   4118960 | 1299 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1300 | `{` |
|         - | 1301 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1302 | `	sxi32 rc;` |
|         - | 1303 | `	/* Process function arguments from left to right */` |
|   4118965 | 1304 | `	iCur = 0;` |
|   4942338 | 1305 | `	for(;;){` |
|   9884681 | 1306 | `		if( iCur >= nToken ){` |
|         - | 1307 | `			/* No more arguments to process */` |
|   4118939 | 1308 | `			break;` |
|         - | 1309 | `		}` |
|   5765747 | 1310 | `		iNode = iCur;` |
|   5765747 | 1311 | `		iNest = 0;` |
|  19150879 | 1312 | `		while( iCur < nToken ){` |
|  15031943 | 1313 | `			if( apNode[iCur] ){` |
|  14986165 | 1314 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    823408 | 1315 | `					break;` |
|  13339354 | 1316 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   7190392 | 1317 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|   1038994 | 1318 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1319 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1320 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1321 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1322 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1323 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1324 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|   1036553 | 1325 | `					iNest++;` |
|  12821085 | 1326 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|   1036553 | 1327 | `					iNest--;` |
|    518274 | 1328 | `				}` |
|   6669677 | 1329 | `			}` |
|  13385137 | 1330 | `			iCur++;` |
|         5 | 1331 | `		}` |
|   5765747 | 1332 | `		if( iCur > iNode ){` |
|   5765741 | 1333 | `			SyString sArgName = {0, 0};` |
|         - | 1334 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1335 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1336 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   5765736 | 1337 | `			if( (iCur - iNode) >= 2` |
|   3971879 | 1338 | `				&& apNode[iNode]` |
|   2178008 | 1339 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|   1154886 | 1340 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|    131524 | 1341 | `				&& apNode[iNode+1]` |
|    131275 | 1342 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1343 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       291 | 1344 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       291 | 1345 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       291 | 1346 | `				apNode[iNode] = 0;` |
|       291 | 1347 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       291 | 1348 | `				apNode[iNode+1] = 0;` |
|       291 | 1349 | `				iNode += 2;` |
|         - | 1350 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1351 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       291 | 1352 | `				if( iNode >= iCur ){` |
|         4 | 1353 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1354 | `						pOp->pStart->nLine,` |
|         - | 1355 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1356 | `						&sArgName);` |
|         3 | 1357 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1358 | `						rc = SXERR_SYNTAX;` |
|         1 | 1359 | `					}` |
|         3 | 1360 | `					return rc;` |
|         - | 1361 | `				}` |
|       142 | 1362 | `			}` |
|   5765734 | 1363 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1364 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1365 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1366 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1367 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1368 | `					apNode[iNode] = 0;` |
|       ! 0 | 1369 | `			}` |
|         - | 1370 | `			{` |
|         - | 1371 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1372 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1373 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1374 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1375 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1376 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1377 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1378 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1379 | `				 * NULL slots ahead of the flagged subtree. */` |
|   5765739 | 1380 | `				int bSpreadArg = 0;` |
|         - | 1381 | `				sxi32 iScan;` |
|   5765767 | 1382 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   5765767 | 1383 | `					if( apNode[iScan] ){` |
|   5765739 | 1384 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   5765739 | 1385 | `						break;` |
|         - | 1386 | `					}` |
|        15 | 1387 | `				}` |
|   5765739 | 1388 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   5765739 | 1389 | `				if( bSpreadArg && apNode[iNode] ){` |
|      3989 | 1390 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|      1992 | 1391 | `				}` |
|         - | 1392 | `			}` |
|   5765739 | 1393 | `			if( apNode[iNode] ){` |
|   5765739 | 1394 | `				if( sArgName.nByte > 0 ){` |
|       289 | 1395 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       289 | 1396 | `					apNode[iNode]->sArgName = sArgName;` |
|       142 | 1397 | `				}` |
|         - | 1398 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   5765739 | 1399 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   2882872 | 1400 | `			}else{` |
|         - | 1401 | `				/* No expression before comma */` |
|       ! 0 | 1402 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1403 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1404 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1405 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1406 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1407 | `				}` |
|       ! 0 | 1408 | `				return rc;` |
|         - | 1409 | `			}` |
|   2882872 | 1410 | `		}else{` |
|         - | 1411 | `			/* Comma with no preceding argument */` |
|         8 | 1412 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1413 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1414 | `				rc = SXERR_SYNTAX;` |
|         3 | 1415 | `			}` |
|         8 | 1416 | `			return rc;` |
|         - | 1417 | `		}` |
|         - | 1418 | `		/* Jump trailing comma */` |
|   5765739 | 1419 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|   1646805 | 1420 | `			iCur++;` |
|   1646805 | 1421 | `			if( iCur >= nToken ){` |
|         - | 1422 | `				/* Trailing comma after last argument */` |
|        19 | 1423 | `				break;` |
|         - | 1424 | `			}` |
|    823391 | 1425 | `		}` |
|         5 | 1426 | `	}` |
|   4118957 | 1427 | `	return SXRET_OK;` |
|   2059485 | 1428 | `}` |
|         - | 1429 | ` /*` |
|         - | 1430 | `  * Create an expression tree from an array of tokens.` |
|         - | 1431 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1432 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1433 | `  */` |
|  22119910 | 1434 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1435 | ` {` |
|         - | 1436 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1437 | `	 ph7_expr_node *pNode;` |
|         - | 1438 | `	 ph7_expr_node *pSuppress;` |
|         - | 1439 | `	 sxi32 iCur;` |
|         - | 1440 | `	 sxi32 rc;` |
|  22119915 | 1441 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1442 | `		 /* TICKET 1433-17: self evaluating node */` |
|   9632191 | 1443 | `		 return SXRET_OK;` |
|         - | 1444 | `	 }` |
|         - | 1445 | `	 /* Process expressions enclosed in parenthesis first */` |
|  90281651 | 1446 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1447 | `		 sxi32 iNest;` |
|         - | 1448 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1449 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1450 | `		  */` |
|  77793929 | 1451 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  77472621 | 1452 | `			 continue;` |
|         - | 1453 | `		 }` |
|    321313 | 1454 | `		 iNest = 1;` |
|    321313 | 1455 | `		 iLeft = iCur;` |
|         - | 1456 | `		 /* Find the closing parenthesis */` |
|    321313 | 1457 | `		 iCur++;` |
|   2853825 | 1458 | `		 while( iCur < nToken ){` |
|   2853825 | 1459 | `			 if( apNode[iCur] ){` |
|   2853825 | 1460 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1461 | `					 /* Decrement nesting level */` |
|    447893 | 1462 | `					 iNest--;` |
|    447893 | 1463 | `					 if( iNest <= 0 ){` |
|    321313 | 1464 | `						 break;` |
|         5 | 1465 | `					 }` |
|   2469227 | 1466 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1467 | `					 /* Increment nesting level */` |
|    126585 | 1468 | `					 iNest++;` |
|     63290 | 1469 | `				 }` |
|   1266256 | 1470 | `			 }` |
|   2532517 | 1471 | `			 iCur++;` |
|         5 | 1472 | `		 }` |
|    321313 | 1473 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1474 | `			 sxi32 j;` |
|         - | 1475 | `			 /* Recurse and process this expression */` |
|    321313 | 1476 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    321313 | 1477 | `			 if( rc != SXRET_OK ){` |
|         3 | 1478 | `				 return rc;` |
|         - | 1479 | `			 }` |
|         - | 1480 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1481 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1482 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1483 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1484 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1485 | `			  * group's free below silently drops the unpacking. */` |
|    321311 | 1486 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    321311 | 1487 | `				 if( apNode[j] ){` |
|    321311 | 1488 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    321306 | 1489 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    321311 | 1490 | `					 break;` |
|         - | 1491 | `				 }` |
|       ! 0 | 1492 | `			 }` |
|    160653 | 1493 | `		 }` |
|         - | 1494 | `		 /* Free the left and right nodes */` |
|    321311 | 1495 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    321311 | 1496 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    321311 | 1497 | `		 apNode[iLeft] = 0;` |
|    321311 | 1498 | `		 apNode[iCur] = 0;` |
|    160658 | 1499 | `	 }` |
|         - | 1500 | `	  /* Process expressions enclosed in braces */` |
|  92876257 | 1501 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1502 | `		 sxi32 iNest;` |
|         - | 1503 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1504 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1505 | `		  */` |
|  80640123 | 1506 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  80636315 | 1507 | `			 continue;` |
|         - | 1508 | `		 }` |
|      3813 | 1509 | `		 iNest = 1;` |
|      3813 | 1510 | `		 iLeft = iCur;` |
|         - | 1511 | `		 /* Find the closing parenthesis */` |
|      3813 | 1512 | `		 iCur++;` |
|      7619 | 1513 | `		 while( iCur < nToken ){` |
|      7619 | 1514 | `			 if( apNode[iCur] ){` |
|      7619 | 1515 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1516 | `					 /* Decrement nesting level */` |
|      3813 | 1517 | `					 iNest--;` |
|      3813 | 1518 | `					 if( iNest <= 0 ){` |
|      3813 | 1519 | `						 break;` |
|       ! 0 | 1520 | `					 }` |
|      3811 | 1521 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1522 | `					 /* Increment nesting level */` |
|       ! 0 | 1523 | `					 iNest++;` |
|       ! 0 | 1524 | `				 }` |
|      1903 | 1525 | `			 }` |
|      3811 | 1526 | `			 iCur++;` |
|         5 | 1527 | `		 }` |
|      3813 | 1528 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1529 | `			 /* Recurse and process this expression */` |
|      3811 | 1530 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      3811 | 1531 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1532 | `				 return rc;` |
|         - | 1533 | `			 }` |
|      1903 | 1534 | `		 }` |
|         - | 1535 | `		 /* Free the left and right nodes */` |
|      3813 | 1536 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      3813 | 1537 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      3813 | 1538 | `		 apNode[iLeft] = 0;` |
|      3813 | 1539 | `		 apNode[iCur] = 0;` |
|      1909 | 1540 | `	 }` |
|         - | 1541 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|  12236139 | 1542 | `	 iLeft = -1;` |
|  92883835 | 1543 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  80647713 | 1544 | `		 if( apNode[iCur] == 0 ){` |
|  35279945 | 1545 | `			 continue;` |
|         - | 1546 | `		 }` |
|  45367773 | 1547 | `		 pNode = apNode[iCur];` |
|  45367773 | 1548 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|  12936541 | 1549 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1550 | `				 /* Collect function arguments */` |
|   5489779 | 1551 | `				 sxi32 iPtr = 0;` |
|   5489779 | 1552 | `				 sxi32 nFuncTok = 0;` |
|  26011493 | 1553 | `				 while( nFuncTok + iCur < nToken ){` |
|  26011493 | 1554 | `					 if( apNode[nFuncTok+iCur] ){` |
|  25965715 | 1555 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   5800013 | 1556 | `							 iPtr++;` |
|  23065711 | 1557 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   5800013 | 1558 | `							 iPtr--;` |
|   5800013 | 1559 | `							 if( iPtr <= 0 ){` |
|   5489779 | 1560 | `								 break;` |
|         - | 1561 | `							 }` |
|    155117 | 1562 | `						 }` |
|  10237968 | 1563 | `					 }` |
|  20521719 | 1564 | `					 nFuncTok++;` |
|         5 | 1565 | `				 }` |
|   5489779 | 1566 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1567 | `					 /* Syntax error */` |
|       ! 0 | 1568 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1569 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1570 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1571 | `					 }` |
|       ! 0 | 1572 | `					 return rc;` |
|         - | 1573 | `				 }` |
|   5489779 | 1574 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1575 | `					 /* Syntax error */` |
|       ! 0 | 1576 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1577 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1578 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1579 | `					 }` |
|       ! 0 | 1580 | `					 return rc;` |
|         - | 1581 | `				 }` |
|   5489779 | 1582 | `				 if( nFuncTok > 1 ){` |
|         - | 1583 | `					 /* Process function arguments */` |
|   4118965 | 1584 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   4118965 | 1585 | `					 if( rc != SXRET_OK ){` |
|        11 | 1586 | `						 return rc;` |
|         - | 1587 | `					 }` |
|   2059476 | 1588 | `				 }` |
|         - | 1589 | `				 /* Link the node to the tree */` |
|   5489771 | 1590 | `				 pNode->pLeft = apNode[iLeft];` |
|   5489771 | 1591 | `				 apNode[iLeft] = 0;` |
|  26011461 | 1592 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  20521695 | 1593 | `					 apNode[iCur+iPtr] = 0;` |
|  10260850 | 1594 | `				 }` |
|         - | 1595 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1596 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1597 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1598 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1599 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1600 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1601 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1602 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1603 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1604 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1605 | `				 {` |
|   5489771 | 1606 | `					 sxi32 iNew = iLeft - 1;` |
|   7301083 | 1607 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|   1811317 | 1608 | `						 iNew--;` |
|         5 | 1609 | `					 }` |
|   5489766 | 1610 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   3242170 | 1611 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   1971972 | 1612 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    709383 | 1613 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    709383 | 1614 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    709383 | 1615 | `						 apNode[iNew] = 0;` |
|    709383 | 1616 | `						 pNode = apNode[iCur];` |
|    354694 | 1617 | `					 }` |
|         - | 1618 | `				 }` |
|  10191650 | 1619 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1620 | `				 /* Subscripting */` |
|   2590917 | 1621 | `				 sxi32 iArrTok = iCur + 1;` |
|   2590917 | 1622 | `				 sxi32 iNest = 1;` |
|   2590912 | 1623 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1624 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1625 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1626 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   2590912 | 1627 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1628 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1629 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|    298549 | 1630 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1631 | `						 /* Syntax error */` |
|       ! 0 | 1632 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1633 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1634 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1635 | `						 }` |
|       ! 0 | 1636 | `						 return rc;` |
|         - | 1637 | `				 }` |
|         - | 1638 | `				 /* Collect index tokens */` |
|   5421157 | 1639 | `				 while( iArrTok < nToken ){` |
|   5421157 | 1640 | `					 if( apNode[iArrTok] ){` |
|   5421125 | 1641 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1642 | `							 /* Increment nesting level */` |
|     19005 | 1643 | `							 iNest++;` |
|   5411625 | 1644 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1645 | `							 /* Decrement nesting level */` |
|   2609917 | 1646 | `							 iNest--;` |
|   2609917 | 1647 | `							 if( iNest <= 0 ){` |
|   2590917 | 1648 | `								 break;` |
|         - | 1649 | `							 }` |
|      9500 | 1650 | `						 }` |
|   1415104 | 1651 | `					 }` |
|   2830245 | 1652 | `					 ++iArrTok;` |
|         5 | 1653 | `				 }` |
|   2590917 | 1654 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1655 | `					 /* Recurse and process this expression */` |
|   2389297 | 1656 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   2389297 | 1657 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1658 | `						 return rc;` |
|         - | 1659 | `					 }` |
|         - | 1660 | `					 /* Link the node to it's index */` |
|   2389297 | 1661 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|   1194646 | 1662 | `				 }` |
|         - | 1663 | `				 /* Link the node to the tree */` |
|   2590917 | 1664 | `				 pNode->pLeft = apNode[iLeft];` |
|   2590917 | 1665 | `				 pNode->pRight = 0;` |
|   2590917 | 1666 | `				 apNode[iLeft] = 0;` |
|   8012069 | 1667 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   5421157 | 1668 | `					 apNode[iNest] = 0;` |
|   2710581 | 1669 | `				 }` |
|   1295461 | 1670 | `			 }else{` |
|         - | 1671 | `				 /* Member access operators [i.e: '->','::'] */` |
|   4855855 | 1672 | `				  iRight = iCur + 1;` |
|   4859661 | 1673 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      3811 | 1674 | `					 iRight++;` |
|         5 | 1675 | `				 }` |
|   4855855 | 1676 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1677 | `					 /* Syntax error */` |
|         5 | 1678 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1679 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1680 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1681 | `					 }` |
|         5 | 1682 | `					 return rc;` |
|         - | 1683 | `				 }` |
|         - | 1684 | `				 /* Link the node to the tree */` |
|   4855851 | 1685 | `				 pNode->pLeft = apNode[iLeft];` |
|   4855846 | 1686 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   4676803 | 1687 | `					 && pNode->pLeft->pOp == 0 &&` |
|   4421215 | 1688 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|         - | 1689 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|         - | 1690 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|         - | 1691 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|         - | 1692 | `					  * accepts. */` |
|         4 | 1693 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|         - | 1694 | `						 /* Syntax error */` |
|       ! 0 | 1695 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1696 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|       ! 0 | 1697 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1698 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1699 | `						 }` |
|       ! 0 | 1700 | `						 return rc;` |
|         - | 1701 | `				 }` |
|   4855851 | 1702 | `				 pNode->pRight = apNode[iRight];` |
|   4855851 | 1703 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1704 | `			 }` |
|   6468262 | 1705 | `		 }` |
|  45367761 | 1706 | `		 iLeft = iCur;` |
|  22683883 | 1707 | `	 }` |
|         - | 1708 | `	 /* Handle left associative (new, clone) operators */` |
|  92883803 | 1709 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  80647681 | 1710 | `		 if( apNode[iCur] == 0 ){` |
|  48979425 | 1711 | `			 continue;` |
|         - | 1712 | `		 }` |
|  31668261 | 1713 | `		 pNode = apNode[iCur];` |
|  31668261 | 1714 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1715 | `			 SyToken *pToken;` |
|         - | 1716 | `			 /* Get the left node */` |
|     53583 | 1717 | `			 iLeft = iCur + 1;` |
|     53591 | 1718 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         9 | 1719 | `				 iLeft++;` |
|         1 | 1720 | `			 }` |
|     53583 | 1721 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1722 | `				  /* Syntax error */` |
|       ! 0 | 1723 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1724 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1725 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1726 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1727 | `				 }` |
|       ! 0 | 1728 | `				 return rc;` |
|         - | 1729 | `			 }` |
|         - | 1730 | `			 /* Make sure the operand are of a valid type */` |
|     53583 | 1731 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1732 | `				 /* Clone:` |
|         - | 1733 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|         - | 1734 | `				  *  ++ function call (including annonymous)` |
|         - | 1735 | `				  *  ++ array member` |
|         - | 1736 | `				  *  ++ 'new' operator` |
|         - | 1737 | `				  * Example:` |
|         - | 1738 | `				  *   clone $pObj;` |
|         - | 1739 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|         - | 1740 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|         - | 1741 | `				  */` |
|     53245 | 1742 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|     53239 | 1743 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1744 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1745 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1746 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1747 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1748 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1749 | `						 }` |
|       ! 0 | 1750 | `						 return rc;` |
|         - | 1751 | `					 }` |
|     26617 | 1752 | `				 }` |
|     26625 | 1753 | `			 }else{` |
|         - | 1754 | `				 /* New */` |
|       338 | 1755 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 1756 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 1757 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 1758 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 1759 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 1760 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 1761 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 1762 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 1763 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1764 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1765 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1766 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1767 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1768 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1769 | `					 }` |
|       ! 0 | 1770 | `					 return rc;` |
|         - | 1771 | `				 }` |
|       343 | 1772 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       343 | 1773 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       338 | 1774 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        33 | 1775 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 1776 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 1777 | `						 /* Syntax error */` |
|       ! 0 | 1778 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1779 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1780 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1781 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1782 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1783 | `						 }` |
|       ! 0 | 1784 | `						 return rc;` |
|         - | 1785 | `					 }` |
|       169 | 1786 | `				 }` |
|         - | 1787 | `			 }` |
|         - | 1788 | `			  /* Link the node to the tree */` |
|     53583 | 1789 | `			 pNode->pLeft = apNode[iLeft];` |
|     53583 | 1790 | `			 apNode[iLeft] = 0;` |
|     53583 | 1791 | `			 pNode->pRight = 0; /* Paranoid */` |
|     26789 | 1792 | `		 }` |
|  15834133 | 1793 | `	 }` |
|         - | 1794 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|  12236127 | 1795 | `	 iLeft = -1;` |
|  93009597 | 1796 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  80647681 | 1797 | `		 if( apNode[iCur] == 0 ){` |
|  48979425 | 1798 | `			 continue;` |
|         - | 1799 | `		 }` |
|  31668261 | 1800 | `		 pNode = apNode[iCur];` |
|  31668261 | 1801 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    156303 | 1802 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|    133445 | 1803 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1804 | `					 /* Link the node to the tree */` |
|    141071 | 1805 | `					 pNode->pLeft = apNode[iLeft];` |
|    141071 | 1806 | `					 apNode[iLeft] = 0;` |
|     70533 | 1807 | `			 }` |
|    203943 | 1808 | `		  }` |
|  31794055 | 1809 | `		 iLeft = iCur;` |
|  15959927 | 1810 | `	  }` |
|  12361921 | 1811 | `	 iLeft = -1;` |
|  93009597 | 1812 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  80647681 | 1813 | `		 if( apNode[iCur] == 0 ){` |
|  49120491 | 1814 | `			 continue;` |
|         - | 1815 | `		 }` |
|  31527195 | 1816 | `		 pNode = apNode[iCur];` |
|  31527195 | 1817 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     15232 | 1818 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     15237 | 1819 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1820 | `					 /* Syntax error */` |
|       ! 0 | 1821 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1822 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1823 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1824 | `					 }` |
|       ! 0 | 1825 | `					 return rc;` |
|         - | 1826 | `			 }` |
|         - | 1827 | `			 /* Link the node to the tree */` |
|     15237 | 1828 | `			 pNode->pLeft = apNode[iLeft];` |
|     15237 | 1829 | `			 apNode[iLeft] = 0;` |
|         - | 1830 | `			 /* Mark as pre-increment/decrement node */` |
|     15237 | 1831 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      7616 | 1832 | `		  }` |
|  31527195 | 1833 | `		 iLeft = iCur;` |
|  15763600 | 1834 | `	 }` |
|         - | 1835 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|  12361921 | 1836 | `	  iLeft = 0;` |
|  93009591 | 1837 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  80647677 | 1838 | `		  if( apNode[iCur] ){` |
|  31511959 | 1839 | `			  pNode = apNode[iCur];` |
|  31511959 | 1840 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    841679 | 1841 | `				  if( iLeft > 0 ){` |
|         - | 1842 | `					  /* Link the node to the tree */` |
|    841677 | 1843 | `					  pNode->pLeft = apNode[iLeft];` |
|    841677 | 1844 | `					  apNode[iLeft] = 0;` |
|    841677 | 1845 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|     53293 | 1846 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1847 | `							   /* Syntax error */` |
|       ! 0 | 1848 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 1849 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1850 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1851 | `							  }` |
|       ! 0 | 1852 | `							  return rc;` |
|         - | 1853 | `						  }` |
|     26644 | 1854 | `					  }` |
|    420841 | 1855 | `				  }else{` |
|         - | 1856 | `					  /* Syntax error */` |
|         3 | 1857 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 1858 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1859 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1860 | `					  }` |
|         3 | 1861 | `					  return rc;` |
|         - | 1862 | `				  }` |
|    420836 | 1863 | `			  }` |
|         - | 1864 | `			  /* Save terminal position */` |
|  31511957 | 1865 | `			  iLeft = iCur;` |
|  15755976 | 1866 | `		  }` |
|  40323840 | 1867 | `	  }` |
|         - | 1868 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1869 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1870 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1871 | `	  * yielding a right-leaning tree. */` |
|  93009589 | 1872 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  80647675 | 1873 | `		 if( apNode[iCur] == 0 ){` |
|  49977509 | 1874 | `			 continue;` |
|         - | 1875 | `		 }` |
|  30670171 | 1876 | `		 pNode = apNode[iCur];` |
|  30670171 | 1877 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 1878 | `			 sxi32 iL, iR;` |
|         - | 1879 | `			 /* Find the right operand */` |
|       115 | 1880 | `			 iR = -1;` |
|         - | 1881 | `			 {` |
|         - | 1882 | `				 sxi32 j;` |
|       127 | 1883 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       127 | 1884 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 1885 | `				 }` |
|         - | 1886 | `			 }` |
|         - | 1887 | `			 /* Find the left operand */` |
|       115 | 1888 | `			 iL = -1;` |
|         - | 1889 | `			 {` |
|         - | 1890 | `				 sxi32 j;` |
|       183 | 1891 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       183 | 1892 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 1893 | `				 }` |
|         - | 1894 | `			 }` |
|       115 | 1895 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 1896 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 1897 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1898 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1899 | `				 }` |
|       ! 0 | 1900 | `				 return rc;` |
|         - | 1901 | `			 }` |
|       115 | 1902 | `			 pNode->pLeft  = apNode[iL];` |
|       115 | 1903 | `			 pNode->pRight = apNode[iR];` |
|       115 | 1904 | `			 apNode[iL] = 0;` |
|       115 | 1905 | `			 apNode[iR] = 0;` |
|         - | 1906 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 1907 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 1908 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 1909 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 1910 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 1911 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 1912 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 1913 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 1914 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 1915 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 1916 | `			  * operands are respected. */` |
|       114 | 1917 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        75 | 1918 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 1919 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 1920 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 1921 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 1922 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 1923 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 1924 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 1925 | `				 while( pTail->pLeft` |
|        34 | 1926 | `					 && pTail->pLeft->pOp` |
|        23 | 1927 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 1928 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 1929 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 1930 | `					 pTail = pTail->pLeft;` |
|         1 | 1931 | `				 }` |
|         - | 1932 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 1933 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 1934 | `				 pTail->pLeft = pNode;` |
|        27 | 1935 | `				 apNode[iCur] = pHead;` |
|        13 | 1936 | `			 }` |
|        57 | 1937 | `		 }` |
|  15335088 | 1938 | `	 }` |
|         - | 1939 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
| 135980973 | 1940 | `	 for( i = 7 ; i < 17 ; i++ ){` |
| 123619069 | 1941 | `		 iLeft = -1;` |
| 930095475 | 1942 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 806476421 | 1943 | `			 if( apNode[iCur] == 0 ){` |
| 554002709 | 1944 | `				 continue;` |
|         - | 1945 | `			 }` |
| 252473717 | 1946 | `			 pNode = apNode[iCur];` |
| 252473717 | 1947 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1948 | `				 /* Get the right node */` |
|   4368523 | 1949 | `				 iRight = iCur + 1;` |
|   6762677 | 1950 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   2394159 | 1951 | `					 iRight++;` |
|         5 | 1952 | `				 }` |
|   4368523 | 1953 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1954 | `					 /* Syntax error */` |
|        11 | 1955 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        11 | 1956 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1957 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1958 | `					 }` |
|        11 | 1959 | `					 return rc;` |
|         - | 1960 | `				 }` |
|   4368515 | 1961 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1962 | `					 sxi32  iTmp;` |
|         - | 1963 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1964 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 1965 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 1966 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 1967 | `					  * is swapped below. */` |
|        66 | 1968 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 1969 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 1970 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 1971 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1972 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1973 | `						 }` |
|         3 | 1974 | `						 return rc;` |
|         - | 1975 | `					 }` |
|        63 | 1976 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|         - | 1977 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 1978 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 1979 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1980 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1981 | `						 }` |
|       ! 0 | 1982 | `						 return rc;` |
|         - | 1983 | `					 }` |
|        63 | 1984 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|        45 | 1985 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 1986 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 1987 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 1988 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1989 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 1990 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1991 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 1992 | `									 }` |
|       ! 0 | 1993 | `									 return rc;` |
|         - | 1994 | `							 }` |
|       ! 0 | 1995 | `						 }` |
|        21 | 1996 | `					 }` |
|         - | 1997 | `					 /* Swap operands */` |
|        63 | 1998 | `					 iTmp = iRight;` |
|        63 | 1999 | `					 iRight = iLeft;` |
|        63 | 2000 | `					 iLeft = iTmp;` |
|        30 | 2001 | `				 }` |
|         - | 2002 | `				 /* Link the node to the tree */` |
|   4368513 | 2003 | `				 pNode->pLeft = apNode[iLeft];` |
|   4368513 | 2004 | `				 pNode->pRight = apNode[iRight];` |
|   4368513 | 2005 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   2184254 | 2006 | `			 }` |
| 252473707 | 2007 | `			 iLeft = iCur;` |
| 126236856 | 2008 | `		 }` |
|  61809532 | 2009 | `	 }` |
|         - | 2010 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2011 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2012 | `	  * we are dealing with a single operator.` |
|         - | 2013 | `	  */` |
|  12361909 | 2014 | `	  iLeft = -1;` |
|  89860417 | 2015 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  77881801 | 2016 | `		  if( apNode[iCur] == 0 ){` |
|  57094813 | 2017 | `			  continue;` |
|         - | 2018 | `		  }` |
|  20786993 | 2019 | `		  pNode = apNode[iCur];` |
|  20786993 | 2020 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    383293 | 2021 | `			  sxi32 iNest = 1;` |
|    383293 | 2022 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2023 | `				  /* Missing condition */` |
|         3 | 2024 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 2025 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2026 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2027 | `				  }` |
|         3 | 2028 | `				  return rc;` |
|         - | 2029 | `			  }` |
|         - | 2030 | `			  /* Get the right node */` |
|    383291 | 2031 | `			  iRight = iCur + 1;` |
|   1557279 | 2032 | `			  while( iRight < nToken  ){` |
|   1557279 | 2033 | `				  if( apNode[iRight] ){` |
|    762709 | 2034 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2035 | `						  /* Increment nesting level */` |
|       ! 0 | 2036 | `						  ++iNest;` |
|    762709 | 2037 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2038 | `						  /* Decrement nesting level */` |
|    383291 | 2039 | `						  --iNest;` |
|    383291 | 2040 | `						  if( iNest <= 0 ){` |
|    383291 | 2041 | `							  break;` |
|         - | 2042 | `						  }` |
|       ! 0 | 2043 | `					  }` |
|    189709 | 2044 | `				  }` |
|   1173993 | 2045 | `				  iRight++;` |
|         5 | 2046 | `			  }` |
|    383291 | 2047 | `			  if( iRight > iCur + 1 ){` |
|         - | 2048 | `				  /* Recurse and process the then expression */` |
|    379423 | 2049 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    379423 | 2050 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2051 | `					  return rc;` |
|         - | 2052 | `				  }` |
|         - | 2053 | `				  /* Link the node to the tree */` |
|    379423 | 2054 | `				  pNode->pLeft = apNode[iCur + 1];` |
|    189709 | 2055 | `			  }else{` |
|         - | 2056 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2057 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2058 | `			  }` |
|    383291 | 2059 | `			  apNode[iCur + 1] = 0;` |
|    383291 | 2060 | `			  if( iRight + 1 < nToken ){` |
|         - | 2061 | `				  /* Recurse and process the else expression */` |
|    383291 | 2062 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    383291 | 2063 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2064 | `					  return rc;` |
|         - | 2065 | `				  }` |
|         - | 2066 | `				  /* Link the node to the tree */` |
|    383291 | 2067 | `				  pNode->pRight = apNode[iRight + 1];` |
|    383291 | 2068 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|    191648 | 2069 | `			  }else{` |
|       ! 0 | 2070 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2071 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2072 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2073 | `				 }` |
|       ! 0 | 2074 | `				 return rc;` |
|         - | 2075 | `			  }` |
|         - | 2076 | `			  /* Point to the condition */` |
|    383291 | 2077 | `			  pNode->pCond  = apNode[iLeft];` |
|    383291 | 2078 | `			  apNode[iLeft] = 0;` |
|    383291 | 2079 | `			  break;` |
|         - | 2080 | `		  }` |
|  20403705 | 2081 | `		  iLeft = iCur;` |
|  10201855 | 2082 | `	  }` |
|         - | 2083 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2084 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2085 | `	  * so there is no need for a precedence loop here.` |
|         - | 2086 | `	  */` |
|  12361907 | 2087 | `	 iRight = -1;` |
|  93009393 | 2088 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  80647545 | 2089 | `		 if( apNode[iCur] == 0 ){` |
|  64264707 | 2090 | `			 continue;` |
|         - | 2091 | `		 }` |
|  16382843 | 2092 | `		 pNode = apNode[iCur];` |
|  16382843 | 2093 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2094 | `			 /* Get the left node */` |
|   4020863 | 2095 | `			 iLeft = iCur - 1;` |
|   5508697 | 2096 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   1487839 | 2097 | `				 iLeft--;` |
|         5 | 2098 | `			 }` |
|   4020863 | 2099 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2100 | `				 /* Syntax error */` |
|        45 | 2101 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2102 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2103 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2104 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2105 | `				 }else{` |
|        41 | 2106 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2107 | `				 }` |
|        45 | 2108 | `				 if( rc != SXERR_ABORT ){` |
|        43 | 2109 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2110 | `				 }` |
|        45 | 2111 | `				 return rc;` |
|         - | 2112 | `			 }` |
|         - | 2113 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2114 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2115 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2116 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2117 | `			  * a write. */` |
|   4020821 | 2118 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2119 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2120 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2121 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2122 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2123 | `				 }` |
|        11 | 2124 | `				 return rc;` |
|         - | 2125 | `			 }` |
|         - | 2126 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|         - | 2127 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|         - | 2128 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|         - | 2129 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|         - | 2130 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|   4020813 | 2131 | `			 pSuppress = 0;` |
|   4020808 | 2132 | `			 if( apNode[iLeft]->pOp` |
|   2596328 | 2133 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|    585924 | 2134 | `				 && apNode[iLeft]->pLeft != 0` |
|         5 | 2135 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2136 | `				 pSuppress = apNode[iLeft];` |
|       ! 0 | 2137 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|       ! 0 | 2138 | `			 }` |
|   4020813 | 2139 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       123 | 2140 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        88 | 2141 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2142 | `					 /* Left operand must be a modifiable l-value */` |
|         6 | 2143 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2144 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2145 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2146 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2147 | `					 }else{` |
|         4 | 2148 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2149 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2150 | `					 }` |
|         6 | 2151 | `					 if( rc != SXERR_ABORT ){` |
|         6 | 2152 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2153 | `					 }` |
|         6 | 2154 | `					 return rc;` |
|         - | 2155 | `				 }` |
|        43 | 2156 | `			 }` |
|         - | 2157 | `			 /* Link the node to the tree (Reverse) */` |
|   4020809 | 2158 | `			 pNode->pLeft = apNode[iRight];` |
|   4020809 | 2159 | `			 pNode->pRight = apNode[iLeft];` |
|   4020809 | 2160 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   4020809 | 2161 | `			 if( pSuppress ){` |
|         - | 2162 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|       ! 0 | 2163 | `				 pSuppress->pLeft = pNode;` |
|       ! 0 | 2164 | `				 apNode[iCur] = pSuppress;` |
|       ! 0 | 2165 | `			 }` |
|   2010402 | 2166 | `		 }` |
|  16382789 | 2167 | `		 iRight = iCur;` |
|   8191397 | 2168 | `	 }` |
|         - | 2169 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  61809245 | 2170 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  49447397 | 2171 | `		 iLeft = -1;` |
| 372037285 | 2172 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 322589893 | 2173 | `			 if( apNode[iCur] == 0 ){` |
| 273142249 | 2174 | `				 continue;` |
|         - | 2175 | `			 }` |
|  49447649 | 2176 | `			 pNode = apNode[iCur];` |
|  49447649 | 2177 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2178 | `				 /* Get the right node */` |
|        51 | 2179 | `				 iRight = iCur + 1;` |
|        63 | 2180 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        13 | 2181 | `					 iRight++;` |
|         1 | 2182 | `				 }` |
|        51 | 2183 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2184 | `					 /* Syntax error */` |
|       ! 0 | 2185 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2186 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2187 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2188 | `					 }` |
|       ! 0 | 2189 | `					 return rc;` |
|         - | 2190 | `				 }` |
|         - | 2191 | `				 /* Link the node to the tree */` |
|        51 | 2192 | `				 pNode->pLeft = apNode[iLeft];` |
|        51 | 2193 | `				 pNode->pRight = apNode[iRight];` |
|        51 | 2194 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        24 | 2195 | `			 }` |
|  49447649 | 2196 | `			 iLeft = iCur;` |
|  24723827 | 2197 | `		 }` |
|  24723701 | 2198 | `	 }` |
|         - | 2199 | `	 /* Point to the root of the expression tree */` |
|  80647449 | 2200 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  68285619 | 2201 | `		 if( apNode[iCur] ){` |
|  11874063 | 2202 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        22 | 2203 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|        22 | 2204 | `				  if( rc != SXERR_ABORT ){` |
|        22 | 2205 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2206 | `				  }` |
|        22 | 2207 | `				  return rc;` |
|         - | 2208 | `			 }` |
|  11874045 | 2209 | `			 apNode[0] = apNode[iCur];` |
|  11874045 | 2210 | `			 apNode[iCur] = 0;` |
|   5937020 | 2211 | `		 }` |
|  34142803 | 2212 | `	 }` |
|  12361835 | 2213 | `	 return SXRET_OK;` |
|  10997063 | 2214 | ` }` |
|         - | 2215 | ` /*` |
|         - | 2216 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2217 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2218 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2219 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2220 | `  */` |
|  12751358 | 2221 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2222 | `{` |
|         - | 2223 | `	ph7_expr_node **apNode;` |
|         - | 2224 | `	ph7_expr_node *pNode;` |
|         - | 2225 | `	sxi32 rc;` |
|         - | 2226 | `	/* Reset node container */` |
|  12751363 | 2227 | `	SySetReset(pExprNode);` |
|  12751363 | 2228 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2229 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2230 | `	{` |
|  12751363 | 2231 | `		int iLastWasTerm = 0;` |
|  12751363 | 2232 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  81897881 | 2233 | `		while( pGen->pIn < pGen->pEnd ){` |
|  69146557 | 2234 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  69146557 | 2235 | `			if( rc != SXRET_OK ){` |
|        38 | 2236 | `				return rc;` |
|         - | 2237 | `			}` |
|         - | 2238 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  69146523 | 2239 | `			if( pNode->xCode ){` |
|         - | 2240 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  34914783 | 2241 | `				iLastWasTerm = 1;` |
|  51689134 | 2242 | `			}else if( pNode->pOp ){` |
|         - | 2243 | `				/* Operator node */` |
|  19627403 | 2244 | `				iLastWasTerm = 0;` |
|   9813704 | 2245 | `			}else{` |
|         - | 2246 | `				/* Delimiter: ')' and ']' end terms */` |
|  14604347 | 2247 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2248 | `			}` |
|         - | 2249 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2250 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2251 | `			 * node kind, so this single test covers all branches. */` |
|  69146523 | 2252 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2253 | `			/* Save the extracted node */` |
|  69146523 | 2254 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2255 | `		}` |
|         - | 2256 | `	}` |
|  12751329 | 2257 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2258 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2259 | `		*ppRoot = 0;` |
|       ! 0 | 2260 | `		return SXRET_OK;` |
|         - | 2261 | `	}` |
|  12751329 | 2262 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2263 | `	/* Make sure we are dealing with valid nodes */` |
|  12751329 | 2264 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  12751329 | 2265 | `	if( rc != SXRET_OK ){` |
|         - | 2266 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2267 | `		 * cleanup the mess left behind.` |
|         - | 2268 | `		 */` |
|        56 | 2269 | `		*ppRoot = 0;` |
|        56 | 2270 | `		return rc;` |
|         - | 2271 | `	}` |
|         - | 2272 | `	/* Build the tree */` |
|  12751277 | 2273 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  12751277 | 2274 | `	if( rc != SXRET_OK ){` |
|         - | 2275 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2276 | `		*ppRoot = 0;` |
|       103 | 2277 | `		return rc;` |
|         - | 2278 | `	}` |
|         - | 2279 | `	/* Point to the root of the tree */` |
|  12751179 | 2280 | `	*ppRoot = apNode[0];` |
|  12751179 | 2281 | `	return SXRET_OK;` |
|   6375684 | 2282 | `}` |
|         - | 2283 |  |
