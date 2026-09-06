# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1169/1343 lines (87.04%)

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
|  22455394 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  274 | `{` |
|  22455399 |  275 | `	sxu32 n = 0;` |
|         - |  276 | `	sxi32 rc;` |
|         - |  277 | `	/* Do a linear lookup on the operators table */` |
| 335752371 |  278 | `	for(;;){` |
| 671504747 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  280 | `			break;` |
|         - |  281 | `		}` |
| 671504747 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  65475239 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  32737622 |  285 | `		}else{` |
| 606029513 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  287 | `		}` |
| 671504747 |  288 | `		if( rc == 0 ){` |
|  22708461 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  22375991 |  291 | `				return &aOpTable[n];` |
|         - |  292 | `			}` |
|         - |  293 | `			/* Handle ambiguity */` |
|    332475 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|     24121 |  296 | `				return &aOpTable[n];` |
|         - |  297 | `			}` |
|    308359 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|     55305 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     55305 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|     55297 |  303 | `					return &aOpTable[n];` |
|         - |  304 | `				}` |
|         - |  305 |  |
|         4 |  306 | `			}` |
|    126531 |  307 | `		}` |
| 649049353 |  308 | `		++n; /* Next operator in the table */` |
|         5 |  309 | `	}` |
|         - |  310 | `	/* No such operator */` |
|       ! 0 |  311 | `	return 0;` |
|  11227702 |  312 | `}` |
|         - |  313 | `/*` |
|         - |  314 | ` * Delimit a set of token stream.` |
|         - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  317 | ` */` |
|   6302856 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  319 | `{` |
|   6302861 |  320 | `	SyToken *pCur = pIn;` |
|   6302861 |  321 | `	sxi32 iNest = 1;` |
|  70665819 |  322 | `	for(;;){` |
| 141331643 |  323 | `		if( pCur >= pEnd ){` |
|     16295 |  324 | `			break;` |
|         - |  325 | `		}` |
| 141315353 |  326 | `		if( pCur->nType & nTokStart ){` |
|         - |  327 | `			/* Increment nesting level */` |
|   5496989 |  328 | `			iNest++;` |
| 138566861 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  330 | `			/* Decrement nesting level */` |
|  11783555 |  331 | `			iNest--;` |
|  11783555 |  332 | `			if( iNest <= 0 ){` |
|   6286571 |  333 | `				break;` |
|         - |  334 | `			}` |
|   2748492 |  335 | `		}` |
|         - |  336 | `		/* Advance cursor */` |
| 135028787 |  337 | `		pCur++;` |
|         5 |  338 | `	}` |
|         - |  339 | `	/* Point to the end of the chunk */` |
|   6302861 |  340 | `	*ppEnd = pCur;` |
|   6302861 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  344 | ` * Note on reserved keywords.` |
|         - |  345 | ` *  According to the PHP language reference manual:` |
|         - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  350 | ` */` |
|    430750 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  352 | `{` |
|    430750 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    430652 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  355 | `		){` |
|       167 |  356 | `			return TRUE;` |
|         - |  357 | `	}` |
|    430593 |  358 | `	if( bCheckFunc ){` |
|     39820 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|     39813 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|     39794 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|        51 |  362 | `				return TRUE;` |
|         - |  363 | `		}` |
|     19887 |  364 | `	}` |
|         - |  365 | `	/* Not a language construct */` |
|    430547 |  366 | `	return FALSE;` |
|    215380 |  367 | `}` |
|         - |  368 | `/*` |
|         - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  373 | ` */` |
|  13032750 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  375 | `{` |
|         - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  377 | `	sxi32 i,rc;` |
|         - |  378 |  |
|  13032755 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|        34 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|        34 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        16 |  383 | `	}` |
|  13032755 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  83349173 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  70316457 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|    222619 |  388 | `			continue;` |
|         - |  389 | `		}` |
|  70093843 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   5925729 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    273098 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   5592639 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|         - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  397 | `						 */` |
|   5592639 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   5592639 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   5592639 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   2796317 |  401 | `					}` |
|   2796317 |  402 | `			}` |
|   5925729 |  403 | `			iParen++;` |
|  67130981 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   5925729 |  405 | `			if( iParen <= 0 ){` |
|        16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|        16 |  407 | `				if( rc != SXERR_ABORT ){` |
|        16 |  408 | `					rc = SXERR_SYNTAX;` |
|         6 |  409 | `				}` |
|        16 |  410 | `				return rc;` |
|         - |  411 | `			}` |
|   5925717 |  412 | `			iParen--;` |
|  61205251 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|   2591689 |  414 | `			iSquare++;` |
|  56946553 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|   2591703 |  416 | `			if( iSquare <= 0 ){` |
|         8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|         8 |  418 | `				if( rc != SXERR_ABORT ){` |
|         8 |  419 | `					rc = SXERR_SYNTAX;` |
|         3 |  420 | `				}` |
|         8 |  421 | `				return rc;` |
|         - |  422 | `			}` |
|   2591697 |  423 | `			iSquare--;` |
|  54354859 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|      3963 |  425 | `			iBraces++;` |
|      3963 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
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
|  53057034 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|      3965 |  472 | `			if( iBraces <= 0 ){` |
|        15 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|        15 |  474 | `				if( rc != SXERR_ABORT ){` |
|        15 |  475 | `					rc = SXERR_SYNTAX;` |
|         6 |  476 | `				}` |
|        15 |  477 | `				return rc;` |
|         - |  478 | `			}` |
|      3953 |  479 | `			iBraces--;` |
|  53053069 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|    381827 |  481 | `			if( iQuesty > 0 ){` |
|    381537 |  482 | `				iQuesty--;` |
|    191061 |  483 | `			}else if( iParen <= 0 ){` |
|         - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  488 | `				if( rc != SXERR_ABORT ){` |
|         6 |  489 | `					rc = SXERR_SYNTAX;` |
|         2 |  490 | `				}` |
|         6 |  491 | `				return rc;` |
|         5 |  492 | `			}` |
|  52860182 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|  17346805 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|  17346805 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|    381539 |  496 | `				iQuesty++;` |
|  17156038 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     67517 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|     33756 |  516 | `			}` |
|   8673400 |  517 | `		}` |
|  35046907 |  518 | `	}` |
|  13032721 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        19 |  520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|        19 |  521 | `		if( rc != SXERR_ABORT ){` |
|        19 |  522 | `			rc = SXERR_SYNTAX;` |
|         8 |  523 | `		}` |
|        19 |  524 | `		return rc;` |
|         - |  525 | `	}` |
|  13032705 |  526 | `	return SXRET_OK;` |
|   6516380 |  527 | `}` |
|         - |  528 | `/*` |
|         - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  531 | ` */` |
|  11184830 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  533 | `{` |
|  11184835 |  534 | `	SyToken *pIn = *ppCur;` |
|         - |  535 | `	/* Jump the first literal seen */` |
|  11184835 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|  11180847 |  537 | `		pIn++;` |
|   5590421 |  538 | `	}` |
|   5594436 |  539 | `	for(;;){` |
|  11188877 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      4047 |  541 | `			pIn++;` |
|      4047 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4045 |  543 | `				pIn++;` |
|      2020 |  544 | `			}` |
|      2026 |  545 | `		}else{` |
|   5592420 |  546 | `			break;` |
|         - |  547 | `		}` |
|         5 |  548 | `	}` |
|         - |  549 | `	/* Synchronize pointers */` |
|  11184835 |  550 | `	*ppCur = pIn;` |
|  11184835 |  551 | `}` |
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
|         - |  642 | `	sxu32 nLine;` |
|         - |  643 | `	sxi32 rc;` |
|         - |  644 | `	/* Jump the 'function' keyword */` |
|       495 |  645 | `	nLine = pIn->nLine;` |
|       495 |  646 | `	pIn++;` |
|       495 |  647 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        12 |  648 | `		pIn++;` |
|         5 |  649 | `	}` |
|       495 |  650 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  651 | `		/* Syntax error */` |
|         6 |  652 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|         6 |  653 | `		if( rc != SXERR_ABORT ){` |
|         6 |  654 | `			rc = SXERR_SYNTAX;` |
|         2 |  655 | `		}` |
|         6 |  656 | `		goto Synchronize;` |
|         - |  657 | `	}` |
|       491 |  658 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|       491 |  659 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       491 |  660 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  661 | `		/* Syntax error */` |
|         6 |  662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|         6 |  663 | `		if( rc != SXERR_ABORT ){` |
|         6 |  664 | `			rc = SXERR_SYNTAX;` |
|         2 |  665 | `		}` |
|         6 |  666 | `		goto Synchronize;` |
|         - |  667 | `	}` |
|       487 |  668 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  669 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|       487 |  670 | `	ExprSkipReturnType(&pIn,pEnd);` |
|       487 |  671 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
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
|       471 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|       471 |  711 | `		pIn++; /* Jump the leading curly '{' */` |
|       471 |  712 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       471 |  713 | `		if( pIn < pEnd ){` |
|       471 |  714 | `			pIn++;` |
|       233 |  715 | `		}` |
|       238 |  716 | `	}else{` |
|         - |  717 | `		/* Syntax error */` |
|       ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|       ! 0 |  719 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  720 | `			return SXERR_ABORT;` |
|         - |  721 | `		}` |
|         - |  722 | `	}` |
|       471 |  723 | `	rc = SXRET_OK;` |
|       245 |  724 | `Synchronize:` |
|         - |  725 | `	/* Synchronize pointers */` |
|       495 |  726 | `	*ppCur = pIn;` |
|       495 |  727 | `	return rc;` |
|       250 |  728 | `}` |
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
|  70320792 |  892 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 |  893 | `{` |
|         - |  894 | `	ph7_expr_node *pNode;` |
|         - |  895 | `	SyToken *pCur;` |
|         - |  896 | `	sxi32 rc;` |
|         - |  897 | `	/* Allocate a new node */` |
|  70320797 |  898 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  70320797 |  899 | `	if( pNode == 0 ){` |
|         - |  900 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  901 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  902 | `		 */` |
|       ! 0 |  903 | `		return SXERR_MEM;` |
|         - |  904 | `	}` |
|         - |  905 | `	/* Zero the structure */` |
|  70320797 |  906 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  70320797 |  907 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - |  908 | `	/* Point to the head of the token stream */` |
|  70320797 |  909 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - |  910 | `	/* Start collecting tokens */` |
|  70320797 |  911 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      4275 |  912 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
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
|      4195 |  927 | `		pCur++;` |
|      4195 |  928 | `		pGen->pIn = pCur;` |
|      4195 |  929 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|      4195 |  930 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|      4195 |  931 | `		if( rc == SXRET_OK && *ppNode ){` |
|      4195 |  932 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|      2095 |  933 | `		}` |
|      4195 |  934 | `		return rc;` |
|         - |  935 | `	}` |
|  70316527 |  936 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - |  937 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - |  938 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - |  939 | `		 */` |
|    222621 |  940 | `		pCur++; /* Skip the opening '[' */` |
|    222621 |  941 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|    222621 |  942 | `		if( pCur < pGen->pEnd ){` |
|    222621 |  943 | `			pCur++; /* Skip past the closing ']' */` |
|    111313 |  944 | `		}else{` |
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
|    222791 |  956 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|       342 |  957 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|       342 |  958 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|        56 |  959 | `				pNode->xCode = PH7_CompileShortList;` |
|        29 |  960 | `			}else{` |
|       287 |  961 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - |  962 | `			}` |
|       172 |  963 | `		}else{` |
|    222281 |  964 | `			pNode->xCode = PH7_CompileShortArray;` |
|         5 |  965 | `		}` |
|  70205219 |  966 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
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
|  70093899 |  977 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|  19938540 |  978 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   9996886 |  979 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
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
|  70093884 | 1005 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1006 | `		/* Point to the instance that describe this operator */` |
|  19938523 | 1007 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1008 | `		/* Advance the stream cursor */` |
|  19938523 | 1009 | `		pCur++;` |
|  60124614 | 1010 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1011 | `		/* Isolate variable */` |
|  33003705 | 1012 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  16501861 | 1013 | `			pCur++; /* Variable variable */` |
|         5 | 1014 | `		}` |
|  16501849 | 1015 | `		if( pCur < pGen->pEnd ){` |
|  16501849 | 1016 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1017 | `				/* Variable name */` |
|  16501819 | 1018 | `				pCur++;` |
|   8250942 | 1019 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
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
|   8250920 | 1034 | `		}` |
|  16501845 | 1035 | `		pNode->xCode = PH7_CompileVariable;` |
|  41904431 | 1036 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    831597 | 1037 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    831597 | 1038 | `		 if( bAfterMemberOp ){` |
|         - | 1039 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1040 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1041 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1042 | `			  * as a plain literal like an ordinary identifier member name. */` |
|    130233 | 1043 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    130233 | 1044 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    766483 | 1045 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1046 | `			 /* List/Array node */` |
|    293453 | 1047 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1048 | `				 /* Assume a literal */` |
|       ! 0 | 1049 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1050 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1051 | `			 }else{` |
|    293453 | 1052 | `				 pCur += 2;` |
|         - | 1053 | `				 /* Collect array/list tokens */` |
|    293453 | 1054 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    293453 | 1055 | `				 if( pCur < pGen->pEnd ){` |
|    293451 | 1056 | `					 pCur++;` |
|    146728 | 1057 | `				 }else{` |
|         - | 1058 | `					 /* Syntax error */` |
|         4 | 1059 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         1 | 1060 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|         3 | 1061 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1062 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1063 | `					 }` |
|         3 | 1064 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1065 | `					 return rc;` |
|         - | 1066 | `				 }` |
|    293451 | 1067 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    293451 | 1068 | `				 if( pNode->xCode == PH7_CompileList ){` |
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
|    554643 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1082 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|     16149 | 1083 | `			 pCur++; /* Skip 'yield' keyword */` |
|     16149 | 1084 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1085 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1086 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|     16149 | 1087 | `			 pNode->xCode = PH7_CompileYield;` |
|    399849 | 1088 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|    391543 | 1089 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        54 | 1090 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        33 | 1091 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1092 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|       495 | 1093 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1094 | `				 /* Assume a literal */` |
|       ! 0 | 1095 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1096 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1097 | `			 }else{` |
|         - | 1098 | `				 /* Assemble annonymous functions body */` |
|       495 | 1099 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|       495 | 1100 | `				 if( rc != SXRET_OK ){` |
|        28 | 1101 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        28 | 1102 | `					 return rc;` |
|         - | 1103 | `				 }` |
|       471 | 1104 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1105 | `			  }` |
|    391520 | 1106 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
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
|    391272 | 1121 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    391122 | 1122 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|        46 | 1123 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|        25 | 1124 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|         - | 1125 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|       291 | 1126 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|       291 | 1127 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1128 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1129 | `				 return rc;` |
|         - | 1130 | `			 }` |
|       291 | 1131 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    391116 | 1132 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1133 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|        77 | 1134 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|        77 | 1135 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1136 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1137 | `				 return rc;` |
|         - | 1138 | `			 }` |
|        77 | 1139 | `			 pNode->xCode = PH7_CompileMatch;` |
|    390937 | 1140 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1141 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1142 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1143 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        38 | 1144 | `			 pCur++; /* Skip 'throw' */` |
|        38 | 1145 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|         - | 1146 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|         - | 1147 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        38 | 1148 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    390883 | 1149 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1150 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|        93 | 1151 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|        93 | 1152 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|        49 | 1153 | `		 }else{` |
|         - | 1154 | `			 /* Assume a literal */` |
|    390777 | 1155 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    390777 | 1156 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1157 | `		 }` |
|  33237701 | 1158 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1159 | `		 /* Constants,function name,namespace path,class name... */` |
|  10663819 | 1160 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|  10663819 | 1161 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   5331912 | 1162 | `	 }else{` |
|  22158105 | 1163 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1164 | `			 /* Point to the code generator routine */` |
|   7325197 | 1165 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   7325197 | 1166 | `			 if( pNode->xCode == 0 ){` |
|         3 | 1167 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|         3 | 1168 | `				 if( rc != SXERR_ABORT ){` |
|         3 | 1169 | `					 rc = SXERR_SYNTAX;` |
|         1 | 1170 | `				 }` |
|         3 | 1171 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1172 | `				 return rc;` |
|         - | 1173 | `			 }` |
|   3662595 | 1174 | `		 }` |
|         - | 1175 | `		/* Advance the stream cursor */` |
|  22158103 | 1176 | `		pCur++;` |
|         - | 1177 | `	 }` |
|         - | 1178 | `	/* Point to the end of the token stream */` |
|  70316493 | 1179 | `	pNode->pEnd = pCur;` |
|         - | 1180 | `	/* Save the node for later processing */` |
|  70316493 | 1181 | `	*ppNode = pNode;` |
|         - | 1182 | `	/* Synchronize cursors */` |
|  70316493 | 1183 | `	pGen->pIn = pCur;` |
|  70316493 | 1184 | `	return SXRET_OK;` |
|  35160401 | 1185 | `}` |
|         - | 1186 | `/*` |
|         - | 1187 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1188 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1189 | ` * level is zero.` |
|         - | 1190 | ` */` |
|   1355420 | 1191 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1192 | `{` |
|   1355425 | 1193 | `	SyToken *pCur = pStart;` |
|   1355425 | 1194 | `	sxi32 iNest = 0;` |
|   1355425 | 1195 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1196 | `		/* Last expression */` |
|    560677 | 1197 | `		return SXERR_EOF;` |
|         - | 1198 | `	}` |
|   3475303 | 1199 | `	while( pCur < pEnd ){` |
|   3224577 | 1200 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    544027 | 1201 | `			break;` |
|         - | 1202 | `		}` |
|   2680555 | 1203 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    219897 | 1204 | `			iNest++;` |
|   2570609 | 1205 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    219899 | 1206 | `			iNest--;` |
|    109947 | 1207 | `		}` |
|   2680555 | 1208 | `		pCur++;` |
|         5 | 1209 | `	}` |
|    794753 | 1210 | `	*ppNext = pCur;` |
|    794753 | 1211 | `	return SXRET_OK;` |
|    677715 | 1212 | `}` |
|         - | 1213 | `/*` |
|         - | 1214 | ` * Free an expression tree.` |
|         - | 1215 | ` */` |
|  60079150 | 1216 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1217 | `{` |
|  60079155 | 1218 | `	if( pNode->pLeft ){` |
|         - | 1219 | `		/* Release the left tree */` |
|  23855427 | 1220 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|  11927711 | 1221 | `	}` |
|  60079155 | 1222 | `	if( pNode->pRight ){` |
|         - | 1223 | `		/* Release the right tree */` |
|  13877105 | 1224 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   6938550 | 1225 | `	}` |
|  60079155 | 1226 | `	if( pNode->pCond ){` |
|         - | 1227 | `		/* Release the conditional tree used by the ternary operator */` |
|    381535 | 1228 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|    190765 | 1229 | `	}` |
|  60079155 | 1230 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|         - | 1231 | `		ph7_expr_node **apArg;` |
|         - | 1232 | `		sxu32 n;` |
|         - | 1233 | `		/* Release node arguments */` |
|   6585829 | 1234 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  14843189 | 1235 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   8257365 | 1236 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   4128685 | 1237 | `		}` |
|   6585829 | 1238 | `		SySetRelease(&pNode->aNodeArgs);` |
|   3292912 | 1239 | `	}` |
|         - | 1240 | `	/* Finally,release this node */` |
|  60079155 | 1241 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  60079155 | 1242 | `}` |
|         - | 1243 | `/*` |
|         - | 1244 | ` * Free an expression tree.` |
|         - | 1245 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|         - | 1246 | ` */` |
|  13032780 | 1247 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1248 | `{` |
|         - | 1249 | `	ph7_expr_node **apNode;` |
|         - | 1250 | `	sxu32 n;` |
|  13032785 | 1251 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  83349323 | 1252 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  70316543 | 1253 | `		if( apNode[n] ){` |
|  13033119 | 1254 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   6516557 | 1255 | `		}` |
|  35158274 | 1256 | `	}` |
|  13032785 | 1257 | `	return SXRET_OK;` |
|         5 | 1258 | `}` |
|         - | 1259 | `/*` |
|         - | 1260 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1261 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1262 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1263 | ` * (PHP 8.0 makes this a fatal parse error:` |
|         - | 1264 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1265 | ` */` |
|  17087014 | 1266 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1267 | `{` |
|  17087019 | 1268 | `	if( pNode == 0 ){` |
|  10607727 | 1269 | `		return 0;` |
|         - | 1270 | `	}` |
|   6479297 | 1271 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        16 | 1272 | `		return 1;` |
|         - | 1273 | `	}` |
|   6479285 | 1274 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1275 | `		return 1;` |
|         - | 1276 | `	}` |
|   6479281 | 1277 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1278 | `		return 1;` |
|         - | 1279 | `	}` |
|   6479281 | 1280 | `	return 0;` |
|   8543512 | 1281 | `}` |
|         - | 1282 | `/*` |
|         - | 1283 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1284 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1285 | ` */` |
|   4097640 | 1286 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|         5 | 1287 | `{` |
|         - | 1288 | `	sxi32 iExprOp;` |
|   4097645 | 1289 | `	if( pNode->pOp == 0 ){` |
|   2906297 | 1290 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1291 | `	}` |
|   1191353 | 1292 | `	iExprOp = pNode->pOp->iOp;` |
|   1191353 | 1293 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    832109 | 1294 | `			return TRUE;` |
|         - | 1295 | `	}` |
|    359249 | 1296 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    359243 | 1297 | `		if( pNode->pLeft->pOp ) {` |
|    122222 | 1298 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|     51256 | 1299 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|       ! 0 | 1300 | `				return FALSE;` |
|         5 | 1301 | `			}` |
|    298132 | 1302 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1303 | `			return FALSE;` |
|         - | 1304 | `		}` |
|    359243 | 1305 | `		return TRUE;` |
|         - | 1306 | `	}` |
|         8 | 1307 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|         8 | 1308 | `		return TRUE;` |
|         - | 1309 | `	}` |
|         - | 1310 | `	/* Not a modifiable l or r-value */` |
|       ! 0 | 1311 | `	return FALSE;` |
|   2048825 | 1312 | `}` |
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
|   4155894 | 1324 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1325 | `{` |
|         - | 1326 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1327 | `	sxi32 rc;` |
|         - | 1328 | `	/* Process function arguments from left to right */` |
|   4155899 | 1329 | `	iCur = 0;` |
|   4991650 | 1330 | `	for(;;){` |
|   9983305 | 1331 | `		if( iCur >= nToken ){` |
|         - | 1332 | `			/* No more arguments to process */` |
|   4155873 | 1333 | `			break;` |
|         - | 1334 | `		}` |
|   5827437 | 1335 | `		iNode = iCur;` |
|   5827437 | 1336 | `		iNest = 0;` |
|  19491225 | 1337 | `		while( iCur < nToken ){` |
|  15335355 | 1338 | `			if( apNode[iCur] ){` |
|  15287897 | 1339 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    835786 | 1340 | `					break;` |
|  13616330 | 1341 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   7340086 | 1342 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|   1061344 | 1343 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1344 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1345 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1346 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1347 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1348 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1349 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|   1058841 | 1350 | `					iNest++;` |
|  13086917 | 1351 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|   1058841 | 1352 | `					iNest--;` |
|    529418 | 1353 | `				}` |
|   6808165 | 1354 | `			}` |
|  13663793 | 1355 | `			iCur++;` |
|         5 | 1356 | `		}` |
|   5827437 | 1357 | `		if( iCur > iNode ){` |
|   5827431 | 1358 | `			SyString sArgName = {0, 0};` |
|         - | 1359 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1360 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1361 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   5827426 | 1362 | `			if( (iCur - iNode) >= 2` |
|   4030929 | 1363 | `				&& apNode[iNode]` |
|   2234418 | 1364 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|   1185461 | 1365 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|    136264 | 1366 | `				&& apNode[iNode+1]` |
|    136015 | 1367 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
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
|   5827424 | 1388 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
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
|   5827429 | 1405 | `				int bSpreadArg = 0;` |
|         - | 1406 | `				sxi32 iScan;` |
|   5827457 | 1407 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   5827457 | 1408 | `					if( apNode[iScan] ){` |
|   5827429 | 1409 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   5827429 | 1410 | `						break;` |
|         - | 1411 | `					}` |
|        15 | 1412 | `				}` |
|   5827429 | 1413 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   5827429 | 1414 | `				if( bSpreadArg && apNode[iNode] ){` |
|      4129 | 1415 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|      2062 | 1416 | `				}` |
|         - | 1417 | `			}` |
|   5827429 | 1418 | `			if( apNode[iNode] ){` |
|   5827429 | 1419 | `				if( sArgName.nByte > 0 ){` |
|       289 | 1420 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       289 | 1421 | `					apNode[iNode]->sArgName = sArgName;` |
|       142 | 1422 | `				}` |
|         - | 1423 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   5827429 | 1424 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   2913717 | 1425 | `			}else{` |
|         - | 1426 | `				/* No expression before comma */` |
|       ! 0 | 1427 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1428 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1429 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1430 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1431 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1432 | `				}` |
|       ! 0 | 1433 | `				return rc;` |
|         - | 1434 | `			}` |
|   2913717 | 1435 | `		}else{` |
|         - | 1436 | `			/* Comma with no preceding argument */` |
|         8 | 1437 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1438 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1439 | `				rc = SXERR_SYNTAX;` |
|         3 | 1440 | `			}` |
|         8 | 1441 | `			return rc;` |
|         - | 1442 | `		}` |
|         - | 1443 | `		/* Jump trailing comma */` |
|   5827429 | 1444 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|   1671561 | 1445 | `			iCur++;` |
|   1671561 | 1446 | `			if( iCur >= nToken ){` |
|         - | 1447 | `				/* Trailing comma after last argument */` |
|        19 | 1448 | `				break;` |
|         - | 1449 | `			}` |
|    835769 | 1450 | `		}` |
|         5 | 1451 | `	}` |
|   4155891 | 1452 | `	return SXRET_OK;` |
|   2077952 | 1453 | `}` |
|         - | 1454 | ` /*` |
|         - | 1455 | `  * Create an expression tree from an array of tokens.` |
|         - | 1456 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1457 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1458 | `  */` |
|  22508670 | 1459 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1460 | ` {` |
|         - | 1461 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1462 | `	 ph7_expr_node *pNode;` |
|         - | 1463 | `	 ph7_expr_node *pSuppress;` |
|         - | 1464 | `	 sxi32 iCur;` |
|         - | 1465 | `	 sxi32 rc;` |
|  22508675 | 1466 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1467 | `		 /* TICKET 1433-17: self evaluating node */` |
|   9779193 | 1468 | `		 return SXRET_OK;` |
|         - | 1469 | `	 }` |
|         - | 1470 | `	 /* Process expressions enclosed in parenthesis first */` |
|  91808159 | 1471 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1472 | `		 sxi32 iNest;` |
|         - | 1473 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1474 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 1475 | `		  */` |
|  79078679 | 1476 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  78745599 | 1477 | `			 continue;` |
|         - | 1478 | `		 }` |
|    333085 | 1479 | `		 iNest = 1;` |
|    333085 | 1480 | `		 iLeft = iCur;` |
|         - | 1481 | `		 /* Find the closing parenthesis */` |
|    333085 | 1482 | `		 iCur++;` |
|   2958425 | 1483 | `		 while( iCur < nToken ){` |
|   2958425 | 1484 | `			 if( apNode[iCur] ){` |
|   2958425 | 1485 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 1486 | `					 /* Decrement nesting level */` |
|    464287 | 1487 | `					 iNest--;` |
|    464287 | 1488 | `					 if( iNest <= 0 ){` |
|    333085 | 1489 | `						 break;` |
|         5 | 1490 | `					 }` |
|   2559744 | 1491 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 1492 | `					 /* Increment nesting level */` |
|    131207 | 1493 | `					 iNest++;` |
|     65601 | 1494 | `				 }` |
|   1312670 | 1495 | `			 }` |
|   2625345 | 1496 | `			 iCur++;` |
|         5 | 1497 | `		 }` |
|    333085 | 1498 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1499 | `			 sxi32 j;` |
|         - | 1500 | `			 /* Recurse and process this expression */` |
|    333085 | 1501 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    333085 | 1502 | `			 if( rc != SXRET_OK ){` |
|         3 | 1503 | `				 return rc;` |
|         - | 1504 | `			 }` |
|         - | 1505 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 1506 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 1507 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 1508 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 1509 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 1510 | `			  * group's free below silently drops the unpacking. */` |
|    333083 | 1511 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    333083 | 1512 | `				 if( apNode[j] ){` |
|    333083 | 1513 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    333078 | 1514 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    333083 | 1515 | `					 break;` |
|         - | 1516 | `				 }` |
|       ! 0 | 1517 | `			 }` |
|    166539 | 1518 | `		 }` |
|         - | 1519 | `		 /* Free the left and right nodes */` |
|    333083 | 1520 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    333083 | 1521 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    333083 | 1522 | `		 apNode[iLeft] = 0;` |
|    333083 | 1523 | `		 apNode[iCur] = 0;` |
|    166544 | 1524 | `	 }` |
|         - | 1525 | `	  /* Process expressions enclosed in braces */` |
|  94513609 | 1526 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 1527 | `		 sxi32 iNest;` |
|         - | 1528 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 1529 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 1530 | `		  */` |
|  82029193 | 1531 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  82025245 | 1532 | `			 continue;` |
|         - | 1533 | `		 }` |
|      3953 | 1534 | `		 iNest = 1;` |
|      3953 | 1535 | `		 iLeft = iCur;` |
|         - | 1536 | `		 /* Find the closing parenthesis */` |
|      3953 | 1537 | `		 iCur++;` |
|      7899 | 1538 | `		 while( iCur < nToken ){` |
|      7899 | 1539 | `			 if( apNode[iCur] ){` |
|      7899 | 1540 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 1541 | `					 /* Decrement nesting level */` |
|      3953 | 1542 | `					 iNest--;` |
|      3953 | 1543 | `					 if( iNest <= 0 ){` |
|      3953 | 1544 | `						 break;` |
|       ! 0 | 1545 | `					 }` |
|      3951 | 1546 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 1547 | `					 /* Increment nesting level */` |
|       ! 0 | 1548 | `					 iNest++;` |
|       ! 0 | 1549 | `				 }` |
|      1973 | 1550 | `			 }` |
|      3951 | 1551 | `			 iCur++;` |
|         5 | 1552 | `		 }` |
|      3953 | 1553 | `		 if( iCur - iLeft > 1 ){` |
|         - | 1554 | `			 /* Recurse and process this expression */` |
|      3951 | 1555 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|      3951 | 1556 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1557 | `				 return rc;` |
|         - | 1558 | `			 }` |
|      1973 | 1559 | `		 }` |
|         - | 1560 | `		 /* Free the left and right nodes */` |
|      3953 | 1561 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|      3953 | 1562 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|      3953 | 1563 | `		 apNode[iLeft] = 0;` |
|      3953 | 1564 | `		 apNode[iCur] = 0;` |
|      1979 | 1565 | `	 }` |
|         - | 1566 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|  12484421 | 1567 | `	 iLeft = -1;` |
|  94521467 | 1568 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  82037063 | 1569 | `		 if( apNode[iCur] == 0 ){` |
|  35976671 | 1570 | `			 continue;` |
|         - | 1571 | `		 }` |
|  46060397 | 1572 | `		 pNode = apNode[iCur];` |
|  46060397 | 1573 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|  13226653 | 1574 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1575 | `				 /* Collect function arguments */` |
|   5592635 | 1576 | `				 sxi32 iPtr = 0;` |
|   5592635 | 1577 | `				 sxi32 nFuncTok = 0;` |
|  26520617 | 1578 | `				 while( nFuncTok + iCur < nToken ){` |
|  26520617 | 1579 | `					 if( apNode[nFuncTok+iCur] ){` |
|  26473159 | 1580 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   5914181 | 1581 | `							 iPtr++;` |
|  23516071 | 1582 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   5914181 | 1583 | `							 iPtr--;` |
|   5914181 | 1584 | `							 if( iPtr <= 0 ){` |
|   5592635 | 1585 | `								 break;` |
|         - | 1586 | `							 }` |
|    160773 | 1587 | `						 }` |
|  10440262 | 1588 | `					 }` |
|  20927987 | 1589 | `					 nFuncTok++;` |
|         5 | 1590 | `				 }` |
|   5592635 | 1591 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 1592 | `					 /* Syntax error */` |
|       ! 0 | 1593 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 1594 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1595 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1596 | `					 }` |
|       ! 0 | 1597 | `					 return rc;` |
|         - | 1598 | `				 }` |
|   5592635 | 1599 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 1600 | `					 /* Syntax error */` |
|       ! 0 | 1601 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 1602 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1603 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1604 | `					 }` |
|       ! 0 | 1605 | `					 return rc;` |
|         - | 1606 | `				 }` |
|   5592635 | 1607 | `				 if( nFuncTok > 1 ){` |
|         - | 1608 | `					 /* Process function arguments */` |
|   4155899 | 1609 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   4155899 | 1610 | `					 if( rc != SXRET_OK ){` |
|        10 | 1611 | `						 return rc;` |
|         - | 1612 | `					 }` |
|   2077943 | 1613 | `				 }` |
|         - | 1614 | `				 /* Link the node to the tree */` |
|   5592627 | 1615 | `				 pNode->pLeft = apNode[iLeft];` |
|   5592627 | 1616 | `				 apNode[iLeft] = 0;` |
|  26520585 | 1617 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  20927963 | 1618 | `					 apNode[iCur+iPtr] = 0;` |
|  10463984 | 1619 | `				 }` |
|         - | 1620 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 1621 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 1622 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 1623 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 1624 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 1625 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 1626 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 1627 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 1628 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 1629 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 1630 | `				 {` |
|   5592627 | 1631 | `					 sxi32 iNew = iLeft - 1;` |
|   7470389 | 1632 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|   1877767 | 1633 | `						 iNew--;` |
|         5 | 1634 | `					 }` |
|   5592622 | 1635 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   3317990 | 1636 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   2022755 | 1637 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    735409 | 1638 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    735409 | 1639 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    735409 | 1640 | `						 apNode[iNew] = 0;` |
|    735409 | 1641 | `						 pNode = apNode[iCur];` |
|    367707 | 1642 | `					 }` |
|         - | 1643 | `				 }` |
|  10430334 | 1644 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 1645 | `				 /* Subscripting */` |
|   2591697 | 1646 | `				 sxi32 iArrTok = iCur + 1;` |
|   2591697 | 1647 | `				 sxi32 iNest = 1;` |
|   2591692 | 1648 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        18 | 1649 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        14 | 1650 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|        14 | 1651 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|   2591692 | 1652 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 1653 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 1654 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|    309539 | 1655 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|         - | 1656 | `						 /* Syntax error */` |
|       ! 0 | 1657 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 1658 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1659 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1660 | `						 }` |
|       ! 0 | 1661 | `						 return rc;` |
|         - | 1662 | `				 }` |
|         - | 1663 | `				 /* Collect index tokens */` |
|   5478819 | 1664 | `				 while( iArrTok < nToken ){` |
|   5478819 | 1665 | `					 if( apNode[iArrTok] ){` |
|   5478787 | 1666 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 1667 | `							 /* Increment nesting level */` |
|     19705 | 1668 | `							 iNest++;` |
|   5468937 | 1669 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 1670 | `							 /* Decrement nesting level */` |
|   2611397 | 1671 | `							 iNest--;` |
|   2611397 | 1672 | `							 if( iNest <= 0 ){` |
|   2591697 | 1673 | `								 break;` |
|         - | 1674 | `							 }` |
|      9850 | 1675 | `						 }` |
|   1443545 | 1676 | `					 }` |
|   2887127 | 1677 | `					 ++iArrTok;` |
|         5 | 1678 | `				 }` |
|   2591697 | 1679 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 1680 | `					 /* Recurse and process this expression */` |
|   2429941 | 1681 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|   2429941 | 1682 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 1683 | `						 return rc;` |
|         - | 1684 | `					 }` |
|         - | 1685 | `					 /* Link the node to it's index */` |
|   2429941 | 1686 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|   1214968 | 1687 | `				 }` |
|         - | 1688 | `				 /* Link the node to the tree */` |
|   2591697 | 1689 | `				 pNode->pLeft = apNode[iLeft];` |
|   2591697 | 1690 | `				 pNode->pRight = 0;` |
|   2591697 | 1691 | `				 apNode[iLeft] = 0;` |
|   8070511 | 1692 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   5478819 | 1693 | `					 apNode[iNest] = 0;` |
|   2739412 | 1694 | `				 }` |
|   1295851 | 1695 | `			 }else{` |
|         - | 1696 | `				 /* Member access operators [i.e: '->','::'] */` |
|   5042331 | 1697 | `				  iRight = iCur + 1;` |
|   5046277 | 1698 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|      3951 | 1699 | `					 iRight++;` |
|         5 | 1700 | `				 }` |
|   5042331 | 1701 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1702 | `					 /* Syntax error */` |
|         5 | 1703 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 1704 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 1705 | `						 rc = SXERR_SYNTAX;` |
|         2 | 1706 | `					 }` |
|         5 | 1707 | `					 return rc;` |
|         - | 1708 | `				 }` |
|         - | 1709 | `				 /* Link the node to the tree */` |
|   5042327 | 1710 | `				 pNode->pLeft = apNode[iLeft];` |
|   5042322 | 1711 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   4856701 | 1712 | `					 && pNode->pLeft->pOp == 0 &&` |
|   4591739 | 1713 | `					 pNode->pLeft->xCode != PH7_CompileVariable &&` |
|         - | 1714 | `					 /* A clone(...) call term (pOp==0, xCode set) produces an object,` |
|         - | 1715 | ``					  * so `(clone($o))->x` is a valid arrow left operand — like the`` |
|         - | 1716 | ``					  * `clone $o` operator form (pOp!=0), which this guard already`` |
|         - | 1717 | `					  * accepts. */` |
|         4 | 1718 | `					 pNode->pLeft->xCode != PH7_CompileCloneCall ){` |
|         - | 1719 | `						 /* Syntax error */` |
|       ! 0 | 1720 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1721 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|       ! 0 | 1722 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1723 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1724 | `						 }` |
|       ! 0 | 1725 | `						 return rc;` |
|         - | 1726 | `				 }` |
|   5042327 | 1727 | `				 pNode->pRight = apNode[iRight];` |
|   5042327 | 1728 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 1729 | `			 }` |
|   6613318 | 1730 | `		 }` |
|  46060385 | 1731 | `		 iLeft = iCur;` |
|  23030195 | 1732 | `	 }` |
|         - | 1733 | `	 /* Handle left associative (new, clone) operators */` |
|  94521435 | 1734 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  82037031 | 1735 | `		 if( apNode[iCur] == 0 ){` |
|  49994235 | 1736 | `			 continue;` |
|         - | 1737 | `		 }` |
|  32042801 | 1738 | `		 pNode = apNode[iCur];` |
|  32042801 | 1739 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 1740 | `			 SyToken *pToken;` |
|         - | 1741 | `			 /* Get the left node */` |
|     55529 | 1742 | `			 iLeft = iCur + 1;` |
|     55537 | 1743 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|         9 | 1744 | `				 iLeft++;` |
|         1 | 1745 | `			 }` |
|     55529 | 1746 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1747 | `				  /* Syntax error */` |
|       ! 0 | 1748 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 1749 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 1750 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1751 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1752 | `				 }` |
|       ! 0 | 1753 | `				 return rc;` |
|         - | 1754 | `			 }` |
|         - | 1755 | `			 /* Make sure the operand are of a valid type */` |
|     55529 | 1756 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1757 | `				 /* Clone:` |
|         - | 1758 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|         - | 1759 | `				  *  ++ function call (including annonymous)` |
|         - | 1760 | `				  *  ++ array member` |
|         - | 1761 | `				  *  ++ 'new' operator` |
|         - | 1762 | `				  * Example:` |
|         - | 1763 | `				  *   clone $pObj;` |
|         - | 1764 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|         - | 1765 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|         - | 1766 | `				  */` |
|     55205 | 1767 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|     55199 | 1768 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|       ! 0 | 1769 | `						 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1770 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|       ! 0 | 1771 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1772 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1773 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1774 | `						 }` |
|       ! 0 | 1775 | `						 return rc;` |
|         - | 1776 | `					 }` |
|     27597 | 1777 | `				 }` |
|     27605 | 1778 | `			 }else{` |
|         - | 1779 | `				 /* New */` |
|       324 | 1780 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         5 | 1781 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 1782 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 1783 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 1784 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 1785 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 1786 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 1787 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 1788 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 1789 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1790 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1791 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1792 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1793 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1794 | `					 }` |
|       ! 0 | 1795 | `					 return rc;` |
|         - | 1796 | `				 }` |
|       329 | 1797 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       329 | 1798 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|       324 | 1799 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|        33 | 1800 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 1801 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 1802 | `						 /* Syntax error */` |
|       ! 0 | 1803 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1804 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 1805 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 1806 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1807 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 1808 | `						 }` |
|       ! 0 | 1809 | `						 return rc;` |
|         - | 1810 | `					 }` |
|       162 | 1811 | `				 }` |
|         - | 1812 | `			 }` |
|         - | 1813 | `			  /* Link the node to the tree */` |
|     55529 | 1814 | `			 pNode->pLeft = apNode[iLeft];` |
|     55529 | 1815 | `			 apNode[iLeft] = 0;` |
|     55529 | 1816 | `			 pNode->pRight = 0; /* Paranoid */` |
|     27762 | 1817 | `		 }` |
|  16021403 | 1818 | `	 }` |
|         - | 1819 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|  12484409 | 1820 | `	 iLeft = -1;` |
|  94643967 | 1821 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  82037031 | 1822 | `		 if( apNode[iCur] == 0 ){` |
|  49994235 | 1823 | `			 continue;` |
|         - | 1824 | `		 }` |
|  32042801 | 1825 | `		 pNode = apNode[iCur];` |
|  32042801 | 1826 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    150221 | 1827 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|    130463 | 1828 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 1829 | `					 /* Link the node to the tree */` |
|    138369 | 1830 | `					 pNode->pLeft = apNode[iLeft];` |
|    138369 | 1831 | `					 apNode[iLeft] = 0;` |
|     69182 | 1832 | `			 }` |
|    197640 | 1833 | `		  }` |
|  32165333 | 1834 | `		 iLeft = iCur;` |
|  16143935 | 1835 | `	  }` |
|  12606941 | 1836 | `	 iLeft = -1;` |
|  94643967 | 1837 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  82037031 | 1838 | `		 if( apNode[iCur] == 0 ){` |
|  50132599 | 1839 | `			 continue;` |
|         - | 1840 | `		 }` |
|  31904437 | 1841 | `		 pNode = apNode[iCur];` |
|  31904437 | 1842 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     11852 | 1843 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|     11857 | 1844 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|         - | 1845 | `					 /* Syntax error */` |
|       ! 0 | 1846 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|       ! 0 | 1847 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1848 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 1849 | `					 }` |
|       ! 0 | 1850 | `					 return rc;` |
|         - | 1851 | `			 }` |
|         - | 1852 | `			 /* Link the node to the tree */` |
|     11857 | 1853 | `			 pNode->pLeft = apNode[iLeft];` |
|     11857 | 1854 | `			 apNode[iLeft] = 0;` |
|         - | 1855 | `			 /* Mark as pre-increment/decrement node */` |
|     11857 | 1856 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      5926 | 1857 | `		  }` |
|  31904437 | 1858 | `		 iLeft = iCur;` |
|  15952221 | 1859 | `	 }` |
|         - | 1860 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|  12606941 | 1861 | `	  iLeft = 0;` |
|  94643961 | 1862 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  82037027 | 1863 | `		  if( apNode[iCur] ){` |
|  31892581 | 1864 | `			  pNode = apNode[iCur];` |
|  31892581 | 1865 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    856881 | 1866 | `				  if( iLeft > 0 ){` |
|         - | 1867 | `					  /* Link the node to the tree */` |
|    856879 | 1868 | `					  pNode->pLeft = apNode[iLeft];` |
|    856879 | 1869 | `					  apNode[iLeft] = 0;` |
|    856879 | 1870 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|     55253 | 1871 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|         - | 1872 | `							   /* Syntax error */` |
|       ! 0 | 1873 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|       ! 0 | 1874 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 1875 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 1876 | `							  }` |
|       ! 0 | 1877 | `							  return rc;` |
|         - | 1878 | `						  }` |
|     27624 | 1879 | `					  }` |
|    428442 | 1880 | `				  }else{` |
|         - | 1881 | `					  /* Syntax error */` |
|         3 | 1882 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|         3 | 1883 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 1884 | `						  rc = SXERR_SYNTAX;` |
|         1 | 1885 | `					  }` |
|         3 | 1886 | `					  return rc;` |
|         - | 1887 | `				  }` |
|    428437 | 1888 | `			  }` |
|         - | 1889 | `			  /* Save terminal position */` |
|  31892579 | 1890 | `			  iLeft = iCur;` |
|  15946287 | 1891 | `		  }` |
|  41018515 | 1892 | `	  }` |
|         - | 1893 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 1894 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 1895 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 1896 | `	  * yielding a right-leaning tree. */` |
|  94643959 | 1897 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  82037025 | 1898 | `		 if( apNode[iCur] == 0 ){` |
|  51001437 | 1899 | `			 continue;` |
|         - | 1900 | `		 }` |
|  31035593 | 1901 | `		 pNode = apNode[iCur];` |
|  31035593 | 1902 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 1903 | `			 sxi32 iL, iR;` |
|         - | 1904 | `			 /* Find the right operand */` |
|       113 | 1905 | `			 iR = -1;` |
|         - | 1906 | `			 {` |
|         - | 1907 | `				 sxi32 j;` |
|       125 | 1908 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|       125 | 1909 | `					 if( apNode[j] ){ iR = j; break; }` |
|         7 | 1910 | `				 }` |
|         - | 1911 | `			 }` |
|         - | 1912 | `			 /* Find the left operand */` |
|       113 | 1913 | `			 iL = -1;` |
|         - | 1914 | `			 {` |
|         - | 1915 | `				 sxi32 j;` |
|       181 | 1916 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|       181 | 1917 | `					 if( apNode[j] ){ iL = j; break; }` |
|        35 | 1918 | `				 }` |
|         - | 1919 | `			 }` |
|       113 | 1920 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 1921 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|       ! 0 | 1922 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 1923 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 1924 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 1925 | `				 }` |
|       ! 0 | 1926 | `				 return rc;` |
|         - | 1927 | `			 }` |
|       113 | 1928 | `			 pNode->pLeft  = apNode[iL];` |
|       113 | 1929 | `			 pNode->pRight = apNode[iR];` |
|       113 | 1930 | `			 apNode[iL] = 0;` |
|       113 | 1931 | `			 apNode[iR] = 0;` |
|         - | 1932 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 1933 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 1934 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 1935 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 1936 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 1937 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 1938 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 1939 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 1940 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 1941 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 1942 | `			  * operands are respected. */` |
|       112 | 1943 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|        74 | 1944 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        35 | 1945 | `				 && pNode->pLeft->pLeft != 0` |
|        35 | 1946 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 1947 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 1948 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 1949 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 1950 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 1951 | `				 while( pTail->pLeft` |
|        34 | 1952 | `					 && pTail->pLeft->pOp` |
|        23 | 1953 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 1954 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 1955 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 1956 | `					 pTail = pTail->pLeft;` |
|         1 | 1957 | `				 }` |
|         - | 1958 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 1959 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 1960 | `				 pTail->pLeft = pNode;` |
|        27 | 1961 | `				 apNode[iCur] = pHead;` |
|        13 | 1962 | `			 }` |
|        56 | 1963 | `		 }` |
|  15517799 | 1964 | `	 }` |
|         - | 1965 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
| 138676193 | 1966 | `	 for( i = 7 ; i < 17 ; i++ ){` |
| 126069269 | 1967 | `		 iLeft = -1;` |
| 946439175 | 1968 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 820369921 | 1969 | `			 if( apNode[iCur] == 0 ){` |
| 563465943 | 1970 | `				 continue;` |
|         - | 1971 | `			 }` |
| 256903983 | 1972 | `			 pNode = apNode[iCur];` |
| 256903983 | 1973 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 1974 | `				 /* Get the right node */` |
|   4355577 | 1975 | `				 iRight = iCur + 1;` |
|   6782639 | 1976 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   2427067 | 1977 | `					 iRight++;` |
|         5 | 1978 | `				 }` |
|   4355577 | 1979 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 1980 | `					 /* Syntax error */` |
|        11 | 1981 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        11 | 1982 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 1983 | `						 rc = SXERR_SYNTAX;` |
|         4 | 1984 | `					 }` |
|        11 | 1985 | `					 return rc;` |
|         - | 1986 | `				 }` |
|   4355569 | 1987 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1988 | `					 sxi32  iTmp;` |
|         - | 1989 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 1990 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|         - | 1991 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|         - | 1992 | `					  * right operand first since EXPR_OP_REF's operand order` |
|         - | 1993 | `					  * is swapped below. */` |
|        65 | 1994 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         3 | 1995 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 1996 | `							 "Can't use nullsafe operator in write context");` |
|         3 | 1997 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1998 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1999 | `						 }` |
|         3 | 2000 | `						 return rc;` |
|         - | 2001 | `					 }` |
|        62 | 2002 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|         - | 2003 | `						 /* Left operand must be a modifiable l-value */` |
|       ! 0 | 2004 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|       ! 0 | 2005 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2006 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2007 | `						 }` |
|       ! 0 | 2008 | `						 return rc;` |
|         - | 2009 | `					 }` |
|        62 | 2010 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|        44 | 2011 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|       ! 0 | 2012 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|       ! 0 | 2013 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|       ! 0 | 2014 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2015 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|       ! 0 | 2016 | `									 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2017 | `										 rc = SXERR_SYNTAX;` |
|       ! 0 | 2018 | `									 }` |
|       ! 0 | 2019 | `									 return rc;` |
|         - | 2020 | `							 }` |
|       ! 0 | 2021 | `						 }` |
|        21 | 2022 | `					 }` |
|         - | 2023 | `					 /* Swap operands */` |
|        62 | 2024 | `					 iTmp = iRight;` |
|        62 | 2025 | `					 iRight = iLeft;` |
|        62 | 2026 | `					 iLeft = iTmp;` |
|        30 | 2027 | `				 }` |
|         - | 2028 | `				 /* Link the node to the tree */` |
|   4355567 | 2029 | `				 pNode->pLeft = apNode[iLeft];` |
|   4355567 | 2030 | `				 pNode->pRight = apNode[iRight];` |
|   4355567 | 2031 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   2177781 | 2032 | `			 }` |
| 256903973 | 2033 | `			 iLeft = iCur;` |
| 128451989 | 2034 | `		 }` |
|  63034632 | 2035 | `	 }` |
|         - | 2036 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2037 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2038 | `	  * we are dealing with a single operator.` |
|         - | 2039 | `	  */` |
|  12606929 | 2040 | `	  iLeft = -1;` |
|  91560511 | 2041 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  79335119 | 2042 | `		  if( apNode[iCur] == 0 ){` |
|  58151407 | 2043 | `			  continue;` |
|         - | 2044 | `		  }` |
|  21183717 | 2045 | `		  pNode = apNode[iCur];` |
|  21183717 | 2046 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|    381537 | 2047 | `			  sxi32 iNest = 1;` |
|    381537 | 2048 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2049 | `				  /* Missing condition */` |
|         3 | 2050 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|         3 | 2051 | `				  if( rc != SXERR_ABORT ){` |
|         3 | 2052 | `					  rc = SXERR_SYNTAX;` |
|         1 | 2053 | `				  }` |
|         3 | 2054 | `				  return rc;` |
|         - | 2055 | `			  }` |
|         - | 2056 | `			  /* Get the right node */` |
|    381535 | 2057 | `			  iRight = iCur + 1;` |
|   1496207 | 2058 | `			  while( iRight < nToken  ){` |
|   1496207 | 2059 | `				  if( apNode[iRight] ){` |
|    759057 | 2060 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2061 | `						  /* Increment nesting level */` |
|       ! 0 | 2062 | `						  ++iNest;` |
|    759057 | 2063 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2064 | `						  /* Decrement nesting level */` |
|    381535 | 2065 | `						  --iNest;` |
|    381535 | 2066 | `						  if( iNest <= 0 ){` |
|    381535 | 2067 | `							  break;` |
|         - | 2068 | `						  }` |
|       ! 0 | 2069 | `					  }` |
|    188761 | 2070 | `				  }` |
|   1114677 | 2071 | `				  iRight++;` |
|         5 | 2072 | `			  }` |
|    381535 | 2073 | `			  if( iRight > iCur + 1 ){` |
|         - | 2074 | `				  /* Recurse and process the then expression */` |
|    377527 | 2075 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|    377527 | 2076 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2077 | `					  return rc;` |
|         - | 2078 | `				  }` |
|         - | 2079 | `				  /* Link the node to the tree */` |
|    377527 | 2080 | `				  pNode->pLeft = apNode[iCur + 1];` |
|    188761 | 2081 | `			  }else{` |
|         - | 2082 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2083 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2084 | `			  }` |
|    381535 | 2085 | `			  apNode[iCur + 1] = 0;` |
|    381535 | 2086 | `			  if( iRight + 1 < nToken ){` |
|         - | 2087 | `				  /* Recurse and process the else expression */` |
|    381535 | 2088 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|    381535 | 2089 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2090 | `					  return rc;` |
|         - | 2091 | `				  }` |
|         - | 2092 | `				  /* Link the node to the tree */` |
|    381535 | 2093 | `				  pNode->pRight = apNode[iRight + 1];` |
|    381535 | 2094 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|    190770 | 2095 | `			  }else{` |
|       ! 0 | 2096 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2097 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2098 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2099 | `				 }` |
|       ! 0 | 2100 | `				 return rc;` |
|         - | 2101 | `			  }` |
|         - | 2102 | `			  /* Point to the condition */` |
|    381535 | 2103 | `			  pNode->pCond  = apNode[iLeft];` |
|    381535 | 2104 | `			  apNode[iLeft] = 0;` |
|    381535 | 2105 | `			  break;` |
|         - | 2106 | `		  }` |
|  20802185 | 2107 | `		  iLeft = iCur;` |
|  10401095 | 2108 | `	  }` |
|         - | 2109 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2110 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2111 | `	  * so there is no need for a precedence loop here.` |
|         - | 2112 | `	  */` |
|  12606927 | 2113 | `	 iRight = -1;` |
|  94643763 | 2114 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  82036895 | 2115 | `		 if( apNode[iCur] == 0 ){` |
|  65332307 | 2116 | `			 continue;` |
|         - | 2117 | `		 }` |
|  16704593 | 2118 | `		 pNode = apNode[iCur];` |
|  16704593 | 2119 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2120 | `			 /* Get the left node */` |
|   4097593 | 2121 | `			 iLeft = iCur - 1;` |
|   5616515 | 2122 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   1518927 | 2123 | `				 iLeft--;` |
|         5 | 2124 | `			 }` |
|   4097593 | 2125 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2126 | `				 /* Syntax error */` |
|        44 | 2127 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2128 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2129 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2130 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2131 | `				 }else{` |
|        40 | 2132 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|         - | 2133 | `				 }` |
|        44 | 2134 | `				 if( rc != SXERR_ABORT ){` |
|        42 | 2135 | `					 rc = SXERR_SYNTAX;` |
|        20 | 2136 | `				 }` |
|        44 | 2137 | `				 return rc;` |
|         - | 2138 | `			 }` |
|         - | 2139 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2140 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2141 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2142 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2143 | `			  * a write. */` |
|   4097551 | 2144 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        11 | 2145 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         - | 2146 | `					 "Can't use nullsafe operator in write context");` |
|        11 | 2147 | `				 if( rc != SXERR_ABORT ){` |
|        11 | 2148 | `					 rc = SXERR_SYNTAX;` |
|         4 | 2149 | `				 }` |
|        11 | 2150 | `				 return rc;` |
|         - | 2151 | `			 }` |
|         - | 2152 | ``			 /* php parses `@$x = expr` as `@($x = expr)` — the suppression covers the`` |
|         - | 2153 | `			  * whole assignment, not just its target. The unary phase already bound '@'` |
|         - | 2154 | `			  * to the LHS, which left the assignment staring at a non-lvalue, so detach` |
|         - | 2155 | `			  * it here, let the assignment bind to the real target, and re-wrap below.` |
|         - | 2156 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|   4097543 | 2157 | `			 pSuppress = 0;` |
|   4097538 | 2158 | `			 if( apNode[iLeft]->pOp` |
|   2644429 | 2159 | `				 && apNode[iLeft]->pOp->iVmOp == PH7_OP_ERR_CTRL` |
|    595660 | 2160 | `				 && apNode[iLeft]->pLeft != 0` |
|         5 | 2161 | `				 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2162 | `				 pSuppress = apNode[iLeft];` |
|       ! 0 | 2163 | `				 apNode[iLeft] = pSuppress->pLeft;` |
|       ! 0 | 2164 | `			 }` |
|   4097543 | 2165 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|       123 | 2166 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|        88 | 2167 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2168 | `					 /* Left operand must be a modifiable l-value */` |
|         6 | 2169 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2170 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|         4 | 2171 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         2 | 2172 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         2 | 2173 | `					 }else{` |
|         4 | 2174 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2175 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|         - | 2176 | `					 }` |
|         6 | 2177 | `					 if( rc != SXERR_ABORT ){` |
|         6 | 2178 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2179 | `					 }` |
|         6 | 2180 | `					 return rc;` |
|         - | 2181 | `				 }` |
|        43 | 2182 | `			 }` |
|         - | 2183 | `			 /* Link the node to the tree (Reverse) */` |
|   4097539 | 2184 | `			 pNode->pLeft = apNode[iRight];` |
|   4097539 | 2185 | `			 pNode->pRight = apNode[iLeft];` |
|   4097539 | 2186 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   4097539 | 2187 | `			 if( pSuppress ){` |
|         - | 2188 | `				 /* Re-wrap: the '@' now suppresses the whole assignment */` |
|       ! 0 | 2189 | `				 pSuppress->pLeft = pNode;` |
|       ! 0 | 2190 | `				 apNode[iCur] = pSuppress;` |
|       ! 0 | 2191 | `			 }` |
|   2048767 | 2192 | `		 }` |
|  16704539 | 2193 | `		 iRight = iCur;` |
|   8352272 | 2194 | `	 }` |
|         - | 2195 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  63034345 | 2196 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  50427477 | 2197 | `		 iLeft = -1;` |
| 378574765 | 2198 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 328147293 | 2199 | `			 if( apNode[iCur] == 0 ){` |
| 277719569 | 2200 | `				 continue;` |
|         - | 2201 | `			 }` |
|  50427729 | 2202 | `			 pNode = apNode[iCur];` |
|  50427729 | 2203 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2204 | `				 /* Get the right node */` |
|        51 | 2205 | `				 iRight = iCur + 1;` |
|        63 | 2206 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        13 | 2207 | `					 iRight++;` |
|         1 | 2208 | `				 }` |
|        51 | 2209 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2210 | `					 /* Syntax error */` |
|       ! 0 | 2211 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       ! 0 | 2212 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2213 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2214 | `					 }` |
|       ! 0 | 2215 | `					 return rc;` |
|         - | 2216 | `				 }` |
|         - | 2217 | `				 /* Link the node to the tree */` |
|        51 | 2218 | `				 pNode->pLeft = apNode[iLeft];` |
|        51 | 2219 | `				 pNode->pRight = apNode[iRight];` |
|        51 | 2220 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        24 | 2221 | `			 }` |
|  50427729 | 2222 | `			 iLeft = iCur;` |
|  25213867 | 2223 | `		 }` |
|  25213741 | 2224 | `	 }` |
|         - | 2225 | `	 /* Point to the root of the expression tree */` |
|  82036799 | 2226 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  69429949 | 2227 | `		 if( apNode[iCur] ){` |
|  12109035 | 2228 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|        22 | 2229 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|        22 | 2230 | `				  if( rc != SXERR_ABORT ){` |
|        22 | 2231 | `					  rc = SXERR_SYNTAX;` |
|         9 | 2232 | `				  }` |
|        22 | 2233 | `				  return rc;` |
|         - | 2234 | `			 }` |
|  12109017 | 2235 | `			 apNode[0] = apNode[iCur];` |
|  12109017 | 2236 | `			 apNode[iCur] = 0;` |
|   6054506 | 2237 | `		 }` |
|  34714968 | 2238 | `	 }` |
|  12606855 | 2239 | `	 return SXRET_OK;` |
|  11193074 | 2240 | ` }` |
|         - | 2241 | ` /*` |
|         - | 2242 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 2243 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 2244 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2245 | `  * This is the public interface used by the most code generator routines.` |
|         - | 2246 | `  */` |
|  13032784 | 2247 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 2248 | `{` |
|         - | 2249 | `	ph7_expr_node **apNode;` |
|         - | 2250 | `	ph7_expr_node *pNode;` |
|         - | 2251 | `	sxi32 rc;` |
|         - | 2252 | `	/* Reset node container */` |
|  13032789 | 2253 | `	SySetReset(pExprNode);` |
|  13032789 | 2254 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 2255 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 2256 | `	{` |
|  13032789 | 2257 | `		int iLastWasTerm = 0;` |
|  13032789 | 2258 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  83349357 | 2259 | `		while( pGen->pIn < pGen->pEnd ){` |
|  70316607 | 2260 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  70316607 | 2261 | `			if( rc != SXRET_OK ){` |
|        38 | 2262 | `				return rc;` |
|         - | 2263 | `			}` |
|         - | 2264 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  70316573 | 2265 | `			if( pNode->xCode ){` |
|         - | 2266 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  35545147 | 2267 | `				iLastWasTerm = 1;` |
|  52544002 | 2268 | `			}else if( pNode->pOp ){` |
|         - | 2269 | `				/* Operator node */` |
|  19938523 | 2270 | `				iLastWasTerm = 0;` |
|   9969264 | 2271 | `			}else{` |
|         - | 2272 | `				/* Delimiter: ')' and ']' end terms */` |
|  14832913 | 2273 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 2274 | `			}` |
|         - | 2275 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 2276 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 2277 | `			 * node kind, so this single test covers all branches. */` |
|  70316573 | 2278 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 2279 | `			/* Save the extracted node */` |
|  70316573 | 2280 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 2281 | `		}` |
|         - | 2282 | `	}` |
|  13032755 | 2283 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 2284 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 2285 | `		*ppRoot = 0;` |
|       ! 0 | 2286 | `		return SXRET_OK;` |
|         - | 2287 | `	}` |
|  13032755 | 2288 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|         - | 2289 | `	/* Make sure we are dealing with valid nodes */` |
|  13032755 | 2290 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  13032755 | 2291 | `	if( rc != SXRET_OK ){` |
|         - | 2292 | `		/* Don't worry about freeing memory,upper layer will` |
|         - | 2293 | `		 * cleanup the mess left behind.` |
|         - | 2294 | `		 */` |
|        54 | 2295 | `		*ppRoot = 0;` |
|        54 | 2296 | `		return rc;` |
|         - | 2297 | `	}` |
|         - | 2298 | `	/* Build the tree */` |
|  13032705 | 2299 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  13032705 | 2300 | `	if( rc != SXRET_OK ){` |
|         - | 2301 | `		/* Something goes wrong [i.e: Syntax error] */` |
|       103 | 2302 | `		*ppRoot = 0;` |
|       103 | 2303 | `		return rc;` |
|         - | 2304 | `	}` |
|         - | 2305 | `	/* Point to the root of the tree */` |
|  13032607 | 2306 | `	*ppRoot = apNode[0];` |
|  13032607 | 2307 | `	return SXRET_OK;` |
|   6516397 | 2308 | `}` |
|         - | 2309 |  |
