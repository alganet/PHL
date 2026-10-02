# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1548/1705 lines (90.79%)

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
|         - |  179 | ``	/* php 8 REMOVED `(real)`, and removed it in the SCANNER: the cast token is still`` |
|         - |  180 | `	 * matched and then refused outright, with a sentence of its own. Kept as a row so` |
|         - |  181 | `	 * the token has an operator to hang on; the refusal is raised where the chunk is` |
|         - |  182 | `	 * tokenized, before any of it compiles. */` |
|         - |  183 | `	{ {"(real)",   sizeof("(real)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|         - |  184 | ``	/* php 8.5's `(void)`: not a conversion at all — it converts nothing and answers`` |
|         - |  185 | `	 * the operand — but it sits exactly where a cast sits and binds exactly as` |
|         - |  186 | ``	 * tightly. Only the codegen's `for`-clause pass ever hands a token this row`` |
|         - |  187 | ``	 * (GenStateEnableClauseVoidCasts): php's grammar takes `(void)` at the head of`` |
|         - |  188 | ``	 * an expression STATEMENT and at the head of each `for` clause element, and`` |
|         - |  189 | `	 * NOWHERE else, so the token is otherwise left unrecognized on purpose. */` |
|         - |  190 | `	{ {"(void)",   sizeof("(void)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_NOOP     },` |
|         - |  191 | `	                           /* Binary operators */` |
|         - |  192 | `	/* Precedence 5,right-associative: exponentiation (PHP 5.6) */` |
|         - |  193 | `	{ {"**",sizeof(char)*2}, EXPR_OP_POW, 5, EXPR_OP_ASSOC_RIGHT, PH7_OP_POW},` |
|         - |  194 | `	/* Precedence 7,left-associative */` |
|         - |  195 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|         - |  196 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|         - |  197 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|         - |  198 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|         - |  199 | `	/* Precedence 8,left-associative */` |
|         - |  200 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|         - |  201 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|         - |  202 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|         - |  203 | `	/* Precedence 9,left-associative */` |
|         - |  204 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|         - |  205 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|         - |  206 | ``	/* PHP 8.5 pipe operator: `$x \|> f(...)` desugars to `f($x)`. It binds`` |
|         - |  207 | `	 * looser than shift/arithmetic and tighter than comparison — PHP places it` |
|         - |  208 | `	 * between precedence 9 and 10. We share level 9 (left-associative) so the` |
|         - |  209 | `	 * generic binary tree-builder links it correctly; the actual codegen is` |
|         - |  210 | `	 * custom (a one-argument call of the RHS callable), handled in` |
|         - |  211 | `	 * GenStateEmitExprCode. iVmOp is 0 like the other codegen-only operators. */` |
|         - |  212 | `	{ {"\|>",sizeof(char)*2}, EXPR_OP_PIPE, 9, EXPR_OP_ASSOC_LEFT, 0},` |
|         - |  213 | `	/* Precedence 10,non-associative */` |
|         - |  214 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|         - |  215 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|         - |  216 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|         - |  217 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|         - |  218 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|         - |  219 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|         - |  220 | `	/* Precedence 11,non-associative */` |
|         - |  221 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|         - |  222 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|         - |  223 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|         - |  224 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|         - |  225 | `	/* Precedence 12,left-associative */` |
|         - |  226 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|         - |  227 | ``	/* Precedence 12,left-associative. php puts `=&` at ASSIGNMENT level, looser than`` |
|         - |  228 | `	 * every comparison; this table cannot say that without moving the operator out of` |
|         - |  229 | `	 * the pass that carries all of its own rules (the not-a-variable refusals, the` |
|         - |  230 | `	 * unary hoist, the nullsafe screens), so the one shape the difference is visible in` |
|         - |  231 | `	 * -- a comparison to its left -- is re-associated in that pass instead. See` |
|         - |  232 | ``	 * "php gives `=&` ASSIGNMENT precedence" in PH7_ExprMakeTree. */`` |
|         - |  233 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|         - |  234 | `	                         /* Binary operators */` |
|         - |  235 | `	/* Precedence 13,left-associative */` |
|         - |  236 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|         - |  237 | `	/* Precedence 14,left-associative */` |
|         - |  238 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|         - |  239 | `	/* Precedence 15,left-associative */` |
|         - |  240 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|         - |  241 | `	/* Precedence 16,left-associative */` |
|         - |  242 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|         - |  243 | `	                      /* Null coalescing operator */` |
|         - |  244 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|         - |  245 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|         - |  246 | `	                      /* Ternary operator */` |
|         - |  247 | `	/* Precedence 17,left-associative */` |
|         - |  248 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|         - |  249 | `	                     /* Combined binary operators */` |
|         - |  250 | `	/* Precedence 18,right-associative */` |
|         - |  251 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|         - |  252 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|         - |  253 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|         - |  254 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|         - |  255 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|         - |  256 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|         - |  257 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|         - |  258 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|         - |  259 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|         - |  260 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|         - |  261 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|         - |  262 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|         - |  263 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|         - |  264 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|         - |  265 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|         - |  266 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|         - |  267 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|         - |  268 | `	 * in this file: keep one of the question marks escaped. */` |
|         - |  269 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|         - |  270 | `	/* Precedence 19,left-associative */` |
|         - |  271 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|         - |  272 | `	/* Precedence 20,left-associative */` |
|         - |  273 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|         - |  274 | `	/* Precedence 21,left-associative */` |
|         - |  275 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|         - |  276 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|         - |  277 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|         - |  278 | `};` |
|         - |  279 | `/* Function call operator need special handling */` |
|         - |  280 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|         - |  281 | `/*` |
|         - |  282 | ` * Check if the given token is a potential operator or not.` |
|         - |  283 | ` * This function is called by the lexer each time it extract a token that may` |
|         - |  284 | ` * look like an operator.` |
|         - |  285 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|         - |  286 | ` * Otherwise NULL.` |
|         - |  287 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|         - |  288 | ` * a binary minus or unary minus.]` |
|         - |  289 | ` */` |
|   4177694 |  290 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  291 | `{` |
|   4177699 |  292 | `	sxu32 n = 0;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `	/* Do a linear lookup on the operators table */` |
|  85472322 |  295 | `	for(;;){` |
| 171240441 |  296 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  297 | `			break;` |
|         - |  298 | `		}` |
| 171240441 |  299 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  300 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  13895024 |  301 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   6935297 |  302 | `		}else{` |
| 157345422 |  303 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  304 | `		}` |
| 171240441 |  305 | `		if( rc == 0 ){` |
|   4291413 |  306 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  307 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   4127120 |  308 | `				return &aOpTable[n];` |
|         - |  309 | `			}` |
|         - |  310 | `			/* Handle ambiguity */` |
|    164298 |  311 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  312 | `				/* Unary opertors have prcedence here over binary operators */` |
|     25909 |  313 | `				return &aOpTable[n];` |
|         - |  314 | `			}` |
|    138394 |  315 | `			if( pLast->nType & PH7_TK_OP ){` |
|     24688 |  316 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  317 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     24688 |  318 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  319 | `					/* Unary opertors have prcedence here over binary operators */` |
|     24680 |  320 | `					return &aOpTable[n];` |
|         - |  321 | `				}` |
|         - |  322 |  |
|         4 |  323 | `			}` |
|     56776 |  324 | `		}` |
| 167062747 |  325 | `		++n; /* Next operator in the table */` |
|         5 |  326 | `	}` |
|         - |  327 | `	/* No such operator */` |
|       ! 0 |  328 | `	return 0;` |
|   2085438 |  329 | `}` |
|         - |  330 | `/*` |
|         - |  331 | ` * Delimit a set of token stream.` |
|         - |  332 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  333 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  334 | ` */` |
|   1119522 |  335 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  336 | `{` |
|   1119527 |  337 | `	SyToken *pCur = pIn;` |
|   1119527 |  338 | `	sxi32 iNest = 1;` |
|   4842327 |  339 | `	for(;;){` |
|   9699434 |  340 | `		if( pCur >= pEnd ){` |
|        23 |  341 | `			break;` |
|         - |  342 | `		}` |
|   9699414 |  343 | `		if( pCur->nType & nTokStart ){` |
|         - |  344 | `			/* Increment nesting level */` |
|    532383 |  345 | `			iNest++;` |
|   9432859 |  346 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  347 | `			/* Decrement nesting level */` |
|   1651885 |  348 | `			iNest--;` |
|   1651885 |  349 | `			if( iNest <= 0 ){` |
|   1119507 |  350 | `				break;` |
|         - |  351 | `			}` |
|    265823 |  352 | `		}` |
|         - |  353 | `		/* Advance cursor */` |
|   8579912 |  354 | `		pCur++;` |
|         5 |  355 | `	}` |
|         - |  356 | `	/* Point to the end of the chunk */` |
|   1119527 |  357 | `	*ppEnd = pCur;` |
|   1119527 |  358 | `}` |
|         - |  359 | `/*` |
|         - |  360 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  361 | ` * Note on reserved keywords.` |
|         - |  362 | ` *  According to the PHP language reference manual:` |
|         - |  363 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  364 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  365 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  366 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  367 | ` */` |
|     19698 |  368 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  369 | `{` |
|     19698 |  370 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     19579 |  371 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  372 | `		){` |
|       611 |  373 | `			return TRUE;` |
|         - |  374 | `	}` |
|     19097 |  375 | `	if( bCheckFunc ){` |
|       918 |  376 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       853 |  377 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       763 |  378 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       193 |  379 | `				return TRUE;` |
|         - |  380 | `		}` |
|       365 |  381 | `	}` |
|         - |  382 | `	/* Not a language construct */` |
|     18909 |  383 | `	return FALSE;` |
|      9843 |  384 | `}` |
|         - |  385 | `/*` |
|         - |  386 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  387 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  388 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  389 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  390 | ` */` |
|   2730165 |  391 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|         5 |  392 | `{` |
|         - |  393 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|         - |  394 | `	/* The nesting depth each still-open '?' was seen at. A ternary's ':' is the one` |
|         - |  395 | ``	 * that stands at its OWN depth: `$c ? f(b: 2) : 'n'` opens its '?' outside the`` |
|         - |  396 | `	 * call and closes it outside, and the named argument's ':' one paren deeper is` |
|         - |  397 | `	 * not it. Counting colons against a bare '?' tally spent the ternary's question` |
|         - |  398 | `	 * mark on the argument LABEL, so the real ':' arrived with nothing open and` |
|         - |  399 | ``	 * `Syntax error: Unexpected token ':'` was a compile fatal on source php --`` |
|         - |  400 | `	 * nette/utils' Type::fromReflection() and Pest's own Mixins/Expectation.php.` |
|         - |  401 | `	 * Only the unparenthesized form ever showed it: an enclosing '(' put the '?'` |
|         - |  402 | `	 * and both colons at depths that happened to work out. */` |
|         - |  403 | `	sxi32 aQuestyDepth[64];` |
|         - |  404 | `	sxi32 i,rc;` |
|         - |  405 |  |
|   2730170 |  406 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  407 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       145 |  408 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       145 |  409 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        70 |  410 | `	}` |
|   2730170 |  411 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  16784897 |  412 | `	for( i = 0 ; i < nNode ; ++i ){` |
|         - |  413 | ``		/* A closure LITERAL is not dereferencable in php: `function () {…}` and`` |
|         - |  414 | ``		 * `fn (…) => …` may not be followed by `(`, `[`, `->`, `?->` or `::` unless`` |
|         - |  415 | `		 * the user parenthesised them, which is why every IIFE in the wild is` |
|         - |  416 | ``		 * written `(function () {…})()`. PHL accepted all five spellings — a`` |
|         - |  417 | `		 * silent acceptance of what php refuses, and the reason it was found: the` |
|         - |  418 | ``		 * statement-position rule above sends `function () {…};` down this path,`` |
|         - |  419 | ``		 * and without the screen `function () {…}();` would have become the one`` |
|         - |  420 | `		 * shape php rejects that PHL runs. */` |
|  14063077 |  421 | `		if( (apNode[i]->xCode == PH7_CompileAnnonFunc \|\| apNode[i]->xCode == PH7_CompileArrowFunc)` |
|   7031408 |  422 | `		 && (apNode[i]->iFlags & EXPR_NODE_PARENS) == 0` |
|     19265 |  423 | `		 && i + 1 < nNode && apNode[i+1] ){` |
|     12814 |  424 | `			ph7_expr_node *pAfter = apNode[i+1];` |
|     12814 |  425 | `			int bDeref = (pAfter->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB)) != 0;` |
|     12809 |  426 | `			if( !bDeref && pAfter->pOp` |
|      6731 |  427 | `			 && ( pAfter->pOp->iOp == EXPR_OP_ARROW` |
|       439 |  428 | `			   \|\| pAfter->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       437 |  429 | `			   \|\| pAfter->pOp->iOp == EXPR_OP_DC ) ){` |
|         7 |  430 | `				bDeref = 1;` |
|         3 |  431 | `			}` |
|     12814 |  432 | `			if( bDeref ){` |
|        11 |  433 | `				rc = PH7_GenSyntaxError(pGen,pAfter->pStart,0);` |
|        11 |  434 | `				if( rc != SXERR_ABORT ){` |
|        11 |  435 | `					rc = SXERR_SYNTAX;` |
|         5 |  436 | `				}` |
|        11 |  437 | `				return rc;` |
|         - |  438 | `			}` |
|      6285 |  439 | `		}` |
|  14063072 |  440 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  441 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     18815 |  442 | `			continue;` |
|         - |  443 | `		}` |
|  14035960 |  444 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|         - |  445 | `			/* A short-array literal is a SELF-CONTAINED node whose start token is '['` |
|         - |  446 | `			 * (its ']' was consumed), so the raw-token CSB test below can never see it —` |
|         - |  447 | ``			 * `[$obj, 'm']()` parsed the '(' as a grouping paren and silently DROPPED`` |
|         - |  448 | `			 * the call (the expression evaluated to the array). php invokes the literal` |
|         - |  449 | `			 * array callable exactly like the variable-held form. */` |
|   1422654 |  450 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    158945 |  451 | `				apNode[i-1]->xCode == PH7_CompileShortArray \|\|` |
|    158927 |  452 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  453 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis.` |
|         - |  454 | `					 * A self-contained short-array node is exempt: its start token is '[',` |
|         - |  455 | `					 * which carries PH7_TK_OP as the subscript operator, but the node is a` |
|         - |  456 | ``					 * complete array-literal TERM — `[$obj, 'm'](...)` is a call. */`` |
|   1121903 |  457 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0` |
|    559812 |  458 | `					 \|\| apNode[i-1]->xCode == PH7_CompileShortArray ){` |
|         - |  459 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  460 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  461 | `						 */` |
|   1121890 |  462 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   1121890 |  463 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   1121890 |  464 | `						apNode[i]->pOp = &sFCallOp;` |
|    559780 |  465 | `					}` |
|    559789 |  466 | `			}` |
|   1343299 |  467 | `			iParen++;` |
|  13363004 |  468 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   1343297 |  469 | `			if( iParen <= 0 ){` |
|        15 |  470 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        15 |  471 | `				if( rc != SXERR_ABORT ){` |
|        15 |  472 | `					rc = SXERR_SYNTAX;` |
|         6 |  473 | `				}` |
|        15 |  474 | `				return rc;` |
|         - |  475 | `			}` |
|   1343285 |  476 | `			iParen--;` |
|  12019705 |  477 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    359982 |  478 | `			iSquare++;` |
|  11169131 |  479 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    359986 |  480 | `			if( iSquare <= 0 ){` |
|         9 |  481 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         9 |  482 | `				if( rc != SXERR_ABORT ){` |
|         9 |  483 | `					rc = SXERR_SYNTAX;` |
|         3 |  484 | `				}` |
|         9 |  485 | `				return rc;` |
|         - |  486 | `			}` |
|    359980 |  487 | `			iSquare--;` |
|  10809149 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       189 |  489 | `			iBraces++;` |
|       189 |  490 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  491 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  492 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  493 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  494 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  495 | `				if( rc != SXERR_ABORT ){` |
|         3 |  496 | `					rc = SXERR_SYNTAX;` |
|         1 |  497 | `				}` |
|         3 |  498 | `				return rc;` |
|         4 |  499 | `			}` |
|  10629323 |  500 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       201 |  501 | `			if( iBraces <= 0 ){` |
|        18 |  502 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        18 |  503 | `				if( rc != SXERR_ABORT ){` |
|        18 |  504 | `					rc = SXERR_SYNTAX;` |
|         7 |  505 | `				}` |
|        18 |  506 | `				return rc;` |
|         - |  507 | `			}` |
|       186 |  508 | `			iBraces--;` |
|  10629127 |  509 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     44985 |  510 | `			sxi32 iDepth = iParen + iSquare + iBraces;` |
|     44980 |  511 | `			if( iQuesty > 0` |
|     44612 |  512 | `			 && ( iQuesty > (sxi32)SX_ARRAYSIZE(aQuestyDepth)` |
|     44234 |  513 | `			   \|\| aQuestyDepth[iQuesty - 1] == iDepth ) ){` |
|     44221 |  514 | `				iQuesty--;` |
|     22847 |  515 | `			}else if( iParen <= 0 ){` |
|         - |  516 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  517 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  518 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  519 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  520 | `				if( rc != SXERR_ABORT ){` |
|         6 |  521 | `					rc = SXERR_SYNTAX;` |
|         2 |  522 | `				}` |
|         6 |  523 | `				return rc;` |
|         5 |  524 | `			}` |
|  10606514 |  525 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   3447658 |  526 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   3447658 |  527 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     44223 |  528 | `				if( iQuesty < (sxi32)SX_ARRAYSIZE(aQuestyDepth) ){` |
|     44223 |  529 | `					aQuestyDepth[iQuesty] = iParen + iSquare + iBraces;` |
|     22079 |  530 | `				}` |
|     44223 |  531 | `				iQuesty++;` |
|   3425519 |  532 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     50154 |  533 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|         9 |  534 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|         9 |  535 | `					sxu32 n = 0;` |
|         9 |  536 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|         5 |  537 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|         2 |  538 | `					}` |
|         - |  539 | `					/*` |
|         - |  540 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|         - |  541 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|         - |  542 | `					 */` |
|       229 |  543 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|       221 |  544 | `						++n;` |
|         1 |  545 | `					}` |
|         9 |  546 | `					pOp = &aOpTable[n];` |
|         - |  547 | `					/* Mark as binary '+' or '-',not an unary */` |
|         9 |  548 | `					apNode[i]->pOp = pOp;` |
|         9 |  549 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|         4 |  550 | `				}` |
|     25036 |  551 | `			}` |
|   1721088 |  552 | `		}` |
|   7006060 |  553 | `	}` |
|   2721820 |  554 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        17 |  555 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        17 |  556 | `		if( rc != SXERR_ABORT ){` |
|        17 |  557 | `			rc = SXERR_SYNTAX;` |
|         7 |  558 | `		}` |
|        17 |  559 | `		return rc;` |
|         - |  560 | `	}` |
|   2721806 |  561 | `	return SXRET_OK;` |
|   1358623 |  562 | `}` |
|         - |  563 | `/*` |
|         - |  564 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  565 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  566 | ` */` |
|   1727227 |  567 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  568 | `{` |
|   1727232 |  569 | `	SyToken *pIn = *ppCur;` |
|         - |  570 | `	/* Jump the first literal seen */` |
|   1727232 |  571 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   1726988 |  572 | `		pIn++;` |
|    861833 |  573 | `	}` |
|    862335 |  574 | `	for(;;){` |
|   1727993 |  575 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       766 |  576 | `			pIn++;` |
|       766 |  577 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       766 |  578 | `				pIn++;` |
|       380 |  579 | `			}` |
|       385 |  580 | `		}else{` |
|    861960 |  581 | `			break;` |
|         - |  582 | `		}` |
|         5 |  583 | `	}` |
|         - |  584 | `	/* Synchronize pointers */` |
|   1727232 |  585 | `	*ppCur = pIn;` |
|   1727232 |  586 | `}` |
|         - |  587 | `/*` |
|         - |  588 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|         - |  589 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  590 | ` * Note on annonymous functions.` |
|         - |  591 | ` *  According to the PHP language reference manual:` |
|         - |  592 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  593 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  594 | ` *  parameters, but they have many other uses.` |
|         - |  595 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|         - |  596 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|         - |  597 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|         - |  598 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|         - |  599 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|         - |  600 | ` *` |
|         - |  601 | ` * Some example:` |
|         - |  602 | ` *  $greet = function($name)` |
|         - |  603 | ` * {` |
|         - |  604 | ` *   printf("Hello %s\r\n", $name);` |
|         - |  605 | ` * };` |
|         - |  606 | ` *  $greet('World');` |
|         - |  607 | ` *  $greet('PHP');` |
|         - |  608 | ` *` |
|         - |  609 | ` * $double = function($a) {` |
|         - |  610 | ` *   return $a * 2;` |
|         - |  611 | ` * };` |
|         - |  612 | ` * // This is our range of numbers` |
|         - |  613 | ` * $numbers = range(1, 5);` |
|         - |  614 | ` * // Use the Annonymous function as a callback here to` |
|         - |  615 | ` * // double the size of each element in our` |
|         - |  616 | ` * // range` |
|         - |  617 | ` * $new_numbers = array_map($double, $numbers);` |
|         - |  618 | ` * print implode(' ', $new_numbers);` |
|         - |  619 | ` */` |
|         - |  620 | `/*` |
|         - |  621 | ` * Skip an optional return-type declaration at *ppIn:` |
|         - |  622 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|         - |  623 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|         - |  624 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|         - |  625 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|         - |  626 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|         - |  627 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|         - |  628 | ` * authoritative type parse, so this must accept every shape it does` |
|         - |  629 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|         - |  630 | ` */` |
|     17298 |  631 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  632 | `{` |
|     17303 |  633 | `	SyToken *pIn = *ppIn;` |
|     17303 |  634 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|       209 |  635 | `		pIn++; /* Skip ':' */` |
|        97 |  636 | `		for(;;){` |
|         - |  637 | `			/* Optional '?' nullable prefix */` |
|       221 |  638 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        14 |  639 | `				pIn++;` |
|         6 |  640 | `			}` |
|       221 |  641 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  642 | `				/* Parenthesized DNF group '(A&B)' */` |
|         5 |  643 | `				pIn++;` |
|         5 |  644 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         5 |  645 | `				if( pIn < pEnd ){` |
|         5 |  646 | `					pIn++; /* ')' */` |
|         2 |  647 | `				}` |
|       215 |  648 | `			}else if( pIn < pEnd` |
|       217 |  649 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  650 | `				/* ['\']Name('\'Name)* */` |
|       217 |  651 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|       217 |  652 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       217 |  653 | `					pIn++;` |
|       217 |  654 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  655 | `						pIn += 2;` |
|       ! 0 |  656 | `					}` |
|       101 |  657 | `				}` |
|       106 |  658 | `			}else{` |
|         - |  659 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  660 | `				break;` |
|         - |  661 | `			}` |
|         - |  662 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|       216 |  663 | `			if( pIn < pEnd` |
|       221 |  664 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|       212 |  665 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|        13 |  666 | `				pIn++;` |
|        13 |  667 | `				continue;` |
|         - |  668 | `			}` |
|       209 |  669 | `			break;` |
|       ! 0 |  670 | `		}` |
|        97 |  671 | `	}` |
|     17303 |  672 | `	*ppIn = pIn;` |
|     17303 |  673 | `}` |
|      6637 |  674 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  675 | `{` |
|      6642 |  676 | `	SyToken *pIn = *ppCur;` |
|         - |  677 | `	sxi32 rc;` |
|         - |  678 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  679 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  680 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  681 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  682 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|      6642 |  683 | `	pIn++;` |
|      6637 |  684 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      3506 |  685 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|       401 |  686 | `		pIn++;` |
|       198 |  687 | `	}` |
|         - |  688 | ``	/* `function &(…) {…}` returns by reference, exactly as the named form does, and`` |
|         - |  689 | ``	 * the `&` sits in the same place. The node assembler is what has to step over`` |
|         - |  690 | `	 * it (PH7_CompileAnnonFunc reads it again for the flag); leaving it here made` |
|         - |  691 | ``	 * every by-ref closure `syntax error, unexpected token "&", expecting "("`. */`` |
|      6642 |  692 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        14 |  693 | `		pIn++;` |
|         6 |  694 | `	}` |
|      6642 |  695 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  696 | `		/* Syntax error */` |
|         6 |  697 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  698 | `		if( rc != SXERR_ABORT ){` |
|         6 |  699 | `			rc = SXERR_SYNTAX;` |
|         2 |  700 | `		}` |
|         6 |  701 | `		goto Synchronize;` |
|         - |  702 | `	}` |
|      6638 |  703 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      6638 |  704 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      6638 |  705 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|         - |  706 | `		/* Two different failures used to share this arm and both claimed the body was` |
|         - |  707 | `		 * missing. They are distinguishable: the delimiter search leaves pIn ON the` |
|         - |  708 | `		 * ')' when it found one, and AT pEnd when it did not.` |
|         - |  709 | ``		 *   pIn >= pEnd      the parameter list never closed (`function($x {`)`` |
|         - |  710 | `		 *                    -> php expects ')'` |
|         - |  711 | `		 *   &pIn[1] >= pEnd  ')' closed it but nothing follows -> php expects '{'` |
|         - |  712 | `		 * php names the token that actually comes next, which lives just past the` |
|         - |  713 | `		 * expression slice, still in the raw stream. */` |
|         8 |  714 | `		SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         8 |  715 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,pIn >= pEnd ? "\")\"" : "\"{\"");` |
|         8 |  716 | `		if( rc != SXERR_ABORT ){` |
|         8 |  717 | `			rc = SXERR_SYNTAX;` |
|         3 |  718 | `		}` |
|         8 |  719 | `		goto Synchronize;` |
|         - |  720 | `	}` |
|      6632 |  721 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  722 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      6632 |  723 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      6632 |  724 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|      2000 |  725 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  726 | `		/* Check if we are dealing with a closure */` |
|      2000 |  727 | `		if( nKey == PH7_TKWRD_USE ){` |
|      1992 |  728 | `			pIn++; /* Jump the 'use' keyword */` |
|      1992 |  729 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  730 | `				/* Syntax error */` |
|         6 |  731 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  732 | `				if( rc != SXERR_ABORT ){` |
|         6 |  733 | `					rc = SXERR_SYNTAX;` |
|         2 |  734 | `				}` |
|         6 |  735 | `				goto Synchronize;` |
|         - |  736 | `			}` |
|      1988 |  737 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|         - |  738 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|         - |  739 | `			 * has no nested structure to balance here, so the first token that is not` |
|         - |  740 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|         - |  741 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|         - |  742 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|         - |  743 | `			 * the first foreign token. */` |
|         - |  744 | `			{` |
|      1988 |  745 | `				SyToken *pUse = pIn;` |
|      1988 |  746 | `				int bClosed = 0;` |
|      7258 |  747 | `				while( pUse < pEnd ){` |
|      7258 |  748 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|      5277 |  749 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      5275 |  750 | `						pUse++;` |
|      5275 |  751 | `						continue;` |
|         - |  752 | `					}` |
|         3 |  753 | `					break; /* foreign token: php names this one */` |
|       ! 0 |  754 | `				}` |
|      1988 |  755 | `				if( !bClosed ){` |
|         - |  756 | `					/* php names the offending token and expects ')'; if the list simply` |
|         - |  757 | `					 * ran off the end of the slice, that token sits just past it. */` |
|         3 |  758 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|         3 |  759 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|         3 |  760 | `					if( rc != SXERR_ABORT ){` |
|         3 |  761 | `						rc = SXERR_SYNTAX;` |
|         1 |  762 | `					}` |
|         3 |  763 | `					goto Synchronize;` |
|         - |  764 | `				}` |
|      1986 |  765 | `				pIn = pUse; /* on the ')' */` |
|         - |  766 | `			}` |
|      1986 |  767 | `			if( &pIn[1] >= pEnd ){` |
|         - |  768 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|         3 |  769 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         3 |  770 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         3 |  771 | `				if( rc != SXERR_ABORT ){` |
|         3 |  772 | `					rc = SXERR_SYNTAX;` |
|         1 |  773 | `				}` |
|         3 |  774 | `				goto Synchronize;` |
|         - |  775 | `			}` |
|      1984 |  776 | `			pIn++;` |
|         - |  777 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  778 | ``			 * `function (...) use (...) : int {` */`` |
|      1984 |  779 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       987 |  780 | `		}else{` |
|         - |  781 | `			/* Syntax error */` |
|        12 |  782 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        12 |  783 | `			if( rc != SXERR_ABORT ){` |
|        12 |  784 | `				rc = SXERR_SYNTAX;` |
|         4 |  785 | `			}` |
|        12 |  786 | `			goto Synchronize;` |
|         - |  787 | `		}` |
|       982 |  788 | `	}` |
|         - |  789 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  790 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  791 | `	 * the type), and pEnd is one past the last token. */` |
|      6616 |  792 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      6616 |  793 | `		pIn++; /* Jump the leading curly '{' */` |
|      6616 |  794 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      6616 |  795 | `		if( pIn < pEnd ){` |
|      6616 |  796 | `			pIn++;` |
|      3289 |  797 | `		}` |
|      3294 |  798 | `	}else{` |
|         - |  799 | `		/* Syntax error. The closure's token range stops at the expression end, so on` |
|         - |  800 | ``		 * `$f = function() ;` the '{' is missing and pIn has already reached pEnd —`` |
|         - |  801 | `		 * php names the token that actually follows (the ';'), which is still in the` |
|         - |  802 | `		 * raw stream just past our slice. Peek at it rather than claiming EOF. */` |
|       ! 0 |  803 | `		SyToken *pBad = pIn < pEnd ? pIn : (pEnd < pGen->pEnd ? pEnd : 0);` |
|       ! 0 |  804 | `		rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|       ! 0 |  805 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  806 | `			return SXERR_ABORT;` |
|         - |  807 | `		}` |
|         - |  808 | `	}` |
|      6616 |  809 | `	rc = SXRET_OK;` |
|      3335 |  810 | `Synchronize:` |
|         - |  811 | `	/* Synchronize pointers */` |
|      6642 |  812 | `	*ppCur = pIn;` |
|      6642 |  813 | `	return rc;` |
|      3307 |  814 | `}` |
|         - |  815 | `/*` |
|         - |  816 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  817 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  818 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  819 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  820 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  821 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  822 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  823 | ` */` |
|       186 |  824 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  825 | `{` |
|       191 |  826 | `	SyToken *pIn = *ppCur;` |
|       191 |  827 | `	sxu32 nLine = pIn->nLine;` |
|         - |  828 | `	sxi32 rc;` |
|       191 |  829 | `	if( GenStateIsReadonly(pIn) ){` |
|         5 |  830 | ``		pIn++; /* `new readonly class …` (PHP 8.3): step over the modifier */`` |
|         2 |  831 | `	}` |
|       191 |  832 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  833 | `	/* Optional constructor argument list */` |
|       191 |  834 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        75 |  835 | `		pIn++; /* Jump '(' */` |
|        75 |  836 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        75 |  837 | `		if( pIn < pEnd ){` |
|        75 |  838 | `			pIn++; /* Jump ')' */` |
|        35 |  839 | `		}` |
|        35 |  840 | `	}` |
|         - |  841 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  842 | `	 * (no braces appear between ')' and the class body). */` |
|       473 |  843 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       287 |  844 | `		pIn++;` |
|         5 |  845 | `	}` |
|       191 |  846 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  847 | `		/* Syntax error: missing class body */` |
|       ! 0 |  848 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  849 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  850 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  851 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  852 | `		}` |
|       ! 0 |  853 | `		*ppCur = pIn;` |
|       ! 0 |  854 | `		return rc;` |
|         - |  855 | `	}` |
|       191 |  856 | `	pIn++; /* Jump the leading '{' */` |
|       191 |  857 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       191 |  858 | `	if( pIn < pEnd ){` |
|       191 |  859 | `		pIn++; /* Jump the trailing '}' */` |
|        93 |  860 | `	}` |
|       191 |  861 | `	*ppCur = pIn;` |
|       191 |  862 | `	return SXRET_OK;` |
|        98 |  863 | `}` |
|         - |  864 | `/*` |
|         - |  865 | ` * TRUE when a KEYWORD token actually OPENS an arrow function.` |
|         - |  866 | ` *` |
|         - |  867 | `` * `fn` is reserved, but it only ever introduces `[static] fn[&](…) => expr`.`` |
|         - |  868 | ``  * Everywhere php expects a NAME the same word is an ordinary identifier: `$fn` `` |
|         - |  869 | ` * (the lexer emits '$' plus the keyword, so the keyword IS the variable name),` |
|         - |  870 | ``  * `$fn(…)` calling that variable, `C::fn`, `$o->fn`, `\A\fn`, and the `fn:` `` |
|         - |  871 | ` * named-argument label. Every raw-token lookahead that steps over an arrow` |
|         - |  872 | ` * function has to make that distinction or it swallows a plain name and loses` |
|         - |  873 | `` * the '=>' that follows it (`[$fn => 1]` became `syntax error, unexpected token`` |
|         - |  874 | `` * "=>"`).`` |
|         - |  875 | ` *` |
|         - |  876 | `` * The test is POSITIONAL, never "is it well formed": a malformed `fn` (`fn $x`` |
|         - |  877 | `` * => $x`, or a bare `fn` used as a key) must still reach the arrow parser,`` |
|         - |  878 | `` * which is what reports php's `expecting "("`. Two name positions:`` |
|         - |  879 | ` *   - member/variable/namespace: '$', '->', '?->', '::' or '\' immediately` |
|         - |  880 | ` *     before the word;` |
|         - |  881 | ``  *   - a named-argument LABEL: a bare `fn` directly before ':' (`static fn:` `` |
|         - |  882 | `` *     and `fn&:` cannot be labels, so they stay the arrow parser's business).`` |
|         - |  883 | ` *     The argument list is re-parsed from the argument's own first token, so` |
|         - |  884 | ` *     there is no '(' to look back at — the label test cannot be scoped to` |
|         - |  885 | `` *     call context, and the degenerate `true ? fn : 0` (only reachable through`` |
|         - |  886 | ` *     define('fn',…), which php itself cannot parse) is accepted as a` |
|         - |  887 | ` *     constant instead of rejected. A recorded divergence; rejecting it` |
|         - |  888 | `` *     would cost the real `f(fn: 1)` spelling.`` |
|         - |  889 | `` * pStart bounds the look-back; pTok may point at `static`, which must then be`` |
|         - |  890 | `` * followed by `fn`.`` |
|         - |  891 | ` */` |
|     10464 |  892 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd)` |
|         5 |  893 | `{` |
|     10469 |  894 | `	int bStatic = FALSE;` |
|     10469 |  895 | `	if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  896 | `		return FALSE;` |
|         - |  897 | `	}` |
|     10469 |  898 | `	if( pTok > pStart ){` |
|      1089 |  899 | `		SyToken *pPrev = &pTok[-1];` |
|      1089 |  900 | `		if( pPrev->nType & (PH7_TK_DOLLAR\|PH7_TK_NSSEP) ){` |
|        96 |  901 | `			return FALSE; /* $fn / \A\fn — the keyword IS the name */` |
|         - |  902 | `		}` |
|       997 |  903 | `		if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData ){` |
|       447 |  904 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)pPrev->pUserData;` |
|       442 |  905 | `			if( pOp->iOp == EXPR_OP_ARROW \|\| pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       412 |  906 | `			 \|\| pOp->iOp == EXPR_OP_DC ){` |
|       122 |  907 | `				return FALSE; /* $o->fn, $o?->fn, C::fn — a member name */` |
|         - |  908 | `			}` |
|       162 |  909 | `		}` |
|       437 |  910 | `	}` |
|     10259 |  911 | `	if( SX_PTR_TO_INT(pTok->pUserData) == PH7_TKWRD_STATIC ){` |
|       349 |  912 | `		bStatic = TRUE;` |
|       349 |  913 | `		pTok++;` |
|       349 |  914 | `		if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       190 |  915 | `			return FALSE;` |
|         - |  916 | `		}` |
|        79 |  917 | `	}` |
|     10073 |  918 | `	if( SX_PTR_TO_INT(pTok->pUserData) != PH7_TKWRD_FN ){` |
|       939 |  919 | `		return FALSE;` |
|         - |  920 | `	}` |
|      9139 |  921 | `	if( !bStatic && &pTok[1] < pEnd && (pTok[1].nType & PH7_TK_COLON) ){` |
|       ! 0 |  922 | `		return FALSE; /* f(fn: 1) — a named-argument label */` |
|         - |  923 | `	}` |
|      9139 |  924 | `	return TRUE;` |
|      5138 |  925 | `}` |
|         - |  926 | `/*` |
|         - |  927 | `` * Step over an arrow function's HEAD, `fn [&] ( params ) [: [?] type]` and the`` |
|         - |  928 | `` * `=>` after it, from the `fn` keyword at *ppIn. Boundary scanning only: a`` |
|         - |  929 | ` * malformed head stops at the first token that does not fit, and the compile` |
|         - |  930 | ` * pass (PH7_CompileArrowFunc) reports php's error for it. Shared by the arrow` |
|         - |  931 | ` * assembler for its own head and for every arrow nested in its body.` |
|         - |  932 | ` */` |
|      8692 |  933 | `static void ExprSkipArrowHead(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  934 | `{` |
|      8697 |  935 | `	SyToken *pIn = *ppIn;` |
|      8697 |  936 | `	pIn++; /* Jump 'fn' */` |
|         - |  937 | `	/* Optional '&' for return-by-reference */` |
|      8697 |  938 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  939 | `		pIn++;` |
|       ! 0 |  940 | `	}` |
|      8697 |  941 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      8695 |  942 | `		pIn++; /* '(' */` |
|      8695 |  943 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      8695 |  944 | `		if( pIn < pEnd ){` |
|      8693 |  945 | `			pIn++; /* ')' */` |
|      4245 |  946 | `		}` |
|      4246 |  947 | `	}` |
|         - |  948 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      8697 |  949 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  950 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      8697 |  951 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      8691 |  952 | `		pIn++;` |
|      4244 |  953 | `	}` |
|      8697 |  954 | `	*ppIn = pIn;` |
|      8697 |  955 | `}` |
|         - |  956 | `/*` |
|         - |  957 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  958 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  959 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  960 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  961 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  962 | ` */` |
|      8500 |  963 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  964 | `{` |
|      8505 |  965 | `	SyToken *pIn = *ppCur;` |
|         - |  966 | `	SyToken *pBody;  /* first token of the body: bounds the nested-arrow look-back */` |
|         - |  967 | `	sxu32 nLine;` |
|         - |  968 | `	sxi32 rc;` |
|         - |  969 | `	int iNest;` |
|         - |  970 | `	int iTern;   /* ternary '?'s opened inside the body and not yet closed */` |
|      8505 |  971 | `	nLine = pIn->nLine;` |
|         - |  972 | `	/* Optional 'static' prefix */` |
|      8500 |  973 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      8505 |  974 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       155 |  975 | `		pIn++;` |
|        75 |  976 | `	}` |
|         - |  977 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      8500 |  978 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      8505 |  979 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  980 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  981 | `		goto Synchronize;` |
|         - |  982 | `	}` |
|      4151 |  983 | `	SXUNUSED(nLine);` |
|      4151 |  984 | `	SXUNUSED(pGen);` |
|         - |  985 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  986 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  987 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  988 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      8505 |  989 | `	ExprSkipArrowHead(&pIn,pEnd);` |
|      8505 |  990 | `	pBody = pIn;` |
|         - |  991 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' -- or a ':' that belongs` |
|         - |  992 | `	 * to an enclosing TERNARY. php's grammar ends an arrow body there, which is` |
|         - |  993 | ``	 * what makes `$c ? fn($v) => a : fn($v) => b` legal: the first body stops at`` |
|         - |  994 | ``	 * the `:` and the second is the false branch. Without the rule the body ran on`` |
|         - |  995 | ``	 * and swallowed `: fn($v) => b`, and the statement's `;` was `unexpected token`` |
|         - |  996 | ``	 * ";"` -- symfony/config and symfony/var-exporter both write that shape.`` |
|         - |  997 | ``	 * A ternary opened INSIDE the body owns its own colon (`fn() => $a ? $b : $c`),`` |
|         - |  998 | ``	 * so count them; `?:`, `??` and `?->` never reach the counter as a bare '?'`` |
|         - |  999 | `	 * (the first is two tokens whose colon pairs with its own '?', the other two` |
|         - | 1000 | `	 * are single tokens). */` |
|      8505 | 1001 | `	iNest = 0;` |
|      8505 | 1002 | `	iTern = 0;` |
|     84613 | 1003 | `	while( pIn < pEnd ){` |
|     83416 | 1004 | `		if( iNest == 0 && (pIn->nType &` |
|         - | 1005 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      7300 | 1006 | `			break;` |
|         - | 1007 | `		}` |
|     76121 | 1008 | `		if( (pIn->nType & PH7_TK_KEYWORD) && PH7_TokenOpensArrowFunc(pBody,pIn,pEnd) ){` |
|         - | 1009 | `			/* A NESTED arrow function: its body is the tail of this one, so the` |
|         - | 1010 | `			 * scan simply continues into it -- but its HEAD must be stepped over` |
|         - | 1011 | `			 * as a unit. Left to the loop, the colon of its return type read as` |
|         - | 1012 | `			 * an enclosing ternary's and ended the outer body right there, so` |
|         - | 1013 | ``			 * `fn($t) => fn($v): string => $v` (doctrine/orm's DQL cookbook) was`` |
|         - | 1014 | ``			 * `unexpected token "=>"`, and a `?` in `fn($v): ?string` would have`` |
|         - | 1015 | `			 * been counted as a ternary. */` |
|       197 | 1016 | `			if( SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         3 | 1017 | ``				pIn++; /* `static fn`: the head skipper starts at `fn` */`` |
|         1 | 1018 | `			}` |
|       197 | 1019 | `			ExprSkipArrowHead(&pIn,pEnd);` |
|       197 | 1020 | `			continue;` |
|         - | 1021 | `		}` |
|     75929 | 1022 | `		if( iNest == 0 && (pIn->nType & PH7_TK_COLON) ){` |
|        67 | 1023 | `			if( iTern < 1 ){` |
|        10 | 1024 | `				break;     /* the colon of an enclosing '?': the body ends here */` |
|         - | 1025 | `			}` |
|        59 | 1026 | `			iTern--;       /* ...or of a ternary this body opened itself */` |
|     75894 | 1027 | `		}else if( iNest == 0 && (pIn->nType & PH7_TK_OP)` |
|     15515 | 1028 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        59 | 1029 | `			iTern++;` |
|        27 | 1030 | `		}` |
|     75921 | 1031 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     13570 | 1032 | `			iNest++;` |
|     68993 | 1033 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     13570 | 1034 | `			iNest--;` |
|      6637 | 1035 | `		}` |
|     75921 | 1036 | `		pIn++;` |
|         5 | 1037 | `	}` |
|      8505 | 1038 | `	rc = SXRET_OK;` |
|      4349 | 1039 | `Synchronize:` |
|      8505 | 1040 | `	*ppCur = pIn;` |
|      8505 | 1041 | `	return rc;` |
|         5 | 1042 | `}` |
|         - | 1043 | `/*` |
|         - | 1044 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - | 1045 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - | 1046 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - | 1047 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - | 1048 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - | 1049 | ` */` |
|       156 | 1050 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 | 1051 | `{` |
|       161 | 1052 | `	SyToken *pIn = *ppCur;` |
|         - | 1053 | `	sxi32 rc;` |
|        78 | 1054 | `	SXUNUSED(pGen);` |
|         - | 1055 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       156 | 1056 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       161 | 1057 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 | 1058 | `		rc = SXERR_SYNTAX;` |
|       ! 0 | 1059 | `		goto Synchronize;` |
|         - | 1060 | `	}` |
|       161 | 1061 | `	pIn++; /* Jump 'match' */` |
|         - | 1062 | `	/* Optional '(' subject ')' */` |
|       161 | 1063 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       161 | 1064 | `		pIn++;` |
|       161 | 1065 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       161 | 1066 | `		if( pIn < pEnd ){` |
|       161 | 1067 | `			pIn++; /* ')' */` |
|        78 | 1068 | `		}` |
|        78 | 1069 | `	}` |
|         - | 1070 | `	/* Optional '{' arms '}' */` |
|       161 | 1071 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       161 | 1072 | `		pIn++;` |
|       161 | 1073 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       161 | 1074 | `		if( pIn < pEnd ){` |
|       161 | 1075 | `			pIn++; /* '}' */` |
|        78 | 1076 | `		}` |
|        78 | 1077 | `	}` |
|       161 | 1078 | `	rc = SXRET_OK;` |
|        78 | 1079 | `Synchronize:` |
|       161 | 1080 | `	*ppCur = pIn;` |
|       161 | 1081 | `	return rc;` |
|         5 | 1082 | `}` |
|         - | 1083 | `/*` |
|         - | 1084 | `` * PHP 8.5 `clone (`: tell the clone() CALL form from the clone OPERATOR applied to a`` |
|         - | 1085 | ` * parenthesised operand. php's grammar has both, and its parser resolves the conflict` |
|         - | 1086 | ` * by continuing the parenthesised expression whenever the token after the ')' can` |
|         - | 1087 | ``  * dereference it — so `clone ($a)->b()` clones what `b()` returns, `clone ($c)[0]` `` |
|         - | 1088 | `` * clones the ELEMENT and `clone ($f)()` clones the call's result, while a plain`` |
|         - | 1089 | `` * `clone ($a)` (nothing dereferencing) is the one-argument call, which means the same`` |
|         - | 1090 | `` * thing either way. PHL took the call form for every `clone (`, so the receiver was`` |
|         - | 1091 | ` * cloned and the member access ran on the ORIGINAL — a silent wrong answer with no` |
|         - | 1092 | `` * diagnostic, out of `clone (new A)->b()`.`` |
|         - | 1093 | ` *` |
|         - | 1094 | ` * pClone points at the 'clone' token and pClone[1] at its '('. Returns TRUE when the` |
|         - | 1095 | ` * call-form branch should take the tokens (including the unterminated case, which that` |
|         - | 1096 | ` * branch reports), FALSE to leave them to the precedence-1 operator path.` |
|         - | 1097 | ` */` |
|        68 | 1098 | `static int CloneCallFormFollows(SyToken *pClone,SyToken *pEnd)` |
|         2 | 1099 | `{` |
|        70 | 1100 | `	SyToken *pNext = &pClone[2]; /* first token inside the '(' */` |
|        70 | 1101 | `	PH7_DelimitNestedTokens(pNext,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pNext);` |
|        70 | 1102 | `	if( pNext >= pEnd ){` |
|       ! 0 | 1103 | `		return TRUE; /* unterminated '(' — the call-form branch raises php's ')' error */` |
|         - | 1104 | `	}` |
|        70 | 1105 | `	pNext++; /* step past the matching ')' */` |
|        70 | 1106 | `	if( pNext >= pEnd ){` |
|        52 | 1107 | `		return TRUE;` |
|         - | 1108 | `	}` |
|        19 | 1109 | `	if( pNext->nType & (PH7_TK_OSB /*'['*/\|PH7_TK_LPAREN /*'('*/) ){` |
|         5 | 1110 | `		return FALSE;` |
|         - | 1111 | `	}` |
|        15 | 1112 | `	if( (pNext->nType & PH7_TK_OP) && pNext->pUserData ){` |
|         9 | 1113 | `		sxi32 iOp = ((const ph7_expr_op *)pNext->pUserData)->iOp;` |
|         9 | 1114 | `		if( iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW \|\| iOp == EXPR_OP_DC ){` |
|         9 | 1115 | `			return FALSE;` |
|         - | 1116 | `		}` |
|       ! 0 | 1117 | `	}` |
|         7 | 1118 | `	return TRUE;` |
|        36 | 1119 | `}` |
|         - | 1120 | `/*` |
|         - | 1121 | ` * Extract a single expression node from the input.` |
|         - | 1122 | ` * On success store the freshly extractd node in ppNode.` |
|         - | 1123 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1124 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - | 1125 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - | 1126 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - | 1127 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - | 1128 | ` */` |
|         - | 1129 | `/*` |
|         - | 1130 | `` * Where a KEYWORD-headed operand ends: `yield <expr>`, `throw <expr>` and the`` |
|         - | 1131 | `` * one-operand language constructs (`print`, `include`, `require`, …) each take`` |
|         - | 1132 | ` * the rest of the enclosing group, so PH7_DelimitNestedTokens is the right shape` |
|         - | 1133 | `` * for them — EXCEPT that php's grammar gives them `expr`, and a top-level COMMA`` |
|         - | 1134 | `` * is not part of an `expr`. In an ARGUMENT LIST that comma is the separator, so`` |
|         - | 1135 | `` * `f(yield 1, 2)` and `f(print "p", 2)` are two arguments in php; here the`` |
|         - | 1136 | `` * operand ran straight past it and the leftover `, 2` came back as`` |
|         - | 1137 | `` * `syntax error, unexpected token ","` on source php runs. (An ARRAY literal`` |
|         - | 1138 | ` * never showed it: its body is re-split on commas before these nodes are ever` |
|         - | 1139 | ` * extracted.) At statement level nothing legitimate follows such an operand with` |
|         - | 1140 | `` * a comma, so stopping is php's answer there too — `yield 1, 2;` and`` |
|         - | 1141 | `` * `print "a", "b";` stay the parse error both engines already gave.`` |
|         - | 1142 | ` */` |
|      1066 | 1143 | `static void ExprDelimitKeywordOperand(SyToken *pIn,SyToken *pEnd,SyToken **ppEnd)` |
|         5 | 1144 | `{` |
|      1071 | 1145 | `	SyToken *pCur = pIn;` |
|      1071 | 1146 | `	sxi32 iNest = 1;` |
|      1071 | 1147 | ``	sxi32 iQuesty = 0;   /* `?`s opened inside the operand and still unclosed */`` |
|      2002 | 1148 | `	for(;;){` |
|      4009 | 1149 | `		if( pCur >= pEnd ){` |
|       963 | 1150 | `			break;` |
|         - | 1151 | `		}` |
|      3051 | 1152 | `		if( (pCur->nType & PH7_TK_COMMA) && iNest <= 1 ){` |
|        17 | 1153 | `			break;` |
|         - | 1154 | `		}` |
|         - | 1155 | ``		/* Every construct that shares this delimiter -- `include`/`require` and`` |
|         - | 1156 | ``		 * their `_once` forms, `print`, `echo`, `throw`, `yield` -- sits BELOW the`` |
|         - | 1157 | ``		 * ternary in php's precedence table, so a `:` that closes a `?` opened`` |
|         - | 1158 | ``		 * OUTSIDE the operand ends it: `c ? include $f : null` includes $f and the`` |
|         - | 1159 | ``		 * `: null` is the ternary's. A `?` opened INSIDE takes its own `:` with it,`` |
|         - | 1160 | `` 		 * which is the other half of the same rule -- `include $f ? "y" : "n"` `` |
|         - | 1161 | ``		 * includes the whole conditional's answer, and `include $f ?: 1` the elvis`` |
|         - | 1162 | ``		 * one. Swallowing the `:` regardless left `? <operand>` with no colon, and`` |
|         - | 1163 | ``		 * every `cond ? require $file : null` bootstrap failed to parse. `??` and`` |
|         - | 1164 | ``		 * `?->` are their own tokens, so the one-byte test cannot see them, and a`` |
|         - | 1165 | ``		 * named argument's `:` sits inside parentheses at iNest >= 2. */`` |
|      3030 | 1166 | `		if( iNest <= 1 && (pCur->nType & PH7_TK_OP)` |
|      1338 | 1167 | `		 && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|         3 | 1168 | `			iQuesty++;` |
|      3034 | 1169 | `		}else if( iNest <= 1 && (pCur->nType & PH7_TK_COLON) ){` |
|        17 | 1170 | `			if( iQuesty < 1 ){` |
|        15 | 1171 | `				break;` |
|         - | 1172 | `			}` |
|         3 | 1173 | `			iQuesty--;` |
|         1 | 1174 | `		}` |
|      3021 | 1175 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB) ){` |
|       219 | 1176 | `			iNest++;` |
|      2914 | 1177 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|       297 | 1178 | `			iNest--;` |
|       297 | 1179 | `			if( iNest <= 0 ){` |
|        82 | 1180 | `				break;` |
|         - | 1181 | `			}` |
|       107 | 1182 | `		}` |
|      2943 | 1183 | `		pCur++;` |
|         5 | 1184 | `	}` |
|      1071 | 1185 | `	*ppEnd = pCur;` |
|      1071 | 1186 | `}` |
|  14055510 | 1187 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 | 1188 | `{` |
|         - | 1189 | `	ph7_expr_node *pNode;` |
|         - | 1190 | `	SyToken *pCur;` |
|         - | 1191 | `	sxi32 rc;` |
|         - | 1192 | `	/* Allocate a new node */` |
|  14055515 | 1193 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  14055515 | 1194 | `	if( pNode == 0 ){` |
|         - | 1195 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1196 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1197 | `		 */` |
|       ! 0 | 1198 | `		return SXERR_MEM;` |
|         - | 1199 | `	}` |
|         - | 1200 | `	/* Zero the structure */` |
|  14055515 | 1201 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  14055515 | 1202 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - | 1203 | `	/* Point to the head of the token stream */` |
|  14055515 | 1204 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - | 1205 | `	/* Start collecting tokens */` |
|  14055515 | 1206 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       748 | 1207 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - | 1208 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - | 1209 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - | 1210 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - | 1211 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       279 | 1212 | `			pNode->pEnd = pCur;` |
|       279 | 1213 | `			pCur++;` |
|       279 | 1214 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       279 | 1215 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       279 | 1216 | `			pGen->pIn = pCur;` |
|       279 | 1217 | `			*ppNode = pNode;` |
|       279 | 1218 | `			return SXRET_OK;` |
|         - | 1219 | `		}` |
|         - | 1220 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - | 1221 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       474 | 1222 | `		pCur++;` |
|       474 | 1223 | `		pGen->pIn = pCur;` |
|       474 | 1224 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       474 | 1225 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       474 | 1226 | `		if( rc == SXRET_OK && *ppNode ){` |
|       474 | 1227 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       234 | 1228 | `		}` |
|       474 | 1229 | `		return rc;` |
|         - | 1230 | `	}` |
|  14054772 | 1231 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - | 1232 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - | 1233 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - | 1234 | `		 */` |
|     18817 | 1235 | `		pCur++; /* Skip the opening '[' */` |
|     18817 | 1236 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     18817 | 1237 | `		if( pCur < pGen->pEnd ){` |
|     18817 | 1238 | `			pCur++; /* Skip past the closing ']' */` |
|      9363 | 1239 | `		}else{` |
|       ! 0 | 1240 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1241 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 | 1242 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 1243 | `				rc = SXERR_SYNTAX;` |
|       ! 0 | 1244 | `			}` |
|       ! 0 | 1245 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1246 | `			return rc;` |
|         - | 1247 | `		}` |
|         - | 1248 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - | 1249 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - | 1250 | `		 */` |
|     19846 | 1251 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      2044 | 1252 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      2044 | 1253 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       295 | 1254 | `				pNode->xCode = PH7_CompileShortList;` |
|       150 | 1255 | `			}else{` |
|      1754 | 1256 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - | 1257 | `			}` |
|      1015 | 1258 | `		}else{` |
|     16778 | 1259 | `			pNode->xCode = PH7_CompileShortArray;` |
|         - | 1260 | `		}` |
|  14045318 | 1261 | `	}else if( !bAfterMemberOp && (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID))` |
|   7959172 | 1262 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_COLON) ){` |
|         - | 1263 | `		/* A RESERVED WORD immediately followed by a single ':' is a named-argument` |
|         - | 1264 | `		 * LABEL — php accepts all 73 of them there, since a parameter may be called` |
|         - | 1265 | ``		 * anything (`function f($new, $print, $and)`). Sixteen of them were claimed`` |
|         - | 1266 | ``		 * by their own construct branch below instead, so `f(new: 1)`, `f(print: 2)`,`` |
|         - | 1267 | ``		 * `f(match: $m)` and thirteen more were a compile fatal on source php runs.`` |
|         - | 1268 | ``		 * The lexer gives `::` its own operator token, so the only other shape this`` |
|         - | 1269 | `		 * can see — a bare word before a colon — is already a literal on the` |
|         - | 1270 | ``		 * fallthrough path; a `?:` cannot reach here at all, its '?' being neither`` |
|         - | 1271 | `		 * an identifier nor a keyword. The argument list is re-parsed from each` |
|         - | 1272 | `		 * argument's own first token, so there is no '(' to look back at and no way` |
|         - | 1273 | `		 * to scope this to call context; ExprProcessFuncArguments makes the` |
|         - | 1274 | `		 * POSITIONAL test that decides whether the label is really one. */` |
|      8778 | 1275 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|      8778 | 1276 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      8778 | 1277 | `		pNode->xCode = PH7_CompileLiteral;` |
|  14031568 | 1278 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - | 1279 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - | 1280 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - | 1281 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - | 1282 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - | 1283 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - | 1284 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - | 1285 | `		 * call, not the clone() intrinsic. */` |
|        25 | 1286 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        25 | 1287 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        25 | 1288 | `		pNode->xCode = PH7_CompileLiteral;` |
|  14027171 | 1289 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   3807738 | 1290 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   1901059 | 1291 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN)` |
|       222 | 1292 | `		&& CloneCallFormFollows(pCur,pGen->pEnd) ){` |
|         - | 1293 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - | 1294 | ``		 * `clone` is a real internal FUNCTION in php 8.5, so this spelling is an`` |
|         - | 1295 | `		 * ordinary call — and every property the call machinery owns comes with` |
|         - | 1296 | ``		 * it: named arguments, spread, the first-class-callable `clone(...)`, and`` |
|         - | 1297 | `		 * the runtime ArgumentCountError/TypeError php raises for a degenerate` |
|         - | 1298 | ``		 * argument list (PHL used to refuse `clone()` and a three-argument call at`` |
|         - | 1299 | ``		 * COMPILE time, and had no FCC form at all). `clone` is an alpha-stream`` |
|         - | 1300 | `` 		 * operator token, so `clone(` is not auto-marked as a call the way `foo(` `` |
|         - | 1301 | `		 * is: clear PH7_TK_OP and leave a plain name TERM behind, and the postfix` |
|         - | 1302 | `		 * pass then binds the '(' to it. The bare operator/statement form` |
|         - | 1303 | ``		 * `clone $obj` (no immediately-following '(') keeps the precedence-1`` |
|         - | 1304 | `		 * operator path below. */` |
|        58 | 1305 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        58 | 1306 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        58 | 1307 | `		pNode->xCode = PH7_CompileLiteral;` |
|  14027135 | 1308 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1309 | `		/* Point to the instance that describe this operator */` |
|   3807687 | 1310 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1311 | `		/* Advance the stream cursor */` |
|   3807687 | 1312 | `		pCur++;` |
|  12120273 | 1313 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1314 | `		/* Isolate variable */` |
|   7079723 | 1315 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   3539889 | 1316 | `			pCur++; /* Variable variable */` |
|         5 | 1317 | `		}` |
|   3539839 | 1318 | `		if( pCur < pGen->pEnd ){` |
|   3539839 | 1319 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1320 | `				/* Variable name */` |
|   3539793 | 1321 | `				pCur++;` |
|   1767354 | 1322 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        43 | 1323 | `				pCur++;` |
|         - | 1324 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        43 | 1325 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        43 | 1326 | `				if( pCur < pGen->pEnd ){` |
|        40 | 1327 | `					pCur++;` |
|        22 | 1328 | `				}else{` |
|         - | 1329 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|         - | 1330 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|         - | 1331 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|         - | 1332 | `					 * delimiter search stops at the slice end, so the token php names` |
|         - | 1333 | `					 * usually sits just past it, still inside the chunk stream. */` |
|         - | 1334 | `					{` |
|         3 | 1335 | `						SyToken *pBad = 0;` |
|         3 | 1336 | `						if( pGen->pTokenSet ){` |
|         3 | 1337 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         3 | 1338 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         3 | 1339 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|       ! 0 | 1340 | `								pBad = pCur;` |
|       ! 0 | 1341 | `							}` |
|         1 | 1342 | `						}` |
|         3 | 1343 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|         - | 1344 | `					}` |
|         3 | 1345 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1346 | `						rc = SXERR_SYNTAX;` |
|         1 | 1347 | `					}` |
|         3 | 1348 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1349 | `					return rc;` |
|         - | 1350 | `				}` |
|        22 | 1351 | `			}else{` |
|         - | 1352 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|         - | 1353 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|         - | 1354 | `				 * covered only the '$' and the offending token drifted into a later` |
|         - | 1355 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|         - | 1356 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|        10 | 1357 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|        10 | 1358 | `				if( rc != SXERR_ABORT ){` |
|        10 | 1359 | `					rc = SXERR_SYNTAX;` |
|         4 | 1360 | `				}` |
|        10 | 1361 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        10 | 1362 | `				return rc;` |
|         - | 1363 | `			}` |
|   1767321 | 1364 | `		}` |
|   3539829 | 1365 | `		pNode->xCode = PH7_CompileVariable;` |
|   8446912 | 1366 | `	 }else if( !bAfterMemberOp && GenStateStartsReadonlyAnonClass(pCur,pGen->pEnd) ){` |
|         - | 1367 | ``		 /* `new readonly class(args) [extends/implements] { body }` (PHP 8.3).`` |
|         - | 1368 | ``		  * `readonly` is a context-sensitive ID, so it never reaches the keyword`` |
|         - | 1369 | ``		  * chain below and the `class` after it read as the `::class` constant --`` |
|         - | 1370 | ``		  * `syntax error, unexpected token "class"`. It is the ONLY modifier php`` |
|         - | 1371 | ``		  * allows here (`new final class {}` and `new abstract class {}` are parse`` |
|         - | 1372 | `		  * errors in both engines), and pest writes one in its parallel result` |
|         - | 1373 | `		  * printer. */` |
|         5 | 1374 | `		 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         5 | 1375 | `		 if( rc != SXRET_OK ){` |
|       ! 0 | 1376 | `			 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1377 | `			 return rc;` |
|         - | 1378 | `		 }` |
|         5 | 1379 | `		 pNode->xCode = PH7_CompileAnnonClass;` |
|   6679589 | 1380 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    133842 | 1381 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    133842 | 1382 | `		 if( bAfterMemberOp ){` |
|         - | 1383 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1384 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1385 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1386 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|         - | 1387 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|         - | 1388 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|         - | 1389 | `			  * the word itself. */` |
|       601 | 1390 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|       601 | 1391 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       601 | 1392 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    133544 | 1393 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1394 | `			 /* List/Array node */` |
|     98527 | 1395 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1396 | `				 /* Assume a literal */` |
|       ! 0 | 1397 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1398 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1399 | `			 }else{` |
|     98527 | 1400 | `				 pCur += 2;` |
|         - | 1401 | `				 /* Collect array/list tokens */` |
|     98527 | 1402 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     98527 | 1403 | `				 if( pCur < pGen->pEnd ){` |
|     98525 | 1404 | `					 pCur++;` |
|     49199 | 1405 | `				 }else{` |
|         - | 1406 | `					 /* Syntax error */` |
|         - | 1407 | `					 /* php names the token it stopped on and says it expected ")". */` |
|         3 | 1408 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|         3 | 1409 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1410 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1411 | `					 }` |
|         3 | 1412 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1413 | `					 return rc;` |
|         - | 1414 | `				 }` |
|     98525 | 1415 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     98525 | 1416 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        65 | 1417 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        65 | 1418 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1419 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|         - | 1420 | ``						  * not the `list` the construct started at. */`` |
|         3 | 1421 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|         3 | 1422 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1423 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1424 | `						 }` |
|         3 | 1425 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1426 | `						 return rc;` |
|         - | 1427 | `					 }` |
|        29 | 1428 | `				 }` |
|         5 | 1429 | `			 }` |
|     83917 | 1430 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1431 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       649 | 1432 | `			 pCur++; /* Skip 'yield' keyword */` |
|       649 | 1433 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       649 | 1434 | `			 pNode->xCode = PH7_CompileYield;` |
|     34402 | 1435 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     31216 | 1436 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       708 | 1437 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       623 | 1438 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1439 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|      6642 | 1440 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1441 | `				 /* Assume a literal */` |
|       ! 0 | 1442 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1443 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1444 | `			 }else{` |
|         - | 1445 | `				 /* Assemble annonymous functions body */` |
|      6642 | 1446 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      6642 | 1447 | `				 if( rc != SXRET_OK ){` |
|        30 | 1448 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        30 | 1449 | `					 return rc;` |
|         - | 1450 | `				 }` |
|      6616 | 1451 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1452 | `			  }` |
|     30732 | 1453 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       196 | 1454 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       121 | 1455 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        60 | 1456 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        38 | 1457 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1458 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1459 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1460 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1461 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       187 | 1462 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       187 | 1463 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1464 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1465 | `				 return rc;` |
|         - | 1466 | `			 }` |
|       187 | 1467 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     27352 | 1468 | `		 }else if( (nKeyword == PH7_TKWRD_FN \|\| nKeyword == PH7_TKWRD_STATIC)` |
|     17957 | 1469 | `			&& PH7_TokenOpensArrowFunc(pGen->pIn,pCur,pGen->pEnd) ){` |
|         - | 1470 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      8505 | 1471 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      8505 | 1472 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1473 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1474 | `				 return rc;` |
|         - | 1475 | `			 }` |
|      8505 | 1476 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     22912 | 1477 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1478 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       161 | 1479 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       161 | 1480 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1481 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1482 | `				 return rc;` |
|         - | 1483 | `			 }` |
|       161 | 1484 | `			 pNode->xCode = PH7_CompileMatch;` |
|     18683 | 1485 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1486 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1487 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1488 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        51 | 1489 | `			 pCur++; /* Skip 'throw' */` |
|        51 | 1490 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|        51 | 1491 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     18582 | 1492 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1493 | `			 /* Language constructs [i.e: print,echo,die...] require special handling.` |
|         - | 1494 | `			  * Each of the six that reach here takes exactly ONE operand. */` |
|       381 | 1495 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       381 | 1496 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       193 | 1497 | `		 }else{` |
|         - | 1498 | `			 /* Assume a literal */` |
|     18183 | 1499 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     18183 | 1500 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1501 | `		 }` |
|   6612461 | 1502 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1503 | `		 /* Constants,function name,namespace path,class name... */` |
|   1699605 | 1504 | `		 if( bAfterMemberOp ){` |
|         - | 1505 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|         - | 1506 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|         - | 1507 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|         - | 1508 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|     34997 | 1509 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     17477 | 1510 | `		 }` |
|   1699605 | 1511 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   1699605 | 1512 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    848163 | 1513 | `	 }else{` |
|   4846150 | 1514 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1515 | `			 /* Point to the code generator routine */` |
|   1754189 | 1516 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   1754189 | 1517 | `			 if( pNode->xCode == 0 ){` |
|        35 | 1518 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|        35 | 1519 | `				 if( rc != SXERR_ABORT ){` |
|        35 | 1520 | `					 rc = SXERR_SYNTAX;` |
|        16 | 1521 | `				 }` |
|        35 | 1522 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        35 | 1523 | `				 return rc;` |
|         - | 1524 | `			 }` |
|    875499 | 1525 | `		 }` |
|         - | 1526 | `		/* Advance the stream cursor */` |
|   4846118 | 1527 | `		pCur++;` |
|         - | 1528 | `	 }` |
|         - | 1529 | `	/* Point to the end of the token stream */` |
|  14054700 | 1530 | `	pNode->pEnd = pCur;` |
|         - | 1531 | `	/* Save the node for later processing */` |
|  14054700 | 1532 | `	*ppNode = pNode;` |
|         - | 1533 | `	/* Synchronize cursors */` |
|  14054700 | 1534 | `	pGen->pIn = pCur;` |
|  14054700 | 1535 | `	return SXRET_OK;` |
|   7015808 | 1536 | `}` |
|         - | 1537 | `/*` |
|         - | 1538 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1539 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1540 | ` * level is zero.` |
|         - | 1541 | ` */` |
|    390799 | 1542 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1543 | `{` |
|    390804 | 1544 | `	SyToken *pCur = pStart;` |
|    390804 | 1545 | `	sxi32 iNest = 0;` |
|    390804 | 1546 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1547 | `		/* Last expression */` |
|    154411 | 1548 | `		return SXERR_EOF;` |
|         - | 1549 | `	}` |
|    747863 | 1550 | `	while( pCur < pEnd ){` |
|    708210 | 1551 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    196745 | 1552 | `			break;` |
|         - | 1553 | `		}` |
|    511470 | 1554 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     42684 | 1555 | `			iNest++;` |
|    490093 | 1556 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     42688 | 1557 | `			iNest--;` |
|     21304 | 1558 | `		}` |
|    511470 | 1559 | `		pCur++;` |
|         5 | 1560 | `	}` |
|    236398 | 1561 | `	*ppNext = pCur;` |
|    236398 | 1562 | `	return SXRET_OK;` |
|    195017 | 1563 | `}` |
|         - | 1564 | `/*` |
|         - | 1565 | ` * Release one node -- its own storage and the argument set it carries, and` |
|         - | 1566 | ` * nothing else. The tree it is part of is NOT walked: every node an expression` |
|         - | 1567 | ` * ever produced is in the extraction set, and that set is what owns them (see` |
|         - | 1568 | ` * PH7_ExprFreeTree).` |
|         - | 1569 | ` */` |
|  14054953 | 1570 | `static void ExprFreeNode(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1571 | `{` |
|  14054958 | 1572 | `	SySetRelease(&pNode->aNodeArgs);` |
|  14054958 | 1573 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  14054958 | 1574 | `}` |
|         - | 1575 | `/*` |
|         - | 1576 | ` * Free every node of an expression.` |
|         - | 1577 | ` *` |
|         - | 1578 | ` * The EXTRACTION SET owns the nodes, one entry per node, in the order the` |
|         - | 1579 | ` * tokens were read -- which is the only container that ever sees all of them.` |
|         - | 1580 | ` * Tree building runs on a COPY of that list and consumes it by NULLING each` |
|         - | 1581 | ` * slot it folds in, so a walk from the roots reaches only what ended up in a` |
|         - | 1582 | ` * tree: every delimiter a fold swallowed ('(' ')' '[' ']' ',' and the label` |
|         - | 1583 | ` * and colon of a named argument) was dropped from the copy and then reachable` |
|         - | 1584 | ` * from nothing. That leaked 17% of all the nodes an ordinary program compiles` |
|         - | 1585 | ` * -- 40,072 of 235,466 on one 40-file lint run, 5.1 MB held for the life of` |
|         - | 1586 | ` * the process -- because the compiler's AST is pool-allocated and the pool is` |
|         - | 1587 | ` * only handed back at VM teardown, so no leak checker ever named it.` |
|         - | 1588 | ` *` |
|         - | 1589 | ` * Freeing from the set instead of from the roots also makes the count exact in` |
|         - | 1590 | ` * the other direction: a node cannot be reached twice, so the ownership rule` |
|         - | 1591 | ` * that used to have to be maintained at every fold site ("null it here, free` |
|         - | 1592 | ` * it there, and never both") is gone.` |
|         - | 1593 | ` */` |
|   2721951 | 1594 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1595 | `{` |
|         - | 1596 | `	ph7_expr_node **apNode;` |
|         - | 1597 | `	sxu32 n;` |
|   2721956 | 1598 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  16776909 | 1599 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  14054958 | 1600 | `		if( apNode[n] ){` |
|  14054958 | 1601 | `			ExprFreeNode(&(*pGen),apNode[n]);` |
|   7015525 | 1602 | `		}` |
|   7015530 | 1603 | `	}` |
|   2721956 | 1604 | `	SySetReset(pNodeSet);` |
|   2721956 | 1605 | `	return SXRET_OK;` |
|         5 | 1606 | `}` |
|         - | 1607 | `/*` |
|         - | 1608 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1609 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1610 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1611 | ` * (PHP 8.0 makes this a compile fatal:` |
|         - | 1612 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1613 | ` */` |
|         - | 1614 | `/*` |
|         - | 1615 | `` * TRUE when this node's operator is a link of an ACCESS CHAIN -- `->`, `?->`,`` |
|         - | 1616 | `` * `::`, `[`, a call -- or one of the prefix unaries php hoists an assignment out`` |
|         - | 1617 | `` * of, so that `!$a?->b = 1` is `!($a?->b = 1)` and the `!` still stands over the`` |
|         - | 1618 | `` * target. Everything else (a comparison, `&&`, `+`) is a NEIGHBOUR of the target,`` |
|         - | 1619 | ` * not part of it.` |
|         - | 1620 | ` */` |
|   1067289 | 1621 | `static int ExprIsAccessChainRoot(ph7_expr_node *pNode)` |
|         5 | 1622 | `{` |
|   1067294 | 1623 | `	if( pNode == 0 \|\| pNode->pOp == 0 ){` |
|    870946 | 1624 | `		return 0;` |
|         - | 1625 | `	}` |
|    196348 | 1626 | `	if( pNode->pOp->iOp == EXPR_OP_ARROW \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|    193931 | 1627 | `	 \|\| pNode->pOp->iOp == EXPR_OP_DC \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|     96845 | 1628 | `	 \|\| pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|    196247 | 1629 | `		return 1;` |
|         - | 1630 | `	}` |
|       158 | 1631 | `	return (pNode->iFlags & EXPR_NODE_PARENS) == 0` |
|       174 | 1632 | `		&& (pNode->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|        68 | 1633 | `		 \|\| pNode->pOp->iOp == EXPR_OP_CLONE);` |
|    532885 | 1634 | `}` |
|         - | 1635 | `/*` |
|         - | 1636 | `` * TRUE when the ACCESS CHAIN this node roots contains a `?->`. Unlike`` |
|         - | 1637 | ` * PH7_ExprContainsNullsafe it stops at the first link that is not one, so a` |
|         - | 1638 | `` * nullsafe standing in a NEIGHBOURING operand -- `$o?->m() !== null && $x = 1`,`` |
|         - | 1639 | `` * where php's target is `$x` alone -- is not mistaken for one in the target.`` |
|         - | 1640 | ` */` |
|    871015 | 1641 | `static int ExprChainHasNullsafe(ph7_expr_node *pNode)` |
|         5 | 1642 | `{` |
|   1067288 | 1643 | `	while( ExprIsAccessChainRoot(pNode) ){` |
|    196301 | 1644 | `		if( pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        31 | 1645 | `			return 1;` |
|         - | 1646 | `		}` |
|    196273 | 1647 | `		pNode = pNode->pLeft;` |
|         5 | 1648 | `	}` |
|    870992 | 1649 | `	return 0;` |
|    434885 | 1650 | `}` |
|   3572270 | 1651 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1652 | `{` |
|   3572275 | 1653 | `	if( pNode == 0 ){` |
|   2314065 | 1654 | `		return 0;` |
|         - | 1655 | `	}` |
|   1258215 | 1656 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        20 | 1657 | `		return 1;` |
|         - | 1658 | `	}` |
|   1258197 | 1659 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|       ! 0 | 1660 | `		return 1;` |
|         - | 1661 | `	}` |
|   1258197 | 1662 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1663 | `		return 1;` |
|         - | 1664 | `	}` |
|   1258197 | 1665 | `	return 0;` |
|   1783568 | 1666 | `}` |
|         - | 1667 | `/*` |
|         - | 1668 | `` * TRUE when a `::` node names a class CONSTANT (`A::K`, `A::class`) rather than`` |
|         - | 1669 | `` * a static PROPERTY (`A::$s`, `A::$$name`).`` |
|         - | 1670 | ` *` |
|         - | 1671 | ` * php's grammar puts the two in different rules and only the property is a` |
|         - | 1672 | `` * `variable`: a class constant is a `constant`, which nothing but a DEREFERENCE`` |
|         - | 1673 | `` * (`A::K[0]`, `A::K->p`) can turn into one — and that dereference answers a`` |
|         - | 1674 | `` * TEMPORARY. So `A::K = 5`, `A::K++` and `unset(A::K)` are php PARSE errors`` |
|         - | 1675 | `` * while `A::K[0] = 5` is its "Cannot use temporary expression in write`` |
|         - | 1676 | `` * context". PHL treated every `::` as class-level STORAGE, so the first three`` |
|         - | 1677 | `` * were reported at runtime with a PH7-ism (or, for `A::K++` and `--A::K`, ran in`` |
|         - | 1678 | ` * silence) and the fourth wrote into a discarded copy of the constant.` |
|         - | 1679 | ` *` |
|         - | 1680 | `` * The token after `::` decides it: a static property always spells a `$`.`` |
|         - | 1681 | ` */` |
|       704 | 1682 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode)` |
|         5 | 1683 | `{` |
|       709 | 1684 | `	if( pNode == 0 \|\| pNode->pOp == 0 \|\| pNode->pOp->iOp != EXPR_OP_DC ){` |
|       213 | 1685 | `		return FALSE;` |
|         - | 1686 | `	}` |
|       501 | 1687 | `	if( pNode->pRight == 0 \|\| pNode->pRight->pStart == 0 ){` |
|         - | 1688 | `		/* Not linked yet / nothing to look at: keep the old permissive answer. */` |
|       ! 0 | 1689 | `		return FALSE;` |
|         - | 1690 | `	}` |
|       501 | 1691 | `	return (pNode->pRight->pStart->nType & PH7_TK_DOLLAR) == 0;` |
|       357 | 1692 | `}` |
|         - | 1693 | `/*` |
|         - | 1694 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1695 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1696 | ` *` |
|         - | 1697 | ` * This is the SHAPE question only: is the target an access chain at all?` |
|         - | 1698 | ` * Whether the chain's BASE may be written through is php's separate rule, made` |
|         - | 1699 | ` * at codegen by GenStateWriteTargetCheck (php: zend_compile_var_inner) so every` |
|         - | 1700 | `` * write kind — `=`, `+=`, `=&`, `++`, `unset()`, a foreach target — reaches it`` |
|         - | 1701 | ` * and reports php's own two refusals instead of a message naming the operator.` |
|         - | 1702 | ` */` |
|    872309 | 1703 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode)` |
|         5 | 1704 | `{` |
|         - | 1705 | `	sxi32 iExprOp;` |
|    872314 | 1706 | `	if( pNode->pOp == 0 ){` |
|    675952 | 1707 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1708 | `	}` |
|    196367 | 1709 | `	iExprOp = pNode->pOp->iOp;` |
|    196367 | 1710 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ ){` |
|      2375 | 1711 | `			return TRUE;` |
|         - | 1712 | `	}` |
|    193997 | 1713 | `	if( iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|         - | 1714 | ``		/* `C::$s` is storage; `C::K` is a constant, and php's grammar will not`` |
|         - | 1715 | `		 * take one as a write target at all. */` |
|       175 | 1716 | `		return PH7_ExprNodeIsClassConst(pNode) ? FALSE : TRUE;` |
|         - | 1717 | `	}` |
|    193827 | 1718 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|         - | 1719 | `		/* A subscript is a writable shape whatever it subscripts. php compiles` |
|         - | 1720 | ``		 * and RUNS `f()[0] = 5` and `str_split("ab")[0] = "z"` — the write lands`` |
|         - | 1721 | `		 * on the temporary the call answered and is discarded — and refuses a` |
|         - | 1722 | ``		 * literal/cast/computed/`new` base with a wording of its own. Screening`` |
|         - | 1723 | ``		 * the base here rejected both alike with `'=': Left operand must be a`` |
|         - | 1724 | ``		 * modifiable l-value`, and did it BEFORE the codegen check that knows`` |
|         - | 1725 | `		 * php's rules could speak. */` |
|    193731 | 1726 | `		return TRUE;` |
|         - | 1727 | `	}` |
|       100 | 1728 | `	if( iExprOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1729 | ``		/* A call is a shape both ways: as a reference SOURCE (`$r =& f()`) it is`` |
|         - | 1730 | `		 * php-legal, and as a write TARGET it is php's own compile fatal naming` |
|         - | 1731 | `		 * the kind of call, which GenStateWriteTargetCheck raises. */` |
|        44 | 1732 | `		return TRUE;` |
|         - | 1733 | `	}` |
|         - | 1734 | `	/* Not a modifiable l or r-value */` |
|        59 | 1735 | `	return FALSE;` |
|    435532 | 1736 | `}` |
|         - | 1737 | `/*` |
|         - | 1738 | `` * php refuses a write to something that is not a `variable` in its GRAMMAR, so`` |
|         - | 1739 | ` * what comes out is a SYNTAX error naming a token — never a sentence about the` |
|         - | 1740 | ` * operator, which is all PHL had ("'=': Left operand must be a modifiable` |
|         - | 1741 | `` * l-value", "'++' operator needs l-value", and two more for `=&` and `unset()`).`` |
|         - | 1742 | ` * Which token php names depends on which side of the operator the offending` |
|         - | 1743 | ` * operand sits, and both shapes are here:` |
|         - | 1744 | ` *` |
|         - | 1745 | `` *   the operand LEFT of the operator (`5 = 1`, `A::K += 1`, `(1+2)++`) —  php`` |
|         - | 1746 | ` *   has already shifted it and stops AT the operator, so that is what it names.` |
|         - | 1747 | ` */` |
|        10 | 1748 | `static sxi32 ExprWriteTargetNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOpNode)` |
|         3 | 1749 | `{` |
|         - | 1750 | `	sxi32 rc;` |
|        13 | 1751 | `	if( pOpNode->pOp && pOpNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1752 | ``		/* php lexes `=&` as `=` then `&`, and stops on the `=`. */`` |
|       ! 0 | 1753 | `		rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       ! 0 | 1754 | `			pOpNode->pStart ? pOpNode->pStart->nLine : 0,` |
|         - | 1755 | `			"syntax error, unexpected token \"=\"");` |
|       ! 0 | 1756 | `	}else{` |
|        13 | 1757 | `		rc = PH7_GenSyntaxError(pGen,pOpNode->pStart,0);` |
|         - | 1758 | `	}` |
|        13 | 1759 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         3 | 1760 | `}` |
|         - | 1761 | `/*` |
|         - | 1762 | ` * A token pointer one PAST the last token of a subtree is only a real token` |
|         - | 1763 | `` * while the subtree is not the last thing in the file. `<?php --A::K` (no`` |
|         - | 1764 | ` * terminator) walked off the end of the stream and named a garbage token on` |
|         - | 1765 | ` * line 0; php says "unexpected end of file" there, which is what` |
|         - | 1766 | ` * PH7_GenSyntaxError answers for a NULL. Returns pTok, or 0 when it is outside` |
|         - | 1767 | ` * the chunk's token stream.` |
|         - | 1768 | ` */` |
|        12 | 1769 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok)` |
|         3 | 1770 | `{` |
|         - | 1771 | `	SyToken *pBase, *pStreamEnd;` |
|        15 | 1772 | `	if( pTok == 0 \|\| pGen->pTokenSet == 0 ){` |
|       ! 0 | 1773 | `		return pTok;` |
|         - | 1774 | `	}` |
|        15 | 1775 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        15 | 1776 | `	pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        15 | 1777 | `	return (pTok >= pBase && pTok < pStreamEnd) ? pTok : 0;` |
|         9 | 1778 | `}` |
|         - | 1779 | `/*` |
|         - | 1780 | `` *   the operand RIGHT of the operator (`--A::K`, `$r =& "s"`, `unset(GK)`) — it`` |
|         - | 1781 | `` *   was shifted as a `constant`/`dereferencable_scalar`, so php runs past it and`` |
|         - | 1782 | ` *   stops on whatever FOLLOWS, still expecting the dereference that would have` |
|         - | 1783 | ` *   made it a variable. A bare integer/float is the exception: nothing in php's` |
|         - | 1784 | ` *   grammar dereferences one, so the literal itself is named.` |
|         - | 1785 | ` */` |
|        10 | 1786 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand)` |
|         2 | 1787 | `{` |
|        12 | 1788 | `	SyToken *pMin = 0, *pMax = 0;` |
|         - | 1789 | `	sxi32 rc;` |
|        10 | 1790 | `	if( pOperand && pOperand->pOp == 0 && pOperand->pStart` |
|         6 | 1791 | `	 && (pOperand->pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL)) ){` |
|       ! 0 | 1792 | `		rc = PH7_GenSyntaxError(pGen,pOperand->pStart,0);` |
|       ! 0 | 1793 | `	}else{` |
|         - | 1794 | `		/* The whole SUBTREE has to be stepped over, not just the node's own` |
|         - | 1795 | ``		 * tokens: `A::K` and `(1+2)` each named an inner token otherwise. The`` |
|         - | 1796 | `		 * span's max is already one past the last token. */` |
|        12 | 1797 | `		PH7_ExprSubtreeSpan(pOperand,&pMin,&pMax);` |
|        12 | 1798 | `		rc = PH7_GenSyntaxError(pGen,PH7_ExprTokenInStream(pGen,pMax),` |
|         - | 1799 | `			"\"->\" or \"?->\" or \"[\"");` |
|         - | 1800 | `	}` |
|        12 | 1801 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         2 | 1802 | `}` |
|         - | 1803 | `/* Forward declaration */` |
|         - | 1804 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1805 | `/* How many nodes of tree-building scratch PH7_ExprMakeTree carries in its own` |
|         - | 1806 | ` * frame. One pointer each, and an expression longer than this borrows from the` |
|         - | 1807 | ` * pool instead -- 64 covers everything a hand-written statement is likely to` |
|         - | 1808 | ` * be, and the frame only exists while ONE expression is being folded (a nested` |
|         - | 1809 | ` * one is compiled later, from the tree). */` |
|         - | 1810 | `#define EXPR_STACK_NODES 64` |
|         - | 1811 | `/* Macro to check if the given node is a terminal.` |
|         - | 1812 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1813 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1814 | ` * linked ternary/elvis node). */` |
|         - | 1815 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1816 | `/*` |
|         - | 1817 | ` * Buid an expression tree for each given function argument.` |
|         - | 1818 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1819 | ` */` |
|   1080791 | 1820 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1821 | `{` |
|         - | 1822 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1823 | `	sxi32 rc;` |
|         - | 1824 | ``	/* php: a stray token in a call argument is `... expecting ")"`. Each arg's`` |
|         - | 1825 | `	 * tree is built by the shared ExprMakeTree below, whose leftover-node error` |
|         - | 1826 | `	 * reads this. Saved/restored so a nested call or array element inside an arg` |
|         - | 1827 | `	 * gets its own closer. */` |
|   1080796 | 1828 | `	const char *zSaveArg = pGen->zClauseCloser;` |
|   1080796 | 1829 | `	pGen->zClauseCloser = "\")\"";` |
|         - | 1830 | `	/* Process function arguments from left to right */` |
|   1080796 | 1831 | `	iCur = 0;` |
|   1333921 | 1832 | `	for(;;){` |
|   2673455 | 1833 | `		if( iCur >= nToken ){` |
|         - | 1834 | `			/* No more arguments to process */` |
|   1080764 | 1835 | `			break;` |
|         - | 1836 | `		}` |
|   1592696 | 1837 | `		iNode = iCur;` |
|   1592696 | 1838 | `		iNest = 0;` |
|   4359117 | 1839 | `		while( iCur < nToken ){` |
|   3278356 | 1840 | `			if( apNode[iCur] ){` |
|   3200753 | 1841 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    255333 | 1842 | `					break;` |
|   2688818 | 1843 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   1442333 | 1844 | `					&& apNode[iCur]->pLeft == 0` |
|    200809 | 1845 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    197104 | 1846 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1847 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1848 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1849 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1850 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1851 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1852 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|         - | 1853 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|         - | 1854 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|         - | 1855 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|         - | 1856 | `					 * and swallow the following argument separator. */` |
|    193467 | 1857 | `					iNest++;` |
|   2591897 | 1858 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|   1342092 | 1859 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    193467 | 1860 | `					iNest--;` |
|     96536 | 1861 | `				}` |
|   1341697 | 1862 | `			}` |
|   2766426 | 1863 | `			iCur++;` |
|         5 | 1864 | `		}` |
|   1592696 | 1865 | `		if( iCur > iNode ){` |
|   1592690 | 1866 | `			SyString sArgName = {0, 0};` |
|         - | 1867 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1868 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1869 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   1592685 | 1870 | `			if( (iCur - iNode) >= 2` |
|    945029 | 1871 | `				&& apNode[iNode]` |
|    295893 | 1872 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    196019 | 1873 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     99667 | 1874 | `				&& apNode[iNode+1]` |
|     98537 | 1875 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1876 | `				/* Named argument detected: save the name and drop the label and` |
|         - | 1877 | `				 * colon nodes from the working copy. Dropping is all a fold ever` |
|         - | 1878 | `				 * does now -- the extraction set frees them (PH7_ExprFreeTree). */` |
|       765 | 1879 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       765 | 1880 | `				apNode[iNode] = 0;` |
|       765 | 1881 | `				apNode[iNode+1] = 0;` |
|       765 | 1882 | `				iNode += 2;` |
|         - | 1883 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1884 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       765 | 1885 | `				if( iNode >= iCur ){` |
|         4 | 1886 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1887 | `						pOp->pStart->nLine,` |
|         - | 1888 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1889 | `						&sArgName);` |
|         3 | 1890 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1891 | `						rc = SXERR_SYNTAX;` |
|         1 | 1892 | `					}` |
|         3 | 1893 | `					pGen->zClauseCloser = zSaveArg;` |
|         3 | 1894 | `					return rc;` |
|         - | 1895 | `				}` |
|       379 | 1896 | `			}` |
|   1592683 | 1897 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1898 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1899 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1900 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1901 | `					apNode[iNode] = 0;` |
|       ! 0 | 1902 | `			}` |
|         - | 1903 | `			{` |
|         - | 1904 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1905 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1906 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1907 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1908 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1909 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1910 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1911 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1912 | `				 * NULL slots ahead of the flagged subtree. */` |
|   1592688 | 1913 | `				int bSpreadArg = 0;` |
|         - | 1914 | `				sxi32 iScan;` |
|   1601462 | 1915 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   1601462 | 1916 | `					if( apNode[iScan] ){` |
|   1592688 | 1917 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   1592688 | 1918 | `						break;` |
|         - | 1919 | `					}` |
|      4385 | 1920 | `				}` |
|   1592688 | 1921 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   1592688 | 1922 | `				if( bSpreadArg && apNode[iNode] ){` |
|       378 | 1923 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       186 | 1924 | `				}` |
|         - | 1925 | `			}` |
|   1592688 | 1926 | `			if( apNode[iNode] ){` |
|   1592688 | 1927 | `				if( sArgName.nByte > 0 ){` |
|       763 | 1928 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       763 | 1929 | `					apNode[iNode]->sArgName = sArgName;` |
|       379 | 1930 | `				}` |
|         - | 1931 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   1592688 | 1932 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    794626 | 1933 | `			}else{` |
|         - | 1934 | `				/* No expression before comma */` |
|       ! 0 | 1935 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1936 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1937 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1938 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1939 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1940 | `				}` |
|       ! 0 | 1941 | `				pGen->zClauseCloser = zSaveArg;` |
|       ! 0 | 1942 | `				return rc;` |
|         - | 1943 | `			}` |
|    794626 | 1944 | `		}else{` |
|         - | 1945 | `			/* Comma with no preceding argument */` |
|         8 | 1946 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1947 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1948 | `				rc = SXERR_SYNTAX;` |
|         3 | 1949 | `			}` |
|         8 | 1950 | `			pGen->zClauseCloser = zSaveArg;` |
|         8 | 1951 | `			return rc;` |
|         - | 1952 | `		}` |
|         - | 1953 | `		/* Jump trailing comma */` |
|   1592688 | 1954 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    511929 | 1955 | `			iCur++;` |
|    511929 | 1956 | `			if( iCur >= nToken ){` |
|         - | 1957 | `				/* Trailing comma after last argument */` |
|        26 | 1958 | `				break;` |
|         - | 1959 | `			}` |
|    255313 | 1960 | `		}` |
|         5 | 1961 | `	}` |
|   1080788 | 1962 | `	pGen->zClauseCloser = zSaveArg;` |
|   1080788 | 1963 | `	return SXRET_OK;` |
|    539317 | 1964 | `}` |
|         - | 1965 | ` /*` |
|         - | 1966 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|         - | 1967 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|         - | 1968 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|         - | 1969 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|         - | 1970 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|         - | 1971 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|         - | 1972 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|         - | 1973 | `  */` |
|       236 | 1974 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|         5 | 1975 | ` {` |
|         - | 1976 | `	 SyToken *pMin;` |
|         - | 1977 | `	 SyToken *pChild;` |
|       241 | 1978 | `	 if( pNode == 0 ){` |
|       159 | 1979 | `		 return 0;` |
|         - | 1980 | `	 }` |
|        87 | 1981 | `	 pMin = pNode->pStart;` |
|        87 | 1982 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|        87 | 1983 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|         6 | 1984 | `		 pMin = pChild;` |
|         2 | 1985 | `	 }` |
|        87 | 1986 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|        87 | 1987 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|       ! 0 | 1988 | `		 pMin = pChild;` |
|       ! 0 | 1989 | `	 }` |
|        87 | 1990 | `	 return pMin;` |
|       123 | 1991 | ` }` |
|         - | 1992 | `/*` |
|         - | 1993 | ` * The full RAW token extent of a linked subtree: minimum pStart / maximum pEnd` |
|         - | 1994 | ` * over every node (pLeft/pRight/pCond and postfix aNodeArgs children), widened` |
|         - | 1995 | ` * by one token on each side for a node whose group parens were consumed by` |
|         - | 1996 | ` * ExprMakeTree's paren pass (EXPR_NODE_PARENS — its '('/')' slots were nulled,` |
|         - | 1997 | ` * but token contiguity guarantees they sit exactly one token outside the inner` |
|         - | 1998 | ` * extent). Tokens live in one contiguous set, so pointer min/max IS source` |
|         - | 1999 | ` * order. Consumed by the assert() source-text capture, which` |
|         - | 2000 | ` * needs the argument's whole source span where a root's own pStart/pEnd name` |
|         - | 2001 | `` * only the operator token. Note: nested redundant groups `((x))` share the`` |
|         - | 2002 | ` * single PARENS bit, so only one paren layer is recovered — the renderer` |
|         - | 2003 | ` * strips redundant outermost parens anyway, matching php's export.` |
|         - | 2004 | ` */` |
|       626 | 2005 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax)` |
|         5 | 2006 | `{` |
|         - | 2007 | `	SyToken *pMin;` |
|         - | 2008 | `	SyToken *pMax;` |
|       631 | 2009 | `	SyToken *pCMin = 0;` |
|       631 | 2010 | `	SyToken *pCMax = 0;` |
|         - | 2011 | `	ph7_expr_node **apArg;` |
|         - | 2012 | `	sxu32 n;` |
|       631 | 2013 | `	if( pNode == 0 ){` |
|       449 | 2014 | `		return;` |
|         - | 2015 | `	}` |
|       187 | 2016 | `	pMin = pNode->pStart;` |
|       187 | 2017 | `	pMax = pNode->pEnd;` |
|       187 | 2018 | `	PH7_ExprSubtreeSpan(pNode->pLeft,&pCMin,&pCMax);` |
|       187 | 2019 | `	PH7_ExprSubtreeSpan(pNode->pRight,&pCMin,&pCMax);` |
|       187 | 2020 | `	PH7_ExprSubtreeSpan(pNode->pCond,&pCMin,&pCMax);` |
|       187 | 2021 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       193 | 2022 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|         7 | 2023 | `		PH7_ExprSubtreeSpan(apArg[n],&pCMin,&pCMax);` |
|         4 | 2024 | `	}` |
|       187 | 2025 | `	if( pCMin && (pMin == 0 \|\| pCMin < pMin) ){` |
|        54 | 2026 | `		pMin = pCMin;` |
|        25 | 2027 | `	}` |
|       187 | 2028 | `	if( pCMax && (pMax == 0 \|\| pCMax > pMax) ){` |
|        60 | 2029 | `		pMax = pCMax;` |
|        28 | 2030 | `	}` |
|       187 | 2031 | `	if( (pNode->iFlags & EXPR_NODE_PARENS) && pMin && pMax ){` |
|         5 | 2032 | `		pMin--;` |
|         5 | 2033 | `		pMax++;` |
|         2 | 2034 | `	}` |
|       182 | 2035 | `	if( pNode->pOp && pMax` |
|        61 | 2036 | `	 && (pNode->pOp->iOp == EXPR_OP_FUNC_CALL \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT) ){` |
|         - | 2037 | `		/* A postfix call/subscript's extent stops AT its closing ')' / ']' (the` |
|         - | 2038 | `		 * closer's node was consumed building the postfix op); token contiguity` |
|         - | 2039 | `		 * puts the closer exactly at the extent, so widen one token past it. */` |
|         5 | 2040 | `		pMax++;` |
|         2 | 2041 | `	}` |
|       187 | 2042 | `	if( pMin && (*ppMin == 0 \|\| pMin < *ppMin) ){` |
|       137 | 2043 | `		*ppMin = pMin;` |
|        66 | 2044 | `	}` |
|       187 | 2045 | `	if( pMax && (*ppMax == 0 \|\| pMax > *ppMax) ){` |
|       185 | 2046 | `		*ppMax = pMax;` |
|        90 | 2047 | `	}` |
|       318 | 2048 | `}` |
|         - | 2049 | ` /*` |
|         - | 2050 | `  * Create an expression tree from an array of tokens.` |
|         - | 2051 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 2052 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2053 | `  */` |
|   4895492 | 2054 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 2055 | ` {` |
|         - | 2056 | `	 sxi32 i,iLeft,iRight;` |
|         - | 2057 | `	 ph7_expr_node *pNode;` |
|         - | 2058 | `	 ph7_expr_node *pSuppress;` |
|   4895497 | 2059 | `	 ph7_expr_node *pUnOuter = 0;` |
|         - | 2060 | `	 sxi32 iCur;` |
|         - | 2061 | `	 sxi32 rc;` |
|   4895497 | 2062 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 2063 | `		 /* TICKET 1433-17: self evaluating node */` |
|   2290824 | 2064 | `		 return SXRET_OK;` |
|         - | 2065 | `	 }` |
|         - | 2066 | `	 /* Process expressions enclosed in parenthesis first */` |
|  17422101 | 2067 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 2068 | `		 sxi32 iNest;` |
|         - | 2069 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 2070 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 2071 | `		  */` |
|  14817432 | 2072 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  14596035 | 2073 | `			 continue;` |
|         - | 2074 | `		 }` |
|    221402 | 2075 | `		 iNest = 1;` |
|    221402 | 2076 | `		 iLeft = iCur;` |
|         - | 2077 | `		 /* Find the closing parenthesis */` |
|    221402 | 2078 | `		 iCur++;` |
|   1460662 | 2079 | `		 while( iCur < nToken ){` |
|   1460662 | 2080 | `			 if( apNode[iCur] ){` |
|   1460662 | 2081 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 2082 | `					 /* Decrement nesting level */` |
|    328099 | 2083 | `					 iNest--;` |
|    328099 | 2084 | `					 if( iNest <= 0 ){` |
|    221402 | 2085 | `						 break;` |
|         5 | 2086 | `					 }` |
|   1185842 | 2087 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 2088 | `					 /* Increment nesting level */` |
|    106702 | 2089 | `					 iNest++;` |
|     53274 | 2090 | `				 }` |
|    618784 | 2091 | `			 }` |
|   1239265 | 2092 | `			 iCur++;` |
|         5 | 2093 | `		 }` |
|    221402 | 2094 | `		 if( iCur - iLeft > 1 ){` |
|         - | 2095 | `			 sxi32 j;` |
|         - | 2096 | `			 /* Recurse and process this expression */` |
|    221402 | 2097 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    221402 | 2098 | `			 if( rc != SXRET_OK ){` |
|         5 | 2099 | `				 return rc;` |
|         - | 2100 | `			 }` |
|         - | 2101 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 2102 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 2103 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 2104 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 2105 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 2106 | `			  * group's free below silently drops the unpacking. */` |
|    221398 | 2107 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    221398 | 2108 | `				 if( apNode[j] ){` |
|    221398 | 2109 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    221393 | 2110 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    221398 | 2111 | `					 break;` |
|         - | 2112 | `				 }` |
|       ! 0 | 2113 | `			 }` |
|    110550 | 2114 | `		 }` |
|         - | 2115 | `		 /* Drop the enclosing delimiters; the extraction set frees them. */` |
|    221398 | 2116 | `		 apNode[iLeft] = 0;` |
|    221398 | 2117 | `		 apNode[iCur] = 0;` |
|    110555 | 2118 | `	 }` |
|         - | 2119 | `	  /* Process expressions enclosed in braces */` |
|  18882366 | 2120 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 2121 | `		 sxi32 iNest;` |
|         - | 2122 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 2123 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 2124 | `		  */` |
|  16277697 | 2125 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  16277515 | 2126 | `			 continue;` |
|         - | 2127 | `		 }` |
|       186 | 2128 | `		 iNest = 1;` |
|       186 | 2129 | `		 iLeft = iCur;` |
|         - | 2130 | `		 /* Find the closing parenthesis */` |
|       186 | 2131 | `		 iCur++;` |
|       374 | 2132 | `		 while( iCur < nToken ){` |
|       374 | 2133 | `			 if( apNode[iCur] ){` |
|       374 | 2134 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 2135 | `					 /* Decrement nesting level */` |
|       186 | 2136 | `					 iNest--;` |
|       186 | 2137 | `					 if( iNest <= 0 ){` |
|       186 | 2138 | `						 break;` |
|       ! 0 | 2139 | `					 }` |
|       189 | 2140 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 2141 | `					 /* Increment nesting level */` |
|       ! 0 | 2142 | `					 iNest++;` |
|       ! 0 | 2143 | `				 }` |
|        94 | 2144 | `			 }` |
|       189 | 2145 | `			 iCur++;` |
|         1 | 2146 | `		 }` |
|       186 | 2147 | `		 if( iCur - iLeft > 1 ){` |
|         - | 2148 | `			 /* Recurse and process this expression */` |
|       177 | 2149 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       177 | 2150 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 2151 | `				 return rc;` |
|         - | 2152 | `			 }` |
|        88 | 2153 | `		 }` |
|         - | 2154 | `		 /* Drop the enclosing delimiters; the extraction set frees them. */` |
|       186 | 2155 | `		 apNode[iLeft] = 0;` |
|       186 | 2156 | `		 apNode[iCur] = 0;` |
|        95 | 2157 | `	 }` |
|         - | 2158 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   2604674 | 2159 | `	 iLeft = -1;` |
|  18882696 | 2160 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  16278041 | 2161 | `		 if( apNode[iCur] == 0 ){` |
|   6679448 | 2162 | `			 continue;` |
|         - | 2163 | `		 }` |
|   9598598 | 2164 | `		 pNode = apNode[iCur];` |
|   9598598 | 2165 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   1518465 | 2166 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 2167 | `				 /* Collect function arguments */` |
|   1121886 | 2168 | `				 sxi32 iPtr = 0;` |
|   1121886 | 2169 | `				 sxi32 nFuncTok = 0;` |
|   5522120 | 2170 | `				 while( nFuncTok + iCur < nToken ){` |
|   5522120 | 2171 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|         - | 2172 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|         - | 2173 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|         - | 2174 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|         - | 2175 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|         - | 2176 | `					  * nulled, so counting it here would over-count and never find` |
|         - | 2177 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   5522120 | 2178 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   5435524 | 2179 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   1247350 | 2180 | `							 iPtr++;` |
|   4810542 | 2181 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   1247350 | 2182 | `							 iPtr--;` |
|   1247350 | 2183 | `							 if( iPtr <= 0 ){` |
|   1121886 | 2184 | `								 break;` |
|         - | 2185 | `							 }` |
|     62585 | 2186 | `						 }` |
|   2152315 | 2187 | `					 }` |
|   4400239 | 2188 | `					 nFuncTok++;` |
|         5 | 2189 | `				 }` |
|   1121886 | 2190 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 2191 | `					 /* Syntax error */` |
|       ! 0 | 2192 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 2193 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2194 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2195 | `					 }` |
|       ! 0 | 2196 | `					 return rc;` |
|         - | 2197 | `				 }` |
|   1121886 | 2198 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 2199 | `					 /* Syntax error */` |
|       ! 0 | 2200 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 2201 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2202 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2203 | `					 }` |
|       ! 0 | 2204 | `					 return rc;` |
|         - | 2205 | `				 }` |
|   1121886 | 2206 | `				 if( nFuncTok > 1 ){` |
|         - | 2207 | `					 /* Process function arguments */` |
|   1080796 | 2208 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   1080796 | 2209 | `					 if( rc != SXRET_OK ){` |
|        10 | 2210 | `						 return rc;` |
|         - | 2211 | `					 }` |
|    539308 | 2212 | `				 }` |
|         - | 2213 | `				 /* Link the node to the tree */` |
|   1121878 | 2214 | `				 pNode->pLeft = apNode[iLeft];` |
|   1121878 | 2215 | `				 apNode[iLeft] = 0;` |
|   5522088 | 2216 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   4400215 | 2217 | `					 apNode[iCur+iPtr] = 0;` |
|   2195536 | 2218 | `				 }` |
|         - | 2219 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 2220 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 2221 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 2222 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 2223 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 2224 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 2225 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 2226 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 2227 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 2228 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 2229 | `				 {` |
|   1121878 | 2230 | `					 sxi32 iNew = iLeft - 1;` |
|   1164185 | 2231 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     42312 | 2232 | `						 iNew--;` |
|         5 | 2233 | `					 }` |
|   1121873 | 2234 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    623901 | 2235 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    363787 | 2236 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    112521 | 2237 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    112521 | 2238 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    112521 | 2239 | `						 apNode[iNew] = 0;` |
|    112521 | 2240 | `						 pNode = apNode[iCur];` |
|     56184 | 2241 | `					 }` |
|         - | 2242 | `				 }` |
|    956358 | 2243 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 2244 | `				 /* Subscripting */` |
|    359980 | 2245 | `				 sxi32 iArrTok = iCur + 1;` |
|    359980 | 2246 | `				 sxi32 iNest = 1;` |
|         - | 2247 | ``				 /* php's `dereferencable` list has `'(' expr ')'` in it, so WHATEVER a`` |
|         - | 2248 | `				  * parenthesised group evaluates to may be subscripted:` |
|         - | 2249 | ``				  * `((array)$o)['k']`, `((string)$s)[1]`, `(clone $o)[0]`, `(1+2)[0]`,`` |
|         - | 2250 | ``				  * `(1)[0]`. The base test below is a whitelist of node SHAPES, and no`` |
|         - | 2251 | `				  * shape describes "the user wrote parentheses", so every such group` |
|         - | 2252 | `				  * whose root was not already a term or a postfix chain was refused as` |
|         - | 2253 | `` 				  * `Invalid array name` — a compile fatal on source php runs. The `->` `` |
|         - | 2254 | `				  * branch further down reads the same flag for the same reason. */` |
|    359975 | 2255 | `				 if( !(iLeft >= 0 && apNode[iLeft] && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS))` |
|    359948 | 2256 | `					 && ( iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        60 | 2257 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        54 | 2258 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         - | 2259 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|         - | 2260 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|         - | 2261 | `					  * subscripting a global constant raised "Invalid array name" while` |
|         - | 2262 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|         - | 2263 | `					  * worked. */` |
|        54 | 2264 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        36 | 2265 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    359909 | 2266 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 2267 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 2268 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|       951 | 2269 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ) ){` |
|         - | 2270 | `						 /* Syntax error */` |
|       ! 0 | 2271 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 2272 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2273 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2274 | `						 }` |
|       ! 0 | 2275 | `						 return rc;` |
|         - | 2276 | `				 }` |
|         - | 2277 | `				 /* Collect index tokens */` |
|    647737 | 2278 | `				 while( iArrTok < nToken ){` |
|    647737 | 2279 | `					 if( apNode[iArrTok] ){` |
|    647705 | 2280 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 2281 | `							 /* Increment nesting level */` |
|        11 | 2282 | `							 iNest++;` |
|    647701 | 2283 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 2284 | `							 /* Decrement nesting level */` |
|    359988 | 2285 | `							 iNest--;` |
|    359988 | 2286 | `							 if( iNest <= 0 ){` |
|    359980 | 2287 | `								 break;` |
|         - | 2288 | `							 }` |
|         4 | 2289 | `						 }` |
|    143661 | 2290 | `					 }` |
|    287762 | 2291 | `					 ++iArrTok;` |
|         5 | 2292 | `				 }` |
|    359980 | 2293 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 2294 | ``					 /* php: a stray token in a subscript index is `... expecting "]"`. */`` |
|    271170 | 2295 | `					 const char *zSaveIdx = pGen->zClauseCloser;` |
|    271170 | 2296 | `					 pGen->zClauseCloser = "\"]\"";` |
|         - | 2297 | `					 /* Recurse and process this expression */` |
|    271170 | 2298 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    271170 | 2299 | `					 pGen->zClauseCloser = zSaveIdx;` |
|    271170 | 2300 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 2301 | `						 return rc;` |
|         - | 2302 | `					 }` |
|         - | 2303 | `					 /* Link the node to it's index */` |
|    271170 | 2304 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    135392 | 2305 | `				 }` |
|         - | 2306 | `				 /* Link the node to the tree */` |
|    359980 | 2307 | `				 pNode->pLeft = apNode[iLeft];` |
|    359980 | 2308 | `				 pNode->pRight = 0;` |
|    359980 | 2309 | `				 apNode[iLeft] = 0;` |
|   1007712 | 2310 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    647737 | 2311 | `					 apNode[iNest] = 0;` |
|    323415 | 2312 | `				 }` |
|    179738 | 2313 | `			 }else{` |
|         - | 2314 | `				 /* Member access operators [i.e: '->','::'] */` |
|     36609 | 2315 | `				  iRight = iCur + 1;` |
|     36785 | 2316 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       177 | 2317 | `					 iRight++;` |
|         1 | 2318 | `				 }` |
|     36609 | 2319 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2320 | `					 /* Syntax error */` |
|         5 | 2321 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 2322 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 2323 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2324 | `					 }` |
|         5 | 2325 | `					 return rc;` |
|         - | 2326 | `				 }` |
|         - | 2327 | `				 /* Validate the left operand BEFORE linking it. The refusal below` |
|         - | 2328 | `				  * used to be reached with the operand already installed as` |
|         - | 2329 | `				  * pNode->pLeft AND still standing in apNode[], which the recursive` |
|         - | 2330 | ``				  * release then freed twice -- a heap-use-after-free `1->x;`,`` |
|         - | 2331 | ``				  * `"s"->x;` and `[1]->x;` all reached. Nothing owns a node through`` |
|         - | 2332 | `				  * a tree any more (PH7_ExprFreeTree), so the order is no longer` |
|         - | 2333 | `				  * load-bearing; it is kept because refusing before mutating is the` |
|         - | 2334 | `				  * clearer shape either way. */` |
|     36600 | 2335 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|     34295 | 2336 | `					 && apNode[iLeft]->pOp == 0 &&` |
|     26731 | 2337 | `					 apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         - | 2338 | ``					 /* A PARENTHESISED group is php's `( expr )` dereferencable: whatever it`` |
|         - | 2339 | ``					  * evaluates to may be reached through `->`, which is how the closure`` |
|         - | 2340 | ``					  * idioms are written — `(function(){ … })->bindTo($o)`,`` |
|         - | 2341 | ``					  * `(fn() => …)->call($o)`, `(match($k){ … })->m()`. PHL refused all of`` |
|         - | 2342 | `					  * them as "Expecting a variable as left operand", a compile fatal on` |
|         - | 2343 | `					  * valid php, because a literal TERM carries no operator. */` |
|        46 | 2344 | `					 (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 &&` |
|         - | 2345 | ``					 /* php's `dereferencable` also covers the SCALAR forms — a quoted`` |
|         - | 2346 | `					  * string (interpolated or not) and an array literal — plus a` |
|         - | 2347 | ``					  * CONSTANT, and it runs them: `"s"->p` warns `Attempt to read`` |
|         - | 2348 | ``					  * property "p" on string` and yields null, `"s"->m()` is the`` |
|         - | 2349 | `					  * member-function Error. Refusing them at COMPILE time killed the` |
|         - | 2350 | `					  * whole file instead. A NUMBER literal and a heredoc stay refused` |
|         - | 2351 | ``					  * — those are php's own parse error for `1->x`. */`` |
|        26 | 2352 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString &&` |
|        24 | 2353 | `					 apNode[iLeft]->xCode != PH7_CompileString &&` |
|        18 | 2354 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        11 | 2355 | `					 apNode[iLeft]->xCode != PH7_CompileArray &&` |
|         4 | 2356 | `					 apNode[iLeft]->xCode != PH7_CompileShortArray ){` |
|         - | 2357 | `						 /* Syntax error */` |
|         4 | 2358 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2359 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|         3 | 2360 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2361 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2362 | `						 }` |
|         3 | 2363 | `						 return rc;` |
|         - | 2364 | `				 }` |
|         - | 2365 | `				 /* Link the node to the tree */` |
|     36603 | 2366 | `				 pNode->pLeft = apNode[iLeft];` |
|     36603 | 2367 | `				 pNode->pRight = apNode[iRight];` |
|     36603 | 2368 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 2369 | `			 }` |
|    757787 | 2370 | `		 }` |
|   9598584 | 2371 | `		 iLeft = iCur;` |
|   4791749 | 2372 | `	 }` |
|         - | 2373 | `	 /* Handle the prefix (new, clone) operators. Walk RIGHT to LEFT: both take the` |
|         - | 2374 | `	  * operand on their right, so a nested one has to be linked before the outer` |
|         - | 2375 | ``	  * sees it. Left-to-right, `clone new Q` reached the still-unlinked `new` node —`` |
|         - | 2376 | `	  * not a term yet — and answered php's own valid source with the compile fatal` |
|         - | 2377 | ``	  * "'clone': Expecting class constructor call". `clone new Q()` worked only`` |
|         - | 2378 | `	  * because the postfix pass folds a constructor CALL into its new-node early. */` |
|  18882660 | 2379 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  16278005 | 2380 | `		 if( apNode[iCur] == 0 ){` |
|   8310410 | 2381 | `			 continue;` |
|         - | 2382 | `		 }` |
|   7967600 | 2383 | `		 pNode = apNode[iCur];` |
|   7967600 | 2384 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 2385 | `			 SyToken *pToken;` |
|         - | 2386 | `			 /* Get the left node */` |
|      3439 | 2387 | `			 iLeft = iCur + 1;` |
|      3593 | 2388 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|       155 | 2389 | `				 iLeft++;` |
|         1 | 2390 | `			 }` |
|      3439 | 2391 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2392 | `				  /* Syntax error */` |
|       ! 0 | 2393 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 2394 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 2395 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2396 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2397 | `				 }` |
|       ! 0 | 2398 | `				 return rc;` |
|         - | 2399 | `			 }` |
|         - | 2400 | `			 /* Make sure the operand are of a valid type. CLONE takes ANY expression —` |
|         - | 2401 | ``			  * php's grammar is `clone expr`, and what that expression evaluates to is a`` |
|         - | 2402 | `			  * RUNTIME question: a non-object operand is php's catchable` |
|         - | 2403 | ``			  * `clone(): Argument #1 ($object) must be of type object, %s given`, which`` |
|         - | 2404 | `			  * OP_CLONE already raises. The whitelist that used to sit here (a variable,` |
|         - | 2405 | ``			  * or any operator node) refused `clone 5`, `clone []`, `clone null` and`` |
|         - | 2406 | ``			  * `clone match(…){…}` at COMPILE time — the first three with a diagnostic php`` |
|         - | 2407 | `			  * never prints, the last on source php runs. NEW keeps its own, because its` |
|         - | 2408 | `			  * operand is a class-name REFERENCE, not a value. */` |
|      3439 | 2409 | `			 if( pNode->pOp->iOp != EXPR_OP_CLONE ){` |
|         - | 2410 | `				 /* New */` |
|      3124 | 2411 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         7 | 2412 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 2413 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 2414 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 2415 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 2416 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 2417 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 2418 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 2419 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 2420 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2421 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2422 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2423 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2424 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2425 | `					 }` |
|       ! 0 | 2426 | `					 return rc;` |
|         - | 2427 | `				 }` |
|      3129 | 2428 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      3125 | 2429 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      3120 | 2430 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       191 | 2431 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 2432 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 2433 | `						 /* Syntax error */` |
|       ! 0 | 2434 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2435 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2436 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2437 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2438 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2439 | `						 }` |
|       ! 0 | 2440 | `						 return rc;` |
|         - | 2441 | `					 }` |
|      1558 | 2442 | `				 }` |
|      1560 | 2443 | `			 }` |
|         - | 2444 | `			  /* Link the node to the tree */` |
|      3439 | 2445 | `			 pNode->pLeft = apNode[iLeft];` |
|      3439 | 2446 | `			 apNode[iLeft] = 0;` |
|      3439 | 2447 | `			 pNode->pRight = 0; /* Paranoid */` |
|      1715 | 2448 | `		 }` |
|   3977772 | 2449 | `	 }` |
|         - | 2450 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   2604660 | 2451 | `	 iLeft = -1;` |
|  18882660 | 2452 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  16278005 | 2453 | `		 if( apNode[iCur] == 0 ){` |
|   8313844 | 2454 | `			 continue;` |
|         - | 2455 | `		 }` |
|   7964166 | 2456 | `		 pNode = apNode[iCur];` |
|   7964166 | 2457 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     72659 | 2458 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */` |
|         - | 2459 | ``					 /* …but `A::K++` is php's parse error, not an increment of`` |
|         - | 2460 | `					  * class-level storage: a class CONSTANT is not a variable. */` |
|       207 | 2461 | `					 && !PH7_ExprNodeIsClassConst(apNode[iLeft]))` |
|     64444 | 2462 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 2463 | `					 /* Link the node to the tree */` |
|     64542 | 2464 | `					 pNode->pLeft = apNode[iLeft];` |
|     64542 | 2465 | `					 apNode[iLeft] = 0;` |
|     32222 | 2466 | `			 }` |
|     36224 | 2467 | `		  }` |
|   7964166 | 2468 | `		 iLeft = iCur;` |
|   3976057 | 2469 | `	  }` |
|   2604660 | 2470 | `	 iLeft = -1;` |
|  18882650 | 2471 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  16277999 | 2472 | `		 if( apNode[iCur] == 0 ){` |
|   8378377 | 2473 | `			 continue;` |
|         - | 2474 | `		 }` |
|   7899627 | 2475 | `		 pNode = apNode[iCur];` |
|   7899627 | 2476 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      8015 | 2477 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      8018 | 2478 | `				 \|\| ( apNode[iLeft]->pOp && (apNode[iLeft]->pOp->iPrec != 2 /* Postfix */` |
|        28 | 2479 | `					 \|\| PH7_ExprNodeIsClassConst(apNode[iLeft]))) ){` |
|         - | 2480 | `					 /* Not a variable. Nothing to the right at all means the operator` |
|         - | 2481 | `					  * was POSTFIX and its target (already passed over) was refused,` |
|         - | 2482 | `					  * which is where php stops; otherwise this is a PREFIX operator` |
|         - | 2483 | `					  * over a non-variable and php stops past that operand. */` |
|         6 | 2484 | `					 if( iLeft < 0 ){` |
|         3 | 2485 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2486 | `					 }` |
|         3 | 2487 | `					 return PH7_ExprOperandNotAVariable(pGen,apNode[iLeft]);` |
|         - | 2488 | `			 }` |
|         - | 2489 | `			 /* Link the node to the tree */` |
|      8016 | 2490 | `			 pNode->pLeft = apNode[iLeft];` |
|      8016 | 2491 | `			 apNode[iLeft] = 0;` |
|         - | 2492 | `			 /* Mark as pre-increment/decrement node */` |
|      8016 | 2493 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      4000 | 2494 | `		  }` |
|   7899623 | 2495 | `		 iLeft = iCur;` |
|   3943832 | 2496 | `	 }` |
|         - | 2497 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|         - | 2498 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|         - | 2499 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|         - | 2500 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|         - | 2501 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|         - | 2502 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|         - | 2503 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|         - | 2504 | `	  * pass below skips it (pLeft != 0). */` |
|   2604656 | 2505 | `	 iLeft = -1;` |
|  18882640 | 2506 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  16277989 | 2507 | `		 if( apNode[iCur] == 0 ){` |
|   8402878 | 2508 | `			 continue;` |
|         - | 2509 | `		 }` |
|   7875116 | 2510 | `		 pNode = apNode[iCur];` |
|   7875116 | 2511 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|     16499 | 2512 | `			 iRight = iCur + 1;` |
|     16503 | 2513 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         5 | 2514 | `				 iRight++;` |
|         1 | 2515 | `			 }` |
|     16499 | 2516 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|       ! 0 | 2517 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2518 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2519 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2520 | `				 }` |
|       ! 0 | 2521 | `				 return rc;` |
|         - | 2522 | `			 }` |
|     16499 | 2523 | `			 pNode->pLeft = apNode[iLeft];` |
|     16499 | 2524 | `			 pNode->pRight = apNode[iRight];` |
|     16499 | 2525 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      8236 | 2526 | `		 }` |
|   7875116 | 2527 | `		 iLeft = iCur;` |
|   3931595 | 2528 | `	 }` |
|         - | 2529 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   2604656 | 2530 | `	  iLeft = 0;` |
|  18882634 | 2531 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  16277985 | 2532 | `		  if( apNode[iCur] ){` |
|   7858618 | 2533 | `			  pNode = apNode[iCur];` |
|   7858618 | 2534 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    222501 | 2535 | `				  if( iLeft > 0 ){` |
|         - | 2536 | `					  /* Link the node to the tree */` |
|    222499 | 2537 | `					  pNode->pLeft = apNode[iLeft];` |
|    222499 | 2538 | `					  apNode[iLeft] = 0;` |
|    222499 | 2539 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|         - | 2540 | `						  /* "Is the operand a finished subtree?" — a binary node fills` |
|         - | 2541 | `						   * pLeft+pRight and a full ternary fills all three, but a SHORT` |
|         - | 2542 | ``						   * ternary (`a ?: b`, which is what `!($a ?: $b)` hands here)`` |
|         - | 2543 | `						   * fills pCond+pRight and leaves pLeft NULL on purpose. Reading` |
|         - | 2544 | `						   * pLeft alone called it unfinished and refused source php` |
|         - | 2545 | `						   * compiles — every unary and cast over a parenthesised elvis. */` |
|      8156 | 2546 | `						  if( pNode->pLeft->pRight == 0` |
|      8161 | 2547 | `							  \|\| (pNode->pLeft->pLeft == 0 && pNode->pLeft->pCond == 0) ){` |
|         - | 2548 | `							   /* Syntax error */` |
|       ! 0 | 2549 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2550 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2551 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 2552 | `							  }` |
|       ! 0 | 2553 | `							  return rc;` |
|         - | 2554 | `						  }` |
|      4072 | 2555 | `					  }` |
|    111085 | 2556 | `				  }else{` |
|         - | 2557 | `					  /* Syntax error */` |
|         3 | 2558 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 2559 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 2560 | `						  rc = SXERR_SYNTAX;` |
|         1 | 2561 | `					  }` |
|         3 | 2562 | `					  return rc;` |
|         - | 2563 | `				  }` |
|    111080 | 2564 | `			  }` |
|         - | 2565 | `			  /* Save terminal position */` |
|   7858616 | 2566 | `			  iLeft = iCur;` |
|   3923351 | 2567 | `		  }` |
|   8125309 | 2568 | `	  }` |
|         - | 2569 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 2570 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 2571 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 2572 | `	  * yielding a right-leaning tree. */` |
|  18882632 | 2573 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  16277983 | 2574 | `		 if( apNode[iCur] == 0 ){` |
|   8642468 | 2575 | `			 continue;` |
|         - | 2576 | `		 }` |
|   7635520 | 2577 | `		 pNode = apNode[iCur];` |
|   7635520 | 2578 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 2579 | `			 sxi32 iL, iR;` |
|         - | 2580 | `			 /* Find the right operand */` |
|       604 | 2581 | `			 iR = -1;` |
|         - | 2582 | `			 {` |
|         - | 2583 | `				 sxi32 j;` |
|      1026 | 2584 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      1026 | 2585 | `					 if( apNode[j] ){ iR = j; break; }` |
|       212 | 2586 | `				 }` |
|         - | 2587 | `			 }` |
|         - | 2588 | `			 /* Find the left operand */` |
|       604 | 2589 | `			 iL = -1;` |
|         - | 2590 | `			 {` |
|         - | 2591 | `				 sxi32 j;` |
|      1176 | 2592 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      1176 | 2593 | `					 if( apNode[j] ){ iL = j; break; }` |
|       287 | 2594 | `				 }` |
|         - | 2595 | `			 }` |
|       604 | 2596 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 2597 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2598 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2599 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2600 | `				 }` |
|       ! 0 | 2601 | `				 return rc;` |
|         - | 2602 | `			 }` |
|       604 | 2603 | `			 pNode->pLeft  = apNode[iL];` |
|       604 | 2604 | `			 pNode->pRight = apNode[iR];` |
|       604 | 2605 | `			 apNode[iL] = 0;` |
|       604 | 2606 | `			 apNode[iR] = 0;` |
|         - | 2607 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 2608 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 2609 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 2610 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 2611 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 2612 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 2613 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 2614 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 2615 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 2616 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 2617 | `			  * operands are respected. */` |
|       602 | 2618 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       354 | 2619 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        98 | 2620 | `				 && pNode->pLeft->pLeft != 0` |
|        92 | 2621 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 2622 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 2623 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 2624 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 2625 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 2626 | `				 while( pTail->pLeft` |
|        34 | 2627 | `					 && pTail->pLeft->pOp` |
|        23 | 2628 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 2629 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 2630 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 2631 | `					 pTail = pTail->pLeft;` |
|         1 | 2632 | `				 }` |
|         - | 2633 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 2634 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 2635 | `				 pTail->pLeft = pNode;` |
|        27 | 2636 | `				 apNode[iCur] = pHead;` |
|        13 | 2637 | `			 }` |
|       301 | 2638 | `		 }` |
|   3811975 | 2639 | `	 }` |
|         - | 2640 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  28650998 | 2641 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  26046371 | 2642 | `		 iLeft = -1;` |
| 188825613 | 2643 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 162779269 | 2644 | `			 if( apNode[iCur] == 0 ){` |
| 104378617 | 2645 | `				 continue;` |
|         - | 2646 | `			 }` |
|  58400657 | 2647 | `			 pNode = apNode[iCur];` |
|  58400657 | 2648 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|   1555653 | 2649 | ``				 ph7_expr_node *pRefUn = 0;      /* `=&` under a prefix unary */`` |
|   1555653 | 2650 | `				 ph7_expr_node *pRefUnOuter = 0;` |
|   1555653 | 2651 | ``				 ph7_expr_node *pRefCmp = 0;     /* comparison to re-hang a `=&` under */`` |
|         - | 2652 | `				 /* Get the right node */` |
|   1555653 | 2653 | `				 iRight = iCur + 1;` |
|   2102416 | 2654 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    546768 | 2655 | `					 iRight++;` |
|         5 | 2656 | `				 }` |
|   1555653 | 2657 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2658 | `					 /* Syntax error */` |
|        10 | 2659 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        10 | 2660 | `					 if( rc != SXERR_ABORT ){` |
|        10 | 2661 | `						 rc = SXERR_SYNTAX;` |
|         4 | 2662 | `					 }` |
|        10 | 2663 | `					 return rc;` |
|         - | 2664 | `				 }` |
|   1555645 | 2665 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2666 | `					 sxi32  iTmp;` |
|         - | 2667 | ``					 /* php gives `=&` ASSIGNMENT precedence -- looser than every`` |
|         - | 2668 | ``					  * comparison -- so `null === $x =& $a[$k]` is`` |
|         - | 2669 | ``					  * `null === ($x =& $a[$k])`. This table processes `=&` at 12, one`` |
|         - | 2670 | ``					  * step TIGHTER than `===` at 11, because the operator's own rules`` |
|         - | 2671 | `					  * (the refusals below, the unary hoist, the nullsafe screens) live` |
|         - | 2672 | `					  * in this pass and moving it to 18 would leave all of them behind.` |
|         - | 2673 | `					  * A comparison has therefore already taken the variable this bind` |
|         - | 2674 | ``					  * means, and the bind was left with `(null === $x)` as its target --`` |
|         - | 2675 | `					  * not a variable, so the file was a parse error. symfony/translation` |
|         - | 2676 | `					  * writes exactly this shape, and it cost the whole file.` |
|         - | 2677 | `					  *` |
|         - | 2678 | `					  * Take the comparison's RIGHT operand as the bind target and hang` |
|         - | 2679 | `					  * the finished bind back under it, which is the tree php builds.` |
|         - | 2680 | ``					  * Parenthesised groups are left alone: `($a === $b) =& $c` really is`` |
|         - | 2681 | `					  * a refusal. */` |
|       466 | 2682 | `					 if( iLeft >= 0 && apNode[iLeft] && apNode[iLeft]->pOp` |
|       318 | 2683 | `					  && (apNode[iLeft]->pOp->iPrec == 10 \|\| apNode[iLeft]->pOp->iPrec == 11)` |
|        85 | 2684 | `					  && apNode[iLeft]->pRight != 0` |
|        90 | 2685 | `					  && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2686 | `						 pRefCmp = apNode[iLeft];` |
|       ! 0 | 2687 | `						 apNode[iLeft] = pRefCmp->pRight;` |
|       ! 0 | 2688 | `					 }` |
|         - | 2689 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 2690 | ``					 /* A prefix unary covers the whole BIND too — `@$a[0] =& $x` is`` |
|         - | 2691 | ``					  * `@($a[0] =& $x)`, and so are its `-`/`+`/`!`/`~`/cast spellings,`` |
|         - | 2692 | `					  * every one of which php runs. Same hoist the assignment path makes` |
|         - | 2693 | `					  * below, re-wrapped after the operands are swapped and linked. */` |
|         - | 2694 | `					 {` |
|       471 | 2695 | `						 ph7_expr_node *pUn = apNode[iLeft];` |
|       639 | 2696 | `						 while( pUn->pOp && pUn->pLeft` |
|       172 | 2697 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|       411 | 2698 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|       171 | 2699 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|         3 | 2700 | `							 pRefUn = pUn;      /* innermost unary over the bind target */` |
|         3 | 2701 | `							 pUn = pUn->pLeft;` |
|         1 | 2702 | `						 }` |
|       471 | 2703 | `						 if( pRefUn ){` |
|         3 | 2704 | `							 pRefUnOuter = apNode[iLeft]; /* the chain's result node */` |
|         3 | 2705 | `							 apNode[iLeft] = pUn;` |
|         1 | 2706 | `						 }` |
|         - | 2707 | `					 }` |
|         - | 2708 | `					 /* PHP 8.0: a reference and a nullsafe chain do not mix, and php` |
|         - | 2709 | `					  * has a different sentence for each SIDE — the bind target is a` |
|         - | 2710 | `					  * write like any other, while the SOURCE gets a wording of its` |
|         - | 2711 | `					  * own. Both operands are still in written order here; the swap` |
|         - | 2712 | `					  * below turns them over. */` |
|       466 | 2713 | `					 if( PH7_ExprContainsNullsafe(apNode[iLeft])` |
|       470 | 2714 | `					  \|\| PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        13 | 2715 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         8 | 2716 | `							 PH7_ExprContainsNullsafe(apNode[iLeft])` |
|         - | 2717 | `								 ? "Can't use nullsafe operator in write context"` |
|         - | 2718 | `								 : "Cannot take reference of a nullsafe chain");` |
|         9 | 2719 | `						 if( rc != SXERR_ABORT ){` |
|         9 | 2720 | `							 rc = SXERR_SYNTAX;` |
|         4 | 2721 | `						 }` |
|         9 | 2722 | `						 return rc;` |
|         - | 2723 | `					 }` |
|         - | 2724 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|         - | 2725 | `					  * reference target — PH7_ExprIsModifiableValue accepts` |
|         - | 2726 | ``					  * EXPR_OP_ARROW (`->`) and a static-PROPERTY `::`, and rejects`` |
|         - | 2727 | ``					  * both a class CONSTANT and the nullsafe `?->` form, so no extra`` |
|         - | 2728 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|         - | 2729 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|       463 | 2730 | `					 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2731 | ``						 /* The bind TARGET is not a variable: php stops at the `=`. */`` |
|       ! 0 | 2732 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2733 | `					 }` |
|       463 | 2734 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       369 | 2735 | `						 if(  PH7_ExprIsModifiableValue(apNode[iRight]) == FALSE ){` |
|         - | 2736 | ``							 /* The SOURCE has to be a variable too, and php's `&new` /`` |
|         - | 2737 | ``							  * `&clone` legacy productions each stop somewhere of their`` |
|         - | 2738 | ``							  * own: `$r =& clone $o` names the `clone` keyword (nothing`` |
|         - | 2739 | `` 							  * in a `variable` may start with it), while `$r =& new A` `` |
|         - | 2740 | ``							  * enters php 4's `&new` rule, which wants the ARGUMENT`` |
|         - | 2741 | ``							  * list — `expecting "("` — unless one was written, in`` |
|         - | 2742 | `							  * which case the rule completes and php asks for the` |
|         - | 2743 | `							  * dereference like everything else. PHL accepted BOTH` |
|         - | 2744 | `							  * spellings and silently bound a copy. */` |
|         6 | 2745 | `							 if( apNode[iRight]->pOp` |
|         8 | 2746 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_CLONE ){` |
|         3 | 2747 | `								 rc = PH7_GenSyntaxError(pGen,apNode[iRight]->pStart,0);` |
|         3 | 2748 | `								 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2749 | `							 }` |
|         4 | 2750 | `							 if( apNode[iRight]->pOp` |
|         5 | 2751 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_NEW ){` |
|         3 | 2752 | `								 SyToken *pNMin = 0, *pNMax = 0;` |
|         3 | 2753 | `								 PH7_ExprSubtreeSpan(apNode[iRight],&pNMin,&pNMax);` |
|         2 | 2754 | `								 if( pNMax == 0 \|\| pNMax <= apNode[iRight]->pStart` |
|         3 | 2755 | `								  \|\| (pNMax[-1].nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 2756 | `									 rc = PH7_GenSyntaxError(pGen,` |
|         1 | 2757 | `										 PH7_ExprTokenInStream(pGen,pNMax),"\"(\"");` |
|         3 | 2758 | `									 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2759 | `								 }` |
|       ! 0 | 2760 | `							 }` |
|         3 | 2761 | `							 return PH7_ExprOperandNotAVariable(pGen,apNode[iRight]);` |
|         - | 2762 | `						 }` |
|       179 | 2763 | `					 }` |
|         - | 2764 | `					 /* Swap operands */` |
|       457 | 2765 | `					 iTmp = iRight;` |
|       457 | 2766 | `					 iRight = iLeft;` |
|       457 | 2767 | `					 iLeft = iTmp;` |
|       226 | 2768 | `				 }` |
|         - | 2769 | `				 /* Link the node to the tree */` |
|   1555631 | 2770 | `				 pNode->pLeft = apNode[iLeft];` |
|   1555631 | 2771 | `				 pNode->pRight = apNode[iRight];` |
|   1555631 | 2772 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   1555631 | 2773 | `				 if( pRefUn ){` |
|         - | 2774 | `					 /* Re-wrap: the unary chain now covers the whole bind. */` |
|         3 | 2775 | `					 pRefUn->pLeft = pNode;` |
|         3 | 2776 | `					 apNode[iCur] = pRefUnOuter;` |
|         1 | 2777 | `				 }` |
|   1555631 | 2778 | `				 if( pRefCmp ){` |
|         - | 2779 | `					 /* Re-hang: the comparison now compares against the bind's value. */` |
|       ! 0 | 2780 | `					 pRefCmp->pRight = apNode[iCur] ? apNode[iCur] : pNode;` |
|       ! 0 | 2781 | `					 apNode[iCur] = pRefCmp;` |
|       ! 0 | 2782 | `				 }` |
|    776699 | 2783 | `			 }` |
|  58400635 | 2784 | `			 iLeft = iCur;` |
|  29155339 | 2785 | `		 }` |
|  13001332 | 2786 | `	 }` |
|         - | 2787 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2788 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2789 | `	  * we are dealing with a single operator.` |
|         - | 2790 | `	  */` |
|   2604632 | 2791 | `	  iLeft = -1;` |
|  18571904 | 2792 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  16011493 | 2793 | `		  if( apNode[iCur] == 0 ){` |
|  11620418 | 2794 | `			  continue;` |
|         - | 2795 | `		  }` |
|   4391080 | 2796 | `		  pNode = apNode[iCur];` |
|         - | 2797 | ``		  /* `pLeft == 0` alone does NOT mean "not linked yet" for this operator: a`` |
|         - | 2798 | ``		   * SHORT ternary (`a ?: b`) leaves pLeft NULL on purpose and records its`` |
|         - | 2799 | `		   * operands in pCond/pRight. A completed elvis node sitting in this slot —` |
|         - | 2800 | ``		   * which is what a parenthesised group leaves behind, `($a ?: $b)` — was`` |
|         - | 2801 | `		   * therefore re-entered here, and the term to its left is whatever the` |
|         - | 2802 | ``		   * enclosing expression put there (the `=` of `$x = ($a ?: $b);`), so the`` |
|         - | 2803 | `		   * "missing condition" branch fired on source php compiles. pCond is the` |
|         - | 2804 | `		   * real linked/not-linked marker, and the nesting scan below already` |
|         - | 2805 | `		   * reads it that way. */` |
|   4391075 | 2806 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0` |
|     44387 | 2807 | `			  && pNode->pCond == 0 ){` |
|     44221 | 2808 | `			  sxi32 iNest = 1;` |
|     44221 | 2809 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2810 | `				  /* Missing condition */` |
|         6 | 2811 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         6 | 2812 | `				  if( rc != SXERR_ABORT ){` |
|         6 | 2813 | `					  rc = SXERR_SYNTAX;` |
|         2 | 2814 | `				  }` |
|         6 | 2815 | `				  return rc;` |
|         - | 2816 | `			  }` |
|         - | 2817 | `			  /* Get the right node */` |
|     44217 | 2818 | `			  iRight = iCur + 1;` |
|    147233 | 2819 | `			  while( iRight < nToken  ){` |
|    147233 | 2820 | `				  if( apNode[iRight] ){` |
|     88275 | 2821 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2822 | `						  /* Increment nesting level */` |
|       ! 0 | 2823 | `						  ++iNest;` |
|     88275 | 2824 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2825 | `						  /* Decrement nesting level */` |
|     44217 | 2826 | `						  --iNest;` |
|     44217 | 2827 | `						  if( iNest <= 0 ){` |
|     44217 | 2828 | `							  break;` |
|         - | 2829 | `						  }` |
|       ! 0 | 2830 | `					  }` |
|     21999 | 2831 | `				  }` |
|    103021 | 2832 | `				  iRight++;` |
|         5 | 2833 | `			  }` |
|     44217 | 2834 | `			  if( iRight > iCur + 1 ){` |
|         - | 2835 | `				  /* Recurse and process the then expression */` |
|     44063 | 2836 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     44063 | 2837 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2838 | `					  return rc;` |
|         - | 2839 | `				  }` |
|         - | 2840 | `				  /* Link the node to the tree */` |
|     44063 | 2841 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     21999 | 2842 | `			  }else{` |
|         - | 2843 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2844 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2845 | `			  }` |
|     44217 | 2846 | `			  apNode[iCur + 1] = 0;` |
|     44217 | 2847 | `			  if( iRight + 1 < nToken ){` |
|         - | 2848 | `				  /* Recurse and process the else expression */` |
|     44217 | 2849 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     44217 | 2850 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2851 | `					  return rc;` |
|         - | 2852 | `				  }` |
|         - | 2853 | `				  /* Link the node to the tree */` |
|     44217 | 2854 | `				  pNode->pRight = apNode[iRight + 1];` |
|     44217 | 2855 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     22081 | 2856 | `			  }else{` |
|       ! 0 | 2857 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2858 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2859 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2860 | `				 }` |
|       ! 0 | 2861 | `				 return rc;` |
|         - | 2862 | `			  }` |
|         - | 2863 | `			  /* Point to the condition */` |
|     44217 | 2864 | `			  pNode->pCond  = apNode[iLeft];` |
|     44217 | 2865 | `			  apNode[iLeft] = 0;` |
|     44217 | 2866 | `			  break;` |
|         - | 2867 | `		  }` |
|   4346864 | 2868 | `		  iLeft = iCur;` |
|   2169995 | 2869 | `	  }` |
|         - | 2870 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2871 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2872 | `	  * so there is no need for a precedence loop here.` |
|         - | 2873 | `	  */` |
|   2604628 | 2874 | `	 iRight = -1;` |
|  18882170 | 2875 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  16277663 | 2876 | `		 if( apNode[iCur] == 0 ){` |
|  12801859 | 2877 | `			 continue;` |
|         - | 2878 | `		 }` |
|   3475809 | 2879 | `		 pNode = apNode[iCur];` |
|   3475809 | 2880 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2881 | `			 /* Get the left node */` |
|    871058 | 2882 | `			 iLeft = iCur - 1;` |
|   1171728 | 2883 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    300675 | 2884 | `				 iLeft--;` |
|         5 | 2885 | `			 }` |
|    871058 | 2886 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2887 | `				 /* Syntax error */` |
|        84 | 2888 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2889 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2890 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2891 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2892 | `				 }else{` |
|        80 | 2893 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2894 | `				 }` |
|        84 | 2895 | `				 if( rc != SXERR_ABORT ){` |
|        82 | 2896 | `					 rc = SXERR_SYNTAX;` |
|        39 | 2897 | `				 }` |
|        84 | 2898 | `				 return rc;` |
|         - | 2899 | `			 }` |
|         - | 2900 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2901 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2902 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2903 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2904 | `			  * a write.` |
|         - | 2905 | `			  *` |
|         - | 2906 | ``			  * The ACCESS CHAIN only. php binds `A op $lv = B` as `A op ($lv = B)`,`` |
|         - | 2907 | ``			  * so in `$o?->m() !== null && $x = 1` the target is `$x` and the`` |
|         - | 2908 | `			  * nullsafe belongs to a comparison that is merely READ -- a walk of the` |
|         - | 2909 | `			  * whole left operand refused that shape, which aws-sdk-php writes and so` |
|         - | 2910 | `			  * does anything that tests a nullsafe call and captures a value in one` |
|         - | 2911 | `			  * condition. When the left operand is such a NEIGHBOUR the spine walk` |
|         - | 2912 | `			  * below settles the target and asks the same question of it. */` |
|    870978 | 2913 | `			 if( ExprChainHasNullsafe(apNode[iLeft]) ){` |
|        27 | 2914 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2915 | `					 "Can't use nullsafe operator in write context");` |
|        27 | 2916 | `				 if( rc != SXERR_ABORT ){` |
|        27 | 2917 | `					 rc = SXERR_SYNTAX;` |
|        12 | 2918 | `				 }` |
|        27 | 2919 | `				 return rc;` |
|         - | 2920 | `			 }` |
|         - | 2921 | `			 /* Every PREFIX unary in php covers the whole assignment rather than just` |
|         - | 2922 | ``			  * its target: `@$x = expr` is `@($x = expr)`, and so are `-$x = 5`,`` |
|         - | 2923 | ``			  * `+$x = 5`, `!$x = 5`, `~$x = 5`, `(int)$x = 5` and `clone $o = 5` —`` |
|         - | 2924 | `			  * every one of them RUNS in php, writing $x and then applying the` |
|         - | 2925 | `			  * operator to the result. The unary phase has already bound the operator` |
|         - | 2926 | `			  * to the LHS, which leaves the assignment staring at a non-lvalue, so` |
|         - | 2927 | `			  * walk down to the innermost operand, let the assignment bind THERE, and` |
|         - | 2928 | ``			  * re-wrap below. Only `@` was handled here, so the other eight spellings`` |
|         - | 2929 | ``			  * did not compile at all. A PARENTHESISED operand (`(-$x) = 5`) is a`` |
|         - | 2930 | ``			  * genuine non-lvalue and stays refused, and `new` keeps its own`` |
|         - | 2931 | `			  * production, where php refuses too.` |
|         - | 2932 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    870954 | 2933 | `			 pSuppress = 0;` |
|    870954 | 2934 | `			 pUnOuter = 0;` |
|         - | 2935 | `			 {` |
|    870954 | 2936 | `				 ph7_expr_node *pUn = apNode[iLeft];` |
|   1066494 | 2937 | `				 while( pUn->pOp && pUn->pLeft` |
|    195524 | 2938 | `					 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|    631652 | 2939 | `					 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|    195503 | 2940 | `					  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        43 | 2941 | `					 pSuppress = pUn;  /* innermost unary seen so far */` |
|        43 | 2942 | `					 pUn = pUn->pLeft;` |
|         1 | 2943 | `				 }` |
|    870954 | 2944 | `				 if( pSuppress ){` |
|        37 | 2945 | `					 pUnOuter = apNode[iLeft]; /* the chain's result node */` |
|        37 | 2946 | `					 apNode[iLeft] = pUn;` |
|        18 | 2947 | `				 }` |
|         - | 2948 | `			 }` |
|    870954 | 2949 | `			 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2950 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|         - | 2951 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|         - | 2952 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|         - | 2953 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|         - | 2954 | `				  * assignment there, leaving the binary operator as the outer node.` |
|         - | 2955 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|       399 | 2956 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|        45 | 2957 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|        45 | 2958 | `					 ph7_expr_node *pParent = pHost;` |
|        66 | 2959 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|        13 | 2960 | `						 && ExprIsAccessChainRoot(pParent->pRight) == FALSE` |
|        27 | 2961 | `						 && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|       ! 0 | 2962 | `						 pParent = pParent->pRight;` |
|       ! 0 | 2963 | `					 }` |
|         - | 2964 | ``					 /* The spine ends on the target php would take. A `?->` in ITS chain`` |
|         - | 2965 | `					  * is the write refusal the head of this function makes for a target` |
|         - | 2966 | ``					  * standing alone -- `$q && $a?->b = 1` is `$q && ($a?->b = 1)`. */`` |
|        45 | 2967 | `					 if( ExprChainHasNullsafe(pParent->pRight) ){` |
|         4 | 2968 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2969 | `							 "Can't use nullsafe operator in write context");` |
|         4 | 2970 | `						 if( rc != SXERR_ABORT ){` |
|         4 | 2971 | `							 rc = SXERR_SYNTAX;` |
|         2 | 2972 | `						 }` |
|         4 | 2973 | `						 return rc;` |
|         - | 2974 | `					 }` |
|         - | 2975 | `					 /* The spine may end on a PREFIX unary rather than on the lvalue` |
|         - | 2976 | ``					  * itself -- `c && !$d = f()`, which php reads as`` |
|         - | 2977 | ``					  * `c && !($d = f())` exactly as it reads the unparenthesised`` |
|         - | 2978 | ``					  * `!$d = f()`. Walk that chain down the same way the top-level`` |
|         - | 2979 | `					  * hoist above does and let the assignment bind at its innermost` |
|         - | 2980 | `					  * operand, leaving the unary wrapped around the assignment. The` |
|         - | 2981 | `					  * spine walk stopped at the unary (a unary node has no pRight),` |
|         - | 2982 | ``					  * so the whole shape was `syntax error, unexpected token "="`. */`` |
|        41 | 2983 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|        21 | 2984 | `						 ph7_expr_node *pUn = pParent->pRight;` |
|        21 | 2985 | `						 ph7_expr_node *pInnerUn = 0;` |
|        42 | 2986 | `						 while( pUn->pOp && pUn->pLeft` |
|        16 | 2987 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|        36 | 2988 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|         8 | 2989 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        17 | 2990 | `							 pInnerUn = pUn;` |
|        17 | 2991 | `							 pUn = pUn->pLeft;` |
|         1 | 2992 | `						 }` |
|        18 | 2993 | `						 if( pInnerUn && PH7_ExprIsModifiableValue(pUn)` |
|        17 | 2994 | `							 && PH7_ExprContainsNullsafe(pUn) == 0 ){` |
|        15 | 2995 | `							 pNode->pLeft = apNode[iRight]; /* assignment RHS value */` |
|        15 | 2996 | `							 pNode->pRight = pUn;           /* the extracted lvalue */` |
|        15 | 2997 | `							 pInnerUn->pLeft = pNode;       /* the unary chain now covers it */` |
|        15 | 2998 | `							 apNode[iCur] = pHost;` |
|        15 | 2999 | `							 apNode[iLeft] = apNode[iRight] = 0;` |
|        15 | 3000 | `							 iRight = iCur;` |
|        15 | 3001 | `							 continue;` |
|         - | 3002 | `						 }` |
|         2 | 3003 | `					 }` |
|        24 | 3004 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight)` |
|        25 | 3005 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|        21 | 3006 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|        21 | 3007 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|        21 | 3008 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|        21 | 3009 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|        21 | 3010 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|        21 | 3011 | `						 iRight = iCur;` |
|        21 | 3012 | `						 continue;` |
|         - | 3013 | `					 }` |
|         2 | 3014 | `				 }` |
|       508 | 3015 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       352 | 3016 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 3017 | ``					 /* The target is not a `variable` in php's grammar: php stops at`` |
|         - | 3018 | `					  * the assignment operator itself, whatever the target was. */` |
|        11 | 3019 | `					 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 3020 | `				 }` |
|       174 | 3021 | `			 }` |
|         - | 3022 | `			 /* Link the node to the tree (Reverse) */` |
|    870908 | 3023 | `			 pNode->pLeft = apNode[iRight];` |
|    870908 | 3024 | `			 pNode->pRight = apNode[iLeft];` |
|    870908 | 3025 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    870908 | 3026 | `			 if( pSuppress ){` |
|         - | 3027 | `				 /* Re-wrap: the unary chain now covers the whole assignment. */` |
|        37 | 3028 | `				 pSuppress->pLeft = pNode;` |
|        37 | 3029 | `				 apNode[iCur] = pUnOuter;` |
|        18 | 3030 | `			 }` |
|    434824 | 3031 | `		 }` |
|   3475659 | 3032 | `		 iRight = iCur;` |
|   1735020 | 3033 | `	 }` |
|         - | 3034 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  13022540 | 3035 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  10418033 | 3036 | `		 iLeft = -1;` |
|  75528049 | 3037 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  65110021 | 3038 | `			 if( apNode[iCur] == 0 ){` |
|  54691435 | 3039 | `				 continue;` |
|         - | 3040 | `			 }` |
|  10418591 | 3041 | `			 pNode = apNode[iCur];` |
|  10418591 | 3042 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 3043 | `				 /* Get the right node */` |
|        67 | 3044 | `				 iRight = iCur + 1;` |
|        89 | 3045 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        24 | 3046 | `					 iRight++;` |
|         2 | 3047 | `				 }` |
|        67 | 3048 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 3049 | `					 /* Syntax error */` |
|       ! 0 | 3050 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 3051 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 3052 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 3053 | `					 }` |
|       ! 0 | 3054 | `					 return rc;` |
|         - | 3055 | `				 }` |
|         - | 3056 | `				 /* Link the node to the tree */` |
|        67 | 3057 | `				 pNode->pLeft = apNode[iLeft];` |
|        67 | 3058 | `				 pNode->pRight = apNode[iRight];` |
|        67 | 3059 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        31 | 3060 | `			 }` |
|  10418591 | 3061 | `			 iLeft = iCur;` |
|   5200560 | 3062 | `		 }` |
|   5200281 | 3063 | `	 }` |
|         - | 3064 | `	 /* Point to the root of the expression tree */` |
|  16277421 | 3065 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  13672986 | 3066 | `		 if( apNode[iCur] ){` |
|   2509572 | 3067 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         - | 3068 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|         - | 3069 | `				  * operator sitting at its slot. The "expecting" clause is the closer` |
|         - | 3070 | ``				  * the enclosing construct set (`;` after `return`, `,`/`;` after`` |
|         - | 3071 | ``				  * `echo`, `)` for a for() post clause …); a for() clause defaults to`` |
|         - | 3072 | ``				  * `;` when nothing more specific was set. php prints no clause for a`` |
|         - | 3073 | `				  * plain expression statement, so a NULL closer stays clauseless. */` |
|        77 | 3074 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|        77 | 3075 | `				 const char *zExpect = pGen->zClauseCloser;` |
|        77 | 3076 | `				 if( zExpect == 0 && pGen->nCommaExprOk > 0 ){` |
|       ! 0 | 3077 | `					 zExpect = "\";\"";` |
|       ! 0 | 3078 | `				 }` |
|        77 | 3079 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,zExpect);` |
|        77 | 3080 | `				  if( rc != SXERR_ABORT ){` |
|        77 | 3081 | `					  rc = SXERR_SYNTAX;` |
|        36 | 3082 | `				  }` |
|        77 | 3083 | `				  return rc;` |
|         - | 3084 | `			 }` |
|   2509500 | 3085 | `			 apNode[0] = apNode[iCur];` |
|   2509500 | 3086 | `			 apNode[iCur] = 0;` |
|   1252643 | 3087 | `		 }` |
|   6824959 | 3088 | `	 }` |
|   2604440 | 3089 | `	 return SXRET_OK;` |
|   2443320 | 3090 | ` }` |
|         - | 3091 | ` /*` |
|         - | 3092 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 3093 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 3094 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 3095 | `  * This is the public interface used by the most code generator routines.` |
|         - | 3096 | `  */` |
|   2721951 | 3097 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 3098 | `{` |
|         - | 3099 | `	ph7_expr_node *aStack[EXPR_STACK_NODES];` |
|         - | 3100 | `	ph7_expr_node **apNode;` |
|         - | 3101 | `	ph7_expr_node *pNode;` |
|         - | 3102 | `	sxu32 nNode;` |
|         - | 3103 | `	sxi32 rc;` |
|         - | 3104 | `	/* Reset node container */` |
|   2721956 | 3105 | `	SySetReset(pExprNode);` |
|   2721956 | 3106 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 3107 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 3108 | `	{` |
|   2721956 | 3109 | `		int iLastWasTerm = 0;` |
|   2721956 | 3110 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|   2721956 | 3111 | `		ph7_expr_node *pPrev = 0;` |
|  16776909 | 3112 | `		while( pGen->pIn < pGen->pEnd ){` |
|  14055046 | 3113 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  14055046 | 3114 | `			if( rc != SXRET_OK ){` |
|        76 | 3115 | `				return rc;` |
|         - | 3116 | `			}` |
|  14054969 | 3117 | `			if( pNode->xCode == PH7_CompileLiteral && pNode->pEnd == &pNode->pStart[1]` |
|   1726936 | 3118 | `			 && (pNode->pStart->nType & PH7_TK_KEYWORD)` |
|    871059 | 3119 | `			 && (pNode->pStart->nType & PH7_TK_MEMBER_NAME) == 0` |
|     18473 | 3120 | `			 && SX_PTR_TO_INT(pNode->pStart->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 3121 | ``				/* php's grammar takes a bare `static` in an expression in three`` |
|         - | 3122 | ``				 * places only: before `::`, and as the class of `new` and of`` |
|         - | 3123 | ``				 * `instanceof` (the closure forms were taken above). Anywhere else`` |
|         - | 3124 | ``				 * its parser names the token after it and asks for the `::` --`` |
|         - | 3125 | ``				 * `$x = static;` is a parse error there, where this went on to run`` |
|         - | 3126 | `				 * and failed at runtime on an undefined constant "static". */` |
|       176 | 3127 | `				int bOk = 0;` |
|       176 | 3128 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) ){` |
|       122 | 3129 | `					const ph7_expr_op *pOp = (const ph7_expr_op *)pGen->pIn->pUserData;` |
|       122 | 3130 | `					bOk = ( pOp && pOp->iOp == EXPR_OP_DC );` |
|        59 | 3131 | `				}` |
|       176 | 3132 | `				if( !bOk && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         - | 3133 | ``					/* `f(static: true)` -- a NAMED ARGUMENT may carry any semi-reserved`` |
|         - | 3134 | ``					 * word as its label, `static` included, and the label is the`` |
|         - | 3135 | `					 * keyword token followed by a single colon. */` |
|         3 | 3136 | `					bOk = 1;` |
|         1 | 3137 | `				}` |
|       172 | 3138 | `				if( !bOk && pPrev && pPrev->pOp` |
|        47 | 3139 | `				 && (pPrev->pOp->iOp == EXPR_OP_NEW \|\| pPrev->pOp->iOp == EXPR_OP_INSTOF) ){` |
|        40 | 3140 | `					bOk = 1;` |
|        19 | 3141 | `				}` |
|       176 | 3142 | `				if( !bOk ){` |
|        18 | 3143 | `					rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        18 | 3144 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        18 | 3145 | `					return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 3146 | `				}` |
|        78 | 3147 | `			}` |
|  14054958 | 3148 | `			pPrev = pNode;` |
|         - | 3149 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  14054958 | 3150 | `			if( pNode->xCode ){` |
|         - | 3151 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   7155315 | 3152 | `				iLastWasTerm = 1;` |
|  10471247 | 3153 | `			}else if( pNode->pOp ){` |
|         - | 3154 | `				/* Operator node */` |
|   3807687 | 3155 | `				iLastWasTerm = 0;` |
|   1900853 | 3156 | `			}else{` |
|         - | 3157 | `				/* Delimiter: ')' and ']' end terms */` |
|   3091966 | 3158 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 3159 | `			}` |
|         - | 3160 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 3161 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 3162 | `			 * node kind, so this single test covers all branches. */` |
|  14054958 | 3163 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 3164 | `			/* Save the extracted node */` |
|  14054958 | 3165 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 3166 | `		}` |
|         - | 3167 | `	}` |
|   2721868 | 3168 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 3169 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 3170 | `		*ppRoot = 0;` |
|       ! 0 | 3171 | `		return SXRET_OK;` |
|         - | 3172 | `	}` |
|         - | 3173 | `	/* Tree building CONSUMES its array -- every slot it folds into a tree, and` |
|         - | 3174 | `	 * every delimiter a fold swallows, is nulled -- so it cannot run on the set` |
|         - | 3175 | `	 * that OWNS the nodes (PH7_ExprFreeTree). It gets a copy, and the copy dies` |
|         - | 3176 | `	 * here: nothing downstream reads it, because a fold copies the node POINTER` |
|         - | 3177 | `	 * into pLeft/pRight/pCond or into the operator's aNodeArgs. An expression of` |
|         - | 3178 | `	 * up to EXPR_STACK_NODES tokens -- which is nearly all of them -- borrows the` |
|         - | 3179 | `	 * copy from this frame and allocates nothing at all. */` |
|   2721868 | 3180 | `	nNode = SySetUsed(pExprNode);` |
|   2721868 | 3181 | `	apNode = aStack;` |
|   2721868 | 3182 | `	if( nNode > EXPR_STACK_NODES ){` |
|        96 | 3183 | `		apNode = (ph7_expr_node **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,` |
|        31 | 3184 | `			nNode * sizeof(ph7_expr_node *));` |
|        65 | 3185 | `		if( apNode == 0 ){` |
|       ! 0 | 3186 | `			*ppRoot = 0;` |
|       ! 0 | 3187 | `			return SXERR_MEM;` |
|         - | 3188 | `		}` |
|        31 | 3189 | `	}` |
|   2721868 | 3190 | `	SyMemcpy(SySetBasePtr(pExprNode),(void *)apNode,nNode * sizeof(ph7_expr_node *));` |
|         - | 3191 | `	/* Make sure we are dealing with valid nodes */` |
|   2721868 | 3192 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)nNode);` |
|   2721868 | 3193 | `	if( rc == SXRET_OK ){` |
|         - | 3194 | `		/* Build the tree */` |
|   2721806 | 3195 | `		rc = ExprMakeTree(&(*pGen),apNode,(sxi32)nNode);` |
|   1358587 | 3196 | `	}` |
|         - | 3197 | `	/* On a syntax error the nodes stay where they are: the extraction set still` |
|         - | 3198 | `	 * holds every one of them and the caller releases it. */` |
|   2721868 | 3199 | `	*ppRoot = (rc == SXRET_OK) ? apNode[0] : 0;` |
|   2721868 | 3200 | `	if( apNode != aStack ){` |
|        65 | 3201 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,apNode);` |
|        31 | 3202 | `	}` |
|   2721868 | 3203 | `	return rc;` |
|   1358667 | 3204 | `}` |
|         - | 3205 |  |
