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
|  21732498 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  21732503 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 324636793 |  278 | `	for(;;){` |
| 649273591 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 649273591 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  63356327 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  31678166 |  285 | `		}else{` |
| 585917269 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 649273591 |  288 | `		if( rc == 0 ){` |
|  21976251 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  21656013 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|    320243 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|     23247 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|    297001 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|     53261 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     53261 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|     53253 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|    121874 |  307 | `		}` |
| 627541093 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|  10866254 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   6084960 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   6084965 |  320 | `	SyToken *pCur = pIn;` |
|   6084965 |  321 | `	sxi32 iNest = 1;` |
|  68318469 |  322 | `	for(;;){` |
| 136636943 |  323 | `		if( pCur >= pEnd ){` |
|     15693 |  324 | `			break;` |
|         - |  325 | `		}` |
| 136621255 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   5315909 |  328 | `			iNest++;` |
| 133963303 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|  11385181 |  331 | `			iNest--;` |
|  11385181 |  332 | `			if( iNest <= 0 ){` |
|   6069277 |  333 | `				break;` |
|         - |  334 | `			}` |
|   2657952 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
| 130551983 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   6084965 |  340 | `	*ppEnd = pCur;` |
|   6084965 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    414798 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    414798 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    414736 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       131 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    414677 |  358 | `	if( bCheckFunc ){` |
|     38360 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|     38353 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|     38334 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        51 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|     19157 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    414631 |  366 | `	return FALSE;` |
|    207404 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|  12575418 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|  12575423 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|  12575423 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  80538239 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  67962857 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|    214453 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  67748409 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   5699373 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    263022 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   5378563 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   5378563 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   5378563 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   5378563 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   2689279 |  401 | `					}` |
|   2689279 |  402 | `			}` |
|   5699373 |  403 | `			iParen++;` |
|  64898725 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   5699373 |  405 | `			if( iParen <= 0 ){` |
|        16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        16 |  407 | `				if( rc != SXERR_ABORT ){` |
|        16 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        16 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   5699361 |  412 | `			iParen--;` |
|  59199351 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   2518543 |  414 | `			iSquare++;` |
|  55090404 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   2518547 |  416 | `			if( iSquare <= 0 ){` |
|         9 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         9 |  418 | `				if( rc != SXERR_ABORT ){` |
|         9 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         9 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   2518541 |  423 | `			iSquare--;` |
|  52571861 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|      3809 |  425 | `			iBraces++;` |
|      3809 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  427 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  428 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  429 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  430 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  431 | `				if( rc != SXERR_ABORT ){` |
|         3 |  432 | `					rc = SXERR_SYNTAX;` |
|         1 |  433 | `				}` |
|         3 |  434 | `				return rc;` |
|         5 |  435 | `			}` |
|  51310690 |  436 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|      3819 |  437 | `			if( iBraces <= 0 ){` |
|        16 |  438 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        16 |  439 | `				if( rc != SXERR_ABORT ){` |
|        16 |  440 | `					rc = SXERR_SYNTAX;` |
|         6 |  441 | `				}` |
|        16 |  442 | `				return rc;` |
|         - |  443 | `			}` |
|      3807 |  444 | `			iBraces--;` |
|  51306876 |  445 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    367811 |  446 | `			if( iQuesty > 0 ){` |
|    367521 |  447 | `				iQuesty--;` |
|    184053 |  448 | `			}else if( iParen <= 0 ){` |
|         - |  449 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  450 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  451 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  452 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  453 | `				if( rc != SXERR_ABORT ){` |
|         6 |  454 | `					rc = SXERR_SYNTAX;` |
|         2 |  455 | `				}` |
|         6 |  456 | `				return rc;` |
|         5 |  457 | `			}` |
|  51121070 |  458 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|  16789407 |  459 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|  16789407 |  460 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    367523 |  461 | `				iQuesty++;` |
|  16605648 |  462 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     65037 |  463 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|     32516 |  481 | `			}` |
|   8394701 |  482 | `		}` |
|  33874189 |  483 | `	}` |
|  12575387 |  484 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        20 |  485 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        20 |  486 | `		if( rc != SXERR_ABORT ){` |
|        20 |  487 | `			rc = SXERR_SYNTAX;` |
|         8 |  488 | `		}` |
|        20 |  489 | `		return rc;` |
|         - |  490 | `	}` |
|  12575371 |  491 | `	return SXRET_OK;` |
|   6287714 |  492 | `}` |
|         - |  493 | `/*` |
|         - |  494 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  495 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  496 | ` */` |
|  10741122 |  497 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  498 | `{` |
|  10741127 |  499 | `	SyToken *pIn = *ppCur;` |
|         - |  500 | `	/* Jump the first literal seen */` |
|  10741127 |  501 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|  10737285 |  502 | `		pIn++;` |
|   5368640 |  503 | `	}` |
|   5372509 |  504 | `	for(;;){` |
|  10745023 |  505 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      3901 |  506 | `			pIn++;` |
|      3901 |  507 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3899 |  508 | `				pIn++;` |
|      1947 |  509 | `			}` |
|      1953 |  510 | `		}else{` |
|   5370566 |  511 | `			break;` |
|         - |  512 | `		}` |
|         5 |  513 | `	}` |
|         - |  514 | `	/* Synchronize pointers */` |
|  10741127 |  515 | `	*ppCur = pIn;` |
|  10741127 |  516 | `}` |
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
|       900 |  561 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  562 | `{` |
|       905 |  563 | `	SyToken *pIn = *ppIn;` |
|       905 |  564 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       905 |  602 | `	*ppIn = pIn;` |
|       905 |  603 | `}` |
|       502 |  604 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  605 | `{` |
|       507 |  606 | `	SyToken *pIn = *ppCur;` |
|         - |  607 | `	sxi32 rc;` |
|         - |  608 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  609 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  610 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  611 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  612 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|       507 |  613 | `	pIn++;` |
|       502 |  614 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       261 |  615 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|         9 |  616 | `		pIn++;` |
|         4 |  617 | `	}` |
|       507 |  618 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  619 | `		/* Syntax error */` |
|         6 |  620 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  621 | `		if( rc != SXERR_ABORT ){` |
|         6 |  622 | `			rc = SXERR_SYNTAX;` |
|         2 |  623 | `		}` |
|         6 |  624 | `		goto Synchronize;` |
|         - |  625 | `	}` |
|       503 |  626 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       503 |  627 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       503 |  628 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
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
|       499 |  639 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  640 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       499 |  641 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       499 |  642 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
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
|       483 |  681 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       483 |  682 | `		pIn++; /* Jump the leading curly '{' */` |
|       483 |  683 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       483 |  684 | `		if( pIn < pEnd ){` |
|       483 |  685 | `			pIn++;` |
|       239 |  686 | `		}` |
|       244 |  687 | `	}else{` |
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
|       483 |  698 | `	rc = SXRET_OK;` |
|       251 |  699 | `Synchronize:` |
|         - |  700 | `	/* Synchronize pointers */` |
|       507 |  701 | `	*ppCur = pIn;` |
|       507 |  702 | `	return rc;` |
|       256 |  703 | `}` |
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
|       320 |  757 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  758 | `{` |
|       325 |  759 | `	SyToken *pIn = *ppCur;` |
|         - |  760 | `	sxu32 nLine;` |
|         - |  761 | `	sxi32 rc;` |
|         - |  762 | `	int iNest;` |
|       325 |  763 | `	nLine = pIn->nLine;` |
|         - |  764 | `	/* Optional 'static' prefix */` |
|       320 |  765 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       325 |  766 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  767 | `		pIn++;` |
|         3 |  768 | `	}` |
|         - |  769 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       320 |  770 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       325 |  771 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  772 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  773 | `		goto Synchronize;` |
|         - |  774 | `	}` |
|       325 |  775 | `	pIn++; /* Jump 'fn' */` |
|       160 |  776 | `	SXUNUSED(nLine);` |
|       160 |  777 | `	SXUNUSED(pGen);` |
|         - |  778 | `	/* Optional '&' for return-by-reference */` |
|       325 |  779 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  780 | `		pIn++;` |
|       ! 0 |  781 | `	}` |
|         - |  782 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  783 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  784 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  785 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       325 |  786 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       323 |  787 | `		pIn++; /* '(' */` |
|       323 |  788 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       323 |  789 | `		if( pIn < pEnd ){` |
|       321 |  790 | `			pIn++; /* ')' */` |
|       158 |  791 | `		}` |
|       159 |  792 | `	}` |
|         - |  793 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       325 |  794 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  795 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       325 |  796 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       319 |  797 | `		pIn++;` |
|       157 |  798 | `	}` |
|         - |  799 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       325 |  800 | `	iNest = 0;` |
|      2173 |  801 | `	while( pIn < pEnd ){` |
|      2061 |  802 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  803 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       209 |  804 | `			break;` |
|         - |  805 | `		}` |
|      1853 |  806 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       204 |  807 | `			iNest++;` |
|      1753 |  808 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       204 |  809 | `			iNest--;` |
|       100 |  810 | `		}` |
|      1853 |  811 | `		pIn++;` |
|         5 |  812 | `	}` |
|       325 |  813 | `	rc = SXRET_OK;` |
|       160 |  814 | `Synchronize:` |
|       325 |  815 | `	*ppCur = pIn;` |
|       325 |  816 | `	return rc;` |
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
|  67967050 |  867 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  868 | `{` |
|         - |  869 | `	ph7_expr_node *pNode;` |
|         - |  870 | `	SyToken *pCur;` |
|         - |  871 | `	sxi32 rc;` |
|         - |  872 | `	/* Allocate a new node */` |
|  67967055 |  873 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  67967055 |  874 | `	if( pNode == 0 ){` |
|         - |  875 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  876 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  877 | `		 */` |
|       ! 0 |  878 | `		return SXERR_MEM;` |
|         - |  879 | `	}` |
|         - |  880 | `	/* Zero the structure */` |
|  67967055 |  881 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  67967055 |  882 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  883 | `	/* Point to the head of the token stream */` |
|  67967055 |  884 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  885 | `	/* Start collecting tokens */` |
|  67967055 |  886 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4129 |  887 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
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
|      4049 |  902 | `		pCur++;` |
|      4049 |  903 | `		pGen->pIn = pCur;` |
|      4049 |  904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      4049 |  905 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      4049 |  906 | `		if( rc == SXRET_OK && *ppNode ){` |
|      4049 |  907 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      2022 |  908 | `		}` |
|      4049 |  909 | `		return rc;` |
|         - |  910 | `	}` |
|  67962931 |  911 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  912 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  913 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  914 | `		 */` |
|    214455 |  915 | `		pCur++; /* Skip the opening '[' */` |
|    214455 |  916 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|    214455 |  917 | `		if( pCur < pGen->pEnd ){` |
|    214455 |  918 | `			pCur++; /* Skip past the closing ']' */` |
|    107230 |  919 | `		}else{` |
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
|    214627 |  931 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       346 |  932 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       346 |  933 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        56 |  934 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  935 | `			}else{` |
|       291 |  936 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  937 | `			}` |
|       174 |  938 | `		}else{` |
|    214111 |  939 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  940 | `		}` |
|  67855706 |  941 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|  67748469 |  952 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|  19307996 |  953 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   9680592 |  954 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|  67748454 |  980 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - |  981 | `		/* Point to the instance that describe this operator */` |
|  19307979 |  982 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - |  983 | `		/* Advance the stream cursor */` |
|  19307979 |  984 | `		pCur++;` |
|  58094456 |  985 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - |  986 | `		/* Isolate variable */` |
|  31921745 |  987 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  15960881 |  988 | `			pCur++; /* Variable variable */` |
|         5 |  989 | `		}` |
|  15960869 |  990 | `		if( pCur < pGen->pEnd ){` |
|  15960869 |  991 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - |  992 | `				/* Variable name */` |
|  15960841 |  993 | `				pCur++;` |
|   7980450 |  994 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        24 |  995 | `				pCur++;` |
|         - |  996 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        24 |  997 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        24 |  998 | `				if( pCur < pGen->pEnd ){` |
|        19 |  999 | `					pCur++;` |
|        11 | 1000 | `				}else{` |
|         6 | 1001 | `					rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         6 | 1002 | `					if( rc != SXERR_ABORT ){` |
|         6 | 1003 | `						rc = SXERR_SYNTAX;` |
|         2 | 1004 | `					}` |
|         6 | 1005 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         6 | 1006 | `					return rc;` |
|         - | 1007 | `				}` |
|         8 | 1008 | `			}` |
|   7980430 | 1009 | `		}` |
|  15960865 | 1010 | `		pNode->xCode = PH7_CompileVariable;` |
|  40460035 | 1011 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    800961 | 1012 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    800961 | 1013 | `		 if( bAfterMemberOp ){` |
|         - | 1014 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1015 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1016 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1017 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    125417 | 1018 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    125417 | 1019 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    738255 | 1020 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1021 | `			 /* List/Array node */` |
|    282645 | 1022 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1023 | `				 /* Assume a literal */` |
|       ! 0 | 1024 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1025 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1026 | `			 }else{` |
|    282645 | 1027 | `				 pCur += 2;` |
|         - | 1028 | `				 /* Collect array/list tokens */` |
|    282645 | 1029 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    282645 | 1030 | `				 if( pCur < pGen->pEnd ){` |
|    282643 | 1031 | `					 pCur++;` |
|    141324 | 1032 | `				 }else{` |
|         - | 1033 | `					 /* Syntax error */` |
|         4 | 1034 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1035 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1036 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1037 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1038 | `					 }` |
|         3 | 1039 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1040 | `					 return rc;` |
|         - | 1041 | `				 }` |
|    282643 | 1042 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    282643 | 1043 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    534227 | 1056 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1057 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|     15565 | 1058 | `			 pCur++; /* Skip 'yield' keyword */` |
|     15565 | 1059 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1060 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1061 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|     15565 | 1062 | `			 pNode->xCode = PH7_CompileYield;` |
|    385129 | 1063 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|    377109 | 1064 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        52 | 1065 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        32 | 1066 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1067 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|       507 | 1068 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1069 | `				 /* Assume a literal */` |
|       ! 0 | 1070 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1071 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1072 | `			 }else{` |
|         - | 1073 | `				 /* Assemble annonymous functions body */` |
|       507 | 1074 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       507 | 1075 | `				 if( rc != SXRET_OK ){` |
|        28 | 1076 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1077 | `					 return rc;` |
|         - | 1078 | `				 }` |
|       483 | 1079 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1080 | `			  }` |
|    377086 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
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
|    376832 | 1096 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    376665 | 1097 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        44 | 1098 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        24 | 1099 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1100 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       325 | 1101 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       325 | 1102 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1103 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1104 | `				 return rc;` |
|         - | 1105 | `			 }` |
|       325 | 1106 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    376659 | 1107 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1108 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        77 | 1109 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        77 | 1110 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1111 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1112 | `				 return rc;` |
|         - | 1113 | `			 }` |
|        77 | 1114 | `			 pNode->xCode = PH7_CompileMatch;` |
|    376463 | 1115 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1116 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1117 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1118 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1119 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1120 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1121 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1122 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1123 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    376409 | 1124 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1125 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        75 | 1126 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        75 | 1127 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        40 | 1128 | `		 }else{` |
|         - | 1129 | `			 /* Assume a literal */` |
|    376321 | 1130 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    376321 | 1131 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1132 | `		 }` |
|  32079113 | 1133 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1134 | `		 /* Constants,function name,namespace path,class name... */` |
|  10239383 | 1135 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|  10239383 | 1136 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   5119694 | 1137 | `	 }else{` |
|  21439271 | 1138 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1139 | `			 /* Point to the code generator routine */` |
|   7146545 | 1140 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   7146545 | 1141 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1142 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 1143 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1144 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1145 | `				 }` |
|         3 | 1146 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1147 | `				 return rc;` |
|         - | 1148 | `			 }` |
|   3573269 | 1149 | `		 }` |
|         - | 1150 | `		/* Advance the stream cursor */` |
|  21439269 | 1151 | `		pCur++;` |
|         - | 1152 | `	 }` |
|         - | 1153 | `	/* Point to the end of the token stream */` |
|  67962897 | 1154 | `	pNode->pEnd = pCur;` |
|         - | 1155 | `	/* Save the node for later processing */` |
|  67962897 | 1156 | `	*ppNode = pNode;` |
|         - | 1157 | `	/* Synchronize cursors */` |
|  67962897 | 1158 | `	pGen->pIn = pCur;` |
|  67962897 | 1159 | `	return SXRET_OK;` |
|  33983530 | 1160 | `}` |
|         - | 1161 | `/*` |
|         - | 1162 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1163 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1164 | ` * level is zero.` |
|         - | 1165 | ` */` |
|   1307234 | 1166 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1167 | `{` |
|   1307239 | 1168 | `	SyToken *pCur = pStart;` |
|   1307239 | 1169 | `	sxi32 iNest = 0;` |
|   1307239 | 1170 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1171 | `		/* Last expression */` |
|    540577 | 1172 | `		return SXERR_EOF;` |
|         - | 1173 | `	}` |
|   3353559 | 1174 | `	while( pCur < pEnd ){` |
|   3111883 | 1175 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    524991 | 1176 | `			break;` |
|         - | 1177 | `		}` |
|   2586897 | 1178 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    212339 | 1179 | `			iNest++;` |
|   2480730 | 1180 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    212341 | 1181 | `			iNest--;` |
|    106168 | 1182 | `		}` |
|   2586897 | 1183 | `		pCur++;` |
|         5 | 1184 | `	}` |
|    766667 | 1185 | `	*ppNext = pCur;` |
|    766667 | 1186 | `	return SXRET_OK;` |
|    653622 | 1187 | `}` |
|         - | 1188 | `/*` |
|         - | 1189 | ` * Free an expression tree.` |
|         - | 1190 | ` */` |
|  58088410 | 1191 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1192 | `{` |
|  58088415 | 1193 | `	if( pNode->pLeft ){` |
|         - | 1194 | `		/* Release the left tree */` |
|  23072551 | 1195 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|  11536273 | 1196 | `	}` |
|  58088415 | 1197 | `	if( pNode->pRight ){` |
|         - | 1198 | `		/* Release the right tree */` |
|  13440163 | 1199 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   6720079 | 1200 | `	}` |
|  58088415 | 1201 | `	if( pNode->pCond ){` |
|         - | 1202 | `		/* Release the conditional tree used by the ternary operator */` |
|    367519 | 1203 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|    183757 | 1204 | `	}` |
|  58088415 | 1205 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1206 | `		ph7_expr_node **apArg;` |
|         - | 1207 | `		sxu32 n;` |
|         - | 1208 | `		/* Release node arguments */` |
|   6372709 | 1209 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  14355341 | 1210 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   7982637 | 1211 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   3991321 | 1212 | `		}` |
|   6372709 | 1213 | `		SySetRelease(&pNode->aNodeArgs);` |
|   3186352 | 1214 | `	}` |
|         - | 1215 | `	/* Finally,release this node */` |
|  58088415 | 1216 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  58088415 | 1217 | `}` |
|         - | 1218 | `/*` |
|         - | 1219 | ` * Free an expression tree.` |
|         - | 1220 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1221 | ` */` |
|  12575448 | 1222 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1223 | `{` |
|         - | 1224 | `	ph7_expr_node **apNode;` |
|         - | 1225 | `	sxu32 n;` |
|  12575453 | 1226 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  80538395 | 1227 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  67962947 | 1228 | `		if( apNode[n] ){` |
|  12575793 | 1229 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   6287894 | 1230 | `		}` |
|  33981476 | 1231 | `	}` |
|  12575453 | 1232 | `	return SXRET_OK;` |
|         5 | 1233 | `}` |
|         - | 1234 | `/*` |
|         - | 1235 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1236 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1237 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1238 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1239 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1240 | ` */` |
|  16343908 | 1241 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1242 | `{` |
|  16343913 | 1243 | `	if( pNode == 0 ){` |
|  10156501 | 1244 | `		return 0;` |
|         - | 1245 | `	}` |
|   6187417 | 1246 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1247 | `		return 1;` |
|         - | 1248 | `	}` |
|   6187405 | 1249 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1250 | `		return 1;` |
|         - | 1251 | `	}` |
|   6187401 | 1252 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1253 | `		return 1;` |
|         - | 1254 | `	}` |
|   6187401 | 1255 | `	return 0;` |
|   8171959 | 1256 | `}` |
|         - | 1257 | `/*` |
|         - | 1258 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1259 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1260 | ` */` |
|   3946262 | 1261 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1262 | `{` |
|         - | 1263 | `	sxi32 iExprOp;` |
|   3946267 | 1264 | `	if( pNode->pOp == 0 ){` |
|   2821767 | 1265 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1266 | `	}` |
|   1124505 | 1267 | `	iExprOp = pNode->pOp->iOp;` |
|   1124505 | 1268 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    778541 | 1269 | `			return TRUE;` |
|         - | 1270 | `	}` |
|    345969 | 1271 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    345963 | 1272 | `		if( pNode->pLeft->pOp ) {` |
|    117696 | 1273 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|     49358 | 1274 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1275 | `				return FALSE;` |
|         5 | 1276 | `			}` |
|    287115 | 1277 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1278 | `			return FALSE;` |
|         - | 1279 | `		}` |
|    345963 | 1280 | `		return TRUE;` |
|         - | 1281 | `	}` |
|         8 | 1282 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1283 | `		return TRUE;` |
|         - | 1284 | `	}` |
|         - | 1285 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1286 | `	return FALSE;` |
|   1973136 | 1287 | `}` |
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
|   4009948 | 1299 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1300 | `{` |
|         - | 1301 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1302 | `	sxi32 rc;` |
|         - | 1303 | `	/* Process function arguments from left to right */` |
|   4009953 | 1304 | `	iCur = 0;` |
|   4814900 | 1305 | `	for(;;){` |
|   9629805 | 1306 | `		if( iCur >= nToken ){` |
|         - | 1307 | `			/* No more arguments to process */` |
|   4009927 | 1308 | `			break;` |
|         - | 1309 | `		}` |
|   5619883 | 1310 | `		iNode = iCur;` |
|   5619883 | 1311 | `		iNest = 0;` |
|  18801509 | 1312 | `		while( iCur < nToken ){` |
|  14791585 | 1313 | `			if( apNode[iCur] ){` |
|  14745879 | 1314 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    804982 | 1315 | `					break;` |
|  13135920 | 1316 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   7080247 | 1317 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|   1022149 | 1318 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1319 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1320 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1321 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1322 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1323 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1324 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|   1019719 | 1325 | `					iNest++;` |
|  12626068 | 1326 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|   1019719 | 1327 | `					iNest--;` |
|    509857 | 1328 | `				}` |
|   6567960 | 1329 | `			}` |
|  13181631 | 1330 | `			iCur++;` |
|         5 | 1331 | `		}` |
|   5619883 | 1332 | `		if( iCur > iNode ){` |
|   5619877 | 1333 | `			SyString sArgName = {0, 0};` |
|         - | 1334 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1335 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1336 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   5619872 | 1337 | `			if( (iCur - iNode) >= 2` |
|   3889632 | 1338 | `				&& apNode[iNode]` |
|   2159378 | 1339 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|   1145460 | 1340 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|    131302 | 1341 | `				&& apNode[iNode+1]` |
|    131053 | 1342 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
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
|   5619870 | 1363 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
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
|   5619875 | 1380 | `				int bSpreadArg = 0;` |
|         - | 1381 | `				sxi32 iScan;` |
|   5619903 | 1382 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   5619903 | 1383 | `					if( apNode[iScan] ){` |
|   5619875 | 1384 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   5619875 | 1385 | `						break;` |
|         - | 1386 | `					}` |
|        15 | 1387 | `				}` |
|   5619875 | 1388 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   5619875 | 1389 | `				if( bSpreadArg && apNode[iNode] ){` |
|      3983 | 1390 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|      1989 | 1391 | `				}` |
|         - | 1392 | `			}` |
|   5619875 | 1393 | `			if( apNode[iNode] ){` |
|   5619875 | 1394 | `				if( sArgName.nByte > 0 ){` |
|       289 | 1395 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       289 | 1396 | `					apNode[iNode]->sArgName = sArgName;` |
|       142 | 1397 | `				}` |
|         - | 1398 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   5619875 | 1399 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   2809940 | 1400 | `			}else{` |
|         - | 1401 | `				/* No expression before comma */` |
|       ! 0 | 1402 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1403 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1404 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1405 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1406 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1407 | `				}` |
|       ! 0 | 1408 | `				return rc;` |
|         - | 1409 | `			}` |
|   2809940 | 1410 | `		}else{` |
|         - | 1411 | `			/* Comma with no preceding argument */` |
|         9 | 1412 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         9 | 1413 | `			if( rc != SXERR_ABORT ){` |
|         9 | 1414 | `				rc = SXERR_SYNTAX;` |
|         3 | 1415 | `			}` |
|         9 | 1416 | `			return rc;` |
|         - | 1417 | `		}` |
|         - | 1418 | `		/* Jump trailing comma */` |
|   5619875 | 1419 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|   1609953 | 1420 | `			iCur++;` |
|   1609953 | 1421 | `			if( iCur >= nToken ){` |
|         - | 1422 | `				/* Trailing comma after last argument */` |
|        19 | 1423 | `				break;` |
|         - | 1424 | `			}` |
|    804965 | 1425 | `		}` |
|         5 | 1426 | `	}` |
|   4009945 | 1427 | `	return SXRET_OK;` |
|   2004979 | 1428 | `}` |
|         - | 1429 | ` /*` |
|         - | 1430 | `  * Create an expression tree from an array of tokens.` |
|         - | 1431 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1432 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1433 | `  */` |
|  21739360 | 1434 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1435 | ` {` |
|         - | 1436 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1437 | `	 ph7_expr_node *pNode;` |
|         - | 1438 | `	 ph7_expr_node *pSuppress;` |
|         - | 1439 | `	 sxi32 iCur;` |
|         - | 1440 | `	 sxi32 rc;` |
|  21739365 | 1441 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1442 | `		 /* TICKET 1433-17: self evaluating node */` |
|   9442219 | 1443 | `		 return SXRET_OK;` |
|         - | 1444 | `	 }` |
|         - | 1445 | `	 /* Process expressions enclosed in parenthesis first */` |
|  88719355 | 1446 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1447 | `		 sxi32 iNest;` |
|         - | 1448 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1449 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1450 | `		  */` |
|  76422211 | 1451 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  76101411 | 1452 | `			 continue;` |
|         - | 1453 | `		 }` |
|    320805 | 1454 | `		 iNest = 1;` |
|    320805 | 1455 | `		 iLeft = iCur;` |
|         - | 1456 | `		 /* Find the closing parenthesis */` |
|    320805 | 1457 | `		 iCur++;` |
|   2849331 | 1458 | `		 while( iCur < nToken ){` |
|   2849331 | 1459 | `			 if( apNode[iCur] ){` |
|   2849331 | 1460 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1461 | `					 /* Decrement nesting level */` |
|    447187 | 1462 | `					 iNest--;` |
|    447187 | 1463 | `					 if( iNest <= 0 ){` |
|    320805 | 1464 | `						 break;` |
|         5 | 1465 | `					 }` |
|   2465340 | 1466 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1467 | `					 /* Increment nesting level */` |
|    126387 | 1468 | `					 iNest++;` |
|     63191 | 1469 | `				 }` |
|   1264263 | 1470 | `			 }` |
|   2528531 | 1471 | `			 iCur++;` |
|         5 | 1472 | `		 }` |
|    320805 | 1473 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1474 | `			 sxi32 j;` |
|         - | 1475 | `			 /* Recurse and process this expression */` |
|    320805 | 1476 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    320805 | 1477 | `			 if( rc != SXRET_OK ){` |
|         3 | 1478 | `				 return rc;` |
|         - | 1479 | `			 }` |
|         - | 1480 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1481 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1482 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1483 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1484 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1485 | `			  * group's free below silently drops the unpacking. */` |
|    320803 | 1486 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    320803 | 1487 | `				 if( apNode[j] ){` |
|    320803 | 1488 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    320798 | 1489 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    320803 | 1490 | `					 break;` |
|         - | 1491 | `				 }` |
|       ! 0 | 1492 | `			 }` |
|    160399 | 1493 | `		 }` |
|         - | 1494 | `		 /* Free the left and right nodes */` |
|    320803 | 1495 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    320803 | 1496 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    320803 | 1497 | `		 apNode[iLeft] = 0;` |
|    320803 | 1498 | `		 apNode[iCur] = 0;` |
|    160404 | 1499 | `	 }` |
|         - | 1500 | `	  /* Process expressions enclosed in braces */` |
|  91309875 | 1501 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1502 | `		 sxi32 iNest;` |
|         - | 1503 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1504 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1505 | `		  */` |
|  79263923 | 1506 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  79260121 | 1507 | `			 continue;` |
|         - | 1508 | `		 }` |
|      3807 | 1509 | `		 iNest = 1;` |
|      3807 | 1510 | `		 iLeft = iCur;` |
|         - | 1511 | `		 /* Find the closing parenthesis */` |
|      3807 | 1512 | `		 iCur++;` |
|      7607 | 1513 | `		 while( iCur < nToken ){` |
|      7607 | 1514 | `			 if( apNode[iCur] ){` |
|      7607 | 1515 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1516 | `					 /* Decrement nesting level */` |
|      3807 | 1517 | `					 iNest--;` |
|      3807 | 1518 | `					 if( iNest <= 0 ){` |
|      3807 | 1519 | `						 break;` |
|       ! 0 | 1520 | `					 }` |
|      3805 | 1521 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1522 | `					 /* Increment nesting level */` |
|       ! 0 | 1523 | `					 iNest++;` |
|       ! 0 | 1524 | `				 }` |
|      1900 | 1525 | `			 }` |
|      3805 | 1526 | `			 iCur++;` |
|         5 | 1527 | `		 }` |
|      3807 | 1528 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1529 | `			 /* Recurse and process this expression */` |
|      3805 | 1530 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      3805 | 1531 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1532 | `				 return rc;` |
|         - | 1533 | `			 }` |
|      1900 | 1534 | `		 }` |
|         - | 1535 | `		 /* Free the left and right nodes */` |
|      3807 | 1536 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      3807 | 1537 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      3807 | 1538 | `		 apNode[iLeft] = 0;` |
|      3807 | 1539 | `		 apNode[iCur] = 0;` |
|      1906 | 1540 | `	 }` |
|         - | 1541 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|  12045957 | 1542 | `	 iLeft = -1;` |
|  91317441 | 1543 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79271501 | 1544 | `		 if( apNode[iCur] == 0 ){` |
|  34700211 | 1545 | `			 continue;` |
|         - | 1546 | `		 }` |
|  44571295 | 1547 | `		 pNode = apNode[iCur];` |
|  44571295 | 1548 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|  12745273 | 1549 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1550 | `				 /* Collect function arguments */` |
|   5378559 | 1551 | `				 sxi32 iPtr = 0;` |
|   5378559 | 1552 | `				 sxi32 nFuncTok = 0;` |
|  25548695 | 1553 | `				 while( nFuncTok + iCur < nToken ){` |
|  25548695 | 1554 | `					 if( apNode[nFuncTok+iCur] ){` |
|  25502989 | 1555 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   5688285 | 1556 | `							 iPtr++;` |
|  22658849 | 1557 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   5688285 | 1558 | `							 iPtr--;` |
|   5688285 | 1559 | `							 if( iPtr <= 0 ){` |
|   5378559 | 1560 | `								 break;` |
|         - | 1561 | `							 }` |
|    154863 | 1562 | `						 }` |
|  10062215 | 1563 | `					 }` |
|  20170141 | 1564 | `					 nFuncTok++;` |
|         5 | 1565 | `				 }` |
|   5378559 | 1566 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1567 | `					 /* Syntax error */` |
|       ! 0 | 1568 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1569 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1570 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1571 | `					 }` |
|       ! 0 | 1572 | `					 return rc;` |
|         - | 1573 | `				 }` |
|   5378559 | 1574 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1575 | `					 /* Syntax error */` |
|       ! 0 | 1576 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1577 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1578 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1579 | `					 }` |
|       ! 0 | 1580 | `					 return rc;` |
|         - | 1581 | `				 }` |
|   5378559 | 1582 | `				 if( nFuncTok > 1 ){` |
|         - | 1583 | `					 /* Process function arguments */` |
|   4009953 | 1584 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   4009953 | 1585 | `					 if( rc != SXRET_OK ){` |
|        12 | 1586 | `						 return rc;` |
|         - | 1587 | `					 }` |
|   2004970 | 1588 | `				 }` |
|         - | 1589 | `				 /* Link the node to the tree */` |
|   5378551 | 1590 | `				 pNode->pLeft = apNode[iLeft];` |
|   5378551 | 1591 | `				 apNode[iLeft] = 0;` |
|  25548663 | 1592 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  20170117 | 1593 | `					 apNode[iCur+iPtr] = 0;` |
|  10085061 | 1594 | `				 }` |
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
|   5378551 | 1606 | `					 sxi32 iNew = iLeft - 1;` |
|   7186985 | 1607 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|   1808439 | 1608 | `						 iNew--;` |
|         5 | 1609 | `					 }` |
|   5378546 | 1610 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   3187752 | 1611 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   1944208 | 1612 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    708261 | 1613 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    708261 | 1614 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    708261 | 1615 | `						 apNode[iNew] = 0;` |
|    708261 | 1616 | `						 pNode = apNode[iCur];` |
|    354133 | 1617 | `					 }` |
|         - | 1618 | `				 }` |
|  10055992 | 1619 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1620 | `				 /* Subscripting */` |
|   2518541 | 1621 | `				 sxi32 iArrTok = iCur + 1;` |
|   2518541 | 1622 | `				 sxi32 iNest = 1;` |
|   2518536 | 1623 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1624 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1625 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1626 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   2518536 | 1627 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1628 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1629 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|    298078 | 1630 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1631 | `						 /* Syntax error */` |
|       ! 0 | 1632 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1633 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1634 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1635 | `						 }` |
|       ! 0 | 1636 | `						 return rc;` |
|         - | 1637 | `				 }` |
|         - | 1638 | `				 /* Collect index tokens */` |
|   5321555 | 1639 | `				 while( iArrTok < nToken ){` |
|   5321555 | 1640 | `					 if( apNode[iArrTok] ){` |
|   5321523 | 1641 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1642 | `							 /* Increment nesting level */` |
|     18975 | 1643 | `							 iNest++;` |
|   5312038 | 1644 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1645 | `							 /* Decrement nesting level */` |
|   2537511 | 1646 | `							 iNest--;` |
|   2537511 | 1647 | `							 if( iNest <= 0 ){` |
|   2518541 | 1648 | `								 break;` |
|         - | 1649 | `							 }` |
|      9485 | 1650 | `						 }` |
|   1401491 | 1651 | `					 }` |
|   2803019 | 1652 | `					 ++iArrTok;` |
|         5 | 1653 | `				 }` |
|   2518541 | 1654 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1655 | `					 /* Recurse and process this expression */` |
|   2362767 | 1656 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   2362767 | 1657 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1658 | `						 return rc;` |
|         - | 1659 | `					 }` |
|         - | 1660 | `					 /* Link the node to it's index */` |
|   2362767 | 1661 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|   1181381 | 1662 | `				 }` |
|         - | 1663 | `				 /* Link the node to the tree */` |
|   2518541 | 1664 | `				 pNode->pLeft = apNode[iLeft];` |
|   2518541 | 1665 | `				 pNode->pRight = 0;` |
|   2518541 | 1666 | `				 apNode[iLeft] = 0;` |
|   7840091 | 1667 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   5321555 | 1668 | `					 apNode[iNest] = 0;` |
|   2660780 | 1669 | `				 }` |
|   1259273 | 1670 | `			 }else{` |
|         - | 1671 | `				 /* Member access operators [i.e: '->','::'] */` |
|   4848183 | 1672 | `				  iRight = iCur + 1;` |
|   4851983 | 1673 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      3805 | 1674 | `					 iRight++;` |
|         5 | 1675 | `				 }` |
|   4848183 | 1676 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1677 | `					 /* Syntax error */` |
|         5 | 1678 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1679 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1680 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1681 | `					 }` |
|         5 | 1682 | `					 return rc;` |
|         - | 1683 | `				 }` |
|         - | 1684 | `				 /* Link the node to the tree */` |
|   4848179 | 1685 | `				 pNode->pLeft = apNode[iLeft];` |
|   4848174 | 1686 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   4669415 | 1687 | `					 && pNode->pLeft->pOp == 0 &&` |
|   4414235 | 1688 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
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
|   4848179 | 1702 | `				 pNode->pRight = apNode[iRight];` |
|   4848179 | 1703 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1704 | `			 }` |
|   6372628 | 1705 | `		 }` |
|  44571283 | 1706 | `		 iLeft = iCur;` |
|  22285644 | 1707 | `	 }` |
|         - | 1708 | `	 /* Handle left associative (new, clone) operators */` |
|  91317409 | 1709 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79271469 | 1710 | `		 if( apNode[iCur] == 0 ){` |
|  48207205 | 1711 | `			 continue;` |
|         - | 1712 | `		 }` |
|  31064269 | 1713 | `		 pNode = apNode[iCur];` |
|  31064269 | 1714 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1715 | `			 SyToken *pToken;` |
|         - | 1716 | `			 /* Get the left node */` |
|     53487 | 1717 | `			 iLeft = iCur + 1;` |
|     53495 | 1718 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         9 | 1719 | `				 iLeft++;` |
|         1 | 1720 | `			 }` |
|     53487 | 1721 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1722 | `				  /* Syntax error */` |
|       ! 0 | 1723 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1724 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1725 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1726 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1727 | `				 }` |
|       ! 0 | 1728 | `				 return rc;` |
|         - | 1729 | `			 }` |
|         - | 1730 | `			 /* Make sure the operand are of a valid type */` |
|     53487 | 1731 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
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
|     53161 | 1742 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|     53155 | 1743 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1744 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1745 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1746 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1747 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1748 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1749 | `						 }` |
|       ! 0 | 1750 | `						 return rc;` |
|         - | 1751 | `					 }` |
|     26575 | 1752 | `				 }` |
|     26583 | 1753 | `			 }else{` |
|         - | 1754 | `				 /* New */` |
|       326 | 1755 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
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
|       331 | 1772 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       331 | 1773 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       326 | 1774 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
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
|       163 | 1786 | `				 }` |
|         - | 1787 | `			 }` |
|         - | 1788 | `			  /* Link the node to the tree */` |
|     53487 | 1789 | `			 pNode->pLeft = apNode[iLeft];` |
|     53487 | 1790 | `			 apNode[iLeft] = 0;` |
|     53487 | 1791 | `			 pNode->pRight = 0; /* Paranoid */` |
|     26741 | 1792 | `		 }` |
|  15532137 | 1793 | `	 }` |
|         - | 1794 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|  12045945 | 1795 | `	 iLeft = -1;` |
|  91443005 | 1796 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79271469 | 1797 | `		 if( apNode[iCur] == 0 ){` |
|  48207205 | 1798 | `			 continue;` |
|         - | 1799 | `		 }` |
|  31064269 | 1800 | `		 pNode = apNode[iCur];` |
|  31064269 | 1801 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    152263 | 1802 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|    133235 | 1803 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1804 | `					 /* Link the node to the tree */` |
|    140849 | 1805 | `					 pNode->pLeft = apNode[iLeft];` |
|    140849 | 1806 | `					 apNode[iLeft] = 0;` |
|     70422 | 1807 | `			 }` |
|    201725 | 1808 | `		  }` |
|  31189865 | 1809 | `		 iLeft = iCur;` |
|  15657733 | 1810 | `	  }` |
|  12171541 | 1811 | `	 iLeft = -1;` |
|  91443005 | 1812 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  79271469 | 1813 | `		 if( apNode[iCur] == 0 ){` |
|  48348049 | 1814 | `			 continue;` |
|         - | 1815 | `		 }` |
|  30923425 | 1816 | `		 pNode = apNode[iCur];` |
|  30923425 | 1817 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     11414 | 1818 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     11419 | 1819 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1820 | `					 /* Syntax error */` |
|       ! 0 | 1821 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1822 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1823 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1824 | `					 }` |
|       ! 0 | 1825 | `					 return rc;` |
|         - | 1826 | `			 }` |
|         - | 1827 | `			 /* Link the node to the tree */` |
|     11419 | 1828 | `			 pNode->pLeft = apNode[iLeft];` |
|     11419 | 1829 | `			 apNode[iLeft] = 0;` |
|         - | 1830 | `			 /* Mark as pre-increment/decrement node */` |
|     11419 | 1831 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      5707 | 1832 | `		  }` |
|  30923425 | 1833 | `		 iLeft = iCur;` |
|  15461715 | 1834 | `	 }` |
|         - | 1835 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|  12171541 | 1836 | `	  iLeft = 0;` |
|  91442999 | 1837 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  79271465 | 1838 | `		  if( apNode[iCur] ){` |
|  30912007 | 1839 | `			  pNode = apNode[iCur];` |
|  30912007 | 1840 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    825181 | 1841 | `				  if( iLeft > 0 ){` |
|         - | 1842 | `					  /* Link the node to the tree */` |
|    825179 | 1843 | `					  pNode->pLeft = apNode[iLeft];` |
|    825179 | 1844 | `					  apNode[iLeft] = 0;` |
|    825179 | 1845 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|     53209 | 1846 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1847 | `							   /* Syntax error */` |
|       ! 0 | 1848 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 1849 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1850 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1851 | `							  }` |
|       ! 0 | 1852 | `							  return rc;` |
|         - | 1853 | `						  }` |
|     26602 | 1854 | `					  }` |
|    412592 | 1855 | `				  }else{` |
|         - | 1856 | `					  /* Syntax error */` |
|         3 | 1857 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 1858 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1859 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1860 | `					  }` |
|         3 | 1861 | `					  return rc;` |
|         - | 1862 | `				  }` |
|    412587 | 1863 | `			  }` |
|         - | 1864 | `			  /* Save terminal position */` |
|  30912005 | 1865 | `			  iLeft = iCur;` |
|  15456000 | 1866 | `		  }` |
|  39635734 | 1867 | `	  }` |
|         - | 1868 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1869 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1870 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1871 | `	  * yielding a right-leaning tree. */` |
|  91442997 | 1872 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  79271463 | 1873 | `		 if( apNode[iCur] == 0 ){` |
|  49184751 | 1874 | `			 continue;` |
|         - | 1875 | `		 }` |
|  30086717 | 1876 | `		 pNode = apNode[iCur];` |
|  30086717 | 1877 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
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
|  15043361 | 1938 | `	 }` |
|         - | 1939 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
| 133886793 | 1940 | `	 for( i = 7 ; i < 17 ; i++ ){` |
| 121715269 | 1941 | `		 iLeft = -1;` |
| 914429555 | 1942 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 792714301 | 1943 | `			 if( apNode[iCur] == 0 ){` |
| 544745397 | 1944 | `				 continue;` |
|         - | 1945 | `			 }` |
| 247968909 | 1946 | `			 pNode = apNode[iCur];` |
| 247968909 | 1947 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1948 | `				 /* Get the right node */` |
|   4278175 | 1949 | `				 iRight = iCur + 1;` |
|   6638213 | 1950 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   2360043 | 1951 | `					 iRight++;` |
|         5 | 1952 | `				 }` |
|   4278175 | 1953 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1954 | `					 /* Syntax error */` |
|        11 | 1955 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        11 | 1956 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1957 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1958 | `					 }` |
|        11 | 1959 | `					 return rc;` |
|         - | 1960 | `				 }` |
|   4278167 | 1961 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1962 | `					 sxi32  iTmp;` |
|         - | 1963 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1964 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 1965 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 1966 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 1967 | `					  * is swapped below. */` |
|        65 | 1968 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
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
|   4278165 | 2003 | `				 pNode->pLeft = apNode[iLeft];` |
|   4278165 | 2004 | `				 pNode->pRight = apNode[iRight];` |
|   4278165 | 2005 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   2139080 | 2006 | `			 }` |
| 247968899 | 2007 | `			 iLeft = iCur;` |
| 123984452 | 2008 | `		 }` |
|  60857632 | 2009 | `	 }` |
|         - | 2010 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2011 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2012 | `	  * we are dealing with a single operator.` |
|         - | 2013 | `	  */` |
|  12171529 | 2014 | `	  iLeft = -1;` |
|  88473283 | 2015 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  76669275 | 2016 | `		  if( apNode[iCur] == 0 ){` |
|  56237735 | 2017 | `			  continue;` |
|         - | 2018 | `		  }` |
|  20431545 | 2019 | `		  pNode = apNode[iCur];` |
|  20431545 | 2020 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    367521 | 2021 | `			  sxi32 iNest = 1;` |
|    367521 | 2022 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2023 | `				  /* Missing condition */` |
|         3 | 2024 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         3 | 2025 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2026 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2027 | `				  }` |
|         3 | 2028 | `				  return rc;` |
|         - | 2029 | `			  }` |
|         - | 2030 | `			  /* Get the right node */` |
|    367519 | 2031 | `			  iRight = iCur + 1;` |
|   1441019 | 2032 | `			  while( iRight < nToken  ){` |
|   1441019 | 2033 | `				  if( apNode[iRight] ){` |
|    731171 | 2034 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2035 | `						  /* Increment nesting level */` |
|       ! 0 | 2036 | `						  ++iNest;` |
|    731171 | 2037 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2038 | `						  /* Decrement nesting level */` |
|    367519 | 2039 | `						  --iNest;` |
|    367519 | 2040 | `						  if( iNest <= 0 ){` |
|    367519 | 2041 | `							  break;` |
|         - | 2042 | `						  }` |
|       ! 0 | 2043 | `					  }` |
|    181826 | 2044 | `				  }` |
|   1073505 | 2045 | `				  iRight++;` |
|         5 | 2046 | `			  }` |
|    367519 | 2047 | `			  if( iRight > iCur + 1 ){` |
|         - | 2048 | `				  /* Recurse and process the then expression */` |
|    363657 | 2049 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    363657 | 2050 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2051 | `					  return rc;` |
|         - | 2052 | `				  }` |
|         - | 2053 | `				  /* Link the node to the tree */` |
|    363657 | 2054 | `				  pNode->pLeft = apNode[iCur + 1];` |
|    181826 | 2055 | `			  }else{` |
|         - | 2056 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2057 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2058 | `			  }` |
|    367519 | 2059 | `			  apNode[iCur + 1] = 0;` |
|    367519 | 2060 | `			  if( iRight + 1 < nToken ){` |
|         - | 2061 | `				  /* Recurse and process the else expression */` |
|    367519 | 2062 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    367519 | 2063 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2064 | `					  return rc;` |
|         - | 2065 | `				  }` |
|         - | 2066 | `				  /* Link the node to the tree */` |
|    367519 | 2067 | `				  pNode->pRight = apNode[iRight + 1];` |
|    367519 | 2068 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|    183762 | 2069 | `			  }else{` |
|       ! 0 | 2070 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2071 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2072 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2073 | `				 }` |
|       ! 0 | 2074 | `				 return rc;` |
|         - | 2075 | `			  }` |
|         - | 2076 | `			  /* Point to the condition */` |
|    367519 | 2077 | `			  pNode->pCond  = apNode[iLeft];` |
|    367519 | 2078 | `			  apNode[iLeft] = 0;` |
|    367519 | 2079 | `			  break;` |
|         - | 2080 | `		  }` |
|  20064029 | 2081 | `		  iLeft = iCur;` |
|  10032017 | 2082 | `	  }` |
|         - | 2083 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2084 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2085 | `	  * so there is no need for a precedence loop here.` |
|         - | 2086 | `	  */` |
|  12171527 | 2087 | `	 iRight = -1;` |
|  91442801 | 2088 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  79271333 | 2089 | `		 if( apNode[iCur] == 0 ){` |
|  63153523 | 2090 | `			 continue;` |
|         - | 2091 | `		 }` |
|  16117815 | 2092 | `		 pNode = apNode[iCur];` |
|  16117815 | 2093 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2094 | `			 /* Get the left node */` |
|   3946215 | 2095 | `			 iLeft = iCur - 1;` |
|   5386175 | 2096 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   1439965 | 2097 | `				 iLeft--;` |
|         5 | 2098 | `			 }` |
|   3946215 | 2099 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
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
|   3946173 | 2118 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
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
|   3946165 | 2131 | `			 pSuppress = 0;` |
|   3946160 | 2132 | `			 if( apNode[iLeft]->pOp` |
|   2535316 | 2133 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|    562236 | 2134 | `				 && apNode[iLeft]->pLeft != 0` |
|         5 | 2135 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2136 | `				 pSuppress = apNode[iLeft];` |
|       ! 0 | 2137 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|       ! 0 | 2138 | `			 }` |
|   3946165 | 2139 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
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
|   3946161 | 2158 | `			 pNode->pLeft = apNode[iRight];` |
|   3946161 | 2159 | `			 pNode->pRight = apNode[iLeft];` |
|   3946161 | 2160 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   3946161 | 2161 | `			 if( pSuppress ){` |
|         - | 2162 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|       ! 0 | 2163 | `				 pSuppress->pLeft = pNode;` |
|       ! 0 | 2164 | `				 apNode[iCur] = pSuppress;` |
|       ! 0 | 2165 | `			 }` |
|   1973078 | 2166 | `		 }` |
|  16117761 | 2167 | `		 iRight = iCur;` |
|   8058883 | 2168 | `	 }` |
|         - | 2169 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  60857345 | 2170 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  48685877 | 2171 | `		 iLeft = -1;` |
| 365770917 | 2172 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 317085045 | 2173 | `			 if( apNode[iCur] == 0 ){` |
| 268398921 | 2174 | `				 continue;` |
|         - | 2175 | `			 }` |
|  48686129 | 2176 | `			 pNode = apNode[iCur];` |
|  48686129 | 2177 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
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
|  48686129 | 2196 | `			 iLeft = iCur;` |
|  24343067 | 2197 | `		 }` |
|  24342941 | 2198 | `	 }` |
|         - | 2199 | `	 /* Point to the root of the expression tree */` |
|  79271237 | 2200 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  67099787 | 2201 | `		 if( apNode[iCur] ){` |
|  11692047 | 2202 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        23 | 2203 | `				 rc = PH7_GenSyntaxError(pGen,apNode[iCur]->pStart,pGen->nCommaExprOk > 0 ? "\";\"" : 0);` |
|        23 | 2204 | `				  if( rc != SXERR_ABORT ){` |
|        23 | 2205 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2206 | `				  }` |
|        23 | 2207 | `				  return rc;` |
|         - | 2208 | `			 }` |
|  11692029 | 2209 | `			 apNode[0] = apNode[iCur];` |
|  11692029 | 2210 | `			 apNode[iCur] = 0;` |
|   5846012 | 2211 | `		 }` |
|  33549887 | 2212 | `	 }` |
|  12171455 | 2213 | `	 return SXRET_OK;` |
|  10806887 | 2214 | ` }` |
|         - | 2215 | ` /*` |
|         - | 2216 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2217 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2218 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2219 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2220 | `  */` |
|  12575452 | 2221 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2222 | `{` |
|         - | 2223 | `	ph7_expr_node **apNode;` |
|         - | 2224 | `	ph7_expr_node *pNode;` |
|         - | 2225 | `	sxi32 rc;` |
|         - | 2226 | `	/* Reset node container */` |
|  12575457 | 2227 | `	SySetReset(pExprNode);` |
|  12575457 | 2228 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2229 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2230 | `	{` |
|  12575457 | 2231 | `		int iLastWasTerm = 0;` |
|  12575457 | 2232 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  80538429 | 2233 | `		while( pGen->pIn < pGen->pEnd ){` |
|  67963011 | 2234 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  67963011 | 2235 | `			if( rc != SXRET_OK ){` |
|        38 | 2236 | `				return rc;` |
|         - | 2237 | `			}` |
|         - | 2238 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  67962977 | 2239 | `			if( pNode->xCode ){` |
|         - | 2240 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  34362277 | 2241 | `				iLastWasTerm = 1;` |
|  50781841 | 2242 | `			}else if( pNode->pOp ){` |
|         - | 2243 | `				/* Operator node */` |
|  19307979 | 2244 | `				iLastWasTerm = 0;` |
|   9653992 | 2245 | `			}else{` |
|         - | 2246 | `				/* Delimiter: ')' and ']' end terms */` |
|  14292731 | 2247 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2248 | `			}` |
|         - | 2249 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2250 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2251 | `			 * node kind, so this single test covers all branches. */` |
|  67962977 | 2252 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2253 | `			/* Save the extracted node */` |
|  67962977 | 2254 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2255 | `		}` |
|         - | 2256 | `	}` |
|  12575423 | 2257 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2258 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2259 | `		*ppRoot = 0;` |
|       ! 0 | 2260 | `		return SXRET_OK;` |
|         - | 2261 | `	}` |
|  12575423 | 2262 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2263 | `	/* Make sure we are dealing with valid nodes */` |
|  12575423 | 2264 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  12575423 | 2265 | `	if( rc != SXRET_OK ){` |
|         - | 2266 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2267 | `		 * cleanup the mess left behind.` |
|         - | 2268 | `		 */` |
|        56 | 2269 | `		*ppRoot = 0;` |
|        56 | 2270 | `		return rc;` |
|         - | 2271 | `	}` |
|         - | 2272 | `	/* Build the tree */` |
|  12575371 | 2273 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  12575371 | 2274 | `	if( rc != SXRET_OK ){` |
|         - | 2275 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2276 | `		*ppRoot = 0;` |
|       103 | 2277 | `		return rc;` |
|         - | 2278 | `	}` |
|         - | 2279 | `	/* Point to the root of the tree */` |
|  12575273 | 2280 | `	*ppRoot = apNode[0];` |
|  12575273 | 2281 | `	return SXRET_OK;` |
|   6287731 | 2282 | `}` |
|         - | 2283 |  |
