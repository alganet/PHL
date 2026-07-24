# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1160/1328 lines (87.35%)

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
|  10878456 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  10878461 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 168239680 |  278 | `	for(;;){` |
| 336479365 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 336479365 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  31805453 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  15902729 |  285 | `		}else{` |
| 304673917 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 336479365 |  288 | `		if( rc == 0 ){` |
|  10945093 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  10874003 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|     71095 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|       463 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|     70637 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      4013 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      4013 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      4005 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|     33316 |  307 | `		}` |
| 325600909 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|   5439233 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   3529322 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   3529327 |  320 | `	SyToken *pCur = pIn;` |
|   3529327 |  321 | `	sxi32 iNest = 1;` |
|  35427251 |  322 | `	for(;;){` |
|  70854507 |  323 | `		if( pCur >= pEnd ){` |
|       517 |  324 | `			break;` |
|         - |  325 | `		}` |
|  70853995 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   2971257 |  328 | `			iNest++;` |
|  69368369 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|   6500067 |  331 | `			iNest--;` |
|   6500067 |  332 | `			if( iNest <= 0 ){` |
|   3528815 |  333 | `				break;` |
|         - |  334 | `			}` |
|   1485626 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
|  67325185 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   3529327 |  340 | `	*ppEnd = pCur;` |
|   3529327 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    171358 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    171358 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    171260 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       167 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    171201 |  358 | `	if( bCheckFunc ){` |
|      8136 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      8129 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      8111 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        49 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|      4046 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    171157 |  366 | `	return FALSE;` |
|     85684 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|   7041284 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|   7041289 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|   7041289 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  43700169 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  36658919 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|      1655 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  36657269 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   3269783 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    136582 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   3093719 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   3093719 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   3093719 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   3093719 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   1546857 |  401 | `					}` |
|   1546857 |  402 | `			}` |
|   3269783 |  403 | `			iParen++;` |
|  35022380 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   3269783 |  405 | `			if( iParen <= 0 ){` |
|        16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        16 |  407 | `				if( rc != SXERR_ABORT ){` |
|        16 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        16 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   3269771 |  412 | `			iParen--;` |
|  31752596 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   1530697 |  414 | `			iSquare++;` |
|  29352367 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   1530711 |  416 | `			if( iSquare <= 0 ){` |
|         9 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|         9 |  418 | `				if( rc != SXERR_ABORT ){` |
|         9 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         9 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   1530705 |  423 | `			iSquare--;` |
|  27821665 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
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
|  27056306 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|        25 |  472 | `			if( iBraces <= 0 ){` |
|        16 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|        16 |  474 | `				if( rc != SXERR_ABORT ){` |
|        16 |  475 | `					rc = SXERR_SYNTAX;` |
|         6 |  476 | `				}` |
|        16 |  477 | `				return rc;` |
|         - |  478 | `			}` |
|        10 |  479 | `			iBraces--;` |
|  27056281 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    200615 |  481 | `			if( iQuesty > 0 ){` |
|    200355 |  482 | `				iQuesty--;` |
|    100440 |  483 | `			}else if( iParen <= 0 ){` |
|         - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  488 | `				if( rc != SXERR_ABORT ){` |
|         6 |  489 | `					rc = SXERR_SYNTAX;` |
|         2 |  490 | `				}` |
|         6 |  491 | `				return rc;` |
|         5 |  492 | `			}` |
|  26955970 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   8357443 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   8357443 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    200357 |  496 | `				iQuesty++;` |
|   8257267 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      4387 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|      2191 |  516 | `			}` |
|   4178719 |  517 | `		}` |
|  18328620 |  518 | `	}` |
|   7041255 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        20 |  520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|        20 |  521 | `		if( rc != SXERR_ABORT ){` |
|        20 |  522 | `			rc = SXERR_SYNTAX;` |
|         8 |  523 | `		}` |
|        20 |  524 | `		return rc;` |
|         - |  525 | `	}` |
|   7041239 |  526 | `	return SXRET_OK;` |
|   3520647 |  527 | `}` |
|         - |  528 | `/*` |
|         - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  531 | ` */` |
|   5457096 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  533 | `{` |
|   5457101 |  534 | `	SyToken *pIn = *ppCur;` |
|         - |  535 | `	/* Jump the first literal seen */` |
|   5457101 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   5453185 |  537 | `		pIn++;` |
|   2726590 |  538 | `	}` |
|   2730533 |  539 | `	for(;;){` |
|   5461071 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      3975 |  541 | `			pIn++;` |
|      3975 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3973 |  543 | `				pIn++;` |
|      1984 |  544 | `			}` |
|      1990 |  545 | `		}else{` |
|   2728553 |  546 | `			break;` |
|         - |  547 | `		}` |
|         5 |  548 | `	}` |
|         - |  549 | `	/* Synchronize pointers */` |
|   5457101 |  550 | `	*ppCur = pIn;` |
|   5457101 |  551 | `}` |
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
|       656 |  596 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  597 | `{` |
|       661 |  598 | `	SyToken *pIn = *ppIn;` |
|       661 |  599 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
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
|       661 |  637 | `	*ppIn = pIn;` |
|       661 |  638 | `}` |
|       382 |  639 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  640 | `{` |
|       387 |  641 | `	SyToken *pIn = *ppCur;` |
|         - |  642 | `	sxu32 nLine;` |
|         - |  643 | `	sxi32 rc;` |
|         - |  644 | `	/* Jump the 'function' keyword */` |
|       387 |  645 | `	nLine = pIn->nLine;` |
|       387 |  646 | `	pIn++;` |
|       387 |  647 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  648 | `		pIn++;` |
|         1 |  649 | `	}` |
|       387 |  650 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  651 | `		/* Syntax error */` |
|         6 |  652 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|         6 |  653 | `		if( rc != SXERR_ABORT ){` |
|         6 |  654 | `			rc = SXERR_SYNTAX;` |
|         2 |  655 | `		}` |
|         6 |  656 | `		goto Synchronize;` |
|         - |  657 | `	}` |
|       383 |  658 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       383 |  659 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       383 |  660 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  661 | `		/* Syntax error */` |
|         5 |  662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         5 |  663 | `		if( rc != SXERR_ABORT ){` |
|         5 |  664 | `			rc = SXERR_SYNTAX;` |
|         2 |  665 | `		}` |
|         5 |  666 | `		goto Synchronize;` |
|         - |  667 | `	}` |
|       379 |  668 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  669 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       379 |  670 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       379 |  671 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|        47 |  672 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  673 | `		/* Check if we are dealing with a closure */` |
|        47 |  674 | `		if( nKey == PH7_TKWRD_USE ){` |
|        39 |  675 | `			pIn++; /* Jump the 'use' keyword */` |
|        39 |  676 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  677 | `				/* Syntax error */` |
|         5 |  678 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         5 |  679 | `				if( rc != SXERR_ABORT ){` |
|         5 |  680 | `					rc = SXERR_SYNTAX;` |
|         2 |  681 | `				}` |
|         5 |  682 | `				goto Synchronize;` |
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
|       363 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       363 |  711 | `		pIn++; /* Jump the leading curly '{' */` |
|       363 |  712 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       363 |  713 | `		if( pIn < pEnd ){` |
|       363 |  714 | `			pIn++;` |
|       179 |  715 | `		}` |
|       184 |  716 | `	}else{` |
|         - |  717 | `		/* Syntax error */` |
|       ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|       ! 0 |  719 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  720 | `			return SXERR_ABORT;` |
|         - |  721 | `		}` |
|         - |  722 | `	}` |
|       363 |  723 | `	rc = SXRET_OK;` |
|       191 |  724 | `Synchronize:` |
|         - |  725 | `	/* Synchronize pointers */` |
|       387 |  726 | `	*ppCur = pIn;` |
|       387 |  727 | `	return rc;` |
|       196 |  728 | `}` |
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
|       256 |  782 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  783 | `{` |
|       261 |  784 | `	SyToken *pIn = *ppCur;` |
|         - |  785 | `	sxu32 nLine;` |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	int iNest;` |
|       261 |  788 | `	nLine = pIn->nLine;` |
|         - |  789 | `	/* Optional 'static' prefix */` |
|       256 |  790 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|       261 |  791 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         3 |  792 | `		pIn++;` |
|         1 |  793 | `	}` |
|         - |  794 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|       256 |  795 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       261 |  796 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  797 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  798 | `		goto Synchronize;` |
|         - |  799 | `	}` |
|       261 |  800 | `	pIn++; /* Jump 'fn' */` |
|       128 |  801 | `	SXUNUSED(nLine);` |
|       128 |  802 | `	SXUNUSED(pGen);` |
|         - |  803 | `	/* Optional '&' for return-by-reference */` |
|       261 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  805 | `		pIn++;` |
|       ! 0 |  806 | `	}` |
|         - |  807 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  808 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  809 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  810 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|       261 |  811 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       259 |  812 | `		pIn++; /* '(' */` |
|       259 |  813 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       259 |  814 | `		if( pIn < pEnd ){` |
|       257 |  815 | `			pIn++; /* ')' */` |
|       126 |  816 | `		}` |
|       127 |  817 | `	}` |
|         - |  818 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|       261 |  819 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  820 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|       261 |  821 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       254 |  822 | `		pIn++;` |
|       125 |  823 | `	}` |
|         - |  824 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|       261 |  825 | `	iNest = 0;` |
|      1877 |  826 | `	while( pIn < pEnd ){` |
|      1771 |  827 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  828 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       151 |  829 | `			break;` |
|         - |  830 | `		}` |
|      1621 |  831 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       189 |  832 | `			iNest++;` |
|      1528 |  833 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       189 |  834 | `			iNest--;` |
|        93 |  835 | `		}` |
|      1621 |  836 | `		pIn++;` |
|         5 |  837 | `	}` |
|       261 |  838 | `	rc = SXRET_OK;` |
|       128 |  839 | `Synchronize:` |
|       261 |  840 | `	*ppCur = pIn;` |
|       261 |  841 | `	return rc;` |
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
|  36663056 |  892 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  893 | `{` |
|         - |  894 | `	ph7_expr_node *pNode;` |
|         - |  895 | `	SyToken *pCur;` |
|         - |  896 | `	sxi32 rc;` |
|         - |  897 | `	/* Allocate a new node */` |
|  36663061 |  898 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  36663061 |  899 | `	if( pNode == 0 ){` |
|         - |  900 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  901 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  902 | `		 */` |
|       ! 0 |  903 | `		return SXERR_MEM;` |
|         - |  904 | `	}` |
|         - |  905 | `	/* Zero the structure */` |
|  36663061 |  906 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  36663061 |  907 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  908 | `	/* Point to the head of the token stream */` |
|  36663061 |  909 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  910 | `	/* Start collecting tokens */` |
|  36663061 |  911 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4073 |  912 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
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
|      3997 |  927 | `		pCur++;` |
|      3997 |  928 | `		pGen->pIn = pCur;` |
|      3997 |  929 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      3997 |  930 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      3997 |  931 | `		if( rc == SXRET_OK && *ppNode ){` |
|      3997 |  932 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      1996 |  933 | `		}` |
|      3997 |  934 | `		return rc;` |
|         - |  935 | `	}` |
|  36658993 |  936 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  937 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  938 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  939 | `		 */` |
|      1657 |  940 | `		pCur++; /* Skip the opening '[' */` |
|      1657 |  941 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|      1657 |  942 | `		if( pCur < pGen->pEnd ){` |
|      1657 |  943 | `			pCur++; /* Skip past the closing ']' */` |
|       831 |  944 | `		}else{` |
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
|      1807 |  956 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       304 |  957 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       304 |  958 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        54 |  959 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  960 | `			}else{` |
|       251 |  961 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  962 | `			}` |
|       154 |  963 | `		}else{` |
|      1357 |  964 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  965 | `		}` |
|  36658167 |  966 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|  36657329 |  977 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   9888186 |  978 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   4944128 |  979 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|  36657314 | 1005 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1006 | `		/* Point to the instance that describe this operator */` |
|   9888169 | 1007 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1008 | `		/* Advance the stream cursor */` |
|   9888169 | 1009 | `		pCur++;` |
|  31713221 | 1010 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1011 | `		/* Isolate variable */` |
|  17092157 | 1012 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   8546087 | 1013 | `			pCur++; /* Variable variable */` |
|         5 | 1014 | `		}` |
|   8546075 | 1015 | `		if( pCur < pGen->pEnd ){` |
|   8546075 | 1016 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1017 | `				/* Variable name */` |
|   8546045 | 1018 | `				pCur++;` |
|   4273054 | 1019 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        26 | 1020 | `				pCur++;` |
|         - | 1021 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        26 | 1022 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        26 | 1023 | `				if( pCur < pGen->pEnd ){` |
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
|   4273033 | 1034 | `		}` |
|   8546071 | 1035 | `		pNode->xCode = PH7_CompileVariable;` |
|  22496102 | 1036 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    568651 | 1037 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    568651 | 1038 | `		 if( bAfterMemberOp ){` |
|         - | 1039 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1040 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1041 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1042 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    120109 | 1043 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    120109 | 1044 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    508599 | 1045 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1046 | `			 /* List/Array node */` |
|    284251 | 1047 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1048 | `				 /* Assume a literal */` |
|       ! 0 | 1049 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1050 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1051 | `			 }else{` |
|    284251 | 1052 | `				 pCur += 2;` |
|         - | 1053 | `				 /* Collect array/list tokens */` |
|    284251 | 1054 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    284251 | 1055 | `				 if( pCur < pGen->pEnd ){` |
|    284249 | 1056 | `					 pCur++;` |
|    142127 | 1057 | `				 }else{` |
|         - | 1058 | `					 /* Syntax error */` |
|         4 | 1059 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1060 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1061 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1062 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1063 | `					 }` |
|         3 | 1064 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1065 | `					 return rc;` |
|         - | 1066 | `				 }` |
|    284249 | 1067 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    284249 | 1068 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    306422 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1082 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       379 | 1083 | `			 pCur++; /* Skip 'yield' keyword */` |
|       379 | 1084 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1085 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1086 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       379 | 1087 | `			 pNode->xCode = PH7_CompileYield;` |
|    164114 | 1088 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|         - | 1089 | `			 /* Annonymous function */` |
|       387 | 1090 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1091 | `				 /* Assume a literal */` |
|       ! 0 | 1092 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1093 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1094 | `			 }else{` |
|         - | 1095 | `				 /* Assemble annonymous functions body */` |
|       387 | 1096 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       387 | 1097 | `				 if( rc != SXRET_OK ){` |
|        28 | 1098 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1099 | `					 return rc;` |
|         - | 1100 | `				 }` |
|       363 | 1101 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1102 | `			  }` |
|    163724 | 1103 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
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
|    163531 | 1118 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    163393 | 1119 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        30 | 1120 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        15 | 1121 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1122 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       261 | 1123 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       261 | 1124 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1125 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1126 | `				 return rc;` |
|         - | 1127 | `			 }` |
|       261 | 1128 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    163391 | 1129 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1130 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        75 | 1131 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        75 | 1132 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1133 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1134 | `				 return rc;` |
|         - | 1135 | `			 }` |
|        75 | 1136 | `			 pNode->xCode = PH7_CompileMatch;` |
|    163228 | 1137 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1138 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1139 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1140 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1141 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1142 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1143 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1144 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1145 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    163175 | 1146 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1147 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        93 | 1148 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        93 | 1149 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        49 | 1150 | `		 }else{` |
|         - | 1151 | `			 /* Assume a literal */` |
|    163069 | 1152 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    163069 | 1153 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1154 | `		 }` |
|  17938732 | 1155 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1156 | `		 /* Constants,function name,namespace path,class name... */` |
|   5173917 | 1157 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   5173917 | 1158 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   2586961 | 1159 | `	 }else{` |
|  12480511 | 1160 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1161 | `			 /* Point to the code generator routine */` |
|   4209579 | 1162 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   4209579 | 1163 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1164 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|         3 | 1165 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1166 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1167 | `				 }` |
|         3 | 1168 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1169 | `				 return rc;` |
|         - | 1170 | `			 }` |
|   2104786 | 1171 | `		 }` |
|         - | 1172 | `		/* Advance the stream cursor */` |
|  12480509 | 1173 | `		pCur++;` |
|         - | 1174 | `	 }` |
|         - | 1175 | `	/* Point to the end of the token stream */` |
|  36658959 | 1176 | `	pNode->pEnd = pCur;` |
|         - | 1177 | `	/* Save the node for later processing */` |
|  36658959 | 1178 | `	*ppNode = pNode;` |
|         - | 1179 | `	/* Synchronize cursors */` |
|  36658959 | 1180 | `	pGen->pIn = pCur;` |
|  36658959 | 1181 | `	return SXRET_OK;` |
|  18331533 | 1182 | `}` |
|         - | 1183 | `/*` |
|         - | 1184 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1185 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1186 | ` * level is zero.` |
|         - | 1187 | ` */` |
|    711256 | 1188 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1189 | `{` |
|    711261 | 1190 | `	SyToken *pCur = pStart;` |
|    711261 | 1191 | `	sxi32 iNest = 0;` |
|    711261 | 1192 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1193 | `		/* Last expression */` |
|    305717 | 1194 | `		return SXERR_EOF;` |
|         - | 1195 | `	}` |
|   1658767 | 1196 | `	while( pCur < pEnd ){` |
|   1559809 | 1197 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    306591 | 1198 | `			break;` |
|         - | 1199 | `		}` |
|   1253223 | 1200 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     75027 | 1201 | `			iNest++;` |
|   1215712 | 1202 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     75029 | 1203 | `			iNest--;` |
|     37512 | 1204 | `		}` |
|   1253223 | 1205 | `		pCur++;` |
|         5 | 1206 | `	}` |
|    405549 | 1207 | `	*ppNext = pCur;` |
|    405549 | 1208 | `	return SXRET_OK;` |
|    355633 | 1209 | `}` |
|         - | 1210 | `/*` |
|         - | 1211 | ` * Free an expression tree.` |
|         - | 1212 | ` */` |
|  31060528 | 1213 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1214 | `{` |
|  31060533 | 1215 | `	if( pNode->pLeft ){` |
|         - | 1216 | `		/* Release the left tree */` |
|  12207903 | 1217 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   6103949 | 1218 | `	}` |
|  31060533 | 1219 | `	if( pNode->pRight ){` |
|         - | 1220 | `		/* Release the right tree */` |
|   6786069 | 1221 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   3393032 | 1222 | `	}` |
|  31060533 | 1223 | `	if( pNode->pCond ){` |
|         - | 1224 | `		/* Release the conditional tree used by the ternary operator */` |
|    200353 | 1225 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|    100174 | 1226 | `	}` |
|  31060533 | 1227 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1228 | `		ph7_expr_node **apArg;` |
|         - | 1229 | `		sxu32 n;` |
|         - | 1230 | `		/* Release node arguments */` |
|   3698203 | 1231 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   8170137 | 1232 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   4471939 | 1233 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   2235972 | 1234 | `		}` |
|   3698203 | 1235 | `		SySetRelease(&pNode->aNodeArgs);` |
|   1849099 | 1236 | `	}` |
|         - | 1237 | `	/* Finally,release this node */` |
|  31060533 | 1238 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  31060533 | 1239 | `}` |
|         - | 1240 | `/*` |
|         - | 1241 | ` * Free an expression tree.` |
|         - | 1242 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1243 | ` */` |
|   7041318 | 1244 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1245 | `{` |
|         - | 1246 | `	ph7_expr_node **apNode;` |
|         - | 1247 | `	sxu32 n;` |
|   7041323 | 1248 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  43700353 | 1249 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  36659035 | 1250 | `		if( apNode[n] ){` |
|   7041657 | 1251 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   3520826 | 1252 | `		}` |
|  18329520 | 1253 | `	}` |
|   7041323 | 1254 | `	return SXRET_OK;` |
|         5 | 1255 | `}` |
|         - | 1256 | `/*` |
|         - | 1257 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1258 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1259 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1260 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1261 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1262 | ` */` |
|   8160350 | 1263 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1264 | `{` |
|   8160355 | 1265 | `	if( pNode == 0 ){` |
|   5212635 | 1266 | `		return 0;` |
|         - | 1267 | `	}` |
|   2947725 | 1268 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1269 | `		return 1;` |
|         - | 1270 | `	}` |
|   2947713 | 1271 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1272 | `		return 1;` |
|         - | 1273 | `	}` |
|   2947709 | 1274 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1275 | `		return 1;` |
|         - | 1276 | `	}` |
|   2947709 | 1277 | `	return 0;` |
|   4080180 | 1278 | `}` |
|         - | 1279 | `/*` |
|         - | 1280 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1281 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1282 | ` */` |
|   2258138 | 1283 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1284 | `{` |
|         - | 1285 | `	sxi32 iExprOp;` |
|   2258143 | 1286 | `	if( pNode->pOp == 0 ){` |
|   1823805 | 1287 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1288 | `	}` |
|    434343 | 1289 | `	iExprOp = pNode->pOp->iOp;` |
|    434343 | 1290 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    248127 | 1291 | `			return TRUE;` |
|         - | 1292 | `	}` |
|    186221 | 1293 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    186215 | 1294 | `		if( pNode->pLeft->pOp ) {` |
|        70 | 1295 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|        31 | 1296 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1297 | `				return FALSE;` |
|         5 | 1298 | `			}` |
|    186180 | 1299 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1300 | `			return FALSE;` |
|         - | 1301 | `		}` |
|    186215 | 1302 | `		return TRUE;` |
|         - | 1303 | `	}` |
|         8 | 1304 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1305 | `		return TRUE;` |
|         - | 1306 | `	}` |
|         - | 1307 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1308 | `	return FALSE;` |
|   1129074 | 1309 | `}` |
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
|   2291404 | 1321 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1322 | `{` |
|         - | 1323 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	/* Process function arguments from left to right */` |
|   2291409 | 1326 | `	iCur = 0;` |
|   2678260 | 1327 | `	for(;;){` |
|   5356525 | 1328 | `		if( iCur >= nToken ){` |
|         - | 1329 | `			/* No more arguments to process */` |
|   2291383 | 1330 | `			break;` |
|         - | 1331 | `		}` |
|   3065147 | 1332 | `		iNode = iCur;` |
|   3065147 | 1333 | `		iNest = 0;` |
|   9991441 | 1334 | `		while( iCur < nToken ){` |
|   7700061 | 1335 | `			if( apNode[iCur] ){` |
|   7684461 | 1336 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    386886 | 1337 | `					break;` |
|   6910694 | 1338 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   3729572 | 1339 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    548075 | 1340 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1341 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1342 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1343 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1344 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1345 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1346 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    547695 | 1347 | `					iNest++;` |
|   6636854 | 1348 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    547695 | 1349 | `					iNest--;` |
|    273845 | 1350 | `				}` |
|   3455347 | 1351 | `			}` |
|   6926299 | 1352 | `			iCur++;` |
|         5 | 1353 | `		}` |
|   3065147 | 1354 | `		if( iCur > iNode ){` |
|   3065141 | 1355 | `			SyString sArgName = {0, 0};` |
|         - | 1356 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1357 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1358 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   3065136 | 1359 | `			if( (iCur - iNode) >= 2` |
|   2043034 | 1360 | `				&& apNode[iNode]` |
|   1020922 | 1361 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    534676 | 1362 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     48274 | 1363 | `				&& apNode[iNode+1]` |
|     48113 | 1364 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1365 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|       261 | 1366 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       261 | 1367 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       261 | 1368 | `				apNode[iNode] = 0;` |
|       261 | 1369 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|       261 | 1370 | `				apNode[iNode+1] = 0;` |
|       261 | 1371 | `				iNode += 2;` |
|         - | 1372 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1373 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       261 | 1374 | `				if( iNode >= iCur ){` |
|         4 | 1375 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1376 | `						pOp->pStart->nLine,` |
|         - | 1377 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1378 | `						&sArgName);` |
|         3 | 1379 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1380 | `						rc = SXERR_SYNTAX;` |
|         1 | 1381 | `					}` |
|         3 | 1382 | `					return rc;` |
|         - | 1383 | `				}` |
|       127 | 1384 | `			}` |
|   3065134 | 1385 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1386 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1387 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1388 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1389 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|       ! 0 | 1390 | `					apNode[iNode] = 0;` |
|       ! 0 | 1391 | `			}` |
|         - | 1392 | `			{` |
|         - | 1393 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1394 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1395 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1396 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1397 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1398 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1399 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1400 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1401 | `				 * NULL slots ahead of the flagged subtree. */` |
|   3065139 | 1402 | `				int bSpreadArg = 0;` |
|         - | 1403 | `				sxi32 iScan;` |
|   3065159 | 1404 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   3065159 | 1405 | `					if( apNode[iScan] ){` |
|   3065139 | 1406 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   3065139 | 1407 | `						break;` |
|         - | 1408 | `					}` |
|        11 | 1409 | `				}` |
|   3065139 | 1410 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   3065139 | 1411 | `				if( bSpreadArg && apNode[iNode] ){` |
|      3933 | 1412 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|      1964 | 1413 | `				}` |
|         - | 1414 | `			}` |
|   3065139 | 1415 | `			if( apNode[iNode] ){` |
|   3065139 | 1416 | `				if( sArgName.nByte > 0 ){` |
|       259 | 1417 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       259 | 1418 | `					apNode[iNode]->sArgName = sArgName;` |
|       127 | 1419 | `				}` |
|         - | 1420 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   3065139 | 1421 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   1532572 | 1422 | `			}else{` |
|         - | 1423 | `				/* No expression before comma */` |
|       ! 0 | 1424 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1425 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1426 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1427 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1428 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1429 | `				}` |
|       ! 0 | 1430 | `				return rc;` |
|         - | 1431 | `			}` |
|   1532572 | 1432 | `		}else{` |
|         - | 1433 | `			/* Comma with no preceding argument */` |
|         9 | 1434 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         9 | 1435 | `			if( rc != SXERR_ABORT ){` |
|         9 | 1436 | `				rc = SXERR_SYNTAX;` |
|         3 | 1437 | `			}` |
|         9 | 1438 | `			return rc;` |
|         - | 1439 | `		}` |
|         - | 1440 | `		/* Jump trailing comma */` |
|   3065139 | 1441 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    773761 | 1442 | `			iCur++;` |
|    773761 | 1443 | `			if( iCur >= nToken ){` |
|         - | 1444 | `				/* Trailing comma after last argument */` |
|        19 | 1445 | `				break;` |
|         - | 1446 | `			}` |
|    386869 | 1447 | `		}` |
|         5 | 1448 | `	}` |
|   2291401 | 1449 | `	return SXRET_OK;` |
|   1145707 | 1450 | `}` |
|         - | 1451 | ` /*` |
|         - | 1452 | `  * Create an expression tree from an array of tokens.` |
|         - | 1453 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1454 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1455 | `  */` |
|  12148242 | 1456 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1457 | ` {` |
|         - | 1458 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1459 | `	 ph7_expr_node *pNode;` |
|         - | 1460 | `	 sxi32 iCur;` |
|         - | 1461 | `	 sxi32 rc;` |
|  12148247 | 1462 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1463 | `		 /* TICKET 1433-17: self evaluating node */` |
|   5503179 | 1464 | `		 return SXRET_OK;` |
|         - | 1465 | `	 }` |
|         - | 1466 | `	 /* Process expressions enclosed in parenthesis first */` |
|  47275715 | 1467 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1468 | `		 sxi32 iNest;` |
|         - | 1469 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1470 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1471 | `		  */` |
|  40630649 | 1472 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  40454595 | 1473 | `			 continue;` |
|         - | 1474 | `		 }` |
|    176059 | 1475 | `		 iNest = 1;` |
|    176059 | 1476 | `		 iLeft = iCur;` |
|         - | 1477 | `		 /* Find the closing parenthesis */` |
|    176059 | 1478 | `		 iCur++;` |
|   1898317 | 1479 | `		 while( iCur < nToken ){` |
|   1898317 | 1480 | `			 if( apNode[iCur] ){` |
|   1898317 | 1481 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1482 | `					 /* Decrement nesting level */` |
|    273837 | 1483 | `					 iNest--;` |
|    273837 | 1484 | `					 if( iNest <= 0 ){` |
|    176059 | 1485 | `						 break;` |
|         5 | 1486 | `					 }` |
|   1673374 | 1487 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1488 | `					 /* Increment nesting level */` |
|     97783 | 1489 | `					 iNest++;` |
|     48889 | 1490 | `				 }` |
|    861129 | 1491 | `			 }` |
|   1722263 | 1492 | `			 iCur++;` |
|         5 | 1493 | `		 }` |
|    176059 | 1494 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1495 | `			 sxi32 j;` |
|         - | 1496 | `			 /* Recurse and process this expression */` |
|    176059 | 1497 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    176059 | 1498 | `			 if( rc != SXRET_OK ){` |
|         3 | 1499 | `				 return rc;` |
|         - | 1500 | `			 }` |
|         - | 1501 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1502 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1503 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1504 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1505 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1506 | `			  * group's free below silently drops the unpacking. */` |
|    176057 | 1507 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    176057 | 1508 | `				 if( apNode[j] ){` |
|    176057 | 1509 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    176052 | 1510 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    176057 | 1511 | `					 break;` |
|         - | 1512 | `				 }` |
|       ! 0 | 1513 | `			 }` |
|     88026 | 1514 | `		 }` |
|         - | 1515 | `		 /* Free the left and right nodes */` |
|    176057 | 1516 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    176057 | 1517 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    176057 | 1518 | `		 apNode[iLeft] = 0;` |
|    176057 | 1519 | `		 apNode[iCur] = 0;` |
|     88031 | 1520 | `	 }` |
|         - | 1521 | `	  /* Process expressions enclosed in braces */` |
|  49057229 | 1522 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1523 | `		 sxi32 iNest;` |
|         - | 1524 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1525 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1526 | `		  */` |
|  42528935 | 1527 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  42528927 | 1528 | `			 continue;` |
|         - | 1529 | `		 }` |
|        10 | 1530 | `		 iNest = 1;` |
|        10 | 1531 | `		 iLeft = iCur;` |
|         - | 1532 | `		 /* Find the closing parenthesis */` |
|        10 | 1533 | `		 iCur++;` |
|        16 | 1534 | `		 while( iCur < nToken ){` |
|        16 | 1535 | `			 if( apNode[iCur] ){` |
|        16 | 1536 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1537 | `					 /* Decrement nesting level */` |
|        10 | 1538 | `					 iNest--;` |
|        10 | 1539 | `					 if( iNest <= 0 ){` |
|        10 | 1540 | `						 break;` |
|       ! 0 | 1541 | `					 }` |
|         7 | 1542 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1543 | `					 /* Increment nesting level */` |
|       ! 0 | 1544 | `					 iNest++;` |
|       ! 0 | 1545 | `				 }` |
|         3 | 1546 | `			 }` |
|         7 | 1547 | `			 iCur++;` |
|         1 | 1548 | `		 }` |
|        10 | 1549 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1550 | `			 /* Recurse and process this expression */` |
|         7 | 1551 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|         7 | 1552 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1553 | `				 return rc;` |
|         - | 1554 | `			 }` |
|         3 | 1555 | `		 }` |
|         - | 1556 | `		 /* Free the left and right nodes */` |
|        10 | 1557 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|        10 | 1558 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|        10 | 1559 | `		 apNode[iLeft] = 0;` |
|        10 | 1560 | `		 apNode[iCur] = 0;` |
|         6 | 1561 | `	 }` |
|         - | 1562 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   6528299 | 1563 | `	 iLeft = -1;` |
|  49057207 | 1564 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  42528925 | 1565 | `		 if( apNode[iCur] == 0 ){` |
|  18455679 | 1566 | `			 continue;` |
|         - | 1567 | `		 }` |
|  24073251 | 1568 | `		 pNode = apNode[iCur];` |
|  24073251 | 1569 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   6532677 | 1570 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1571 | `				 /* Collect function arguments */` |
|   3093715 | 1572 | `				 sxi32 iPtr = 0;` |
|   3093715 | 1573 | `				 sxi32 nFuncTok = 0;` |
|  13887483 | 1574 | `				 while( nFuncTok + iCur < nToken ){` |
|  13887483 | 1575 | `					 if( apNode[nFuncTok+iCur] ){` |
|  13871883 | 1576 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   3242525 | 1577 | `							 iPtr++;` |
|  12250623 | 1578 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   3242525 | 1579 | `							 iPtr--;` |
|   3242525 | 1580 | `							 if( iPtr <= 0 ){` |
|   3093715 | 1581 | `								 break;` |
|         - | 1582 | `							 }` |
|     74405 | 1583 | `						 }` |
|   5389084 | 1584 | `					 }` |
|  10793773 | 1585 | `					 nFuncTok++;` |
|         5 | 1586 | `				 }` |
|   3093715 | 1587 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1588 | `					 /* Syntax error */` |
|       ! 0 | 1589 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1590 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1591 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1592 | `					 }` |
|       ! 0 | 1593 | `					 return rc;` |
|         - | 1594 | `				 }` |
|   3093715 | 1595 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1596 | `					 /* Syntax error */` |
|       ! 0 | 1597 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1598 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1599 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1600 | `					 }` |
|       ! 0 | 1601 | `					 return rc;` |
|         - | 1602 | `				 }` |
|   3093715 | 1603 | `				 if( nFuncTok > 1 ){` |
|         - | 1604 | `					 /* Process function arguments */` |
|   2291409 | 1605 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   2291409 | 1606 | `					 if( rc != SXRET_OK ){` |
|        11 | 1607 | `						 return rc;` |
|         - | 1608 | `					 }` |
|   1145698 | 1609 | `				 }` |
|         - | 1610 | `				 /* Link the node to the tree */` |
|   3093707 | 1611 | `				 pNode->pLeft = apNode[iLeft];` |
|   3093707 | 1612 | `				 apNode[iLeft] = 0;` |
|  13887451 | 1613 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  10793749 | 1614 | `					 apNode[iCur+iPtr] = 0;` |
|   5396877 | 1615 | `				 }` |
|         - | 1616 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1617 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1618 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1619 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1620 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1621 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1622 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1623 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1624 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1625 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1626 | `				 {` |
|   3093707 | 1627 | `					 sxi32 iNew = iLeft - 1;` |
|   3879401 | 1628 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|    785699 | 1629 | `						 iNew--;` |
|         5 | 1630 | `					 }` |
|   3093702 | 1631 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   2016899 | 1632 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   1236675 | 1633 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    458391 | 1634 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    458391 | 1635 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    458391 | 1636 | `						 apNode[iNew] = 0;` |
|    458391 | 1637 | `						 pNode = apNode[iCur];` |
|    229198 | 1638 | `					 }` |
|         - | 1639 | `				 }` |
|   4985818 | 1640 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1641 | `				 /* Subscripting */` |
|   1530705 | 1642 | `				 sxi32 iArrTok = iCur + 1;` |
|   1530705 | 1643 | `				 sxi32 iNest = 1;` |
|   1530700 | 1644 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1645 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1646 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1647 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   1530700 | 1648 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1649 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1650 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|     58254 | 1651 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1652 | `						 /* Syntax error */` |
|       ! 0 | 1653 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1654 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1655 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1656 | `						 }` |
|       ! 0 | 1657 | `						 return rc;` |
|         - | 1658 | `				 }` |
|         - | 1659 | `				 /* Collect index tokens */` |
|   3034341 | 1660 | `				 while( iArrTok < nToken ){` |
|   3034341 | 1661 | `					 if( apNode[iArrTok] ){` |
|   3034309 | 1662 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1663 | `							 /* Increment nesting level */` |
|      3873 | 1664 | `							 iNest++;` |
|   3032375 | 1665 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1666 | `							 /* Decrement nesting level */` |
|   1534573 | 1667 | `							 iNest--;` |
|   1534573 | 1668 | `							 if( iNest <= 0 ){` |
|   1530705 | 1669 | `								 break;` |
|         - | 1670 | `							 }` |
|      1934 | 1671 | `						 }` |
|    751802 | 1672 | `					 }` |
|   1503641 | 1673 | `					 ++iArrTok;` |
|         5 | 1674 | `				 }` |
|   1530705 | 1675 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1676 | `					 /* Recurse and process this expression */` |
|   1406805 | 1677 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   1406805 | 1678 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1679 | `						 return rc;` |
|         - | 1680 | `					 }` |
|         - | 1681 | `					 /* Link the node to it's index */` |
|   1406805 | 1682 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    703400 | 1683 | `				 }` |
|         - | 1684 | `				 /* Link the node to the tree */` |
|   1530705 | 1685 | `				 pNode->pLeft = apNode[iLeft];` |
|   1530705 | 1686 | `				 pNode->pRight = 0;` |
|   1530705 | 1687 | `				 apNode[iLeft] = 0;` |
|   4565041 | 1688 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   3034341 | 1689 | `					 apNode[iNest] = 0;` |
|   1517173 | 1690 | `				 }` |
|    765355 | 1691 | `			 }else{` |
|         - | 1692 | `				 /* Member access operators [i.e: '->','::'] */` |
|   1908267 | 1693 | `				  iRight = iCur + 1;` |
|   1908273 | 1694 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         7 | 1695 | `					 iRight++;` |
|         1 | 1696 | `				 }` |
|   1908267 | 1697 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1698 | `					 /* Syntax error */` |
|         5 | 1699 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1700 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1701 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1702 | `					 }` |
|         5 | 1703 | `					 return rc;` |
|         - | 1704 | `				 }` |
|         - | 1705 | `				 /* Link the node to the tree */` |
|   1908263 | 1706 | `				 pNode->pLeft = apNode[iLeft];` |
|   1908258 | 1707 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   1894489 | 1708 | `					 && pNode->pLeft->pOp == 0 &&` |
|   1872651 | 1709 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|         - | 1710 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|         - | 1711 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|         - | 1712 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|         - | 1713 | `					  * accepts. */` |
|         4 | 1714 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|         - | 1715 | `						 /* Syntax error */` |
|       ! 0 | 1716 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1717 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|       ! 0 | 1718 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1719 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1720 | `						 }` |
|       ! 0 | 1721 | `						 return rc;` |
|         - | 1722 | `				 }` |
|   1908263 | 1723 | `				 pNode->pRight = apNode[iRight];` |
|   1908263 | 1724 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1725 | `			 }` |
|   3266330 | 1726 | `		 }` |
|  24073239 | 1727 | `		 iLeft = iCur;` |
|  12036622 | 1728 | `	 }` |
|         - | 1729 | `	 /* Handle left associative (new, clone) operators */` |
|  49057175 | 1730 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  42528893 | 1731 | `		 if( apNode[iCur] == 0 ){` |
|  25447031 | 1732 | `			 continue;` |
|         - | 1733 | `		 }` |
|  17081867 | 1734 | `		 pNode = apNode[iCur];` |
|  17081867 | 1735 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1736 | `			 SyToken *pToken;` |
|         - | 1737 | `			 /* Get the left node */` |
|       311 | 1738 | `			 iLeft = iCur + 1;` |
|       317 | 1739 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         7 | 1740 | `				 iLeft++;` |
|         1 | 1741 | `			 }` |
|       311 | 1742 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1743 | `				  /* Syntax error */` |
|       ! 0 | 1744 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1745 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1746 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1747 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1748 | `				 }` |
|       ! 0 | 1749 | `				 return rc;` |
|         - | 1750 | `			 }` |
|         - | 1751 | `			 /* Make sure the operand are of a valid type */` |
|       311 | 1752 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1753 | `				 /* Clone:` |
|         - | 1754 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|         - | 1755 | `				  *  ++ function call (including annonymous)` |
|         - | 1756 | `				  *  ++ array member` |
|         - | 1757 | `				  *  ++ 'new' operator` |
|         - | 1758 | `				  * Example:` |
|         - | 1759 | `				  *   clone $pObj;` |
|         - | 1760 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|         - | 1761 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|         - | 1762 | `				  */` |
|        42 | 1763 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|        38 | 1764 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1765 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1766 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1767 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1768 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1769 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1770 | `						 }` |
|       ! 0 | 1771 | `						 return rc;` |
|         - | 1772 | `					 }` |
|        17 | 1773 | `				 }` |
|        23 | 1774 | `			 }else{` |
|         - | 1775 | `				 /* New */` |
|       268 | 1776 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 1777 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 1778 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 1779 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 1780 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 1781 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 1782 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 1783 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 1784 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1785 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1786 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1787 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1788 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1789 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1790 | `					 }` |
|       ! 0 | 1791 | `					 return rc;` |
|         - | 1792 | `				 }` |
|       273 | 1793 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       273 | 1794 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       268 | 1795 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        31 | 1796 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 1797 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 1798 | `						 /* Syntax error */` |
|       ! 0 | 1799 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1800 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1801 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1802 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1803 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1804 | `						 }` |
|       ! 0 | 1805 | `						 return rc;` |
|         - | 1806 | `					 }` |
|       134 | 1807 | `				 }` |
|         - | 1808 | `			 }` |
|         - | 1809 | `			  /* Link the node to the tree */` |
|       311 | 1810 | `			 pNode->pLeft = apNode[iLeft];` |
|       311 | 1811 | `			 apNode[iLeft] = 0;` |
|       311 | 1812 | `			 pNode->pRight = 0; /* Paranoid */` |
|       153 | 1813 | `		 }` |
|   8540936 | 1814 | `	 }` |
|         - | 1815 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   6528287 | 1816 | `	 iLeft = -1;` |
|  49115561 | 1817 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  42528893 | 1818 | `		 if( apNode[iCur] == 0 ){` |
|  25447031 | 1819 | `			 continue;` |
|         - | 1820 | `		 }` |
|  17081867 | 1821 | `		 pNode = apNode[iCur];` |
|  17081867 | 1822 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     70059 | 1823 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     58423 | 1824 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1825 | `					 /* Link the node to the tree */` |
|     58435 | 1826 | `					 pNode->pLeft = apNode[iLeft];` |
|     58435 | 1827 | `					 apNode[iLeft] = 0;` |
|     29215 | 1828 | `			 }` |
|     93413 | 1829 | `		  }` |
|  17140253 | 1830 | `		 iLeft = iCur;` |
|   8599322 | 1831 | `	  }` |
|   6586673 | 1832 | `	 iLeft = -1;` |
|  49115561 | 1833 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  42528893 | 1834 | `		 if( apNode[iCur] == 0 ){` |
|  25505461 | 1835 | `			 continue;` |
|         - | 1836 | `		 }` |
|  17023437 | 1837 | `		 pNode = apNode[iCur];` |
|  17023437 | 1838 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     11624 | 1839 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     11629 | 1840 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1841 | `					 /* Syntax error */` |
|       ! 0 | 1842 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1843 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1844 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1845 | `					 }` |
|       ! 0 | 1846 | `					 return rc;` |
|         - | 1847 | `			 }` |
|         - | 1848 | `			 /* Link the node to the tree */` |
|     11629 | 1849 | `			 pNode->pLeft = apNode[iLeft];` |
|     11629 | 1850 | `			 apNode[iLeft] = 0;` |
|         - | 1851 | `			 /* Mark as pre-increment/decrement node */` |
|     11629 | 1852 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      5812 | 1853 | `		  }` |
|  17023437 | 1854 | `		 iLeft = iCur;` |
|   8511721 | 1855 | `	 }` |
|         - | 1856 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   6586673 | 1857 | `	  iLeft = 0;` |
|  49115555 | 1858 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  42528889 | 1859 | `		  if( apNode[iCur] ){` |
|  17011809 | 1860 | `			  pNode = apNode[iCur];` |
|  17011809 | 1861 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    268761 | 1862 | `				  if( iLeft > 0 ){` |
|         - | 1863 | `					  /* Link the node to the tree */` |
|    268759 | 1864 | `					  pNode->pLeft = apNode[iLeft];` |
|    268759 | 1865 | `					  apNode[iLeft] = 0;` |
|    268759 | 1866 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|      7829 | 1867 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1868 | `							   /* Syntax error */` |
|       ! 0 | 1869 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|       ! 0 | 1870 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1871 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1872 | `							  }` |
|       ! 0 | 1873 | `							  return rc;` |
|         - | 1874 | `						  }` |
|      3912 | 1875 | `					  }` |
|    134382 | 1876 | `				  }else{` |
|         - | 1877 | `					  /* Syntax error */` |
|         3 | 1878 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|         3 | 1879 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1880 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1881 | `					  }` |
|         3 | 1882 | `					  return rc;` |
|         - | 1883 | `				  }` |
|    134377 | 1884 | `			  }` |
|         - | 1885 | `			  /* Save terminal position */` |
|  17011807 | 1886 | `			  iLeft = iCur;` |
|   8505901 | 1887 | `		  }` |
|  21264446 | 1888 | `	  }` |
|         - | 1889 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1890 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1891 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1892 | `	  * yielding a right-leaning tree. */` |
|  49115553 | 1893 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  42528887 | 1894 | `		 if( apNode[iCur] == 0 ){` |
|  25785951 | 1895 | `			 continue;` |
|         - | 1896 | `		 }` |
|  16742941 | 1897 | `		 pNode = apNode[iCur];` |
|  16742941 | 1898 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 1899 | `			 sxi32 iL, iR;` |
|         - | 1900 | `			 /* Find the right operand */` |
|       113 | 1901 | `			 iR = -1;` |
|         - | 1902 | `			 {` |
|         - | 1903 | `				 sxi32 j;` |
|       125 | 1904 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       125 | 1905 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 1906 | `				 }` |
|         - | 1907 | `			 }` |
|         - | 1908 | `			 /* Find the left operand */` |
|       113 | 1909 | `			 iL = -1;` |
|         - | 1910 | `			 {` |
|         - | 1911 | `				 sxi32 j;` |
|       181 | 1912 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       181 | 1913 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 1914 | `				 }` |
|         - | 1915 | `			 }` |
|       113 | 1916 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 1917 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1918 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 1919 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1920 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1921 | `				 }` |
|       ! 0 | 1922 | `				 return rc;` |
|         - | 1923 | `			 }` |
|       113 | 1924 | `			 pNode->pLeft  = apNode[iL];` |
|       113 | 1925 | `			 pNode->pRight = apNode[iR];` |
|       113 | 1926 | `			 apNode[iL] = 0;` |
|       113 | 1927 | `			 apNode[iR] = 0;` |
|         - | 1928 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 1929 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 1930 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 1931 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 1932 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 1933 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 1934 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 1935 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 1936 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 1937 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 1938 | `			  * operands are respected. */` |
|       112 | 1939 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        74 | 1940 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 1941 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 1942 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 1943 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 1944 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 1945 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 1946 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 1947 | `				 while( pTail->pLeft` |
|        34 | 1948 | `					 && pTail->pLeft->pOp` |
|        23 | 1949 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 1950 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 1951 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 1952 | `					 pTail = pTail->pLeft;` |
|         1 | 1953 | `				 }` |
|         - | 1954 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 1955 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 1956 | `				 pTail->pLeft = pNode;` |
|        27 | 1957 | `				 apNode[iCur] = pHead;` |
|        13 | 1958 | `			 }` |
|        56 | 1959 | `		 }` |
|   8371473 | 1960 | `	 }` |
|         - | 1961 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  72453245 | 1962 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  65866589 | 1963 | `		 iLeft = -1;` |
| 491155115 | 1964 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 425288541 | 1965 | `			 if( apNode[iCur] == 0 ){` |
| 289592429 | 1966 | `				 continue;` |
|         - | 1967 | `			 }` |
| 135696117 | 1968 | `			 pNode = apNode[iCur];` |
| 135696117 | 1969 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1970 | `				 /* Get the right node */` |
|   2419259 | 1971 | `				 iRight = iCur + 1;` |
|   3602861 | 1972 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   1183607 | 1973 | `					 iRight++;` |
|         5 | 1974 | `				 }` |
|   2419259 | 1975 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1976 | `					 /* Syntax error */` |
|        11 | 1977 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        11 | 1978 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1979 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1980 | `					 }` |
|        11 | 1981 | `					 return rc;` |
|         - | 1982 | `				 }` |
|   2419251 | 1983 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1984 | `					 sxi32  iTmp;` |
|         - | 1985 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1986 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 1987 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 1988 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 1989 | `					  * is swapped below. */` |
|        67 | 1990 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 1991 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 1992 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 1993 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1994 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1995 | `						 }` |
|         3 | 1996 | `						 return rc;` |
|         - | 1997 | `					 }` |
|        64 | 1998 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|         - | 1999 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 2000 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 2001 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2002 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2003 | `						 }` |
|       ! 0 | 2004 | `						 return rc;` |
|         - | 2005 | `					 }` |
|        64 | 2006 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|        46 | 2007 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 2008 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 2009 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 2010 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2011 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 2012 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2013 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 2014 | `									 }` |
|       ! 0 | 2015 | `									 return rc;` |
|         - | 2016 | `							 }` |
|       ! 0 | 2017 | `						 }` |
|        21 | 2018 | `					 }` |
|         - | 2019 | `					 /* Swap operands */` |
|        64 | 2020 | `					 iTmp = iRight;` |
|        64 | 2021 | `					 iRight = iLeft;` |
|        64 | 2022 | `					 iLeft = iTmp;` |
|        30 | 2023 | `				 }` |
|         - | 2024 | `				 /* Link the node to the tree */` |
|   2419249 | 2025 | `				 pNode->pLeft = apNode[iLeft];` |
|   2419249 | 2026 | `				 pNode->pRight = apNode[iRight];` |
|   2419249 | 2027 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   1209622 | 2028 | `			 }` |
| 135696107 | 2029 | `			 iLeft = iCur;` |
|  67848056 | 2030 | `		 }` |
|  32933292 | 2031 | `	 }` |
|         - | 2032 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2033 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2034 | `	  * we are dealing with a single operator.` |
|         - | 2035 | `	  */` |
|   6586661 | 2036 | `	  iLeft = -1;` |
|  47493141 | 2037 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  41106835 | 2038 | `		  if( apNode[iCur] == 0 ){` |
|  29803533 | 2039 | `			  continue;` |
|         - | 2040 | `		  }` |
|  11303307 | 2041 | `		  pNode = apNode[iCur];` |
|  11303307 | 2042 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    200355 | 2043 | `			  sxi32 iNest = 1;` |
|    200355 | 2044 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2045 | `				  /* Missing condition */` |
|         3 | 2046 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|         3 | 2047 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2048 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2049 | `				  }` |
|         3 | 2050 | `				  return rc;` |
|         - | 2051 | `			  }` |
|         - | 2052 | `			  /* Get the right node */` |
|    200353 | 2053 | `			  iRight = iCur + 1;` |
|    617631 | 2054 | `			  while( iRight < nToken  ){` |
|    617631 | 2055 | `				  if( apNode[iRight] ){` |
|    400633 | 2056 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2057 | `						  /* Increment nesting level */` |
|       ! 0 | 2058 | `						  ++iNest;` |
|    400633 | 2059 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2060 | `						  /* Decrement nesting level */` |
|    200353 | 2061 | `						  --iNest;` |
|    200353 | 2062 | `						  if( iNest <= 0 ){` |
|    200353 | 2063 | `							  break;` |
|         - | 2064 | `						  }` |
|       ! 0 | 2065 | `					  }` |
|    100140 | 2066 | `				  }` |
|    417283 | 2067 | `				  iRight++;` |
|         5 | 2068 | `			  }` |
|    200353 | 2069 | `			  if( iRight > iCur + 1 ){` |
|         - | 2070 | `				  /* Recurse and process the then expression */` |
|    200285 | 2071 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    200285 | 2072 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2073 | `					  return rc;` |
|         - | 2074 | `				  }` |
|         - | 2075 | `				  /* Link the node to the tree */` |
|    200285 | 2076 | `				  pNode->pLeft = apNode[iCur + 1];` |
|    100140 | 2077 | `			  }else{` |
|         - | 2078 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2079 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2080 | `			  }` |
|    200353 | 2081 | `			  apNode[iCur + 1] = 0;` |
|    200353 | 2082 | `			  if( iRight + 1 < nToken ){` |
|         - | 2083 | `				  /* Recurse and process the else expression */` |
|    200353 | 2084 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    200353 | 2085 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2086 | `					  return rc;` |
|         - | 2087 | `				  }` |
|         - | 2088 | `				  /* Link the node to the tree */` |
|    200353 | 2089 | `				  pNode->pRight = apNode[iRight + 1];` |
|    200353 | 2090 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|    100179 | 2091 | `			  }else{` |
|       ! 0 | 2092 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2093 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2094 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2095 | `				 }` |
|       ! 0 | 2096 | `				 return rc;` |
|         - | 2097 | `			  }` |
|         - | 2098 | `			  /* Point to the condition */` |
|    200353 | 2099 | `			  pNode->pCond  = apNode[iLeft];` |
|    200353 | 2100 | `			  apNode[iLeft] = 0;` |
|    200353 | 2101 | `			  break;` |
|         - | 2102 | `		  }` |
|  11102957 | 2103 | `		  iLeft = iCur;` |
|   5551481 | 2104 | `	  }` |
|         - | 2105 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2106 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2107 | `	  * so there is no need for a precedence loop here.` |
|         - | 2108 | `	  */` |
|   6586659 | 2109 | `	 iRight = -1;` |
|  49115357 | 2110 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  42528757 | 2111 | `		 if( apNode[iCur] == 0 ){` |
|  33683895 | 2112 | `			 continue;` |
|         - | 2113 | `		 }` |
|   8844867 | 2114 | `		 pNode = apNode[iCur];` |
|   8844867 | 2115 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2116 | `			 /* Get the left node */` |
|   2258091 | 2117 | `			 iLeft = iCur - 1;` |
|   2754807 | 2118 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    496721 | 2119 | `				 iLeft--;` |
|         5 | 2120 | `			 }` |
|   2258091 | 2121 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2122 | `				 /* Syntax error */` |
|        44 | 2123 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2124 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2125 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2126 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2127 | `				 }else{` |
|        40 | 2128 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|         - | 2129 | `				 }` |
|        44 | 2130 | `				 if( rc != SXERR_ABORT ){` |
|        42 | 2131 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2132 | `				 }` |
|        44 | 2133 | `				 return rc;` |
|         - | 2134 | `			 }` |
|         - | 2135 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2136 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2137 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2138 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2139 | `			  * a write. */` |
|   2258049 | 2140 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2141 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2142 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2143 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2144 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2145 | `				 }` |
|        11 | 2146 | `				 return rc;` |
|         - | 2147 | `			 }` |
|   2258041 | 2148 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       115 | 2149 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        82 | 2150 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2151 | `					 /* Left operand must be a modifiable l-value */` |
|         6 | 2152 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2153 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2154 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2155 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2156 | `					 }else{` |
|         4 | 2157 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2158 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2159 | `					 }` |
|         6 | 2160 | `					 if( rc != SXERR_ABORT ){` |
|         6 | 2161 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2162 | `					 }` |
|         6 | 2163 | `					 return rc;` |
|         - | 2164 | `				 }` |
|        40 | 2165 | `			 }` |
|         - | 2166 | `			 /* Link the node to the tree (Reverse) */` |
|   2258037 | 2167 | `			 pNode->pLeft = apNode[iRight];` |
|   2258037 | 2168 | `			 pNode->pRight = apNode[iLeft];` |
|   2258037 | 2169 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   1129016 | 2170 | `		 }` |
|   8844813 | 2171 | `		 iRight = iCur;` |
|   4422409 | 2172 | `	 }` |
|         - | 2173 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  32933005 | 2174 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  26346405 | 2175 | `		 iLeft = -1;` |
| 196461141 | 2176 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 170114741 | 2177 | `			 if( apNode[iCur] == 0 ){` |
| 143767935 | 2178 | `				 continue;` |
|         - | 2179 | `			 }` |
|  26346811 | 2180 | `			 pNode = apNode[iCur];` |
|  26346811 | 2181 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2182 | `				 /* Get the right node */` |
|        72 | 2183 | `				 iRight = iCur + 1;` |
|       110 | 2184 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        40 | 2185 | `					 iRight++;` |
|         2 | 2186 | `				 }` |
|        72 | 2187 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2188 | `					 /* Syntax error */` |
|       ! 0 | 2189 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 2190 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2191 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2192 | `					 }` |
|       ! 0 | 2193 | `					 return rc;` |
|         - | 2194 | `				 }` |
|         - | 2195 | `				 /* Link the node to the tree */` |
|        72 | 2196 | `				 pNode->pLeft = apNode[iLeft];` |
|        72 | 2197 | `				 pNode->pRight = apNode[iRight];` |
|        72 | 2198 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        35 | 2199 | `			 }` |
|  26346811 | 2200 | `			 iLeft = iCur;` |
|  13173408 | 2201 | `		 }` |
|  13173205 | 2202 | `	 }` |
|         - | 2203 | `	 /* Point to the root of the expression tree */` |
|  42528661 | 2204 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  35942079 | 2205 | `		 if( apNode[iCur] ){` |
|   6395771 | 2206 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        23 | 2207 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|        23 | 2208 | `				  if( rc != SXERR_ABORT ){` |
|        23 | 2209 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2210 | `				  }` |
|        23 | 2211 | `				  return rc;` |
|         - | 2212 | `			 }` |
|   6395753 | 2213 | `			 apNode[0] = apNode[iCur];` |
|   6395753 | 2214 | `			 apNode[iCur] = 0;` |
|   3197874 | 2215 | `		 }` |
|  17971033 | 2216 | `	 }` |
|   6586587 | 2217 | `	 return SXRET_OK;` |
|   6044933 | 2218 | ` }` |
|         - | 2219 | ` /*` |
|         - | 2220 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2221 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2222 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2223 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2224 | `  */` |
|   7041318 | 2225 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2226 | `{` |
|         - | 2227 | `	ph7_expr_node **apNode;` |
|         - | 2228 | `	ph7_expr_node *pNode;` |
|         - | 2229 | `	sxi32 rc;` |
|         - | 2230 | `	/* Reset node container */` |
|   7041323 | 2231 | `	SySetReset(pExprNode);` |
|   7041323 | 2232 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2233 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2234 | `	{` |
|   7041323 | 2235 | `		int iLastWasTerm = 0;` |
|   7041323 | 2236 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  43700353 | 2237 | `		while( pGen->pIn < pGen->pEnd ){` |
|  36659069 | 2238 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  36659069 | 2239 | `			if( rc != SXRET_OK ){` |
|        38 | 2240 | `				return rc;` |
|         - | 2241 | `			}` |
|         - | 2242 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  36659035 | 2243 | `			if( pNode->xCode ){` |
|         - | 2244 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  18499939 | 2245 | `				iLastWasTerm = 1;` |
|  27409068 | 2246 | `			}else if( pNode->pOp ){` |
|         - | 2247 | `				/* Operator node */` |
|   9888169 | 2248 | `				iLastWasTerm = 0;` |
|   4944087 | 2249 | `			}else{` |
|         - | 2250 | `				/* Delimiter: ')' and ']' end terms */` |
|   8270937 | 2251 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2252 | `			}` |
|         - | 2253 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2254 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2255 | `			 * node kind, so this single test covers all branches. */` |
|  36659035 | 2256 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2257 | `			/* Save the extracted node */` |
|  36659035 | 2258 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2259 | `		}` |
|         - | 2260 | `	}` |
|   7041289 | 2261 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2262 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2263 | `		*ppRoot = 0;` |
|       ! 0 | 2264 | `		return SXRET_OK;` |
|         - | 2265 | `	}` |
|   7041289 | 2266 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2267 | `	/* Make sure we are dealing with valid nodes */` |
|   7041289 | 2268 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   7041289 | 2269 | `	if( rc != SXRET_OK ){` |
|         - | 2270 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2271 | `		 * cleanup the mess left behind.` |
|         - | 2272 | `		 */` |
|        54 | 2273 | `		*ppRoot = 0;` |
|        54 | 2274 | `		return rc;` |
|         - | 2275 | `	}` |
|         - | 2276 | `	/* Build the tree */` |
|   7041239 | 2277 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|   7041239 | 2278 | `	if( rc != SXRET_OK ){` |
|         - | 2279 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2280 | `		*ppRoot = 0;` |
|       103 | 2281 | `		return rc;` |
|         - | 2282 | `	}` |
|         - | 2283 | `	/* Point to the root of the tree */` |
|   7041141 | 2284 | `	*ppRoot = apNode[0];` |
|   7041141 | 2285 | `	return SXRET_OK;` |
|   3520664 | 2286 | `}` |
|         - | 2287 |  |
