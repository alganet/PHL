# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1163/1331 lines (87.38%)

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
|  22534896 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  22534901 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 336939324 |  278 | `	for(;;){` |
| 673878653 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 673878653 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  65706703 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  32853354 |  285 | `		}else{` |
| 608171955 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 673878653 |  288 | `		if( rc == 0 ){` |
|  22788859 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  22455213 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|    333651 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|     24205 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|    309451 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|     55501 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     55501 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|     55493 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|    126979 |  307 | `		}` |
| 651343757 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|  11267453 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   6325256 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   6325261 |  320 | `	SyToken *pCur = pIn;` |
|   6325261 |  321 | `	sxi32 iNest = 1;` |
|  70916841 |  322 | `	for(;;){` |
| 141833687 |  323 | `		if( pCur >= pEnd ){` |
|     16351 |  324 | `			break;` |
|         - |  325 | `		}` |
| 141817341 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   5516557 |  328 | `			iNest++;` |
| 139059065 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|  11825467 |  331 | `			iNest--;` |
|  11825467 |  332 | `			if( iNest <= 0 ){` |
|   6308915 |  333 | `				break;` |
|         - |  334 | `			}` |
|   2758276 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
| 135508431 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   6325261 |  340 | `	*ppEnd = pCur;` |
|   6325261 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    432276 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    432276 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    432178 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       167 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    432119 |  358 | `	if( bCheckFunc ){` |
|     39960 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|     39953 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|     39934 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        51 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|     19957 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    432073 |  366 | `	return FALSE;` |
|    216143 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|  13078874 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|  13078879 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|  13078879 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  83644501 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  70565661 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|    223403 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  70342263 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   5946715 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    274066 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   5612447 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   5612447 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   5612447 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   5612447 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   2806221 |  401 | `					}` |
|   2806221 |  402 | `			}` |
|   5946715 |  403 | `			iParen++;` |
|  67368908 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   5946715 |  405 | `			if( iParen <= 0 ){` |
|        16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        16 |  407 | `				if( rc != SXERR_ABORT ){` |
|        16 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        16 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   5946703 |  412 | `			iParen--;` |
|  61422192 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   2600887 |  414 | `			iSquare++;` |
|  57148402 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   2600901 |  416 | `			if( iSquare <= 0 ){` |
|         8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|         8 |  418 | `				if( rc != SXERR_ABORT ){` |
|         8 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         8 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   2600895 |  423 | `			iSquare--;` |
|  54547510 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|      3977 |  425 | `			iBraces++;` |
|      3977 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|  53245079 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|      3979 |  472 | `			if( iBraces <= 0 ){` |
|        15 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|        15 |  474 | `				if( rc != SXERR_ABORT ){` |
|        15 |  475 | `					rc = SXERR_SYNTAX;` |
|         6 |  476 | `				}` |
|        15 |  477 | `				return rc;` |
|         - |  478 | `			}` |
|      3967 |  479 | `			iBraces--;` |
|  53241100 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    383187 |  481 | `			if( iQuesty > 0 ){` |
|    382897 |  482 | `				iQuesty--;` |
|    191741 |  483 | `			}else if( iParen <= 0 ){` |
|         - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  488 | `				if( rc != SXERR_ABORT ){` |
|         6 |  489 | `					rc = SXERR_SYNTAX;` |
|         2 |  490 | `				}` |
|         6 |  491 | `				return rc;` |
|         5 |  492 | `			}` |
|  53047526 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|  17408289 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|  17408289 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    382899 |  496 | `				iQuesty++;` |
|  17216842 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     67755 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|     33875 |  516 | `			}` |
|   8704142 |  517 | `		}` |
|  35171117 |  518 | `	}` |
|  13078845 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        19 |  520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|        19 |  521 | `		if( rc != SXERR_ABORT ){` |
|        19 |  522 | `			rc = SXERR_SYNTAX;` |
|         8 |  523 | `		}` |
|        19 |  524 | `		return rc;` |
|         - |  525 | `	}` |
|  13078829 |  526 | `	return SXRET_OK;` |
|   6539442 |  527 | `}` |
|         - |  528 | `/*` |
|         - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  531 | ` */` |
|  11224490 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  533 | `{` |
|  11224495 |  534 | `	SyToken *pIn = *ppCur;` |
|         - |  535 | `	/* Jump the first literal seen */` |
|  11224495 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|  11220493 |  537 | `		pIn++;` |
|   5610244 |  538 | `	}` |
|   5614273 |  539 | `	for(;;){` |
|  11228551 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      4061 |  541 | `			pIn++;` |
|      4061 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4059 |  543 | `				pIn++;` |
|      2027 |  544 | `			}` |
|      2033 |  545 | `		}else{` |
|   5612250 |  546 | `			break;` |
|         - |  547 | `		}` |
|         5 |  548 | `	}` |
|         - |  549 | `	/* Synchronize pointers */` |
|  11224495 |  550 | `	*ppCur = pIn;` |
|  11224495 |  551 | `}` |
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
|       856 |  596 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  597 | `{` |
|       861 |  598 | `	SyToken *pIn = *ppIn;` |
|       861 |  599 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       861 |  637 | `	*ppIn = pIn;` |
|       861 |  638 | `}` |
|       492 |  639 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  640 | `{` |
|       497 |  641 | `	SyToken *pIn = *ppCur;` |
|         - |  642 | `	sxu32 nLine;` |
|         - |  643 | `	sxi32 rc;` |
|         - |  644 | `	/* Jump the 'function' keyword */` |
|       497 |  645 | `	nLine = pIn->nLine;` |
|       497 |  646 | `	pIn++;` |
|       497 |  647 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        12 |  648 | `		pIn++;` |
|         5 |  649 | `	}` |
|       497 |  650 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  651 | `		/* Syntax error */` |
|         6 |  652 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|         6 |  653 | `		if( rc != SXERR_ABORT ){` |
|         6 |  654 | `			rc = SXERR_SYNTAX;` |
|         2 |  655 | `		}` |
|         6 |  656 | `		goto Synchronize;` |
|         - |  657 | `	}` |
|       493 |  658 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       493 |  659 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       493 |  660 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  661 | `		/* Syntax error */` |
|         6 |  662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  663 | `		if( rc != SXERR_ABORT ){` |
|         6 |  664 | `			rc = SXERR_SYNTAX;` |
|         2 |  665 | `		}` |
|         6 |  666 | `		goto Synchronize;` |
|         - |  667 | `	}` |
|       489 |  668 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  669 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       489 |  670 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       489 |  671 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       107 |  672 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  673 | `		/* Check if we are dealing with a closure */` |
|       107 |  674 | `		if( nKey == PH7_TKWRD_USE ){` |
|        99 |  675 | `			pIn++; /* Jump the 'use' keyword */` |
|        99 |  676 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  677 | `				/* Syntax error */` |
|         6 |  678 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  679 | `				if( rc != SXERR_ABORT ){` |
|         6 |  680 | `					rc = SXERR_SYNTAX;` |
|         2 |  681 | `				}` |
|         6 |  682 | `				goto Synchronize;` |
|         - |  683 | `			}` |
|        95 |  684 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|        95 |  685 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        95 |  686 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  687 | `				/* Syntax error */` |
|         6 |  688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  689 | `				if( rc != SXERR_ABORT ){` |
|         6 |  690 | `					rc = SXERR_SYNTAX;` |
|         2 |  691 | `				}` |
|         6 |  692 | `				goto Synchronize;` |
|         - |  693 | `			}` |
|        91 |  694 | `			pIn++;` |
|         - |  695 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  696 | ``			 * `function (...) use (...) : int {` */`` |
|        91 |  697 | `			ExprSkipReturnType(&pIn,pEnd);` |
|        48 |  698 | `		}else{` |
|         - |  699 | `			/* Syntax error */` |
|        11 |  700 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        11 |  701 | `			if( rc != SXERR_ABORT ){` |
|        11 |  702 | `				rc = SXERR_SYNTAX;` |
|         4 |  703 | `			}` |
|        11 |  704 | `			goto Synchronize;` |
|         - |  705 | `		}` |
|        43 |  706 | `	}` |
|         - |  707 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  708 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  709 | `	 * the type), and pEnd is one past the last token. */` |
|       473 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       473 |  711 | `		pIn++; /* Jump the leading curly '{' */` |
|       473 |  712 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       473 |  713 | `		if( pIn < pEnd ){` |
|       473 |  714 | `			pIn++;` |
|       234 |  715 | `		}` |
|       239 |  716 | `	}else{` |
|         - |  717 | `		/* Syntax error */` |
|       ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|       ! 0 |  719 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  720 | `			return SXERR_ABORT;` |
|         - |  721 | `		}` |
|         - |  722 | `	}` |
|       473 |  723 | `	rc = SXRET_OK;` |
|       246 |  724 | `Synchronize:` |
|         - |  725 | `	/* Synchronize pointers */` |
|       497 |  726 | `	*ppCur = pIn;` |
|       497 |  727 | `	return rc;` |
|       251 |  728 | `}` |
|         - |  729 | `/*` |
|         - |  730 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  731 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  732 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  733 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  734 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  735 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  736 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  737 | ` */` |
|        28 |  738 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         4 |  739 | `{` |
|        32 |  740 | `	SyToken *pIn = *ppCur;` |
|        32 |  741 | `	sxu32 nLine = pIn->nLine;` |
|         - |  742 | `	sxi32 rc;` |
|        32 |  743 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  744 | `	/* Optional constructor argument list */` |
|        32 |  745 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         7 |  746 | `		pIn++; /* Jump '(' */` |
|         7 |  747 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         7 |  748 | `		if( pIn < pEnd ){` |
|         7 |  749 | `			pIn++; /* Jump ')' */` |
|         3 |  750 | `		}` |
|         3 |  751 | `	}` |
|         - |  752 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  753 | `	 * (no braces appear between ')' and the class body). */` |
|        60 |  754 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|        32 |  755 | `		pIn++;` |
|         4 |  756 | `	}` |
|        32 |  757 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  758 | `		/* Syntax error: missing class body */` |
|       ! 0 |  759 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  760 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  761 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  762 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  763 | `		}` |
|       ! 0 |  764 | `		*ppCur = pIn;` |
|       ! 0 |  765 | `		return rc;` |
|         - |  766 | `	}` |
|        32 |  767 | `	pIn++; /* Jump the leading '{' */` |
|        32 |  768 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|        32 |  769 | `	if( pIn < pEnd ){` |
|        32 |  770 | `		pIn++; /* Jump the trailing '}' */` |
|        14 |  771 | `	}` |
|        32 |  772 | `	*ppCur = pIn;` |
|        32 |  773 | `	return SXRET_OK;` |
|        18 |  774 | `}` |
|         - |  775 | `/*` |
|         - |  776 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  777 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  778 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  779 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  780 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  781 | ` */` |
|       286 |  782 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  783 | `{` |
|       291 |  784 | `	SyToken *pIn = *ppCur;` |
|         - |  785 | `	sxu32 nLine;` |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	int iNest;` |
|       291 |  788 | `	nLine = pIn->nLine;` |
|         - |  789 | `	/* Optional 'static' prefix */` |
|       286 |  790 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  791 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  792 | `		pIn++;` |
|         3 |  793 | `	}` |
|         - |  794 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       286 |  795 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  796 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  797 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  798 | `		goto Synchronize;` |
|         - |  799 | `	}` |
|       291 |  800 | `	pIn++; /* Jump 'fn' */` |
|       143 |  801 | `	SXUNUSED(nLine);` |
|       143 |  802 | `	SXUNUSED(pGen);` |
|         - |  803 | `	/* Optional '&' for return-by-reference */` |
|       291 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  805 | `		pIn++;` |
|       ! 0 |  806 | `	}` |
|         - |  807 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  808 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  809 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  810 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       291 |  811 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       289 |  812 | `		pIn++; /* '(' */` |
|       289 |  813 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       289 |  814 | `		if( pIn < pEnd ){` |
|       287 |  815 | `			pIn++; /* ')' */` |
|       141 |  816 | `		}` |
|       142 |  817 | `	}` |
|         - |  818 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       291 |  819 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  820 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       291 |  821 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       285 |  822 | `		pIn++;` |
|       140 |  823 | `	}` |
|         - |  824 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       291 |  825 | `	iNest = 0;` |
|      2007 |  826 | `	while( pIn < pEnd ){` |
|      1895 |  827 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  828 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       175 |  829 | `			break;` |
|         - |  830 | `		}` |
|      1721 |  831 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       196 |  832 | `			iNest++;` |
|      1625 |  833 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       196 |  834 | `			iNest--;` |
|        96 |  835 | `		}` |
|      1721 |  836 | `		pIn++;` |
|         5 |  837 | `	}` |
|       291 |  838 | `	rc = SXRET_OK;` |
|       143 |  839 | `Synchronize:` |
|       291 |  840 | `	*ppCur = pIn;` |
|       291 |  841 | `	return rc;` |
|         5 |  842 | `}` |
|         - |  843 | `/*` |
|         - |  844 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - |  845 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - |  846 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - |  847 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - |  848 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - |  849 | ` */` |
|        72 |  850 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  851 | `{` |
|        77 |  852 | `	SyToken *pIn = *ppCur;` |
|         - |  853 | `	sxi32 rc;` |
|        36 |  854 | `	SXUNUSED(pGen);` |
|         - |  855 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|        72 |  856 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        77 |  857 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 |  858 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  859 | `		goto Synchronize;` |
|         - |  860 | `	}` |
|        77 |  861 | `	pIn++; /* Jump 'match' */` |
|         - |  862 | `	/* Optional '(' subject ')' */` |
|        77 |  863 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        77 |  864 | `		pIn++;` |
|        77 |  865 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|        77 |  866 | `		if( pIn < pEnd ){` |
|        77 |  867 | `			pIn++; /* ')' */` |
|        36 |  868 | `		}` |
|        36 |  869 | `	}` |
|         - |  870 | `	/* Optional '{' arms '}' */` |
|        77 |  871 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|        77 |  872 | `		pIn++;` |
|        77 |  873 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|        77 |  874 | `		if( pIn < pEnd ){` |
|        77 |  875 | `			pIn++; /* '}' */` |
|        36 |  876 | `		}` |
|        36 |  877 | `	}` |
|        77 |  878 | `	rc = SXRET_OK;` |
|        36 |  879 | `Synchronize:` |
|        77 |  880 | `	*ppCur = pIn;` |
|        77 |  881 | `	return rc;` |
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
|  70570010 |  892 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  893 | `{` |
|         - |  894 | `	ph7_expr_node *pNode;` |
|         - |  895 | `	SyToken *pCur;` |
|         - |  896 | `	sxi32 rc;` |
|         - |  897 | `	/* Allocate a new node */` |
|  70570015 |  898 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  70570015 |  899 | `	if( pNode == 0 ){` |
|         - |  900 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  901 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  902 | `		 */` |
|       ! 0 |  903 | `		return SXERR_MEM;` |
|         - |  904 | `	}` |
|         - |  905 | `	/* Zero the structure */` |
|  70570015 |  906 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  70570015 |  907 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  908 | `	/* Point to the head of the token stream */` |
|  70570015 |  909 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  910 | `	/* Start collecting tokens */` |
|  70570015 |  911 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4289 |  912 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - |  913 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - |  914 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - |  915 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - |  916 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|        81 |  917 | `			pNode->pEnd = pCur;` |
|        81 |  918 | `			pCur++;` |
|        81 |  919 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|        81 |  920 | `			pNode->xCode = PH7_CompileFccMarker;` |
|        81 |  921 | `			pGen->pIn = pCur;` |
|        81 |  922 | `			*ppNode = pNode;` |
|        81 |  923 | `			return SXRET_OK;` |
|         - |  924 | `		}` |
|         - |  925 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - |  926 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|      4209 |  927 | `		pCur++;` |
|      4209 |  928 | `		pGen->pIn = pCur;` |
|      4209 |  929 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      4209 |  930 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      4209 |  931 | `		if( rc == SXRET_OK && *ppNode ){` |
|      4209 |  932 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      2102 |  933 | `		}` |
|      4209 |  934 | `		return rc;` |
|         - |  935 | `	}` |
|  70565731 |  936 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  937 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  938 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  939 | `		 */` |
|    223405 |  940 | `		pCur++; /* Skip the opening '[' */` |
|    223405 |  941 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|    223405 |  942 | `		if( pCur < pGen->pEnd ){` |
|    223405 |  943 | `			pCur++; /* Skip past the closing ']' */` |
|    111705 |  944 | `		}else{` |
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
|    223575 |  956 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       342 |  957 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       342 |  958 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        56 |  959 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  960 | `			}else{` |
|       287 |  961 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  962 | `			}` |
|       172 |  963 | `		}else{` |
|    223065 |  964 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  965 | `		}` |
|  70454031 |  966 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|  70342319 |  977 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|  20009222 |  978 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|  10032325 |  979 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|  70342304 | 1005 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1006 | `		/* Point to the instance that describe this operator */` |
|  20009205 | 1007 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1008 | `		/* Advance the stream cursor */` |
|  20009205 | 1009 | `		pCur++;` |
|  60337693 | 1010 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1011 | `		/* Isolate variable */` |
|  33120693 | 1012 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  16560355 | 1013 | `			pCur++; /* Variable variable */` |
|         5 | 1014 | `		}` |
|  16560343 | 1015 | `		if( pCur < pGen->pEnd ){` |
|  16560343 | 1016 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1017 | `				/* Variable name */` |
|  16560313 | 1018 | `				pCur++;` |
|   8280189 | 1019 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        26 | 1020 | `				pCur++;` |
|         - | 1021 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        26 | 1022 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        26 | 1023 | `				if( pCur < pGen->pEnd ){` |
|        21 | 1024 | `					pCur++;` |
|        12 | 1025 | `				}else{` |
|         5 | 1026 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|         5 | 1027 | `					if( rc != SXERR_ABORT ){` |
|         5 | 1028 | `						rc = SXERR_SYNTAX;` |
|         2 | 1029 | `					}` |
|         5 | 1030 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         5 | 1031 | `					return rc;` |
|         - | 1032 | `				}` |
|         9 | 1033 | `			}` |
|   8280167 | 1034 | `		}` |
|  16560339 | 1035 | `		pNode->xCode = PH7_CompileVariable;` |
|  42052922 | 1036 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    834539 | 1037 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    834539 | 1038 | `		 if( bAfterMemberOp ){` |
|         - | 1039 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1040 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1041 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1042 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    130695 | 1043 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    130695 | 1044 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    769194 | 1045 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1046 | `			 /* List/Array node */` |
|    294489 | 1047 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1048 | `				 /* Assume a literal */` |
|       ! 0 | 1049 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1050 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1051 | `			 }else{` |
|    294489 | 1052 | `				 pCur += 2;` |
|         - | 1053 | `				 /* Collect array/list tokens */` |
|    294489 | 1054 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    294489 | 1055 | `				 if( pCur < pGen->pEnd ){` |
|    294487 | 1056 | `					 pCur++;` |
|    147246 | 1057 | `				 }else{` |
|         - | 1058 | `					 /* Syntax error */` |
|         4 | 1059 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1060 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1061 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1062 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1063 | `					 }` |
|         3 | 1064 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1065 | `					 return rc;` |
|         - | 1066 | `				 }` |
|    294487 | 1067 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    294487 | 1068 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        39 | 1069 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        39 | 1070 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1071 | `						 /* Syntax error */` |
|         3 | 1072 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|         3 | 1073 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1074 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1075 | `						 }` |
|         3 | 1076 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1077 | `						 return rc;` |
|         - | 1078 | `					 }` |
|        16 | 1079 | `				 }` |
|         5 | 1080 | `			 }` |
|    556605 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1082 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|     16205 | 1083 | `			 pCur++; /* Skip 'yield' keyword */` |
|     16205 | 1084 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1085 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1086 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|     16205 | 1087 | `			 pNode->xCode = PH7_CompileYield;` |
|    401265 | 1088 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|    392930 | 1089 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        54 | 1090 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        33 | 1091 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1092 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|       497 | 1093 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1094 | `				 /* Assume a literal */` |
|       ! 0 | 1095 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1096 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1097 | `			 }else{` |
|         - | 1098 | `				 /* Assemble annonymous functions body */` |
|       497 | 1099 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       497 | 1100 | `				 if( rc != SXRET_OK ){` |
|        28 | 1101 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1102 | `					 return rc;` |
|         - | 1103 | `				 }` |
|       473 | 1104 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1105 | `			  }` |
|    392907 | 1106 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|        41 | 1107 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        23 | 1108 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        12 | 1109 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|         9 | 1110 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1111 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1112 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1113 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1114 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|        32 | 1115 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|        32 | 1116 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1117 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1118 | `				 return rc;` |
|         - | 1119 | `			 }` |
|        32 | 1120 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    392658 | 1121 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    392508 | 1122 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        46 | 1123 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        25 | 1124 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1125 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       291 | 1126 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       291 | 1127 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1128 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1129 | `				 return rc;` |
|         - | 1130 | `			 }` |
|       291 | 1131 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    392502 | 1132 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1133 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        77 | 1134 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        77 | 1135 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1136 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1137 | `				 return rc;` |
|         - | 1138 | `			 }` |
|        77 | 1139 | `			 pNode->xCode = PH7_CompileMatch;` |
|    392323 | 1140 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1141 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1142 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1143 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1144 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1145 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1146 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1147 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1148 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    392269 | 1149 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1150 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        93 | 1151 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        93 | 1152 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        49 | 1153 | `		 }else{` |
|         - | 1154 | `			 /* Assume a literal */` |
|    392163 | 1155 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    392163 | 1156 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1157 | `		 }` |
|  33355474 | 1158 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1159 | `		 /* Constants,function name,namespace path,class name... */` |
|  10701631 | 1160 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|  10701631 | 1161 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   5350818 | 1162 | `	 }else{` |
|  22236595 | 1163 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1164 | `			 /* Point to the code generator routine */` |
|   7351129 | 1165 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   7351129 | 1166 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1167 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|         3 | 1168 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1169 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1170 | `				 }` |
|         3 | 1171 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1172 | `				 return rc;` |
|         - | 1173 | `			 }` |
|   3675561 | 1174 | `		 }` |
|         - | 1175 | `		/* Advance the stream cursor */` |
|  22236593 | 1176 | `		pCur++;` |
|         - | 1177 | `	 }` |
|         - | 1178 | `	/* Point to the end of the token stream */` |
|  70565697 | 1179 | `	pNode->pEnd = pCur;` |
|         - | 1180 | `	/* Save the node for later processing */` |
|  70565697 | 1181 | `	*ppNode = pNode;` |
|         - | 1182 | `	/* Synchronize cursors */` |
|  70565697 | 1183 | `	pGen->pIn = pCur;` |
|  70565697 | 1184 | `	return SXRET_OK;` |
|  35285010 | 1185 | `}` |
|         - | 1186 | `/*` |
|         - | 1187 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1188 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1189 | ` * level is zero.` |
|         - | 1190 | ` */` |
|   1360080 | 1191 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1192 | `{` |
|   1360085 | 1193 | `	SyToken *pCur = pStart;` |
|   1360085 | 1194 | `	sxi32 iNest = 0;` |
|   1360085 | 1195 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1196 | `		/* Last expression */` |
|    562643 | 1197 | `		return SXERR_EOF;` |
|         - | 1198 | `	}` |
|   3487043 | 1199 | `	while( pCur < pEnd ){` |
|   3235443 | 1200 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    545847 | 1201 | `			break;` |
|         - | 1202 | `		}` |
|   2689601 | 1203 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    220627 | 1204 | `			iNest++;` |
|   2579290 | 1205 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    220629 | 1206 | `			iNest--;` |
|    110312 | 1207 | `		}` |
|   2689601 | 1208 | `		pCur++;` |
|         5 | 1209 | `	}` |
|    797447 | 1210 | `	*ppNext = pCur;` |
|    797447 | 1211 | `	return SXRET_OK;` |
|    680045 | 1212 | `}` |
|         - | 1213 | `/*` |
|         - | 1214 | ` * Free an expression tree.` |
|         - | 1215 | ` */` |
|  60292080 | 1216 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1217 | `{` |
|  60292085 | 1218 | `	if( pNode->pLeft ){` |
|         - | 1219 | `		/* Release the left tree */` |
|  23939995 | 1220 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|  11969995 | 1221 | `	}` |
|  60292085 | 1222 | `	if( pNode->pRight ){` |
|         - | 1223 | `		/* Release the right tree */` |
|  13926313 | 1224 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   6963154 | 1225 | `	}` |
|  60292085 | 1226 | `	if( pNode->pCond ){` |
|         - | 1227 | `		/* Release the conditional tree used by the ternary operator */` |
|    382895 | 1228 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|    191445 | 1229 | `	}` |
|  60292085 | 1230 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1231 | `		ph7_expr_node **apArg;` |
|         - | 1232 | `		sxu32 n;` |
|         - | 1233 | `		/* Release node arguments */` |
|   6609207 | 1234 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  14895853 | 1235 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   8286651 | 1236 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   4143328 | 1237 | `		}` |
|   6609207 | 1238 | `		SySetRelease(&pNode->aNodeArgs);` |
|   3304601 | 1239 | `	}` |
|         - | 1240 | `	/* Finally,release this node */` |
|  60292085 | 1241 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  60292085 | 1242 | `}` |
|         - | 1243 | `/*` |
|         - | 1244 | ` * Free an expression tree.` |
|         - | 1245 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1246 | ` */` |
|  13078904 | 1247 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1248 | `{` |
|         - | 1249 | `	ph7_expr_node **apNode;` |
|         - | 1250 | `	sxu32 n;` |
|  13078909 | 1251 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  83644651 | 1252 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  70565747 | 1253 | `		if( apNode[n] ){` |
|  13079243 | 1254 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   6539619 | 1255 | `		}` |
|  35282876 | 1256 | `	}` |
|  13078909 | 1257 | `	return SXRET_OK;` |
|         5 | 1258 | `}` |
|         - | 1259 | `/*` |
|         - | 1260 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1261 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1262 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1263 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1264 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1265 | ` */` |
|  17147568 | 1266 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1267 | `{` |
|  17147573 | 1268 | `	if( pNode == 0 ){` |
|  10645315 | 1269 | `		return 0;` |
|         - | 1270 | `	}` |
|   6502263 | 1271 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1272 | `		return 1;` |
|         - | 1273 | `	}` |
|   6502251 | 1274 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1275 | `		return 1;` |
|         - | 1276 | `	}` |
|   6502247 | 1277 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1278 | `		return 1;` |
|         - | 1279 | `	}` |
|   6502247 | 1280 | `	return 0;` |
|   8573789 | 1281 | `}` |
|         - | 1282 | `/*` |
|         - | 1283 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1284 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1285 | ` */` |
|   4112172 | 1286 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1287 | `{` |
|         - | 1288 | `	sxi32 iExprOp;` |
|   4112177 | 1289 | `	if( pNode->pOp == 0 ){` |
|   2916601 | 1290 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1291 | `	}` |
|   1195581 | 1292 | `	iExprOp = pNode->pOp->iOp;` |
|   1195581 | 1293 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    835063 | 1294 | `			return TRUE;` |
|         - | 1295 | `	}` |
|    360523 | 1296 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    360517 | 1297 | `		if( pNode->pLeft->pOp ) {` |
|    122656 | 1298 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|     51438 | 1299 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1300 | `				return FALSE;` |
|         5 | 1301 | `			}` |
|    299189 | 1302 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1303 | `			return FALSE;` |
|         - | 1304 | `		}` |
|    360517 | 1305 | `		return TRUE;` |
|         - | 1306 | `	}` |
|         8 | 1307 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1308 | `		return TRUE;` |
|         - | 1309 | `	}` |
|         - | 1310 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1311 | `	return FALSE;` |
|   2056091 | 1312 | `}` |
|         - | 1313 | `/* Forward declaration */` |
|         - | 1314 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1315 | `/* Macro to check if the given node is a terminal.` |
|         - | 1316 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1317 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1318 | ` * linked ternary/elvis node). */` |
|         - | 1319 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1320 | `/*` |
|         - | 1321 | ` * Buid an expression tree for each given function argument.` |
|         - | 1322 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1323 | ` */` |
|   4170648 | 1324 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1325 | `{` |
|         - | 1326 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1327 | `	sxi32 rc;` |
|         - | 1328 | `	/* Process function arguments from left to right */` |
|   4170653 | 1329 | `	iCur = 0;` |
|   5009358 | 1330 | `	for(;;){` |
|  10018721 | 1331 | `		if( iCur >= nToken ){` |
|         - | 1332 | `			/* No more arguments to process */` |
|   4170627 | 1333 | `			break;` |
|         - | 1334 | `		}` |
|   5848099 | 1335 | `		iNode = iCur;` |
|   5848099 | 1336 | `		iNest = 0;` |
|  19560327 | 1337 | `		while( iCur < nToken ){` |
|  15389703 | 1338 | `			if( apNode[iCur] ){` |
|  15342077 | 1339 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    838740 | 1340 | `					break;` |
|  13664602 | 1341 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   7366103 | 1342 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|   1065099 | 1343 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1344 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1345 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1346 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1347 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1348 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1349 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|   1062589 | 1350 | `					iNest++;` |
|  13133315 | 1351 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|   1062589 | 1352 | `					iNest--;` |
|    531292 | 1353 | `				}` |
|   6832301 | 1354 | `			}` |
|  13712233 | 1355 | `			iCur++;` |
|         5 | 1356 | `		}` |
|   5848099 | 1357 | `		if( iCur > iNode ){` |
|   5848093 | 1358 | `			SyString sArgName = {0, 0};` |
|         - | 1359 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1360 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1361 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   5848088 | 1362 | `			if( (iCur - iNode) >= 2` |
|   4045220 | 1363 | `				&& apNode[iNode]` |
|   2242338 | 1364 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|   1189657 | 1365 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|    136737 | 1366 | `				&& apNode[iNode+1]` |
|    136489 | 1367 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1368 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       291 | 1369 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       291 | 1370 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       291 | 1371 | `				apNode[iNode] = 0;` |
|       291 | 1372 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       291 | 1373 | `				apNode[iNode+1] = 0;` |
|       291 | 1374 | `				iNode += 2;` |
|         - | 1375 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1376 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       291 | 1377 | `				if( iNode >= iCur ){` |
|         4 | 1378 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1379 | `						pOp->pStart->nLine,` |
|         - | 1380 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1381 | `						&sArgName);` |
|         3 | 1382 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1383 | `						rc = SXERR_SYNTAX;` |
|         1 | 1384 | `					}` |
|         3 | 1385 | `					return rc;` |
|         - | 1386 | `				}` |
|       142 | 1387 | `			}` |
|   5848086 | 1388 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1389 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1390 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1391 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1392 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1393 | `					apNode[iNode] = 0;` |
|       ! 0 | 1394 | `			}` |
|         - | 1395 | `			{` |
|         - | 1396 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1397 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1398 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1399 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1400 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1401 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1402 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1403 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1404 | `				 * NULL slots ahead of the flagged subtree. */` |
|   5848091 | 1405 | `				int bSpreadArg = 0;` |
|         - | 1406 | `				sxi32 iScan;` |
|   5848119 | 1407 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   5848119 | 1408 | `					if( apNode[iScan] ){` |
|   5848091 | 1409 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   5848091 | 1410 | `						break;` |
|         - | 1411 | `					}` |
|        15 | 1412 | `				}` |
|   5848091 | 1413 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   5848091 | 1414 | `				if( bSpreadArg && apNode[iNode] ){` |
|      4143 | 1415 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|      2069 | 1416 | `				}` |
|         - | 1417 | `			}` |
|   5848091 | 1418 | `			if( apNode[iNode] ){` |
|   5848091 | 1419 | `				if( sArgName.nByte > 0 ){` |
|       289 | 1420 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       289 | 1421 | `					apNode[iNode]->sArgName = sArgName;` |
|       142 | 1422 | `				}` |
|         - | 1423 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   5848091 | 1424 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   2924048 | 1425 | `			}else{` |
|         - | 1426 | `				/* No expression before comma */` |
|       ! 0 | 1427 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1428 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1429 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1430 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1431 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1432 | `				}` |
|       ! 0 | 1433 | `				return rc;` |
|         - | 1434 | `			}` |
|   2924048 | 1435 | `		}else{` |
|         - | 1436 | `			/* Comma with no preceding argument */` |
|         8 | 1437 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1438 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1439 | `				rc = SXERR_SYNTAX;` |
|         3 | 1440 | `			}` |
|         8 | 1441 | `			return rc;` |
|         - | 1442 | `		}` |
|         - | 1443 | `		/* Jump trailing comma */` |
|   5848091 | 1444 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|   1677469 | 1445 | `			iCur++;` |
|   1677469 | 1446 | `			if( iCur >= nToken ){` |
|         - | 1447 | `				/* Trailing comma after last argument */` |
|        19 | 1448 | `				break;` |
|         - | 1449 | `			}` |
|    838723 | 1450 | `		}` |
|         5 | 1451 | `	}` |
|   4170645 | 1452 | `	return SXRET_OK;` |
|   2085329 | 1453 | `}` |
|         - | 1454 | ` /*` |
|         - | 1455 | `  * Create an expression tree from an array of tokens.` |
|         - | 1456 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1457 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1458 | `  */` |
|  22588412 | 1459 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1460 | ` {` |
|         - | 1461 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1462 | `	 ph7_expr_node *pNode;` |
|         - | 1463 | `	 sxi32 iCur;` |
|         - | 1464 | `	 sxi32 rc;` |
|  22588417 | 1465 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1466 | `		 /* TICKET 1433-17: self evaluating node */` |
|   9813821 | 1467 | `		 return SXRET_OK;` |
|         - | 1468 | `	 }` |
|         - | 1469 | `	 /* Process expressions enclosed in parenthesis first */` |
|  92133629 | 1470 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1471 | `		 sxi32 iNest;` |
|         - | 1472 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1473 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1474 | `		  */` |
|  79359035 | 1475 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  79024777 | 1476 | `			 continue;` |
|         - | 1477 | `		 }` |
|    334263 | 1478 | `		 iNest = 1;` |
|    334263 | 1479 | `		 iLeft = iCur;` |
|         - | 1480 | `		 /* Find the closing parenthesis */` |
|    334263 | 1481 | `		 iCur++;` |
|   2968917 | 1482 | `		 while( iCur < nToken ){` |
|   2968917 | 1483 | `			 if( apNode[iCur] ){` |
|   2968917 | 1484 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1485 | `					 /* Decrement nesting level */` |
|    465933 | 1486 | `					 iNest--;` |
|    465933 | 1487 | `					 if( iNest <= 0 ){` |
|    334263 | 1488 | `						 break;` |
|         5 | 1489 | `					 }` |
|   2568824 | 1490 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1491 | `					 /* Increment nesting level */` |
|    131675 | 1492 | `					 iNest++;` |
|     65835 | 1493 | `				 }` |
|   1317327 | 1494 | `			 }` |
|   2634659 | 1495 | `			 iCur++;` |
|         5 | 1496 | `		 }` |
|    334263 | 1497 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1498 | `			 sxi32 j;` |
|         - | 1499 | `			 /* Recurse and process this expression */` |
|    334263 | 1500 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    334263 | 1501 | `			 if( rc != SXRET_OK ){` |
|         3 | 1502 | `				 return rc;` |
|         - | 1503 | `			 }` |
|         - | 1504 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1505 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1506 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1507 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1508 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1509 | `			  * group's free below silently drops the unpacking. */` |
|    334261 | 1510 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    334261 | 1511 | `				 if( apNode[j] ){` |
|    334261 | 1512 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    334256 | 1513 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    334261 | 1514 | `					 break;` |
|         - | 1515 | `				 }` |
|       ! 0 | 1516 | `			 }` |
|    167128 | 1517 | `		 }` |
|         - | 1518 | `		 /* Free the left and right nodes */` |
|    334261 | 1519 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    334261 | 1520 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    334261 | 1521 | `		 apNode[iLeft] = 0;` |
|    334261 | 1522 | `		 apNode[iCur] = 0;` |
|    167133 | 1523 | `	 }` |
|         - | 1524 | `	  /* Process expressions enclosed in braces */` |
|  94848675 | 1525 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1526 | `		 sxi32 iNest;` |
|         - | 1527 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1528 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1529 | `		  */` |
|  82320013 | 1530 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  82316051 | 1531 | `			 continue;` |
|         - | 1532 | `		 }` |
|      3967 | 1533 | `		 iNest = 1;` |
|      3967 | 1534 | `		 iLeft = iCur;` |
|         - | 1535 | `		 /* Find the closing parenthesis */` |
|      3967 | 1536 | `		 iCur++;` |
|      7927 | 1537 | `		 while( iCur < nToken ){` |
|      7927 | 1538 | `			 if( apNode[iCur] ){` |
|      7927 | 1539 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1540 | `					 /* Decrement nesting level */` |
|      3967 | 1541 | `					 iNest--;` |
|      3967 | 1542 | `					 if( iNest <= 0 ){` |
|      3967 | 1543 | `						 break;` |
|       ! 0 | 1544 | `					 }` |
|      3965 | 1545 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1546 | `					 /* Increment nesting level */` |
|       ! 0 | 1547 | `					 iNest++;` |
|       ! 0 | 1548 | `				 }` |
|      1980 | 1549 | `			 }` |
|      3965 | 1550 | `			 iCur++;` |
|         5 | 1551 | `		 }` |
|      3967 | 1552 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1553 | `			 /* Recurse and process this expression */` |
|      3965 | 1554 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      3965 | 1555 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1556 | `				 return rc;` |
|         - | 1557 | `			 }` |
|      1980 | 1558 | `		 }` |
|         - | 1559 | `		 /* Free the left and right nodes */` |
|      3967 | 1560 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      3967 | 1561 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      3967 | 1562 | `		 apNode[iLeft] = 0;` |
|      3967 | 1563 | `		 apNode[iCur] = 0;` |
|      1986 | 1564 | `	 }` |
|         - | 1565 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|  12528667 | 1566 | `	 iLeft = -1;` |
|  94856561 | 1567 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  82327911 | 1568 | `		 if( apNode[iCur] == 0 ){` |
|  36104211 | 1569 | `			 continue;` |
|         - | 1570 | `		 }` |
|  46223705 | 1571 | `		 pNode = apNode[iCur];` |
|  46223705 | 1572 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|  13273527 | 1573 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1574 | `				 /* Collect function arguments */` |
|   5612443 | 1575 | `				 sxi32 iPtr = 0;` |
|   5612443 | 1576 | `				 sxi32 nFuncTok = 0;` |
|  26614581 | 1577 | `				 while( nFuncTok + iCur < nToken ){` |
|  26614581 | 1578 | `					 if( apNode[nFuncTok+iCur] ){` |
|  26566955 | 1579 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   5935119 | 1580 | `							 iPtr++;` |
|  23599398 | 1581 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   5935119 | 1582 | `							 iPtr--;` |
|   5935119 | 1583 | `							 if( iPtr <= 0 ){` |
|   5612443 | 1584 | `								 break;` |
|         - | 1585 | `							 }` |
|    161338 | 1586 | `						 }` |
|  10477256 | 1587 | `					 }` |
|  21002143 | 1588 | `					 nFuncTok++;` |
|         5 | 1589 | `				 }` |
|   5612443 | 1590 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1591 | `					 /* Syntax error */` |
|       ! 0 | 1592 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1593 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1594 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1595 | `					 }` |
|       ! 0 | 1596 | `					 return rc;` |
|         - | 1597 | `				 }` |
|   5612443 | 1598 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1599 | `					 /* Syntax error */` |
|       ! 0 | 1600 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1601 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1602 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1603 | `					 }` |
|       ! 0 | 1604 | `					 return rc;` |
|         - | 1605 | `				 }` |
|   5612443 | 1606 | `				 if( nFuncTok > 1 ){` |
|         - | 1607 | `					 /* Process function arguments */` |
|   4170653 | 1608 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   4170653 | 1609 | `					 if( rc != SXRET_OK ){` |
|        10 | 1610 | `						 return rc;` |
|         - | 1611 | `					 }` |
|   2085320 | 1612 | `				 }` |
|         - | 1613 | `				 /* Link the node to the tree */` |
|   5612435 | 1614 | `				 pNode->pLeft = apNode[iLeft];` |
|   5612435 | 1615 | `				 apNode[iLeft] = 0;` |
|  26614549 | 1616 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  21002119 | 1617 | `					 apNode[iCur+iPtr] = 0;` |
|  10501062 | 1618 | `				 }` |
|         - | 1619 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1620 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1621 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1622 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1623 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1624 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1625 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1626 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1627 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1628 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1629 | `				 {` |
|   5612435 | 1630 | `					 sxi32 iNew = iLeft - 1;` |
|   7496823 | 1631 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|   1884393 | 1632 | `						 iNew--;` |
|         5 | 1633 | `					 }` |
|   5612430 | 1634 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   3329770 | 1635 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   2029932 | 1636 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    738011 | 1637 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    738011 | 1638 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    738011 | 1639 | `						 apNode[iNew] = 0;` |
|    738011 | 1640 | `						 pNode = apNode[iCur];` |
|    369008 | 1641 | `					 }` |
|         - | 1642 | `				 }` |
|  10467304 | 1643 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1644 | `				 /* Subscripting */` |
|   2600895 | 1645 | `				 sxi32 iArrTok = iCur + 1;` |
|   2600895 | 1646 | `				 sxi32 iNest = 1;` |
|   2600890 | 1647 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1648 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1649 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1650 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   2600890 | 1651 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1652 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1653 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|    310638 | 1654 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1655 | `						 /* Syntax error */` |
|       ! 0 | 1656 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1657 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1658 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1659 | `						 }` |
|       ! 0 | 1660 | `						 return rc;` |
|         - | 1661 | `				 }` |
|         - | 1662 | `				 /* Collect index tokens */` |
|   5498265 | 1663 | `				 while( iArrTok < nToken ){` |
|   5498265 | 1664 | `					 if( apNode[iArrTok] ){` |
|   5498233 | 1665 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1666 | `							 /* Increment nesting level */` |
|     19775 | 1667 | `							 iNest++;` |
|   5488348 | 1668 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1669 | `							 /* Decrement nesting level */` |
|   2620665 | 1670 | `							 iNest--;` |
|   2620665 | 1671 | `							 if( iNest <= 0 ){` |
|   2600895 | 1672 | `								 break;` |
|         - | 1673 | `							 }` |
|      9885 | 1674 | `						 }` |
|   1448669 | 1675 | `					 }` |
|   2897375 | 1676 | `					 ++iArrTok;` |
|         5 | 1677 | `				 }` |
|   2600895 | 1678 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1679 | `					 /* Recurse and process this expression */` |
|   2438565 | 1680 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   2438565 | 1681 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1682 | `						 return rc;` |
|         - | 1683 | `					 }` |
|         - | 1684 | `					 /* Link the node to it's index */` |
|   2438565 | 1685 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|   1219280 | 1686 | `				 }` |
|         - | 1687 | `				 /* Link the node to the tree */` |
|   2600895 | 1688 | `				 pNode->pLeft = apNode[iLeft];` |
|   2600895 | 1689 | `				 pNode->pRight = 0;` |
|   2600895 | 1690 | `				 apNode[iLeft] = 0;` |
|   8099155 | 1691 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   5498265 | 1692 | `					 apNode[iNest] = 0;` |
|   2749135 | 1693 | `				 }` |
|   1300450 | 1694 | `			 }else{` |
|         - | 1695 | `				 /* Member access operators [i.e: '->','::'] */` |
|   5060199 | 1696 | `				  iRight = iCur + 1;` |
|   5064159 | 1697 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      3965 | 1698 | `					 iRight++;` |
|         5 | 1699 | `				 }` |
|   5060199 | 1700 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1701 | `					 /* Syntax error */` |
|         5 | 1702 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1703 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1704 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1705 | `					 }` |
|         5 | 1706 | `					 return rc;` |
|         - | 1707 | `				 }` |
|         - | 1708 | `				 /* Link the node to the tree */` |
|   5060195 | 1709 | `				 pNode->pLeft = apNode[iLeft];` |
|   5060190 | 1710 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   4873911 | 1711 | `					 && pNode->pLeft->pOp == 0 &&` |
|   4608011 | 1712 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|         - | 1713 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|         - | 1714 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|         - | 1715 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|         - | 1716 | `					  * accepts. */` |
|         4 | 1717 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|         - | 1718 | `						 /* Syntax error */` |
|       ! 0 | 1719 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1720 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|       ! 0 | 1721 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1722 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1723 | `						 }` |
|       ! 0 | 1724 | `						 return rc;` |
|         - | 1725 | `				 }` |
|   5060195 | 1726 | `				 pNode->pRight = apNode[iRight];` |
|   5060195 | 1727 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1728 | `			 }` |
|   6636755 | 1729 | `		 }` |
|  46223693 | 1730 | `		 iLeft = iCur;` |
|  23111849 | 1731 | `	 }` |
|         - | 1732 | `	 /* Handle left associative (new, clone) operators */` |
|  94856529 | 1733 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  82327879 | 1734 | `		 if( apNode[iCur] == 0 ){` |
|  50171447 | 1735 | `			 continue;` |
|         - | 1736 | `		 }` |
|  32156437 | 1737 | `		 pNode = apNode[iCur];` |
|  32156437 | 1738 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1739 | `			 SyToken *pToken;` |
|         - | 1740 | `			 /* Get the left node */` |
|     55725 | 1741 | `			 iLeft = iCur + 1;` |
|     55733 | 1742 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         9 | 1743 | `				 iLeft++;` |
|         1 | 1744 | `			 }` |
|     55725 | 1745 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1746 | `				  /* Syntax error */` |
|       ! 0 | 1747 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1748 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1749 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1750 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1751 | `				 }` |
|       ! 0 | 1752 | `				 return rc;` |
|         - | 1753 | `			 }` |
|         - | 1754 | `			 /* Make sure the operand are of a valid type */` |
|     55725 | 1755 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1756 | `				 /* Clone:` |
|         - | 1757 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|         - | 1758 | `				  *  ++ function call (including annonymous)` |
|         - | 1759 | `				  *  ++ array member` |
|         - | 1760 | `				  *  ++ 'new' operator` |
|         - | 1761 | `				  * Example:` |
|         - | 1762 | `				  *   clone $pObj;` |
|         - | 1763 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|         - | 1764 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|         - | 1765 | `				  */` |
|     55401 | 1766 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|     55395 | 1767 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1768 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1769 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1770 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1771 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1772 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1773 | `						 }` |
|       ! 0 | 1774 | `						 return rc;` |
|         - | 1775 | `					 }` |
|     27695 | 1776 | `				 }` |
|     27703 | 1777 | `			 }else{` |
|         - | 1778 | `				 /* New */` |
|       324 | 1779 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 1780 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 1781 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 1782 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 1783 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 1784 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 1785 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 1786 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 1787 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1788 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1789 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1790 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1791 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1792 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1793 | `					 }` |
|       ! 0 | 1794 | `					 return rc;` |
|         - | 1795 | `				 }` |
|       329 | 1796 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       329 | 1797 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       324 | 1798 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        33 | 1799 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 1800 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 1801 | `						 /* Syntax error */` |
|       ! 0 | 1802 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1803 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1804 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1805 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1806 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1807 | `						 }` |
|       ! 0 | 1808 | `						 return rc;` |
|         - | 1809 | `					 }` |
|       162 | 1810 | `				 }` |
|         - | 1811 | `			 }` |
|         - | 1812 | `			  /* Link the node to the tree */` |
|     55725 | 1813 | `			 pNode->pLeft = apNode[iLeft];` |
|     55725 | 1814 | `			 apNode[iLeft] = 0;` |
|     55725 | 1815 | `			 pNode->pRight = 0; /* Paranoid */` |
|     27860 | 1816 | `		 }` |
|  16078221 | 1817 | `	 }` |
|         - | 1818 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|  12528655 | 1819 | `	 iLeft = -1;` |
|  94979495 | 1820 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  82327879 | 1821 | `		 if( apNode[iCur] == 0 ){` |
|  50171447 | 1822 | `			 continue;` |
|         - | 1823 | `		 }` |
|  32156437 | 1824 | `		 pNode = apNode[iCur];` |
|  32156437 | 1825 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    150753 | 1826 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|    130925 | 1827 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1828 | `					 /* Link the node to the tree */` |
|    138859 | 1829 | `					 pNode->pLeft = apNode[iLeft];` |
|    138859 | 1830 | `					 apNode[iLeft] = 0;` |
|     69427 | 1831 | `			 }` |
|    198340 | 1832 | `		  }` |
|  32279403 | 1833 | `		 iLeft = iCur;` |
|  16201187 | 1834 | `	  }` |
|  12651621 | 1835 | `	 iLeft = -1;` |
|  94979495 | 1836 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  82327879 | 1837 | `		 if( apNode[iCur] == 0 ){` |
|  50310301 | 1838 | `			 continue;` |
|         - | 1839 | `		 }` |
|  32017583 | 1840 | `		 pNode = apNode[iCur];` |
|  32017583 | 1841 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     11894 | 1842 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     11899 | 1843 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1844 | `					 /* Syntax error */` |
|       ! 0 | 1845 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1846 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1847 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1848 | `					 }` |
|       ! 0 | 1849 | `					 return rc;` |
|         - | 1850 | `			 }` |
|         - | 1851 | `			 /* Link the node to the tree */` |
|     11899 | 1852 | `			 pNode->pLeft = apNode[iLeft];` |
|     11899 | 1853 | `			 apNode[iLeft] = 0;` |
|         - | 1854 | `			 /* Mark as pre-increment/decrement node */` |
|     11899 | 1855 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      5947 | 1856 | `		  }` |
|  32017583 | 1857 | `		 iLeft = iCur;` |
|  16008794 | 1858 | `	 }` |
|         - | 1859 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|  12651621 | 1860 | `	  iLeft = 0;` |
|  94979489 | 1861 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  82327875 | 1862 | `		  if( apNode[iCur] ){` |
|  32005685 | 1863 | `			  pNode = apNode[iCur];` |
|  32005685 | 1864 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    859919 | 1865 | `				  if( iLeft > 0 ){` |
|         - | 1866 | `					  /* Link the node to the tree */` |
|    859917 | 1867 | `					  pNode->pLeft = apNode[iLeft];` |
|    859917 | 1868 | `					  apNode[iLeft] = 0;` |
|    859917 | 1869 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|     55449 | 1870 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1871 | `							   /* Syntax error */` |
|       ! 0 | 1872 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|       ! 0 | 1873 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1874 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1875 | `							  }` |
|       ! 0 | 1876 | `							  return rc;` |
|         - | 1877 | `						  }` |
|     27722 | 1878 | `					  }` |
|    429961 | 1879 | `				  }else{` |
|         - | 1880 | `					  /* Syntax error */` |
|         3 | 1881 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|         3 | 1882 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1883 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1884 | `					  }` |
|         3 | 1885 | `					  return rc;` |
|         - | 1886 | `				  }` |
|    429956 | 1887 | `			  }` |
|         - | 1888 | `			  /* Save terminal position */` |
|  32005683 | 1889 | `			  iLeft = iCur;` |
|  16002839 | 1890 | `		  }` |
|  41163939 | 1891 | `	  }` |
|         - | 1892 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1893 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1894 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1895 | `	  * yielding a right-leaning tree. */` |
|  94979487 | 1896 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  82327873 | 1897 | `		 if( apNode[iCur] == 0 ){` |
|  51182219 | 1898 | `			 continue;` |
|         - | 1899 | `		 }` |
|  31145659 | 1900 | `		 pNode = apNode[iCur];` |
|  31145659 | 1901 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 1902 | `			 sxi32 iL, iR;` |
|         - | 1903 | `			 /* Find the right operand */` |
|       113 | 1904 | `			 iR = -1;` |
|         - | 1905 | `			 {` |
|         - | 1906 | `				 sxi32 j;` |
|       125 | 1907 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       125 | 1908 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 1909 | `				 }` |
|         - | 1910 | `			 }` |
|         - | 1911 | `			 /* Find the left operand */` |
|       113 | 1912 | `			 iL = -1;` |
|         - | 1913 | `			 {` |
|         - | 1914 | `				 sxi32 j;` |
|       181 | 1915 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       181 | 1916 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 1917 | `				 }` |
|         - | 1918 | `			 }` |
|       113 | 1919 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 1920 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1921 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 1922 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1923 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1924 | `				 }` |
|       ! 0 | 1925 | `				 return rc;` |
|         - | 1926 | `			 }` |
|       113 | 1927 | `			 pNode->pLeft  = apNode[iL];` |
|       113 | 1928 | `			 pNode->pRight = apNode[iR];` |
|       113 | 1929 | `			 apNode[iL] = 0;` |
|       113 | 1930 | `			 apNode[iR] = 0;` |
|         - | 1931 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 1932 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 1933 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 1934 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 1935 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 1936 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 1937 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 1938 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 1939 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 1940 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 1941 | `			  * operands are respected. */` |
|       112 | 1942 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        74 | 1943 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 1944 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 1945 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 1946 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 1947 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 1948 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 1949 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 1950 | `				 while( pTail->pLeft` |
|        34 | 1951 | `					 && pTail->pLeft->pOp` |
|        23 | 1952 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 1953 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 1954 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 1955 | `					 pTail = pTail->pLeft;` |
|         1 | 1956 | `				 }` |
|         - | 1957 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 1958 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 1959 | `				 pTail->pLeft = pNode;` |
|        27 | 1960 | `				 apNode[iCur] = pHead;` |
|        13 | 1961 | `			 }` |
|        56 | 1962 | `		 }` |
|  15572832 | 1963 | `	 }` |
|         - | 1964 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
| 139167673 | 1965 | `	 for( i = 7 ; i < 17 ; i++ ){` |
| 126516069 | 1966 | `		 iLeft = -1;` |
| 949794455 | 1967 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 823278401 | 1968 | `			 if( apNode[iCur] == 0 ){` |
| 565463227 | 1969 | `				 continue;` |
|         - | 1970 | `			 }` |
| 257815179 | 1971 | `			 pNode = apNode[iCur];` |
| 257815179 | 1972 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1973 | `				 /* Get the right node */` |
|   4371025 | 1974 | `				 iRight = iCur + 1;` |
|   6806701 | 1975 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   2435681 | 1976 | `					 iRight++;` |
|         5 | 1977 | `				 }` |
|   4371025 | 1978 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1979 | `					 /* Syntax error */` |
|        11 | 1980 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        11 | 1981 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1982 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1983 | `					 }` |
|        11 | 1984 | `					 return rc;` |
|         - | 1985 | `				 }` |
|   4371017 | 1986 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1987 | `					 sxi32  iTmp;` |
|         - | 1988 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1989 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 1990 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 1991 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 1992 | `					  * is swapped below. */` |
|        65 | 1993 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 1994 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 1995 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 1996 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1997 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1998 | `						 }` |
|         3 | 1999 | `						 return rc;` |
|         - | 2000 | `					 }` |
|        62 | 2001 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|         - | 2002 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 2003 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 2004 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2005 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2006 | `						 }` |
|       ! 0 | 2007 | `						 return rc;` |
|         - | 2008 | `					 }` |
|        62 | 2009 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|        44 | 2010 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 2011 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 2012 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 2013 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2014 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 2015 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2016 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 2017 | `									 }` |
|       ! 0 | 2018 | `									 return rc;` |
|         - | 2019 | `							 }` |
|       ! 0 | 2020 | `						 }` |
|        21 | 2021 | `					 }` |
|         - | 2022 | `					 /* Swap operands */` |
|        62 | 2023 | `					 iTmp = iRight;` |
|        62 | 2024 | `					 iRight = iLeft;` |
|        62 | 2025 | `					 iLeft = iTmp;` |
|        30 | 2026 | `				 }` |
|         - | 2027 | `				 /* Link the node to the tree */` |
|   4371015 | 2028 | `				 pNode->pLeft = apNode[iLeft];` |
|   4371015 | 2029 | `				 pNode->pRight = apNode[iRight];` |
|   4371015 | 2030 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   2185505 | 2031 | `			 }` |
| 257815169 | 2032 | `			 iLeft = iCur;` |
| 128907587 | 2033 | `		 }` |
|  63258032 | 2034 | `	 }` |
|         - | 2035 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2036 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2037 | `	  * we are dealing with a single operator.` |
|         - | 2038 | `	  */` |
|  12651609 | 2039 | `	  iLeft = -1;` |
|  91885063 | 2040 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79616351 | 2041 | `		  if( apNode[iCur] == 0 ){` |
|  58357535 | 2042 | `			  continue;` |
|         - | 2043 | `		  }` |
|  21258821 | 2044 | `		  pNode = apNode[iCur];` |
|  21258821 | 2045 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    382897 | 2046 | `			  sxi32 iNest = 1;` |
|    382897 | 2047 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2048 | `				  /* Missing condition */` |
|         3 | 2049 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|         3 | 2050 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2051 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2052 | `				  }` |
|         3 | 2053 | `				  return rc;` |
|         - | 2054 | `			  }` |
|         - | 2055 | `			  /* Get the right node */` |
|    382895 | 2056 | `			  iRight = iCur + 1;` |
|   1501537 | 2057 | `			  while( iRight < nToken  ){` |
|   1501537 | 2058 | `				  if( apNode[iRight] ){` |
|    761763 | 2059 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2060 | `						  /* Increment nesting level */` |
|       ! 0 | 2061 | `						  ++iNest;` |
|    761763 | 2062 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2063 | `						  /* Decrement nesting level */` |
|    382895 | 2064 | `						  --iNest;` |
|    382895 | 2065 | `						  if( iNest <= 0 ){` |
|    382895 | 2066 | `							  break;` |
|         - | 2067 | `						  }` |
|       ! 0 | 2068 | `					  }` |
|    189434 | 2069 | `				  }` |
|   1118647 | 2070 | `				  iRight++;` |
|         5 | 2071 | `			  }` |
|    382895 | 2072 | `			  if( iRight > iCur + 1 ){` |
|         - | 2073 | `				  /* Recurse and process the then expression */` |
|    378873 | 2074 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    378873 | 2075 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2076 | `					  return rc;` |
|         - | 2077 | `				  }` |
|         - | 2078 | `				  /* Link the node to the tree */` |
|    378873 | 2079 | `				  pNode->pLeft = apNode[iCur + 1];` |
|    189434 | 2080 | `			  }else{` |
|         - | 2081 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2082 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2083 | `			  }` |
|    382895 | 2084 | `			  apNode[iCur + 1] = 0;` |
|    382895 | 2085 | `			  if( iRight + 1 < nToken ){` |
|         - | 2086 | `				  /* Recurse and process the else expression */` |
|    382895 | 2087 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    382895 | 2088 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2089 | `					  return rc;` |
|         - | 2090 | `				  }` |
|         - | 2091 | `				  /* Link the node to the tree */` |
|    382895 | 2092 | `				  pNode->pRight = apNode[iRight + 1];` |
|    382895 | 2093 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|    191450 | 2094 | `			  }else{` |
|       ! 0 | 2095 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2096 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2097 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2098 | `				 }` |
|       ! 0 | 2099 | `				 return rc;` |
|         - | 2100 | `			  }` |
|         - | 2101 | `			  /* Point to the condition */` |
|    382895 | 2102 | `			  pNode->pCond  = apNode[iLeft];` |
|    382895 | 2103 | `			  apNode[iLeft] = 0;` |
|    382895 | 2104 | `			  break;` |
|         - | 2105 | `		  }` |
|  20875929 | 2106 | `		  iLeft = iCur;` |
|  10437967 | 2107 | `	  }` |
|         - | 2108 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2109 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2110 | `	  * so there is no need for a precedence loop here.` |
|         - | 2111 | `	  */` |
|  12651607 | 2112 | `	 iRight = -1;` |
|  94979291 | 2113 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  82327743 | 2114 | `		 if( apNode[iCur] == 0 ){` |
|  65563943 | 2115 | `			 continue;` |
|         - | 2116 | `		 }` |
|  16763805 | 2117 | `		 pNode = apNode[iCur];` |
|  16763805 | 2118 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2119 | `			 /* Get the left node */` |
|   4112125 | 2120 | `			 iLeft = iCur - 1;` |
|   5636437 | 2121 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   1524317 | 2122 | `				 iLeft--;` |
|         5 | 2123 | `			 }` |
|   4112125 | 2124 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2125 | `				 /* Syntax error */` |
|        44 | 2126 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2127 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2128 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2129 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2130 | `				 }else{` |
|        40 | 2131 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|         - | 2132 | `				 }` |
|        44 | 2133 | `				 if( rc != SXERR_ABORT ){` |
|        42 | 2134 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2135 | `				 }` |
|        44 | 2136 | `				 return rc;` |
|         - | 2137 | `			 }` |
|         - | 2138 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2139 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2140 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2141 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2142 | `			  * a write. */` |
|   4112083 | 2143 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2144 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2145 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2146 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2147 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2148 | `				 }` |
|        11 | 2149 | `				 return rc;` |
|         - | 2150 | `			 }` |
|   4112075 | 2151 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       123 | 2152 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        88 | 2153 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2154 | `					 /* Left operand must be a modifiable l-value */` |
|         6 | 2155 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2156 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2157 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2158 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2159 | `					 }else{` |
|         4 | 2160 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2161 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2162 | `					 }` |
|         6 | 2163 | `					 if( rc != SXERR_ABORT ){` |
|         6 | 2164 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2165 | `					 }` |
|         6 | 2166 | `					 return rc;` |
|         - | 2167 | `				 }` |
|        43 | 2168 | `			 }` |
|         - | 2169 | `			 /* Link the node to the tree (Reverse) */` |
|   4112071 | 2170 | `			 pNode->pLeft = apNode[iRight];` |
|   4112071 | 2171 | `			 pNode->pRight = apNode[iLeft];` |
|   4112071 | 2172 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   2056033 | 2173 | `		 }` |
|  16763751 | 2174 | `		 iRight = iCur;` |
|   8381878 | 2175 | `	 }` |
|         - | 2176 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  63257745 | 2177 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  50606197 | 2178 | `		 iLeft = -1;` |
| 379916877 | 2179 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 329310685 | 2180 | `			 if( apNode[iCur] == 0 ){` |
| 278704241 | 2181 | `				 continue;` |
|         - | 2182 | `			 }` |
|  50606449 | 2183 | `			 pNode = apNode[iCur];` |
|  50606449 | 2184 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2185 | `				 /* Get the right node */` |
|        51 | 2186 | `				 iRight = iCur + 1;` |
|        63 | 2187 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        13 | 2188 | `					 iRight++;` |
|         1 | 2189 | `				 }` |
|        51 | 2190 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2191 | `					 /* Syntax error */` |
|       ! 0 | 2192 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 2193 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2194 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2195 | `					 }` |
|       ! 0 | 2196 | `					 return rc;` |
|         - | 2197 | `				 }` |
|         - | 2198 | `				 /* Link the node to the tree */` |
|        51 | 2199 | `				 pNode->pLeft = apNode[iLeft];` |
|        51 | 2200 | `				 pNode->pRight = apNode[iRight];` |
|        51 | 2201 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        24 | 2202 | `			 }` |
|  50606449 | 2203 | `			 iLeft = iCur;` |
|  25303227 | 2204 | `		 }` |
|  25303101 | 2205 | `	 }` |
|         - | 2206 | `	 /* Point to the root of the expression tree */` |
|  82327647 | 2207 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  69676117 | 2208 | `		 if( apNode[iCur] ){` |
|  12151947 | 2209 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        22 | 2210 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|        22 | 2211 | `				  if( rc != SXERR_ABORT ){` |
|        22 | 2212 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2213 | `				  }` |
|        22 | 2214 | `				  return rc;` |
|         - | 2215 | `			 }` |
|  12151929 | 2216 | `			 apNode[0] = apNode[iCur];` |
|  12151929 | 2217 | `			 apNode[iCur] = 0;` |
|   6075962 | 2218 | `		 }` |
|  34838052 | 2219 | `	 }` |
|  12651535 | 2220 | `	 return SXRET_OK;` |
|  11232728 | 2221 | ` }` |
|         - | 2222 | ` /*` |
|         - | 2223 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2224 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2225 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2226 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2227 | `  */` |
|  13078908 | 2228 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2229 | `{` |
|         - | 2230 | `	ph7_expr_node **apNode;` |
|         - | 2231 | `	ph7_expr_node *pNode;` |
|         - | 2232 | `	sxi32 rc;` |
|         - | 2233 | `	/* Reset node container */` |
|  13078913 | 2234 | `	SySetReset(pExprNode);` |
|  13078913 | 2235 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2236 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2237 | `	{` |
|  13078913 | 2238 | `		int iLastWasTerm = 0;` |
|  13078913 | 2239 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  83644685 | 2240 | `		while( pGen->pIn < pGen->pEnd ){` |
|  70565811 | 2241 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  70565811 | 2242 | `			if( rc != SXRET_OK ){` |
|        38 | 2243 | `				return rc;` |
|         - | 2244 | `			}` |
|         - | 2245 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  70565777 | 2246 | `			if( pNode->xCode ){` |
|         - | 2247 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  35671111 | 2248 | `				iLastWasTerm = 1;` |
|  52730224 | 2249 | `			}else if( pNode->pOp ){` |
|         - | 2250 | `				/* Operator node */` |
|  20009205 | 2251 | `				iLastWasTerm = 0;` |
|  10004605 | 2252 | `			}else{` |
|         - | 2253 | `				/* Delimiter: ')' and ']' end terms */` |
|  14885471 | 2254 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2255 | `			}` |
|         - | 2256 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2257 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2258 | `			 * node kind, so this single test covers all branches. */` |
|  70565777 | 2259 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2260 | `			/* Save the extracted node */` |
|  70565777 | 2261 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2262 | `		}` |
|         - | 2263 | `	}` |
|  13078879 | 2264 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2265 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2266 | `		*ppRoot = 0;` |
|       ! 0 | 2267 | `		return SXRET_OK;` |
|         - | 2268 | `	}` |
|  13078879 | 2269 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2270 | `	/* Make sure we are dealing with valid nodes */` |
|  13078879 | 2271 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  13078879 | 2272 | `	if( rc != SXRET_OK ){` |
|         - | 2273 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2274 | `		 * cleanup the mess left behind.` |
|         - | 2275 | `		 */` |
|        54 | 2276 | `		*ppRoot = 0;` |
|        54 | 2277 | `		return rc;` |
|         - | 2278 | `	}` |
|         - | 2279 | `	/* Build the tree */` |
|  13078829 | 2280 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  13078829 | 2281 | `	if( rc != SXRET_OK ){` |
|         - | 2282 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2283 | `		*ppRoot = 0;` |
|       103 | 2284 | `		return rc;` |
|         - | 2285 | `	}` |
|         - | 2286 | `	/* Point to the root of the tree */` |
|  13078731 | 2287 | `	*ppRoot = apNode[0];` |
|  13078731 | 2288 | `	return SXRET_OK;` |
|   6539459 | 2289 | `}` |
|         - | 2290 |  |
