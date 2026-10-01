# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1480/1641 lines (90.19%)

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
|   3470578 |  290 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  291 | `{` |
|   3470583 |  292 | `	sxu32 n = 0;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `	/* Do a linear lookup on the operators table */` |
|  71432925 |  295 | `	for(;;){` |
| 143115257 |  296 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  297 | `			break;` |
|         - |  298 | `		}` |
| 143115257 |  299 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  300 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  11569306 |  301 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   5774341 |  302 | `		}else{` |
| 131545956 |  303 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  304 | `		}` |
| 143115257 |  305 | `		if( rc == 0 ){` |
|   3567354 |  306 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  307 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   3427288 |  308 | `				return &aOpTable[n];` |
|         - |  309 | `			}` |
|         - |  310 | `			/* Handle ambiguity */` |
|    140071 |  311 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  312 | `				/* Unary opertors have prcedence here over binary operators */` |
|     22237 |  313 | `				return &aOpTable[n];` |
|         - |  314 | `			}` |
|    117839 |  315 | `			if( pLast->nType & PH7_TK_OP ){` |
|     21076 |  316 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  317 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     21076 |  318 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  319 | `					/* Unary opertors have prcedence here over binary operators */` |
|     21068 |  320 | `					return &aOpTable[n];` |
|         - |  321 | `				}` |
|         - |  322 |  |
|         4 |  323 | `			}` |
|     48320 |  324 | `		}` |
| 139644679 |  325 | `		++n; /* Next operator in the table */` |
|         5 |  326 | `	}` |
|         - |  327 | `	/* No such operator */` |
|       ! 0 |  328 | `	return 0;` |
|   1732457 |  329 | `}` |
|         - |  330 | `/*` |
|         - |  331 | ` * Delimit a set of token stream.` |
|         - |  332 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  333 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  334 | ` */` |
|    950544 |  335 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  336 | `{` |
|    950549 |  337 | `	SyToken *pCur = pIn;` |
|    950549 |  338 | `	sxi32 iNest = 1;` |
|   4062206 |  339 | `	for(;;){` |
|   8136677 |  340 | `		if( pCur >= pEnd ){` |
|        24 |  341 | `			break;` |
|         - |  342 | `		}` |
|   8136657 |  343 | `		if( pCur->nType & nTokStart ){` |
|         - |  344 | `			/* Increment nesting level */` |
|    446370 |  345 | `			iNest++;` |
|   7913177 |  346 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  347 | `			/* Decrement nesting level */` |
|   1396894 |  348 | `			iNest--;` |
|   1396894 |  349 | `			if( iNest <= 0 ){` |
|    950529 |  350 | `				break;` |
|         - |  351 | `			}` |
|    222885 |  352 | `		}` |
|         - |  353 | `		/* Advance cursor */` |
|   7186133 |  354 | `		pCur++;` |
|         5 |  355 | `	}` |
|         - |  356 | `	/* Point to the end of the chunk */` |
|    950549 |  357 | `	*ppEnd = pCur;` |
|    950549 |  358 | `}` |
|         - |  359 | `/*` |
|         - |  360 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  361 | ` * Note on reserved keywords.` |
|         - |  362 | ` *  According to the PHP language reference manual:` |
|         - |  363 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  364 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  365 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  366 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  367 | ` */` |
|     16936 |  368 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  369 | `{` |
|     16936 |  370 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     16825 |  371 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  372 | `		){` |
|       485 |  373 | `			return TRUE;` |
|         - |  374 | `	}` |
|     16461 |  375 | `	if( bCheckFunc ){` |
|       864 |  376 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|       808 |  377 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       729 |  378 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       171 |  379 | `				return TRUE;` |
|         - |  380 | `		}` |
|       349 |  381 | `	}` |
|         - |  382 | `	/* Not a language construct */` |
|     16295 |  383 | `	return FALSE;` |
|      8464 |  384 | `}` |
|         - |  385 | `/*` |
|         - |  386 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  387 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  388 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  389 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  390 | ` */` |
|   2312086 |  391 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
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
|   2312091 |  406 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  407 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       145 |  408 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       145 |  409 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        70 |  410 | `	}` |
|   2312091 |  411 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  13966730 |  412 | `	for( i = 0 ; i < nNode ; ++i ){` |
|         - |  413 | ``		/* A closure LITERAL is not dereferencable in php: `function () {…}` and`` |
|         - |  414 | ``		 * `fn (…) => …` may not be followed by `(`, `[`, `->`, `?->` or `::` unless`` |
|         - |  415 | `		 * the user parenthesised them, which is why every IIFE in the wild is` |
|         - |  416 | ``		 * written `(function () {…})()`. PHL accepted all five spellings — a`` |
|         - |  417 | `		 * silent acceptance of what php refuses, and the reason it was found: the` |
|         - |  418 | ``		 * statement-position rule above sends `function () {…};` down this path,`` |
|         - |  419 | ``		 * and without the screen `function () {…}();` would have become the one`` |
|         - |  420 | `		 * shape php rejects that PHL runs. */` |
|  11662633 |  421 | `		if( (apNode[i]->xCode == PH7_CompileAnnonFunc \|\| apNode[i]->xCode == PH7_CompileArrowFunc)` |
|   5832625 |  422 | `		 && (apNode[i]->iFlags & EXPR_NODE_PARENS) == 0` |
|     18382 |  423 | `		 && i + 1 < nNode && apNode[i+1] ){` |
|     12314 |  424 | `			ph7_expr_node *pAfter = apNode[i+1];` |
|     12314 |  425 | `			int bDeref = (pAfter->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB)) != 0;` |
|     12309 |  426 | `			if( !bDeref && pAfter->pOp` |
|      6463 |  427 | `			 && ( pAfter->pOp->iOp == EXPR_OP_ARROW` |
|       421 |  428 | `			   \|\| pAfter->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       419 |  429 | `			   \|\| pAfter->pOp->iOp == EXPR_OP_DC ) ){` |
|         7 |  430 | `				bDeref = 1;` |
|         3 |  431 | `			}` |
|     12314 |  432 | `			if( bDeref ){` |
|        11 |  433 | `				rc = PH7_GenSyntaxError(pGen,pAfter->pStart,0);` |
|        11 |  434 | `				if( rc != SXERR_ABORT ){` |
|        11 |  435 | `					rc = SXERR_SYNTAX;` |
|         5 |  436 | `				}` |
|        11 |  437 | `				return rc;` |
|         - |  438 | `			}` |
|      6035 |  439 | `		}` |
|  11662628 |  440 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  441 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     17884 |  442 | `			continue;` |
|         - |  443 | `		}` |
|  11636801 |  444 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|         - |  445 | `			/* A short-array literal is a SELF-CONTAINED node whose start token is '['` |
|         - |  446 | `			 * (its ']' was consumed), so the raw-token CSB test below can never see it —` |
|         - |  447 | ``			 * `[$obj, 'm']()` parsed the '(' as a grouping paren and silently DROPPED`` |
|         - |  448 | `			 * the call (the expression evaluated to the array). php invokes the literal` |
|         - |  449 | `			 * array callable exactly like the variable-held form. */` |
|   1192160 |  450 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    135787 |  451 | `				apNode[i-1]->xCode == PH7_CompileShortArray \|\|` |
|    135769 |  452 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  453 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis.` |
|         - |  454 | `					 * A self-contained short-array node is exempt: its start token is '[',` |
|         - |  455 | `					 * which carries PH7_TK_OP as the subscript operator, but the node is a` |
|         - |  456 | ``					 * complete array-literal TERM — `[$obj, 'm'](...)` is a call. */`` |
|    934683 |  457 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0` |
|    466361 |  458 | `					 \|\| apNode[i-1]->xCode == PH7_CompileShortArray ){` |
|         - |  459 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  460 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  461 | `						 */` |
|    934670 |  462 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|    934670 |  463 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|    934670 |  464 | `						apNode[i]->pOp = &sFCallOp;` |
|    466329 |  465 | `					}` |
|    466338 |  466 | `			}` |
|   1124365 |  467 | `			iParen++;` |
|  11073497 |  468 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   1124363 |  469 | `			if( iParen <= 0 ){` |
|        16 |  470 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ')'");` |
|        16 |  471 | `				if( rc != SXERR_ABORT ){` |
|        16 |  472 | `					rc = SXERR_SYNTAX;` |
|         6 |  473 | `				}` |
|        16 |  474 | `				return rc;` |
|         - |  475 | `			}` |
|   1124351 |  476 | `			iParen--;` |
|   9949132 |  477 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    265481 |  478 | `			iSquare++;` |
|   9255162 |  479 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    265485 |  480 | `			if( iSquare <= 0 ){` |
|         9 |  481 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched ']'");` |
|         9 |  482 | `				if( rc != SXERR_ABORT ){` |
|         9 |  483 | `					rc = SXERR_SYNTAX;` |
|         3 |  484 | `				}` |
|         9 |  485 | `				return rc;` |
|         - |  486 | `			}` |
|    265479 |  487 | `			iSquare--;` |
|   8989681 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       186 |  489 | `			iBraces++;` |
|       186 |  490 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  491 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  492 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  493 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  494 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  495 | `				if( rc != SXERR_ABORT ){` |
|         3 |  496 | `					rc = SXERR_SYNTAX;` |
|         1 |  497 | `				}` |
|         3 |  498 | `				return rc;` |
|         4 |  499 | `			}` |
|   8857035 |  500 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       197 |  501 | `			if( iBraces <= 0 ){` |
|        16 |  502 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Unmatched '}'");` |
|        16 |  503 | `				if( rc != SXERR_ABORT ){` |
|        16 |  504 | `					rc = SXERR_SYNTAX;` |
|         6 |  505 | `				}` |
|        16 |  506 | `				return rc;` |
|         - |  507 | `			}` |
|       184 |  508 | `			iBraces--;` |
|   8856843 |  509 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     32000 |  510 | `			sxi32 iDepth = iParen + iSquare + iBraces;` |
|     31995 |  511 | `			if( iQuesty > 0` |
|     31663 |  512 | `			 && ( iQuesty > (sxi32)SX_ARRAYSIZE(aQuestyDepth)` |
|     31321 |  513 | `			   \|\| aQuestyDepth[iQuesty - 1] == iDepth ) ){` |
|     31310 |  514 | `				iQuesty--;` |
|     16327 |  515 | `			}else if( iParen <= 0 ){` |
|         - |  516 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  517 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  518 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  519 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  520 | `				if( rc != SXERR_ABORT ){` |
|         6 |  521 | `					rc = SXERR_SYNTAX;` |
|         2 |  522 | `				}` |
|         6 |  523 | `				return rc;` |
|         5 |  524 | `			}` |
|   8840733 |  525 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   2881777 |  526 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   2881777 |  527 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     31312 |  528 | `				if( iQuesty < (sxi32)SX_ARRAYSIZE(aQuestyDepth) ){` |
|     31312 |  529 | `					aQuestyDepth[iQuesty] = iParen + iSquare + iBraces;` |
|     15633 |  530 | `				}` |
|     31312 |  531 | `				iQuesty++;` |
|   2866103 |  532 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     42908 |  533 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|     21419 |  551 | `			}` |
|   1438615 |  552 | `		}` |
|   5808450 |  553 | `	}` |
|   2304097 |  554 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        18 |  555 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        18 |  556 | `		if( rc != SXERR_ABORT ){` |
|        18 |  557 | `			rc = SXERR_SYNTAX;` |
|         7 |  558 | `		}` |
|        18 |  559 | `		return rc;` |
|         - |  560 | `	}` |
|   2304083 |  561 | `	return SXRET_OK;` |
|   1150103 |  562 | `}` |
|         - |  563 | `/*` |
|         - |  564 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  565 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  566 | ` */` |
|   1445383 |  567 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  568 | `{` |
|   1445388 |  569 | `	SyToken *pIn = *ppCur;` |
|         - |  570 | `	/* Jump the first literal seen */` |
|   1445388 |  571 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   1445178 |  572 | `		pIn++;` |
|    721165 |  573 | `	}` |
|    721629 |  574 | `	for(;;){` |
|   1446107 |  575 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       724 |  576 | `			pIn++;` |
|       724 |  577 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       722 |  578 | `				pIn++;` |
|       358 |  579 | `			}` |
|       364 |  580 | `		}else{` |
|    721275 |  581 | `			break;` |
|         - |  582 | `		}` |
|         5 |  583 | `	}` |
|         - |  584 | `	/* Synchronize pointers */` |
|   1445388 |  585 | `	*ppCur = pIn;` |
|   1445388 |  586 | `}` |
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
|     16288 |  631 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  632 | `{` |
|     16293 |  633 | `	SyToken *pIn = *ppIn;` |
|     16293 |  634 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|       129 |  635 | `		pIn++; /* Skip ':' */` |
|        57 |  636 | `		for(;;){` |
|         - |  637 | `			/* Optional '?' nullable prefix */` |
|       133 |  638 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|         6 |  639 | `				pIn++;` |
|         2 |  640 | `			}` |
|       133 |  641 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  642 | `				/* Parenthesized DNF group '(A&B)' */` |
|       ! 0 |  643 | `				pIn++;` |
|       ! 0 |  644 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       ! 0 |  645 | `				if( pIn < pEnd ){` |
|       ! 0 |  646 | `					pIn++; /* ')' */` |
|       ! 0 |  647 | `				}` |
|       128 |  648 | `			}else if( pIn < pEnd` |
|       133 |  649 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  650 | `				/* ['\']Name('\'Name)* */` |
|       133 |  651 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|       133 |  652 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       133 |  653 | `					pIn++;` |
|       133 |  654 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  655 | `						pIn += 2;` |
|       ! 0 |  656 | `					}` |
|        59 |  657 | `				}` |
|        64 |  658 | `			}else{` |
|         - |  659 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  660 | `				break;` |
|         - |  661 | `			}` |
|         - |  662 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|       128 |  663 | `			if( pIn < pEnd` |
|       133 |  664 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|       128 |  665 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|         5 |  666 | `				pIn++;` |
|         5 |  667 | `				continue;` |
|         - |  668 | `			}` |
|       129 |  669 | `			break;` |
|       ! 0 |  670 | `		}` |
|        57 |  671 | `	}` |
|     16293 |  672 | `	*ppIn = pIn;` |
|     16293 |  673 | `}` |
|      6285 |  674 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  675 | `{` |
|      6290 |  676 | `	SyToken *pIn = *ppCur;` |
|         - |  677 | `	sxi32 rc;` |
|         - |  678 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  679 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  680 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  681 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  682 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|      6290 |  683 | `	pIn++;` |
|      6285 |  684 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      3301 |  685 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|       343 |  686 | `		pIn++;` |
|       169 |  687 | `	}` |
|         - |  688 | ``	/* `function &(…) {…}` returns by reference, exactly as the named form does, and`` |
|         - |  689 | ``	 * the `&` sits in the same place. The node assembler is what has to step over`` |
|         - |  690 | `	 * it (PH7_CompileAnnonFunc reads it again for the flag); leaving it here made` |
|         - |  691 | ``	 * every by-ref closure `syntax error, unexpected token "&", expecting "("`. */`` |
|      6290 |  692 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        14 |  693 | `		pIn++;` |
|         6 |  694 | `	}` |
|      6290 |  695 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  696 | `		/* Syntax error */` |
|         6 |  697 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  698 | `		if( rc != SXERR_ABORT ){` |
|         6 |  699 | `			rc = SXERR_SYNTAX;` |
|         2 |  700 | `		}` |
|         6 |  701 | `		goto Synchronize;` |
|         - |  702 | `	}` |
|      6286 |  703 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      6286 |  704 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      6286 |  705 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
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
|      6280 |  721 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  722 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      6280 |  723 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      6280 |  724 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|      1888 |  725 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  726 | `		/* Check if we are dealing with a closure */` |
|      1888 |  727 | `		if( nKey == PH7_TKWRD_USE ){` |
|      1880 |  728 | `			pIn++; /* Jump the 'use' keyword */` |
|      1880 |  729 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  730 | `				/* Syntax error */` |
|         6 |  731 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  732 | `				if( rc != SXERR_ABORT ){` |
|         6 |  733 | `					rc = SXERR_SYNTAX;` |
|         2 |  734 | `				}` |
|         6 |  735 | `				goto Synchronize;` |
|         - |  736 | `			}` |
|      1876 |  737 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|         - |  738 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|         - |  739 | `			 * has no nested structure to balance here, so the first token that is not` |
|         - |  740 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|         - |  741 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|         - |  742 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|         - |  743 | `			 * the first foreign token. */` |
|         - |  744 | `			{` |
|      1876 |  745 | `				SyToken *pUse = pIn;` |
|      1876 |  746 | `				int bClosed = 0;` |
|      6774 |  747 | `				while( pUse < pEnd ){` |
|      6774 |  748 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|      4905 |  749 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      4903 |  750 | `						pUse++;` |
|      4903 |  751 | `						continue;` |
|         - |  752 | `					}` |
|         3 |  753 | `					break; /* foreign token: php names this one */` |
|       ! 0 |  754 | `				}` |
|      1876 |  755 | `				if( !bClosed ){` |
|         - |  756 | `					/* php names the offending token and expects ')'; if the list simply` |
|         - |  757 | `					 * ran off the end of the slice, that token sits just past it. */` |
|         3 |  758 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|         3 |  759 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|         3 |  760 | `					if( rc != SXERR_ABORT ){` |
|         3 |  761 | `						rc = SXERR_SYNTAX;` |
|         1 |  762 | `					}` |
|         3 |  763 | `					goto Synchronize;` |
|         - |  764 | `				}` |
|      1874 |  765 | `				pIn = pUse; /* on the ')' */` |
|         - |  766 | `			}` |
|      1874 |  767 | `			if( &pIn[1] >= pEnd ){` |
|         - |  768 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|         3 |  769 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         3 |  770 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         3 |  771 | `				if( rc != SXERR_ABORT ){` |
|         3 |  772 | `					rc = SXERR_SYNTAX;` |
|         1 |  773 | `				}` |
|         3 |  774 | `				goto Synchronize;` |
|         - |  775 | `			}` |
|      1872 |  776 | `			pIn++;` |
|         - |  777 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  778 | ``			 * `function (...) use (...) : int {` */`` |
|      1872 |  779 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       931 |  780 | `		}else{` |
|         - |  781 | `			/* Syntax error */` |
|        11 |  782 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        11 |  783 | `			if( rc != SXERR_ABORT ){` |
|        11 |  784 | `				rc = SXERR_SYNTAX;` |
|         4 |  785 | `			}` |
|        11 |  786 | `			goto Synchronize;` |
|         - |  787 | `		}` |
|       926 |  788 | `	}` |
|         - |  789 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  790 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  791 | `	 * the type), and pEnd is one past the last token. */` |
|      6264 |  792 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      6264 |  793 | `		pIn++; /* Jump the leading curly '{' */` |
|      6264 |  794 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      6264 |  795 | `		if( pIn < pEnd ){` |
|      6264 |  796 | `			pIn++;` |
|      3113 |  797 | `		}` |
|      3118 |  798 | `	}else{` |
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
|      6264 |  809 | `	rc = SXRET_OK;` |
|      3159 |  810 | `Synchronize:` |
|         - |  811 | `	/* Synchronize pointers */` |
|      6290 |  812 | `	*ppCur = pIn;` |
|      6290 |  813 | `	return rc;` |
|      3131 |  814 | `}` |
|         - |  815 | `/*` |
|         - |  816 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  817 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  818 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  819 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  820 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  821 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  822 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  823 | ` */` |
|       142 |  824 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         4 |  825 | `{` |
|       146 |  826 | `	SyToken *pIn = *ppCur;` |
|       146 |  827 | `	sxu32 nLine = pIn->nLine;` |
|         - |  828 | `	sxi32 rc;` |
|       146 |  829 | `	if( GenStateIsReadonly(pIn) ){` |
|         5 |  830 | ``		pIn++; /* `new readonly class …` (PHP 8.3): step over the modifier */`` |
|         2 |  831 | `	}` |
|       146 |  832 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  833 | `	/* Optional constructor argument list */` |
|       146 |  834 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        71 |  835 | `		pIn++; /* Jump '(' */` |
|        71 |  836 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        71 |  837 | `		if( pIn < pEnd ){` |
|        71 |  838 | `			pIn++; /* Jump ')' */` |
|        34 |  839 | `		}` |
|        34 |  840 | `	}` |
|         - |  841 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  842 | `	 * (no braces appear between ')' and the class body). */` |
|       376 |  843 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       234 |  844 | `		pIn++;` |
|         4 |  845 | `	}` |
|       146 |  846 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  847 | `		/* Syntax error: missing class body */` |
|       ! 0 |  848 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  849 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  850 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  851 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  852 | `		}` |
|       ! 0 |  853 | `		*ppCur = pIn;` |
|       ! 0 |  854 | `		return rc;` |
|         - |  855 | `	}` |
|       146 |  856 | `	pIn++; /* Jump the leading '{' */` |
|       146 |  857 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       146 |  858 | `	if( pIn < pEnd ){` |
|       146 |  859 | `		pIn++; /* Jump the trailing '}' */` |
|        71 |  860 | `	}` |
|       146 |  861 | `	*ppCur = pIn;` |
|       146 |  862 | `	return SXRET_OK;` |
|        75 |  863 | `}` |
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
|      9354 |  892 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd)` |
|         5 |  893 | `{` |
|      9359 |  894 | `	int bStatic = FALSE;` |
|      9359 |  895 | `	if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  896 | `		return FALSE;` |
|         - |  897 | `	}` |
|      9359 |  898 | `	if( pTok > pStart ){` |
|       563 |  899 | `		SyToken *pPrev = &pTok[-1];` |
|       563 |  900 | `		if( pPrev->nType & (PH7_TK_DOLLAR\|PH7_TK_NSSEP) ){` |
|        86 |  901 | `			return FALSE; /* $fn / \A\fn — the keyword IS the name */` |
|         - |  902 | `		}` |
|       481 |  903 | `		if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData ){` |
|       227 |  904 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)pPrev->pUserData;` |
|       222 |  905 | `			if( pOp->iOp == EXPR_OP_ARROW \|\| pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       206 |  906 | `			 \|\| pOp->iOp == EXPR_OP_DC ){` |
|        79 |  907 | `				return FALSE; /* $o->fn, $o?->fn, C::fn — a member name */` |
|         - |  908 | `			}` |
|        74 |  909 | `		}` |
|       201 |  910 | `	}` |
|      9203 |  911 | `	if( SX_PTR_TO_INT(pTok->pUserData) == PH7_TKWRD_STATIC ){` |
|       215 |  912 | `		bStatic = TRUE;` |
|       215 |  913 | `		pTok++;` |
|       215 |  914 | `		if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       132 |  915 | `			return FALSE;` |
|         - |  916 | `		}` |
|        41 |  917 | `	}` |
|      9075 |  918 | `	if( SX_PTR_TO_INT(pTok->pUserData) != PH7_TKWRD_FN ){` |
|       529 |  919 | `		return FALSE;` |
|         - |  920 | `	}` |
|      8551 |  921 | `	if( !bStatic && &pTok[1] < pEnd && (pTok[1].nType & PH7_TK_COLON) ){` |
|       ! 0 |  922 | `		return FALSE; /* f(fn: 1) — a named-argument label */` |
|         - |  923 | `	}` |
|      8551 |  924 | `	return TRUE;` |
|      4583 |  925 | `}` |
|         - |  926 | `/*` |
|         - |  927 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  928 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  929 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  930 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  931 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  932 | ` */` |
|      8146 |  933 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  934 | `{` |
|      8151 |  935 | `	SyToken *pIn = *ppCur;` |
|         - |  936 | `	sxu32 nLine;` |
|         - |  937 | `	sxi32 rc;` |
|         - |  938 | `	int iNest;` |
|         - |  939 | `	int iTern;   /* ternary '?'s opened inside the body and not yet closed */` |
|      8151 |  940 | `	nLine = pIn->nLine;` |
|         - |  941 | `	/* Optional 'static' prefix */` |
|      8146 |  942 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      8151 |  943 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        82 |  944 | `		pIn++;` |
|        39 |  945 | `	}` |
|         - |  946 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      8146 |  947 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      8151 |  948 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  949 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  950 | `		goto Synchronize;` |
|         - |  951 | `	}` |
|      8151 |  952 | `	pIn++; /* Jump 'fn' */` |
|      3974 |  953 | `	SXUNUSED(nLine);` |
|      3974 |  954 | `	SXUNUSED(pGen);` |
|         - |  955 | `	/* Optional '&' for return-by-reference */` |
|      8151 |  956 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  957 | `		pIn++;` |
|       ! 0 |  958 | `	}` |
|         - |  959 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  960 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  961 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  962 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      8151 |  963 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      8149 |  964 | `		pIn++; /* '(' */` |
|      8149 |  965 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      8149 |  966 | `		if( pIn < pEnd ){` |
|      8147 |  967 | `			pIn++; /* ')' */` |
|      3972 |  968 | `		}` |
|      3973 |  969 | `	}` |
|         - |  970 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      8151 |  971 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  972 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      8151 |  973 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      8145 |  974 | `		pIn++;` |
|      3971 |  975 | `	}` |
|         - |  976 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' -- or a ':' that belongs` |
|         - |  977 | `	 * to an enclosing TERNARY. php's grammar ends an arrow body there, which is` |
|         - |  978 | ``	 * what makes `$c ? fn($v) => a : fn($v) => b` legal: the first body stops at`` |
|         - |  979 | ``	 * the `:` and the second is the false branch. Without the rule the body ran on`` |
|         - |  980 | ``	 * and swallowed `: fn($v) => b`, and the statement's `;` was `unexpected token`` |
|         - |  981 | ``	 * ";"` -- symfony/config and symfony/var-exporter both write that shape.`` |
|         - |  982 | ``	 * A ternary opened INSIDE the body owns its own colon (`fn() => $a ? $b : $c`),`` |
|         - |  983 | ``	 * so count them; `?:`, `??` and `?->` never reach the counter as a bare '?'`` |
|         - |  984 | `	 * (the first is two tokens whose colon pairs with its own '?', the other two` |
|         - |  985 | `	 * are single tokens). */` |
|      8151 |  986 | `	iNest = 0;` |
|      8151 |  987 | `	iTern = 0;` |
|     82371 |  988 | `	while( pIn < pEnd ){` |
|     81290 |  989 | `		if( iNest == 0 && (pIn->nType &` |
|         - |  990 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      7064 |  991 | `			break;` |
|         - |  992 | `		}` |
|     74231 |  993 | `		if( iNest == 0 && (pIn->nType & PH7_TK_COLON) ){` |
|        53 |  994 | `			if( iTern < 1 ){` |
|         7 |  995 | `				break;     /* the colon of an enclosing '?': the body ends here */` |
|         - |  996 | `			}` |
|        47 |  997 | `			iTern--;       /* ...or of a ternary this body opened itself */` |
|     74204 |  998 | `		}else if( iNest == 0 && (pIn->nType & PH7_TK_OP)` |
|     15058 |  999 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        47 | 1000 | `			iTern++;` |
|        21 | 1001 | `		}` |
|     74225 | 1002 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     13276 | 1003 | `			iNest++;` |
|     67444 | 1004 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     13276 | 1005 | `			iNest--;` |
|      6490 | 1006 | `		}` |
|     74225 | 1007 | `		pIn++;` |
|         5 | 1008 | `	}` |
|      8151 | 1009 | `	rc = SXRET_OK;` |
|      4172 | 1010 | `Synchronize:` |
|      8151 | 1011 | `	*ppCur = pIn;` |
|      8151 | 1012 | `	return rc;` |
|         5 | 1013 | `}` |
|         - | 1014 | `/*` |
|         - | 1015 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - | 1016 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - | 1017 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - | 1018 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - | 1019 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - | 1020 | ` */` |
|       138 | 1021 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 | 1022 | `{` |
|       143 | 1023 | `	SyToken *pIn = *ppCur;` |
|         - | 1024 | `	sxi32 rc;` |
|        69 | 1025 | `	SXUNUSED(pGen);` |
|         - | 1026 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       138 | 1027 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       143 | 1028 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 | 1029 | `		rc = SXERR_SYNTAX;` |
|       ! 0 | 1030 | `		goto Synchronize;` |
|         - | 1031 | `	}` |
|       143 | 1032 | `	pIn++; /* Jump 'match' */` |
|         - | 1033 | `	/* Optional '(' subject ')' */` |
|       143 | 1034 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       143 | 1035 | `		pIn++;` |
|       143 | 1036 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       143 | 1037 | `		if( pIn < pEnd ){` |
|       143 | 1038 | `			pIn++; /* ')' */` |
|        69 | 1039 | `		}` |
|        69 | 1040 | `	}` |
|         - | 1041 | `	/* Optional '{' arms '}' */` |
|       143 | 1042 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       143 | 1043 | `		pIn++;` |
|       143 | 1044 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       143 | 1045 | `		if( pIn < pEnd ){` |
|       143 | 1046 | `			pIn++; /* '}' */` |
|        69 | 1047 | `		}` |
|        69 | 1048 | `	}` |
|       143 | 1049 | `	rc = SXRET_OK;` |
|        69 | 1050 | `Synchronize:` |
|       143 | 1051 | `	*ppCur = pIn;` |
|       143 | 1052 | `	return rc;` |
|         5 | 1053 | `}` |
|         - | 1054 | `/*` |
|         - | 1055 | `` * PHP 8.5 `clone (`: tell the clone() CALL form from the clone OPERATOR applied to a`` |
|         - | 1056 | ` * parenthesised operand. php's grammar has both, and its parser resolves the conflict` |
|         - | 1057 | ` * by continuing the parenthesised expression whenever the token after the ')' can` |
|         - | 1058 | ``  * dereference it — so `clone ($a)->b()` clones what `b()` returns, `clone ($c)[0]` `` |
|         - | 1059 | `` * clones the ELEMENT and `clone ($f)()` clones the call's result, while a plain`` |
|         - | 1060 | `` * `clone ($a)` (nothing dereferencing) is the one-argument call, which means the same`` |
|         - | 1061 | `` * thing either way. PHL took the call form for every `clone (`, so the receiver was`` |
|         - | 1062 | ` * cloned and the member access ran on the ORIGINAL — a silent wrong answer with no` |
|         - | 1063 | `` * diagnostic, out of `clone (new A)->b()`.`` |
|         - | 1064 | ` *` |
|         - | 1065 | ` * pClone points at the 'clone' token and pClone[1] at its '('. Returns TRUE when the` |
|         - | 1066 | ` * call-form branch should take the tokens (including the unterminated case, which that` |
|         - | 1067 | ` * branch reports), FALSE to leave them to the precedence-1 operator path.` |
|         - | 1068 | ` */` |
|        68 | 1069 | `static int CloneCallFormFollows(SyToken *pClone,SyToken *pEnd)` |
|         2 | 1070 | `{` |
|        70 | 1071 | `	SyToken *pNext = &pClone[2]; /* first token inside the '(' */` |
|        70 | 1072 | `	PH7_DelimitNestedTokens(pNext,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pNext);` |
|        70 | 1073 | `	if( pNext >= pEnd ){` |
|       ! 0 | 1074 | `		return TRUE; /* unterminated '(' — the call-form branch raises php's ')' error */` |
|         - | 1075 | `	}` |
|        70 | 1076 | `	pNext++; /* step past the matching ')' */` |
|        70 | 1077 | `	if( pNext >= pEnd ){` |
|        52 | 1078 | `		return TRUE;` |
|         - | 1079 | `	}` |
|        19 | 1080 | `	if( pNext->nType & (PH7_TK_OSB /*'['*/\|PH7_TK_LPAREN /*'('*/) ){` |
|         5 | 1081 | `		return FALSE;` |
|         - | 1082 | `	}` |
|        15 | 1083 | `	if( (pNext->nType & PH7_TK_OP) && pNext->pUserData ){` |
|         9 | 1084 | `		sxi32 iOp = ((const ph7_expr_op *)pNext->pUserData)->iOp;` |
|         9 | 1085 | `		if( iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW \|\| iOp == EXPR_OP_DC ){` |
|         9 | 1086 | `			return FALSE;` |
|         - | 1087 | `		}` |
|       ! 0 | 1088 | `	}` |
|         7 | 1089 | `	return TRUE;` |
|        36 | 1090 | `}` |
|         - | 1091 | `/*` |
|         - | 1092 | ` * Extract a single expression node from the input.` |
|         - | 1093 | ` * On success store the freshly extractd node in ppNode.` |
|         - | 1094 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1095 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|         - | 1096 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|         - | 1097 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|         - | 1098 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|         - | 1099 | ` */` |
|         - | 1100 | `/*` |
|         - | 1101 | `` * Where a KEYWORD-headed operand ends: `yield <expr>`, `throw <expr>` and the`` |
|         - | 1102 | `` * one-operand language constructs (`print`, `include`, `require`, …) each take`` |
|         - | 1103 | ` * the rest of the enclosing group, so PH7_DelimitNestedTokens is the right shape` |
|         - | 1104 | `` * for them — EXCEPT that php's grammar gives them `expr`, and a top-level COMMA`` |
|         - | 1105 | `` * is not part of an `expr`. In an ARGUMENT LIST that comma is the separator, so`` |
|         - | 1106 | `` * `f(yield 1, 2)` and `f(print "p", 2)` are two arguments in php; here the`` |
|         - | 1107 | `` * operand ran straight past it and the leftover `, 2` came back as`` |
|         - | 1108 | `` * `syntax error, unexpected token ","` on source php runs. (An ARRAY literal`` |
|         - | 1109 | ` * never showed it: its body is re-split on commas before these nodes are ever` |
|         - | 1110 | ` * extracted.) At statement level nothing legitimate follows such an operand with` |
|         - | 1111 | `` * a comma, so stopping is php's answer there too — `yield 1, 2;` and`` |
|         - | 1112 | `` * `print "a", "b";` stay the parse error both engines already gave.`` |
|         - | 1113 | ` */` |
|       956 | 1114 | `static void ExprDelimitKeywordOperand(SyToken *pIn,SyToken *pEnd,SyToken **ppEnd)` |
|         5 | 1115 | `{` |
|       961 | 1116 | `	SyToken *pCur = pIn;` |
|       961 | 1117 | `	sxi32 iNest = 1;` |
|       961 | 1118 | ``	sxi32 iQuesty = 0;   /* `?`s opened inside the operand and still unclosed */`` |
|      1824 | 1119 | `	for(;;){` |
|      3653 | 1120 | `		if( pCur >= pEnd ){` |
|       881 | 1121 | `			break;` |
|         - | 1122 | `		}` |
|      2777 | 1123 | `		if( (pCur->nType & PH7_TK_COMMA) && iNest <= 1 ){` |
|        11 | 1124 | `			break;` |
|         - | 1125 | `		}` |
|         - | 1126 | ``		/* Every construct that shares this delimiter -- `include`/`require` and`` |
|         - | 1127 | ``		 * their `_once` forms, `print`, `echo`, `throw`, `yield` -- sits BELOW the`` |
|         - | 1128 | ``		 * ternary in php's precedence table, so a `:` that closes a `?` opened`` |
|         - | 1129 | ``		 * OUTSIDE the operand ends it: `c ? include $f : null` includes $f and the`` |
|         - | 1130 | ``		 * `: null` is the ternary's. A `?` opened INSIDE takes its own `:` with it,`` |
|         - | 1131 | `` 		 * which is the other half of the same rule -- `include $f ? "y" : "n"` `` |
|         - | 1132 | ``		 * includes the whole conditional's answer, and `include $f ?: 1` the elvis`` |
|         - | 1133 | ``		 * one. Swallowing the `:` regardless left `? <operand>` with no colon, and`` |
|         - | 1134 | ``		 * every `cond ? require $file : null` bootstrap failed to parse. `??` and`` |
|         - | 1135 | ``		 * `?->` are their own tokens, so the one-byte test cannot see them, and a`` |
|         - | 1136 | ``		 * named argument's `:` sits inside parentheses at iNest >= 2. */`` |
|      2762 | 1137 | `		if( iNest <= 1 && (pCur->nType & PH7_TK_OP)` |
|      1200 | 1138 | `		 && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|         3 | 1139 | `			iQuesty++;` |
|      2766 | 1140 | `		}else if( iNest <= 1 && (pCur->nType & PH7_TK_COLON) ){` |
|        17 | 1141 | `			if( iQuesty < 1 ){` |
|        15 | 1142 | `				break;` |
|         - | 1143 | `			}` |
|         3 | 1144 | `			iQuesty--;` |
|         1 | 1145 | `		}` |
|      2753 | 1146 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB) ){` |
|       217 | 1147 | `			iNest++;` |
|      2647 | 1148 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|       273 | 1149 | `			iNest--;` |
|       273 | 1150 | `			if( iNest <= 0 ){` |
|        60 | 1151 | `				break;` |
|         - | 1152 | `			}` |
|       106 | 1153 | `		}` |
|      2697 | 1154 | `		pCur++;` |
|         5 | 1155 | `	}` |
|       961 | 1156 | `	*ppEnd = pCur;` |
|       961 | 1157 | `}` |
|  11655352 | 1158 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 | 1159 | `{` |
|         - | 1160 | `	ph7_expr_node *pNode;` |
|         - | 1161 | `	SyToken *pCur;` |
|         - | 1162 | `	sxi32 rc;` |
|         - | 1163 | `	/* Allocate a new node */` |
|  11655357 | 1164 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  11655357 | 1165 | `	if( pNode == 0 ){` |
|         - | 1166 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1167 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1168 | `		 */` |
|       ! 0 | 1169 | `		return SXERR_MEM;` |
|         - | 1170 | `	}` |
|         - | 1171 | `	/* Zero the structure */` |
|  11655357 | 1172 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  11655357 | 1173 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - | 1174 | `	/* Point to the head of the token stream */` |
|  11655357 | 1175 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - | 1176 | `	/* Start collecting tokens */` |
|  11655357 | 1177 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|       726 | 1178 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - | 1179 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - | 1180 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - | 1181 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - | 1182 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       267 | 1183 | `			pNode->pEnd = pCur;` |
|       267 | 1184 | `			pCur++;` |
|       267 | 1185 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       267 | 1186 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       267 | 1187 | `			pGen->pIn = pCur;` |
|       267 | 1188 | `			*ppNode = pNode;` |
|       267 | 1189 | `			return SXRET_OK;` |
|         - | 1190 | `		}` |
|         - | 1191 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - | 1192 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       464 | 1193 | `		pCur++;` |
|       464 | 1194 | `		pGen->pIn = pCur;` |
|       464 | 1195 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       464 | 1196 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       464 | 1197 | `		if( rc == SXRET_OK && *ppNode ){` |
|       464 | 1198 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       229 | 1199 | `		}` |
|       464 | 1200 | `		return rc;` |
|         - | 1201 | `	}` |
|  11654636 | 1202 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - | 1203 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - | 1204 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - | 1205 | `		 */` |
|     17886 | 1206 | `		pCur++; /* Skip the opening '[' */` |
|     17886 | 1207 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     17886 | 1208 | `		if( pCur < pGen->pEnd ){` |
|     17886 | 1209 | `			pCur++; /* Skip past the closing ']' */` |
|      8898 | 1210 | `		}else{` |
|       ! 0 | 1211 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1212 | `				"Short array: Missing closing bracket ']'");` |
|       ! 0 | 1213 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 1214 | `				rc = SXERR_SYNTAX;` |
|       ! 0 | 1215 | `			}` |
|       ! 0 | 1216 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1217 | `			return rc;` |
|         - | 1218 | `		}` |
|         - | 1219 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - | 1220 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - | 1221 | `		 */` |
|     18866 | 1222 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      1946 | 1223 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      1946 | 1224 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       295 | 1225 | `				pNode->xCode = PH7_CompileShortList;` |
|       150 | 1226 | `			}else{` |
|      1656 | 1227 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - | 1228 | `			}` |
|       966 | 1229 | `		}else{` |
|     15945 | 1230 | `			pNode->xCode = PH7_CompileShortArray;` |
|         - | 1231 | `		}` |
|  11645648 | 1232 | `	}else if( !bAfterMemberOp && (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID))` |
|   6603939 | 1233 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_COLON) ){` |
|         - | 1234 | `		/* A RESERVED WORD immediately followed by a single ':' is a named-argument` |
|         - | 1235 | `		 * LABEL — php accepts all 73 of them there, since a parameter may be called` |
|         - | 1236 | ``		 * anything (`function f($new, $print, $and)`). Sixteen of them were claimed`` |
|         - | 1237 | ``		 * by their own construct branch below instead, so `f(new: 1)`, `f(print: 2)`,`` |
|         - | 1238 | ``		 * `f(match: $m)` and thirteen more were a compile fatal on source php runs.`` |
|         - | 1239 | ``		 * The lexer gives `::` its own operator token, so the only other shape this`` |
|         - | 1240 | `		 * can see — a bare word before a colon — is already a literal on the` |
|         - | 1241 | ``		 * fallthrough path; a `?:` cannot reach here at all, its '?' being neither`` |
|         - | 1242 | `		 * an identifier nor a keyword. The argument list is re-parsed from each` |
|         - | 1243 | `		 * argument's own first token, so there is no '(' to look back at and no way` |
|         - | 1244 | `		 * to scope this to call context; ExprProcessFuncArguments makes the` |
|         - | 1245 | `		 * POSITIONAL test that decides whether the label is really one. */` |
|      7480 | 1246 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|      7480 | 1247 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      7480 | 1248 | `		pNode->xCode = PH7_CompileLiteral;` |
|  11633013 | 1249 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - | 1250 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - | 1251 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - | 1252 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - | 1253 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - | 1254 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - | 1255 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - | 1256 | `		 * call, not the clone() intrinsic. */` |
|        25 | 1257 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        25 | 1258 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        25 | 1259 | `		pNode->xCode = PH7_CompileLiteral;` |
|  11629264 | 1260 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   3147352 | 1261 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   1571396 | 1262 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN)` |
|       213 | 1263 | `		&& CloneCallFormFollows(pCur,pGen->pEnd) ){` |
|         - | 1264 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - | 1265 | ``		 * `clone` is a real internal FUNCTION in php 8.5, so this spelling is an`` |
|         - | 1266 | `		 * ordinary call — and every property the call machinery owns comes with` |
|         - | 1267 | ``		 * it: named arguments, spread, the first-class-callable `clone(...)`, and`` |
|         - | 1268 | `		 * the runtime ArgumentCountError/TypeError php raises for a degenerate` |
|         - | 1269 | ``		 * argument list (PHL used to refuse `clone()` and a three-argument call at`` |
|         - | 1270 | ``		 * COMPILE time, and had no FCC form at all). `clone` is an alpha-stream`` |
|         - | 1271 | `` 		 * operator token, so `clone(` is not auto-marked as a call the way `foo(` `` |
|         - | 1272 | `		 * is: clear PH7_TK_OP and leave a plain name TERM behind, and the postfix` |
|         - | 1273 | `		 * pass then binds the '(' to it. The bare operator/statement form` |
|         - | 1274 | ``		 * `clone $obj` (no immediately-following '(') keeps the precedence-1`` |
|         - | 1275 | `		 * operator path below. */` |
|        58 | 1276 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        58 | 1277 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        58 | 1278 | `		pNode->xCode = PH7_CompileLiteral;` |
|  11629228 | 1279 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1280 | `		/* Point to the instance that describe this operator */` |
|   3147301 | 1281 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1282 | `		/* Advance the stream cursor */` |
|   3147301 | 1283 | `		pCur++;` |
|  10053098 | 1284 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1285 | `		/* Isolate variable */` |
|   5852919 | 1286 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   2926483 | 1287 | `			pCur++; /* Variable variable */` |
|         5 | 1288 | `		}` |
|   2926441 | 1289 | `		if( pCur < pGen->pEnd ){` |
|   2926441 | 1290 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1291 | `				/* Variable name */` |
|   2926397 | 1292 | `				pCur++;` |
|   1461158 | 1293 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        41 | 1294 | `				pCur++;` |
|         - | 1295 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        41 | 1296 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        41 | 1297 | `				if( pCur < pGen->pEnd ){` |
|        38 | 1298 | `					pCur++;` |
|        21 | 1299 | `				}else{` |
|         - | 1300 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|         - | 1301 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|         - | 1302 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|         - | 1303 | `					 * delimiter search stops at the slice end, so the token php names` |
|         - | 1304 | `					 * usually sits just past it, still inside the chunk stream. */` |
|         - | 1305 | `					{` |
|         3 | 1306 | `						SyToken *pBad = 0;` |
|         3 | 1307 | `						if( pGen->pTokenSet ){` |
|         3 | 1308 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         3 | 1309 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         3 | 1310 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|       ! 0 | 1311 | `								pBad = pCur;` |
|       ! 0 | 1312 | `							}` |
|         1 | 1313 | `						}` |
|         3 | 1314 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|         - | 1315 | `					}` |
|         3 | 1316 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1317 | `						rc = SXERR_SYNTAX;` |
|         1 | 1318 | `					}` |
|         3 | 1319 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1320 | `					return rc;` |
|         - | 1321 | `				}` |
|        21 | 1322 | `			}else{` |
|         - | 1323 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|         - | 1324 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|         - | 1325 | `				 * covered only the '$' and the offending token drifted into a later` |
|         - | 1326 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|         - | 1327 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|         9 | 1328 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|         9 | 1329 | `				if( rc != SXERR_ABORT ){` |
|         9 | 1330 | `					rc = SXERR_SYNTAX;` |
|         4 | 1331 | `				}` |
|         9 | 1332 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         9 | 1333 | `				return rc;` |
|         - | 1334 | `			}` |
|   1461126 | 1335 | `		}` |
|   2926431 | 1336 | `		pNode->xCode = PH7_CompileVariable;` |
|   7016594 | 1337 | `	 }else if( !bAfterMemberOp && GenStateStartsReadonlyAnonClass(pCur,pGen->pEnd) ){` |
|         - | 1338 | ``		 /* `new readonly class(args) [extends/implements] { body }` (PHP 8.3).`` |
|         - | 1339 | ``		  * `readonly` is a context-sensitive ID, so it never reaches the keyword`` |
|         - | 1340 | ``		  * chain below and the `class` after it read as the `::class` constant --`` |
|         - | 1341 | ``		  * `syntax error, unexpected token "class"`. It is the ONLY modifier php`` |
|         - | 1342 | ``		  * allows here (`new final class {}` and `new abstract class {}` are parse`` |
|         - | 1343 | `		  * errors in both engines), and pest writes one in its parallel result` |
|         - | 1344 | `		  * printer. */` |
|         5 | 1345 | `		 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         5 | 1346 | `		 if( rc != SXRET_OK ){` |
|       ! 0 | 1347 | `			 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1348 | `			 return rc;` |
|         - | 1349 | `		 }` |
|         5 | 1350 | `		 pNode->xCode = PH7_CompileAnnonClass;` |
|   5555466 | 1351 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    115032 | 1352 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    115032 | 1353 | `		 if( bAfterMemberOp ){` |
|         - | 1354 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1355 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1356 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1357 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|         - | 1358 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|         - | 1359 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|         - | 1360 | `			  * the word itself. */` |
|       563 | 1361 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|       563 | 1362 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       563 | 1363 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    114753 | 1364 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1365 | `			 /* List/Array node */` |
|     83215 | 1366 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1367 | `				 /* Assume a literal */` |
|       ! 0 | 1368 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1369 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1370 | `			 }else{` |
|     83215 | 1371 | `				 pCur += 2;` |
|         - | 1372 | `				 /* Collect array/list tokens */` |
|     83215 | 1373 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|     83215 | 1374 | `				 if( pCur < pGen->pEnd ){` |
|     83213 | 1375 | `					 pCur++;` |
|     41555 | 1376 | `				 }else{` |
|         - | 1377 | `					 /* Syntax error */` |
|         - | 1378 | `					 /* php names the token it stopped on and says it expected ")". */` |
|         3 | 1379 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|         3 | 1380 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1381 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1382 | `					 }` |
|         3 | 1383 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1384 | `					 return rc;` |
|         - | 1385 | `				 }` |
|     83213 | 1386 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|     83213 | 1387 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        61 | 1388 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        61 | 1389 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1390 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|         - | 1391 | ``						  * not the `list` the construct started at. */`` |
|         3 | 1392 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|         3 | 1393 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1394 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1395 | `						 }` |
|         3 | 1396 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1397 | `						 return rc;` |
|         - | 1398 | `					 }` |
|        27 | 1399 | `				 }` |
|         5 | 1400 | `			 }` |
|     72813 | 1401 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1402 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       617 | 1403 | `			 pCur++; /* Skip 'yield' keyword */` |
|       617 | 1404 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       617 | 1405 | `			 pNode->xCode = PH7_CompileYield;` |
|     30958 | 1406 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     27870 | 1407 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       529 | 1408 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       471 | 1409 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1410 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|      6290 | 1411 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1412 | `				 /* Assume a literal */` |
|       ! 0 | 1413 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1414 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1415 | `			 }else{` |
|         - | 1416 | `				 /* Assemble annonymous functions body */` |
|      6290 | 1417 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      6290 | 1418 | `				 if( rc != SXRET_OK ){` |
|        30 | 1419 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        30 | 1420 | `					 return rc;` |
|         - | 1421 | `				 }` |
|      6264 | 1422 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1423 | `			  }` |
|     27480 | 1424 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       151 | 1425 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|        90 | 1426 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        42 | 1427 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        28 | 1428 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1429 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1430 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1431 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1432 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       142 | 1433 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       142 | 1434 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1435 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1436 | `				 return rc;` |
|         - | 1437 | `			 }` |
|       142 | 1438 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     24297 | 1439 | `		 }else if( (nKeyword == PH7_TKWRD_FN \|\| nKeyword == PH7_TKWRD_STATIC)` |
|     16239 | 1440 | `			&& PH7_TokenOpensArrowFunc(pGen->pIn,pCur,pGen->pEnd) ){` |
|         - | 1441 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      8151 | 1442 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      8151 | 1443 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1444 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1445 | `				 return rc;` |
|         - | 1446 | `			 }` |
|      8151 | 1447 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     20057 | 1448 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1449 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       143 | 1450 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       143 | 1451 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1452 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1453 | `				 return rc;` |
|         - | 1454 | `			 }` |
|       143 | 1455 | `			 pNode->xCode = PH7_CompileMatch;` |
|     16014 | 1456 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1457 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1458 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1459 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        50 | 1460 | `			 pCur++; /* Skip 'throw' */` |
|        50 | 1461 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|        50 | 1462 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     15922 | 1463 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1464 | `			 /* Language constructs [i.e: print,echo,die...] require special handling.` |
|         - | 1465 | `			  * Each of the six that reach here takes exactly ONE operand. */` |
|       303 | 1466 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       303 | 1467 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       154 | 1468 | `		 }else{` |
|         - | 1469 | `			 /* Assume a literal */` |
|     15601 | 1470 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     15601 | 1471 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1472 | `		 }` |
|   5497757 | 1473 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1474 | `		 /* Constants,function name,namespace path,class name... */` |
|   1421679 | 1475 | `		 if( bAfterMemberOp ){` |
|         - | 1476 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|         - | 1477 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|         - | 1478 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|         - | 1479 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|     34155 | 1480 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     17056 | 1481 | `		 }` |
|   1421679 | 1482 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   1421679 | 1483 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    709434 | 1484 | `	 }else{` |
|   4018763 | 1485 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1486 | `			 /* Point to the code generator routine */` |
|   1472164 | 1487 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   1472164 | 1488 | `			 if( pNode->xCode == 0 ){` |
|        12 | 1489 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|        12 | 1490 | `				 if( rc != SXERR_ABORT ){` |
|        12 | 1491 | `					 rc = SXERR_SYNTAX;` |
|         5 | 1492 | `				 }` |
|        12 | 1493 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        12 | 1494 | `				 return rc;` |
|         - | 1495 | `			 }` |
|    734723 | 1496 | `		 }` |
|         - | 1497 | `		/* Advance the stream cursor */` |
|   4018753 | 1498 | `		pCur++;` |
|         - | 1499 | `	 }` |
|         - | 1500 | `	/* Point to the end of the token stream */` |
|  11654586 | 1501 | `	pNode->pEnd = pCur;` |
|         - | 1502 | `	/* Save the node for later processing */` |
|  11654586 | 1503 | `	*ppNode = pNode;` |
|         - | 1504 | `	/* Synchronize cursors */` |
|  11654586 | 1505 | `	pGen->pIn = pCur;` |
|  11654586 | 1506 | `	return SXRET_OK;` |
|   5817698 | 1507 | `}` |
|         - | 1508 | `/*` |
|         - | 1509 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1510 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1511 | ` * level is zero.` |
|         - | 1512 | ` */` |
|    348908 | 1513 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1514 | `{` |
|    348913 | 1515 | `	SyToken *pCur = pStart;` |
|    348913 | 1516 | `	sxi32 iNest = 0;` |
|    348913 | 1517 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1518 | `		/* Last expression */` |
|    136435 | 1519 | `		return SXERR_EOF;` |
|         - | 1520 | `	}` |
|    685802 | 1521 | `	while( pCur < pEnd ){` |
|    650272 | 1522 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    176953 | 1523 | `			break;` |
|         - | 1524 | `		}` |
|    473324 | 1525 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     40581 | 1526 | `			iNest++;` |
|    452999 | 1527 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     40585 | 1528 | `			iNest--;` |
|     20253 | 1529 | `		}` |
|    473324 | 1530 | `		pCur++;` |
|         5 | 1531 | `	}` |
|    212483 | 1532 | `	*ppNext = pCur;` |
|    212483 | 1533 | `	return SXRET_OK;` |
|    174105 | 1534 | `}` |
|         - | 1535 | `/*` |
|         - | 1536 | ` * Release one node -- its own storage and the argument set it carries, and` |
|         - | 1537 | ` * nothing else. The tree it is part of is NOT walked: every node an expression` |
|         - | 1538 | ` * ever produced is in the extraction set, and that set is what owns them (see` |
|         - | 1539 | ` * PH7_ExprFreeTree).` |
|         - | 1540 | ` */` |
|  11654843 | 1541 | `static void ExprFreeNode(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1542 | `{` |
|  11654848 | 1543 | `	SySetRelease(&pNode->aNodeArgs);` |
|  11654848 | 1544 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  11654848 | 1545 | `}` |
|         - | 1546 | `/*` |
|         - | 1547 | ` * Free every node of an expression.` |
|         - | 1548 | ` *` |
|         - | 1549 | ` * The EXTRACTION SET owns the nodes, one entry per node, in the order the` |
|         - | 1550 | ` * tokens were read -- which is the only container that ever sees all of them.` |
|         - | 1551 | ` * Tree building runs on a COPY of that list and consumes it by NULLING each` |
|         - | 1552 | ` * slot it folds in, so a walk from the roots reaches only what ended up in a` |
|         - | 1553 | ` * tree: every delimiter a fold swallowed ('(' ')' '[' ']' ',' and the label` |
|         - | 1554 | ` * and colon of a named argument) was dropped from the copy and then reachable` |
|         - | 1555 | ` * from nothing. That leaked 17% of all the nodes an ordinary program compiles` |
|         - | 1556 | ` * -- 40,072 of 235,466 on one 40-file lint run, 5.1 MB held for the life of` |
|         - | 1557 | ` * the process -- because the compiler's AST is pool-allocated and the pool is` |
|         - | 1558 | ` * only handed back at VM teardown, so no leak checker ever named it.` |
|         - | 1559 | ` *` |
|         - | 1560 | ` * Freeing from the set instead of from the roots also makes the count exact in` |
|         - | 1561 | ` * the other direction: a node cannot be reached twice, so the ownership rule` |
|         - | 1562 | ` * that used to have to be maintained at every fold site ("null it here, free` |
|         - | 1563 | ` * it there, and never both") is gone.` |
|         - | 1564 | ` */` |
|   2304188 | 1565 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1566 | `{` |
|         - | 1567 | `	ph7_expr_node **apNode;` |
|         - | 1568 | `	sxu32 n;` |
|   2304193 | 1569 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  13959036 | 1570 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  11654848 | 1571 | `		if( apNode[n] ){` |
|  11654848 | 1572 | `			ExprFreeNode(&(*pGen),apNode[n]);` |
|   5817439 | 1573 | `		}` |
|   5817444 | 1574 | `	}` |
|   2304193 | 1575 | `	SySetReset(pNodeSet);` |
|   2304193 | 1576 | `	return SXRET_OK;` |
|         5 | 1577 | `}` |
|         - | 1578 | `/*` |
|         - | 1579 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1580 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1581 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1582 | ` * (PHP 8.0 makes this a compile fatal:` |
|         - | 1583 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1584 | ` */` |
|   5485430 | 1585 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1586 | `{` |
|   5485435 | 1587 | `	if( pNode == 0 ){` |
|   3547313 | 1588 | `		return 0;` |
|         - | 1589 | `	}` |
|   1938127 | 1590 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        24 | 1591 | `		return 1;` |
|         - | 1592 | `	}` |
|   1938107 | 1593 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|         6 | 1594 | `		return 1;` |
|         - | 1595 | `	}` |
|   1938103 | 1596 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1597 | `		return 1;` |
|         - | 1598 | `	}` |
|   1938103 | 1599 | `	return 0;` |
|   2738914 | 1600 | `}` |
|         - | 1601 | `/*` |
|         - | 1602 | `` * TRUE when a `::` node names a class CONSTANT (`A::K`, `A::class`) rather than`` |
|         - | 1603 | `` * a static PROPERTY (`A::$s`, `A::$$name`).`` |
|         - | 1604 | ` *` |
|         - | 1605 | ` * php's grammar puts the two in different rules and only the property is a` |
|         - | 1606 | `` * `variable`: a class constant is a `constant`, which nothing but a DEREFERENCE`` |
|         - | 1607 | `` * (`A::K[0]`, `A::K->p`) can turn into one — and that dereference answers a`` |
|         - | 1608 | `` * TEMPORARY. So `A::K = 5`, `A::K++` and `unset(A::K)` are php PARSE errors`` |
|         - | 1609 | `` * while `A::K[0] = 5` is its "Cannot use temporary expression in write`` |
|         - | 1610 | `` * context". PHL treated every `::` as class-level STORAGE, so the first three`` |
|         - | 1611 | `` * were reported at runtime with a PH7-ism (or, for `A::K++` and `--A::K`, ran in`` |
|         - | 1612 | ` * silence) and the fourth wrote into a discarded copy of the constant.` |
|         - | 1613 | ` *` |
|         - | 1614 | `` * The token after `::` decides it: a static property always spells a `$`.`` |
|         - | 1615 | ` */` |
|       596 | 1616 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode)` |
|         5 | 1617 | `{` |
|       601 | 1618 | `	if( pNode == 0 \|\| pNode->pOp == 0 \|\| pNode->pOp->iOp != EXPR_OP_DC ){` |
|       211 | 1619 | `		return FALSE;` |
|         - | 1620 | `	}` |
|       395 | 1621 | `	if( pNode->pRight == 0 \|\| pNode->pRight->pStart == 0 ){` |
|         - | 1622 | `		/* Not linked yet / nothing to look at: keep the old permissive answer. */` |
|       ! 0 | 1623 | `		return FALSE;` |
|         - | 1624 | `	}` |
|       395 | 1625 | `	return (pNode->pRight->pStart->nType & PH7_TK_DOLLAR) == 0;` |
|       303 | 1626 | `}` |
|         - | 1627 | `/*` |
|         - | 1628 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1629 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1630 | ` *` |
|         - | 1631 | ` * This is the SHAPE question only: is the target an access chain at all?` |
|         - | 1632 | ` * Whether the chain's BASE may be written through is php's separate rule, made` |
|         - | 1633 | ` * at codegen by GenStateWriteTargetCheck (php: zend_compile_var_inner) so every` |
|         - | 1634 | `` * write kind — `=`, `+=`, `=&`, `++`, `unset()`, a foreach target — reaches it`` |
|         - | 1635 | ` * and reports php's own two refusals instead of a message naming the operator.` |
|         - | 1636 | ` */` |
|    730599 | 1637 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode)` |
|         5 | 1638 | `{` |
|         - | 1639 | `	sxi32 iExprOp;` |
|    730604 | 1640 | `	if( pNode->pOp == 0 ){` |
|    570229 | 1641 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1642 | `	}` |
|    160380 | 1643 | `	iExprOp = pNode->pOp->iOp;` |
|    160380 | 1644 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ ){` |
|      2309 | 1645 | `			return TRUE;` |
|         - | 1646 | `	}` |
|    158076 | 1647 | `	if( iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|         - | 1648 | ``		/* `C::$s` is storage; `C::K` is a constant, and php's grammar will not`` |
|         - | 1649 | `		 * take one as a write target at all. */` |
|       123 | 1650 | `		return PH7_ExprNodeIsClassConst(pNode) ? FALSE : TRUE;` |
|         - | 1651 | `	}` |
|    157958 | 1652 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|         - | 1653 | `		/* A subscript is a writable shape whatever it subscripts. php compiles` |
|         - | 1654 | ``		 * and RUNS `f()[0] = 5` and `str_split("ab")[0] = "z"` — the write lands`` |
|         - | 1655 | `		 * on the temporary the call answered and is discarded — and refuses a` |
|         - | 1656 | ``		 * literal/cast/computed/`new` base with a wording of its own. Screening`` |
|         - | 1657 | ``		 * the base here rejected both alike with `'=': Left operand must be a`` |
|         - | 1658 | ``		 * modifiable l-value`, and did it BEFORE the codegen check that knows`` |
|         - | 1659 | `		 * php's rules could speak. */` |
|    157880 | 1660 | `		return TRUE;` |
|         - | 1661 | `	}` |
|        82 | 1662 | `	if( iExprOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1663 | ``		/* A call is a shape both ways: as a reference SOURCE (`$r =& f()`) it is`` |
|         - | 1664 | `		 * php-legal, and as a write TARGET it is php's own compile fatal naming` |
|         - | 1665 | `		 * the kind of call, which GenStateWriteTargetCheck raises. */` |
|        44 | 1666 | `		return TRUE;` |
|         - | 1667 | `	}` |
|         - | 1668 | `	/* Not a modifiable l or r-value */` |
|        41 | 1669 | `	return FALSE;` |
|    364798 | 1670 | `}` |
|         - | 1671 | `/*` |
|         - | 1672 | `` * php refuses a write to something that is not a `variable` in its GRAMMAR, so`` |
|         - | 1673 | ` * what comes out is a SYNTAX error naming a token — never a sentence about the` |
|         - | 1674 | ` * operator, which is all PHL had ("'=': Left operand must be a modifiable` |
|         - | 1675 | `` * l-value", "'++' operator needs l-value", and two more for `=&` and `unset()`).`` |
|         - | 1676 | ` * Which token php names depends on which side of the operator the offending` |
|         - | 1677 | ` * operand sits, and both shapes are here:` |
|         - | 1678 | ` *` |
|         - | 1679 | `` *   the operand LEFT of the operator (`5 = 1`, `A::K += 1`, `(1+2)++`) —  php`` |
|         - | 1680 | ` *   has already shifted it and stops AT the operator, so that is what it names.` |
|         - | 1681 | ` */` |
|        10 | 1682 | `static sxi32 ExprWriteTargetNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOpNode)` |
|         3 | 1683 | `{` |
|         - | 1684 | `	sxi32 rc;` |
|        13 | 1685 | `	if( pOpNode->pOp && pOpNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1686 | ``		/* php lexes `=&` as `=` then `&`, and stops on the `=`. */`` |
|       ! 0 | 1687 | `		rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       ! 0 | 1688 | `			pOpNode->pStart ? pOpNode->pStart->nLine : 0,` |
|         - | 1689 | `			"syntax error, unexpected token \"=\"");` |
|       ! 0 | 1690 | `	}else{` |
|        13 | 1691 | `		rc = PH7_GenSyntaxError(pGen,pOpNode->pStart,0);` |
|         - | 1692 | `	}` |
|        13 | 1693 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         3 | 1694 | `}` |
|         - | 1695 | `/*` |
|         - | 1696 | ` * A token pointer one PAST the last token of a subtree is only a real token` |
|         - | 1697 | `` * while the subtree is not the last thing in the file. `<?php --A::K` (no`` |
|         - | 1698 | ` * terminator) walked off the end of the stream and named a garbage token on` |
|         - | 1699 | ` * line 0; php says "unexpected end of file" there, which is what` |
|         - | 1700 | ` * PH7_GenSyntaxError answers for a NULL. Returns pTok, or 0 when it is outside` |
|         - | 1701 | ` * the chunk's token stream.` |
|         - | 1702 | ` */` |
|        12 | 1703 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok)` |
|         3 | 1704 | `{` |
|         - | 1705 | `	SyToken *pBase, *pStreamEnd;` |
|        15 | 1706 | `	if( pTok == 0 \|\| pGen->pTokenSet == 0 ){` |
|       ! 0 | 1707 | `		return pTok;` |
|         - | 1708 | `	}` |
|        15 | 1709 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        15 | 1710 | `	pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        15 | 1711 | `	return (pTok >= pBase && pTok < pStreamEnd) ? pTok : 0;` |
|         9 | 1712 | `}` |
|         - | 1713 | `/*` |
|         - | 1714 | `` *   the operand RIGHT of the operator (`--A::K`, `$r =& "s"`, `unset(GK)`) — it`` |
|         - | 1715 | `` *   was shifted as a `constant`/`dereferencable_scalar`, so php runs past it and`` |
|         - | 1716 | ` *   stops on whatever FOLLOWS, still expecting the dereference that would have` |
|         - | 1717 | ` *   made it a variable. A bare integer/float is the exception: nothing in php's` |
|         - | 1718 | ` *   grammar dereferences one, so the literal itself is named.` |
|         - | 1719 | ` */` |
|        10 | 1720 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand)` |
|         2 | 1721 | `{` |
|        12 | 1722 | `	SyToken *pMin = 0, *pMax = 0;` |
|         - | 1723 | `	sxi32 rc;` |
|        10 | 1724 | `	if( pOperand && pOperand->pOp == 0 && pOperand->pStart` |
|         6 | 1725 | `	 && (pOperand->pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL)) ){` |
|       ! 0 | 1726 | `		rc = PH7_GenSyntaxError(pGen,pOperand->pStart,0);` |
|       ! 0 | 1727 | `	}else{` |
|         - | 1728 | `		/* The whole SUBTREE has to be stepped over, not just the node's own` |
|         - | 1729 | ``		 * tokens: `A::K` and `(1+2)` each named an inner token otherwise. The`` |
|         - | 1730 | `		 * span's max is already one past the last token. */` |
|        12 | 1731 | `		PH7_ExprSubtreeSpan(pOperand,&pMin,&pMax);` |
|        12 | 1732 | `		rc = PH7_GenSyntaxError(pGen,PH7_ExprTokenInStream(pGen,pMax),` |
|         - | 1733 | `			"\"->\" or \"?->\" or \"[\"");` |
|         - | 1734 | `	}` |
|        12 | 1735 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         2 | 1736 | `}` |
|         - | 1737 | `/* Forward declaration */` |
|         - | 1738 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1739 | `/* How many nodes of tree-building scratch PH7_ExprMakeTree carries in its own` |
|         - | 1740 | ` * frame. One pointer each, and an expression longer than this borrows from the` |
|         - | 1741 | ` * pool instead -- 64 covers everything a hand-written statement is likely to` |
|         - | 1742 | ` * be, and the frame only exists while ONE expression is being folded (a nested` |
|         - | 1743 | ` * one is compiled later, from the tree). */` |
|         - | 1744 | `#define EXPR_STACK_NODES 64` |
|         - | 1745 | `/* Macro to check if the given node is a terminal.` |
|         - | 1746 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1747 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1748 | ` * linked ternary/elvis node). */` |
|         - | 1749 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1750 | `/*` |
|         - | 1751 | ` * Buid an expression tree for each given function argument.` |
|         - | 1752 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1753 | ` */` |
|    897093 | 1754 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1755 | `{` |
|         - | 1756 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1757 | `	sxi32 rc;` |
|         - | 1758 | ``	/* php: a stray token in a call argument is `... expecting ")"`. Each arg's`` |
|         - | 1759 | `	 * tree is built by the shared ExprMakeTree below, whose leftover-node error` |
|         - | 1760 | `	 * reads this. Saved/restored so a nested call or array element inside an arg` |
|         - | 1761 | `	 * gets its own closer. */` |
|    897098 | 1762 | `	const char *zSaveArg = pGen->zClauseCloser;` |
|    897098 | 1763 | `	pGen->zClauseCloser = "\")\"";` |
|         - | 1764 | `	/* Process function arguments from left to right */` |
|    897098 | 1765 | `	iCur = 0;` |
|   1103759 | 1766 | `	for(;;){` |
|   2212363 | 1767 | `		if( iCur >= nToken ){` |
|         - | 1768 | `			/* No more arguments to process */` |
|    897066 | 1769 | `			break;` |
|         - | 1770 | `		}` |
|   1315302 | 1771 | `		iNode = iCur;` |
|   1315302 | 1772 | `		iNest = 0;` |
|   3523934 | 1773 | `		while( iCur < nToken ){` |
|   2626871 | 1774 | `			if( apNode[iCur] ){` |
|   2560720 | 1775 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    208561 | 1776 | `					break;` |
|   2142481 | 1777 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   1138911 | 1778 | `					&& apNode[iCur]->pLeft == 0` |
|    139506 | 1779 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    135965 | 1780 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1781 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1782 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1783 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1784 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1785 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1786 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|         - | 1787 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|         - | 1788 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|         - | 1789 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|         - | 1790 | `					 * and swallow the following argument separator. */` |
|    132492 | 1791 | `					iNest++;` |
|   2076097 | 1792 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|   1069272 | 1793 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    132492 | 1794 | `					iNest--;` |
|     66098 | 1795 | `				}` |
|   1068976 | 1796 | `			}` |
|   2208637 | 1797 | `			iCur++;` |
|         5 | 1798 | `		}` |
|   1315302 | 1799 | `		if( iCur > iNode ){` |
|   1315296 | 1800 | `			SyString sArgName = {0, 0};` |
|         - | 1801 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1802 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1803 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   1315291 | 1804 | `			if( (iCur - iNode) >= 2` |
|    774885 | 1805 | `				&& apNode[iNode]` |
|    233281 | 1806 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    154751 | 1807 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     79093 | 1808 | `				&& apNode[iNode+1]` |
|     78010 | 1809 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1810 | `				/* Named argument detected: save the name and drop the label and` |
|         - | 1811 | `				 * colon nodes from the working copy. Dropping is all a fold ever` |
|         - | 1812 | `				 * does now -- the extraction set frees them (PH7_ExprFreeTree). */` |
|       691 | 1813 | `				sArgName = apNode[iNode]->pStart->sData;` |
|       691 | 1814 | `				apNode[iNode] = 0;` |
|       691 | 1815 | `				apNode[iNode+1] = 0;` |
|       691 | 1816 | `				iNode += 2;` |
|         - | 1817 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1818 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|       691 | 1819 | `				if( iNode >= iCur ){` |
|         4 | 1820 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1821 | `						pOp->pStart->nLine,` |
|         - | 1822 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1823 | `						&sArgName);` |
|         3 | 1824 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1825 | `						rc = SXERR_SYNTAX;` |
|         1 | 1826 | `					}` |
|         3 | 1827 | `					pGen->zClauseCloser = zSaveArg;` |
|         3 | 1828 | `					return rc;` |
|         - | 1829 | `				}` |
|       342 | 1830 | `			}` |
|   1315289 | 1831 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1832 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1833 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1834 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1835 | `					apNode[iNode] = 0;` |
|       ! 0 | 1836 | `			}` |
|         - | 1837 | `			{` |
|         - | 1838 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1839 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1840 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1841 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1842 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1843 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1844 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1845 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1846 | `				 * NULL slots ahead of the flagged subtree. */` |
|   1315294 | 1847 | `				int bSpreadArg = 0;` |
|         - | 1848 | `				sxi32 iScan;` |
|   1322798 | 1849 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   1322798 | 1850 | `					if( apNode[iScan] ){` |
|   1315294 | 1851 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   1315294 | 1852 | `						break;` |
|         - | 1853 | `					}` |
|      3751 | 1854 | `				}` |
|   1315294 | 1855 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   1315294 | 1856 | `				if( bSpreadArg && apNode[iNode] ){` |
|       372 | 1857 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       183 | 1858 | `				}` |
|         - | 1859 | `			}` |
|   1315294 | 1860 | `			if( apNode[iNode] ){` |
|   1315294 | 1861 | `				if( sArgName.nByte > 0 ){` |
|       689 | 1862 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|       689 | 1863 | `					apNode[iNode]->sArgName = sArgName;` |
|       342 | 1864 | `				}` |
|         - | 1865 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   1315294 | 1866 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    656159 | 1867 | `			}else{` |
|         - | 1868 | `				/* No expression before comma */` |
|       ! 0 | 1869 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1870 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1871 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1872 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1873 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1874 | `				}` |
|       ! 0 | 1875 | `				pGen->zClauseCloser = zSaveArg;` |
|       ! 0 | 1876 | `				return rc;` |
|         - | 1877 | `			}` |
|    656159 | 1878 | `		}else{` |
|         - | 1879 | `			/* Comma with no preceding argument */` |
|         8 | 1880 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         8 | 1881 | `			if( rc != SXERR_ABORT ){` |
|         8 | 1882 | `				rc = SXERR_SYNTAX;` |
|         3 | 1883 | `			}` |
|         8 | 1884 | `			pGen->zClauseCloser = zSaveArg;` |
|         8 | 1885 | `			return rc;` |
|         - | 1886 | `		}` |
|         - | 1887 | `		/* Jump trailing comma */` |
|   1315294 | 1888 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    418233 | 1889 | `			iCur++;` |
|    418233 | 1890 | `			if( iCur >= nToken ){` |
|         - | 1891 | `				/* Trailing comma after last argument */` |
|        26 | 1892 | `				break;` |
|         - | 1893 | `			}` |
|    208541 | 1894 | `		}` |
|         5 | 1895 | `	}` |
|    897090 | 1896 | `	pGen->zClauseCloser = zSaveArg;` |
|    897090 | 1897 | `	return SXRET_OK;` |
|    447622 | 1898 | `}` |
|         - | 1899 | ` /*` |
|         - | 1900 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|         - | 1901 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|         - | 1902 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|         - | 1903 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|         - | 1904 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|         - | 1905 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|         - | 1906 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|         - | 1907 | `  */` |
|       230 | 1908 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|         5 | 1909 | ` {` |
|         - | 1910 | `	 SyToken *pMin;` |
|         - | 1911 | `	 SyToken *pChild;` |
|       235 | 1912 | `	 if( pNode == 0 ){` |
|       155 | 1913 | `		 return 0;` |
|         - | 1914 | `	 }` |
|        85 | 1915 | `	 pMin = pNode->pStart;` |
|        85 | 1916 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|        85 | 1917 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|         6 | 1918 | `		 pMin = pChild;` |
|         2 | 1919 | `	 }` |
|        85 | 1920 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|        85 | 1921 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|       ! 0 | 1922 | `		 pMin = pChild;` |
|       ! 0 | 1923 | `	 }` |
|        85 | 1924 | `	 return pMin;` |
|       120 | 1925 | ` }` |
|         - | 1926 | `/*` |
|         - | 1927 | ` * The full RAW token extent of a linked subtree: minimum pStart / maximum pEnd` |
|         - | 1928 | ` * over every node (pLeft/pRight/pCond and postfix aNodeArgs children), widened` |
|         - | 1929 | ` * by one token on each side for a node whose group parens were consumed by` |
|         - | 1930 | ` * ExprMakeTree's paren pass (EXPR_NODE_PARENS — its '('/')' slots were nulled,` |
|         - | 1931 | ` * but token contiguity guarantees they sit exactly one token outside the inner` |
|         - | 1932 | ` * extent). Tokens live in one contiguous set, so pointer min/max IS source` |
|         - | 1933 | ` * order. Consumed by the assert() source-text capture, which` |
|         - | 1934 | ` * needs the argument's whole source span where a root's own pStart/pEnd name` |
|         - | 1935 | `` * only the operator token. Note: nested redundant groups `((x))` share the`` |
|         - | 1936 | ` * single PARENS bit, so only one paren layer is recovered — the renderer` |
|         - | 1937 | ` * strips redundant outermost parens anyway, matching php's export.` |
|         - | 1938 | ` */` |
|       626 | 1939 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax)` |
|         5 | 1940 | `{` |
|         - | 1941 | `	SyToken *pMin;` |
|         - | 1942 | `	SyToken *pMax;` |
|       631 | 1943 | `	SyToken *pCMin = 0;` |
|       631 | 1944 | `	SyToken *pCMax = 0;` |
|         - | 1945 | `	ph7_expr_node **apArg;` |
|         - | 1946 | `	sxu32 n;` |
|       631 | 1947 | `	if( pNode == 0 ){` |
|       449 | 1948 | `		return;` |
|         - | 1949 | `	}` |
|       187 | 1950 | `	pMin = pNode->pStart;` |
|       187 | 1951 | `	pMax = pNode->pEnd;` |
|       187 | 1952 | `	PH7_ExprSubtreeSpan(pNode->pLeft,&pCMin,&pCMax);` |
|       187 | 1953 | `	PH7_ExprSubtreeSpan(pNode->pRight,&pCMin,&pCMax);` |
|       187 | 1954 | `	PH7_ExprSubtreeSpan(pNode->pCond,&pCMin,&pCMax);` |
|       187 | 1955 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       193 | 1956 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|         7 | 1957 | `		PH7_ExprSubtreeSpan(apArg[n],&pCMin,&pCMax);` |
|         4 | 1958 | `	}` |
|       187 | 1959 | `	if( pCMin && (pMin == 0 \|\| pCMin < pMin) ){` |
|        52 | 1960 | `		pMin = pCMin;` |
|        25 | 1961 | `	}` |
|       187 | 1962 | `	if( pCMax && (pMax == 0 \|\| pCMax > pMax) ){` |
|        59 | 1963 | `		pMax = pCMax;` |
|        28 | 1964 | `	}` |
|       187 | 1965 | `	if( (pNode->iFlags & EXPR_NODE_PARENS) && pMin && pMax ){` |
|         5 | 1966 | `		pMin--;` |
|         5 | 1967 | `		pMax++;` |
|         2 | 1968 | `	}` |
|       182 | 1969 | `	if( pNode->pOp && pMax` |
|        61 | 1970 | `	 && (pNode->pOp->iOp == EXPR_OP_FUNC_CALL \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT) ){` |
|         - | 1971 | `		/* A postfix call/subscript's extent stops AT its closing ')' / ']' (the` |
|         - | 1972 | `		 * closer's node was consumed building the postfix op); token contiguity` |
|         - | 1973 | `		 * puts the closer exactly at the extent, so widen one token past it. */` |
|         5 | 1974 | `		pMax++;` |
|         2 | 1975 | `	}` |
|       187 | 1976 | `	if( pMin && (*ppMin == 0 \|\| pMin < *ppMin) ){` |
|       137 | 1977 | `		*ppMin = pMin;` |
|        66 | 1978 | `	}` |
|       187 | 1979 | `	if( pMax && (*ppMax == 0 \|\| pMax > *ppMax) ){` |
|       185 | 1980 | `		*ppMax = pMax;` |
|        90 | 1981 | `	}` |
|       318 | 1982 | `}` |
|         - | 1983 | ` /*` |
|         - | 1984 | `  * Create an expression tree from an array of tokens.` |
|         - | 1985 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 1986 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1987 | `  */` |
|   4068417 | 1988 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1989 | ` {` |
|         - | 1990 | `	 sxi32 i,iLeft,iRight;` |
|         - | 1991 | `	 ph7_expr_node *pNode;` |
|         - | 1992 | `	 ph7_expr_node *pSuppress;` |
|   4068422 | 1993 | `	 ph7_expr_node *pUnOuter = 0;` |
|         - | 1994 | `	 sxi32 iCur;` |
|         - | 1995 | `	 sxi32 rc;` |
|   4068422 | 1996 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 1997 | `		 /* TICKET 1433-17: self evaluating node */` |
|   1890584 | 1998 | `		 return SXRET_OK;` |
|         - | 1999 | `	 }` |
|         - | 2000 | `	 /* Process expressions enclosed in parenthesis first */` |
|  14319747 | 2001 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 2002 | `		 sxi32 iNest;` |
|         - | 2003 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 2004 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 2005 | `		  */` |
|  12141913 | 2006 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  11952230 | 2007 | `			 continue;` |
|         - | 2008 | `		 }` |
|    189688 | 2009 | `		 iNest = 1;` |
|    189688 | 2010 | `		 iLeft = iCur;` |
|         - | 2011 | `		 /* Find the closing parenthesis */` |
|    189688 | 2012 | `		 iCur++;` |
|   1245446 | 2013 | `		 while( iCur < nToken ){` |
|   1245446 | 2014 | `			 if( apNode[iCur] ){` |
|   1245446 | 2015 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 2016 | `					 /* Decrement nesting level */` |
|    280561 | 2017 | `					 iNest--;` |
|    280561 | 2018 | `					 if( iNest <= 0 ){` |
|    189688 | 2019 | `						 break;` |
|         5 | 2020 | `					 }` |
|   1010265 | 2021 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 2022 | `					 /* Increment nesting level */` |
|     90878 | 2023 | `					 iNest++;` |
|     45375 | 2024 | `				 }` |
|    527184 | 2025 | `			 }` |
|   1055763 | 2026 | `			 iCur++;` |
|         5 | 2027 | `		 }` |
|    189688 | 2028 | `		 if( iCur - iLeft > 1 ){` |
|         - | 2029 | `			 sxi32 j;` |
|         - | 2030 | `			 /* Recurse and process this expression */` |
|    189688 | 2031 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    189688 | 2032 | `			 if( rc != SXRET_OK ){` |
|         5 | 2033 | `				 return rc;` |
|         - | 2034 | `			 }` |
|         - | 2035 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 2036 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 2037 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 2038 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 2039 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 2040 | `			  * group's free below silently drops the unpacking. */` |
|    189684 | 2041 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    189684 | 2042 | `				 if( apNode[j] ){` |
|    189684 | 2043 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    189679 | 2044 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    189684 | 2045 | `					 break;` |
|         - | 2046 | `				 }` |
|       ! 0 | 2047 | `			 }` |
|     94719 | 2048 | `		 }` |
|         - | 2049 | `		 /* Drop the enclosing delimiters; the extraction set frees them. */` |
|    189684 | 2050 | `		 apNode[iLeft] = 0;` |
|    189684 | 2051 | `		 apNode[iCur] = 0;` |
|     94724 | 2052 | `	 }` |
|         - | 2053 | `	  /* Process expressions enclosed in braces */` |
|  15564814 | 2054 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 2055 | `		 sxi32 iNest;` |
|         - | 2056 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 2057 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 2058 | `		  */` |
|  13386980 | 2059 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  13386800 | 2060 | `			 continue;` |
|         - | 2061 | `		 }` |
|       184 | 2062 | `		 iNest = 1;` |
|       184 | 2063 | `		 iLeft = iCur;` |
|         - | 2064 | `		 /* Find the closing parenthesis */` |
|       184 | 2065 | `		 iCur++;` |
|       356 | 2066 | `		 while( iCur < nToken ){` |
|       356 | 2067 | `			 if( apNode[iCur] ){` |
|       356 | 2068 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 2069 | `					 /* Decrement nesting level */` |
|       184 | 2070 | `					 iNest--;` |
|       184 | 2071 | `					 if( iNest <= 0 ){` |
|       184 | 2072 | `						 break;` |
|       ! 0 | 2073 | `					 }` |
|       173 | 2074 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 2075 | `					 /* Increment nesting level */` |
|       ! 0 | 2076 | `					 iNest++;` |
|       ! 0 | 2077 | `				 }` |
|        86 | 2078 | `			 }` |
|       173 | 2079 | `			 iCur++;` |
|         1 | 2080 | `		 }` |
|       184 | 2081 | `		 if( iCur - iLeft > 1 ){` |
|         - | 2082 | `			 /* Recurse and process this expression */` |
|       173 | 2083 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       173 | 2084 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 2085 | `				 return rc;` |
|         - | 2086 | `			 }` |
|        86 | 2087 | `		 }` |
|         - | 2088 | `		 /* Drop the enclosing delimiters; the extraction set frees them. */` |
|       184 | 2089 | `		 apNode[iLeft] = 0;` |
|       184 | 2090 | `		 apNode[iCur] = 0;` |
|        94 | 2091 | `	 }` |
|         - | 2092 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   2177839 | 2093 | `	 iLeft = -1;` |
|  15565126 | 2094 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  13387306 | 2095 | `		 if( apNode[iCur] == 0 ){` |
|   5406146 | 2096 | `			 continue;` |
|         - | 2097 | `		 }` |
|   7981165 | 2098 | `		 pNode = apNode[iCur];` |
|   7981165 | 2099 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   1235788 | 2100 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 2101 | `				 /* Collect function arguments */` |
|    934666 | 2102 | `				 sxi32 iPtr = 0;` |
|    934666 | 2103 | `				 sxi32 nFuncTok = 0;` |
|   4496195 | 2104 | `				 while( nFuncTok + iCur < nToken ){` |
|   4496195 | 2105 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|         - | 2106 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|         - | 2107 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|         - | 2108 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|         - | 2109 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|         - | 2110 | `					  * nulled, so counting it here would over-count and never find` |
|         - | 2111 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   4496195 | 2112 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   4422373 | 2113 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   1036541 | 2114 | `							 iPtr++;` |
|   3902977 | 2115 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   1036541 | 2116 | `							 iPtr--;` |
|   1036541 | 2117 | `							 if( iPtr <= 0 ){` |
|    934666 | 2118 | `								 break;` |
|         - | 2119 | `							 }` |
|     50813 | 2120 | `						 }` |
|   1740031 | 2121 | `					 }` |
|   3561534 | 2122 | `					 nFuncTok++;` |
|         5 | 2123 | `				 }` |
|    934666 | 2124 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 2125 | `					 /* Syntax error */` |
|       ! 0 | 2126 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 2127 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2128 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2129 | `					 }` |
|       ! 0 | 2130 | `					 return rc;` |
|         - | 2131 | `				 }` |
|    934666 | 2132 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 2133 | `					 /* Syntax error */` |
|       ! 0 | 2134 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 2135 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2136 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2137 | `					 }` |
|       ! 0 | 2138 | `					 return rc;` |
|         - | 2139 | `				 }` |
|    934666 | 2140 | `				 if( nFuncTok > 1 ){` |
|         - | 2141 | `					 /* Process function arguments */` |
|    897098 | 2142 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|    897098 | 2143 | `					 if( rc != SXRET_OK ){` |
|        11 | 2144 | `						 return rc;` |
|         - | 2145 | `					 }` |
|    447613 | 2146 | `				 }` |
|         - | 2147 | `				 /* Link the node to the tree */` |
|    934658 | 2148 | `				 pNode->pLeft = apNode[iLeft];` |
|    934658 | 2149 | `				 apNode[iLeft] = 0;` |
|   4496163 | 2150 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   3561510 | 2151 | `					 apNode[iCur+iPtr] = 0;` |
|   1776875 | 2152 | `				 }` |
|         - | 2153 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 2154 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 2155 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 2156 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 2157 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 2158 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 2159 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 2160 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 2161 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 2162 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 2163 | `				 {` |
|    934658 | 2164 | `					 sxi32 iNew = iLeft - 1;` |
|    975321 | 2165 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     40668 | 2166 | `						 iNew--;` |
|         5 | 2167 | `					 }` |
|    934653 | 2168 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    518041 | 2169 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    303577 | 2170 | `						 && apNode[iNew]->pLeft == 0 ){` |
|     96599 | 2171 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|     96599 | 2172 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|     96599 | 2173 | `						 apNode[iNew] = 0;` |
|     96599 | 2174 | `						 pNode = apNode[iCur];` |
|     48236 | 2175 | `					 }` |
|         - | 2176 | `				 }` |
|    767450 | 2177 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 2178 | `				 /* Subscripting */` |
|    265479 | 2179 | `				 sxi32 iArrTok = iCur + 1;` |
|    265479 | 2180 | `				 sxi32 iNest = 1;` |
|         - | 2181 | ``				 /* php's `dereferencable` list has `'(' expr ')'` in it, so WHATEVER a`` |
|         - | 2182 | `				  * parenthesised group evaluates to may be subscripted:` |
|         - | 2183 | ``				  * `((array)$o)['k']`, `((string)$s)[1]`, `(clone $o)[0]`, `(1+2)[0]`,`` |
|         - | 2184 | ``				  * `(1)[0]`. The base test below is a whitelist of node SHAPES, and no`` |
|         - | 2185 | `				  * shape describes "the user wrote parentheses", so every such group` |
|         - | 2186 | `				  * whose root was not already a term or a postfix chain was refused as` |
|         - | 2187 | `` 				  * `Invalid array name` — a compile fatal on source php runs. The `->` `` |
|         - | 2188 | `				  * branch further down reads the same flag for the same reason. */` |
|    265474 | 2189 | `				 if( !(iLeft >= 0 && apNode[iLeft] && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS))` |
|    265451 | 2190 | `					 && ( iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        60 | 2191 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        54 | 2192 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         - | 2193 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|         - | 2194 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|         - | 2195 | `					  * subscripting a global constant raised "Invalid array name" while` |
|         - | 2196 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|         - | 2197 | `					  * worked. */` |
|        54 | 2198 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        36 | 2199 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    265416 | 2200 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 2201 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 2202 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|       891 | 2203 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ) ){` |
|         - | 2204 | `						 /* Syntax error */` |
|       ! 0 | 2205 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 2206 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2207 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2208 | `						 }` |
|       ! 0 | 2209 | `						 return rc;` |
|         - | 2210 | `				 }` |
|         - | 2211 | `				 /* Collect index tokens */` |
|    476198 | 2212 | `				 while( iArrTok < nToken ){` |
|    476198 | 2213 | `					 if( apNode[iArrTok] ){` |
|    476166 | 2214 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 2215 | `							 /* Increment nesting level */` |
|         8 | 2216 | `							 iNest++;` |
|    476163 | 2217 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 2218 | `							 /* Decrement nesting level */` |
|    265485 | 2219 | `							 iNest--;` |
|    265485 | 2220 | `							 if( iNest <= 0 ){` |
|    265479 | 2221 | `								 break;` |
|         - | 2222 | `							 }` |
|         3 | 2223 | `						 }` |
|    105200 | 2224 | `					 }` |
|    210724 | 2225 | `					 ++iArrTok;` |
|         5 | 2226 | `				 }` |
|    265479 | 2227 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 2228 | ``					 /* php: a stray token in a subscript index is `... expecting "]"`. */`` |
|    196752 | 2229 | `					 const char *zSaveIdx = pGen->zClauseCloser;` |
|    196752 | 2230 | `					 pGen->zClauseCloser = "\"]\"";` |
|         - | 2231 | `					 /* Recurse and process this expression */` |
|    196752 | 2232 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    196752 | 2233 | `					 pGen->zClauseCloser = zSaveIdx;` |
|    196752 | 2234 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 2235 | `						 return rc;` |
|         - | 2236 | `					 }` |
|         - | 2237 | `					 /* Link the node to it's index */` |
|    196752 | 2238 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|     98239 | 2239 | `				 }` |
|         - | 2240 | `				 /* Link the node to the tree */` |
|    265479 | 2241 | `				 pNode->pLeft = apNode[iLeft];` |
|    265479 | 2242 | `				 pNode->pRight = 0;` |
|    265479 | 2243 | `				 apNode[iLeft] = 0;` |
|    741672 | 2244 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    476198 | 2245 | `					 apNode[iNest] = 0;` |
|    237775 | 2246 | `				 }` |
|    132559 | 2247 | `			 }else{` |
|         - | 2248 | `				 /* Member access operators [i.e: '->','::'] */` |
|     35653 | 2249 | `				  iRight = iCur + 1;` |
|     35825 | 2250 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       173 | 2251 | `					 iRight++;` |
|         1 | 2252 | `				 }` |
|     35653 | 2253 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2254 | `					 /* Syntax error */` |
|         5 | 2255 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 2256 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 2257 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2258 | `					 }` |
|         5 | 2259 | `					 return rc;` |
|         - | 2260 | `				 }` |
|         - | 2261 | `				 /* Validate the left operand BEFORE linking it. The refusal below` |
|         - | 2262 | `				  * used to be reached with the operand already installed as` |
|         - | 2263 | `				  * pNode->pLeft AND still standing in apNode[], which the recursive` |
|         - | 2264 | ``				  * release then freed twice -- a heap-use-after-free `1->x;`,`` |
|         - | 2265 | ``				  * `"s"->x;` and `[1]->x;` all reached. Nothing owns a node through`` |
|         - | 2266 | `				  * a tree any more (PH7_ExprFreeTree), so the order is no longer` |
|         - | 2267 | `				  * load-bearing; it is kept because refusing before mutating is the` |
|         - | 2268 | `				  * clearer shape either way. */` |
|     35644 | 2269 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|     33429 | 2270 | `					 && apNode[iLeft]->pOp == 0 &&` |
|     26139 | 2271 | `					 apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         - | 2272 | ``					 /* A PARENTHESISED group is php's `( expr )` dereferencable: whatever it`` |
|         - | 2273 | ``					  * evaluates to may be reached through `->`, which is how the closure`` |
|         - | 2274 | ``					  * idioms are written — `(function(){ … })->bindTo($o)`,`` |
|         - | 2275 | ``					  * `(fn() => …)->call($o)`, `(match($k){ … })->m()`. PHL refused all of`` |
|         - | 2276 | `					  * them as "Expecting a variable as left operand", a compile fatal on` |
|         - | 2277 | `					  * valid php, because a literal TERM carries no operator. */` |
|        46 | 2278 | `					 (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 &&` |
|         - | 2279 | ``					 /* php's `dereferencable` also covers the SCALAR forms — a quoted`` |
|         - | 2280 | `					  * string (interpolated or not) and an array literal — plus a` |
|         - | 2281 | ``					  * CONSTANT, and it runs them: `"s"->p` warns `Attempt to read`` |
|         - | 2282 | ``					  * property "p" on string` and yields null, `"s"->m()` is the`` |
|         - | 2283 | `					  * member-function Error. Refusing them at COMPILE time killed the` |
|         - | 2284 | `					  * whole file instead. A NUMBER literal and a heredoc stay refused` |
|         - | 2285 | ``					  * — those are php's own parse error for `1->x`. */`` |
|        26 | 2286 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString &&` |
|        24 | 2287 | `					 apNode[iLeft]->xCode != PH7_CompileString &&` |
|        18 | 2288 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        11 | 2289 | `					 apNode[iLeft]->xCode != PH7_CompileArray &&` |
|         4 | 2290 | `					 apNode[iLeft]->xCode != PH7_CompileShortArray ){` |
|         - | 2291 | `						 /* Syntax error */` |
|         4 | 2292 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2293 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|         3 | 2294 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2295 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2296 | `						 }` |
|         3 | 2297 | `						 return rc;` |
|         - | 2298 | `				 }` |
|         - | 2299 | `				 /* Link the node to the tree */` |
|     35647 | 2300 | `				 pNode->pLeft = apNode[iLeft];` |
|     35647 | 2301 | `				 pNode->pRight = apNode[iRight];` |
|     35647 | 2302 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 2303 | `			 }` |
|    616679 | 2304 | `		 }` |
|   7981151 | 2305 | `		 iLeft = iCur;` |
|   3984373 | 2306 | `	 }` |
|         - | 2307 | `	 /* Handle the prefix (new, clone) operators. Walk RIGHT to LEFT: both take the` |
|         - | 2308 | `	  * operand on their right, so a nested one has to be linked before the outer` |
|         - | 2309 | ``	  * sees it. Left-to-right, `clone new Q` reached the still-unlinked `new` node —`` |
|         - | 2310 | `	  * not a term yet — and answered php's own valid source with the compile fatal` |
|         - | 2311 | ``	  * "'clone': Expecting class constructor call". `clone new Q()` worked only`` |
|         - | 2312 | `	  * because the postfix pass folds a constructor CALL into its new-node early. */` |
|  15565090 | 2313 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  13387270 | 2314 | `		 if( apNode[iCur] == 0 ){` |
|   6738509 | 2315 | `			 continue;` |
|         - | 2316 | `		 }` |
|   6648766 | 2317 | `		 pNode = apNode[iCur];` |
|   6648766 | 2318 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 2319 | `			 SyToken *pToken;` |
|         - | 2320 | `			 /* Get the left node */` |
|      3271 | 2321 | `			 iLeft = iCur + 1;` |
|      3425 | 2322 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|       155 | 2323 | `				 iLeft++;` |
|         1 | 2324 | `			 }` |
|      3271 | 2325 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2326 | `				  /* Syntax error */` |
|       ! 0 | 2327 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 2328 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 2329 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2330 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2331 | `				 }` |
|       ! 0 | 2332 | `				 return rc;` |
|         - | 2333 | `			 }` |
|         - | 2334 | `			 /* Make sure the operand are of a valid type. CLONE takes ANY expression —` |
|         - | 2335 | ``			  * php's grammar is `clone expr`, and what that expression evaluates to is a`` |
|         - | 2336 | `			  * RUNTIME question: a non-object operand is php's catchable` |
|         - | 2337 | ``			  * `clone(): Argument #1 ($object) must be of type object, %s given`, which`` |
|         - | 2338 | `			  * OP_CLONE already raises. The whitelist that used to sit here (a variable,` |
|         - | 2339 | ``			  * or any operator node) refused `clone 5`, `clone []`, `clone null` and`` |
|         - | 2340 | ``			  * `clone match(…){…}` at COMPILE time — the first three with a diagnostic php`` |
|         - | 2341 | `			  * never prints, the last on source php runs. NEW keeps its own, because its` |
|         - | 2342 | `			  * operand is a class-name REFERENCE, not a value. */` |
|      3271 | 2343 | `			 if( pNode->pOp->iOp != EXPR_OP_CLONE ){` |
|         - | 2344 | `				 /* New */` |
|      2974 | 2345 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         7 | 2346 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 2347 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 2348 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 2349 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 2350 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 2351 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 2352 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 2353 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 2354 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2355 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2356 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2357 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2358 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2359 | `					 }` |
|       ! 0 | 2360 | `					 return rc;` |
|         - | 2361 | `				 }` |
|      2979 | 2362 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      2975 | 2363 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      2970 | 2364 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       147 | 2365 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 2366 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 2367 | `						 /* Syntax error */` |
|       ! 0 | 2368 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2369 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2370 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2371 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2372 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2373 | `						 }` |
|       ! 0 | 2374 | `						 return rc;` |
|         - | 2375 | `					 }` |
|      1483 | 2376 | `				 }` |
|      1485 | 2377 | `			 }` |
|         - | 2378 | `			  /* Link the node to the tree */` |
|      3271 | 2379 | `			 pNode->pLeft = apNode[iLeft];` |
|      3271 | 2380 | `			 apNode[iLeft] = 0;` |
|      3271 | 2381 | `			 pNode->pRight = 0; /* Paranoid */` |
|      1631 | 2382 | `		 }` |
|   3319452 | 2383 | `	 }` |
|         - | 2384 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   2177825 | 2385 | `	 iLeft = -1;` |
|  15565090 | 2386 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  13387270 | 2387 | `		 if( apNode[iCur] == 0 ){` |
|   6741775 | 2388 | `			 continue;` |
|         - | 2389 | `		 }` |
|   6645500 | 2390 | `		 pNode = apNode[iCur];` |
|   6645500 | 2391 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     61682 | 2392 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */` |
|         - | 2393 | ``					 /* …but `A::K++` is php's parse error, not an increment of`` |
|         - | 2394 | `					  * class-level storage: a class CONSTANT is not a variable. */` |
|       205 | 2395 | `					 && !PH7_ExprNodeIsClassConst(apNode[iLeft]))` |
|     54693 | 2396 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 2397 | `					 /* Link the node to the tree */` |
|     54790 | 2398 | `					 pNode->pLeft = apNode[iLeft];` |
|     54790 | 2399 | `					 apNode[iLeft] = 0;` |
|     27355 | 2400 | `			 }` |
|     30746 | 2401 | `		  }` |
|   6645500 | 2402 | `		 iLeft = iCur;` |
|   3317821 | 2403 | `	  }` |
|   2177825 | 2404 | `	 iLeft = -1;` |
|  15565080 | 2405 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  13387264 | 2406 | `		 if( apNode[iCur] == 0 ){` |
|   6796556 | 2407 | `			 continue;` |
|         - | 2408 | `		 }` |
|   6590713 | 2409 | `		 pNode = apNode[iCur];` |
|   6590713 | 2410 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      6791 | 2411 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      6794 | 2412 | `				 \|\| ( apNode[iLeft]->pOp && (apNode[iLeft]->pOp->iPrec != 2 /* Postfix */` |
|        28 | 2413 | `					 \|\| PH7_ExprNodeIsClassConst(apNode[iLeft]))) ){` |
|         - | 2414 | `					 /* Not a variable. Nothing to the right at all means the operator` |
|         - | 2415 | `					  * was POSTFIX and its target (already passed over) was refused,` |
|         - | 2416 | `					  * which is where php stops; otherwise this is a PREFIX operator` |
|         - | 2417 | `					  * over a non-variable and php stops past that operand. */` |
|         6 | 2418 | `					 if( iLeft < 0 ){` |
|         3 | 2419 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2420 | `					 }` |
|         3 | 2421 | `					 return PH7_ExprOperandNotAVariable(pGen,apNode[iLeft]);` |
|         - | 2422 | `			 }` |
|         - | 2423 | `			 /* Link the node to the tree */` |
|      6792 | 2424 | `			 pNode->pLeft = apNode[iLeft];` |
|      6792 | 2425 | `			 apNode[iLeft] = 0;` |
|         - | 2426 | `			 /* Mark as pre-increment/decrement node */` |
|      6792 | 2427 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      3389 | 2428 | `		  }` |
|   6590709 | 2429 | `		 iLeft = iCur;` |
|   3290463 | 2430 | `	 }` |
|         - | 2431 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|         - | 2432 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|         - | 2433 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|         - | 2434 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|         - | 2435 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|         - | 2436 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|         - | 2437 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|         - | 2438 | `	  * pass below skips it (pLeft != 0). */` |
|   2177821 | 2439 | `	 iLeft = -1;` |
|  15565070 | 2440 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  13387254 | 2441 | `		 if( apNode[iCur] == 0 ){` |
|   6817417 | 2442 | `			 continue;` |
|         - | 2443 | `		 }` |
|   6569842 | 2444 | `		 pNode = apNode[iCur];` |
|   6569842 | 2445 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|     14083 | 2446 | `			 iRight = iCur + 1;` |
|     14087 | 2447 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         5 | 2448 | `				 iRight++;` |
|         1 | 2449 | `			 }` |
|     14083 | 2450 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|       ! 0 | 2451 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2452 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2453 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2454 | `				 }` |
|       ! 0 | 2455 | `				 return rc;` |
|         - | 2456 | `			 }` |
|     14083 | 2457 | `			 pNode->pLeft = apNode[iLeft];` |
|     14083 | 2458 | `			 pNode->pRight = apNode[iRight];` |
|     14083 | 2459 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      7030 | 2460 | `		 }` |
|   6569842 | 2461 | `		 iLeft = iCur;` |
|   3280043 | 2462 | `	 }` |
|         - | 2463 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   2177821 | 2464 | `	  iLeft = 0;` |
|  15565064 | 2465 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  13387250 | 2466 | `		  if( apNode[iCur] ){` |
|   6555760 | 2467 | `			  pNode = apNode[iCur];` |
|   6555760 | 2468 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    189220 | 2469 | `				  if( iLeft > 0 ){` |
|         - | 2470 | `					  /* Link the node to the tree */` |
|    189218 | 2471 | `					  pNode->pLeft = apNode[iLeft];` |
|    189218 | 2472 | `					  apNode[iLeft] = 0;` |
|    189218 | 2473 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|         - | 2474 | `						  /* "Is the operand a finished subtree?" — a binary node fills` |
|         - | 2475 | `						   * pLeft+pRight and a full ternary fills all three, but a SHORT` |
|         - | 2476 | ``						   * ternary (`a ?: b`, which is what `!($a ?: $b)` hands here)`` |
|         - | 2477 | `						   * fills pCond+pRight and leaves pLeft NULL on purpose. Reading` |
|         - | 2478 | `						   * pLeft alone called it unfinished and refused source php` |
|         - | 2479 | `						   * compiles — every unary and cast over a parenthesised elvis. */` |
|      6940 | 2480 | `						  if( pNode->pLeft->pRight == 0` |
|      6945 | 2481 | `							  \|\| (pNode->pLeft->pLeft == 0 && pNode->pLeft->pCond == 0) ){` |
|         - | 2482 | `							   /* Syntax error */` |
|       ! 0 | 2483 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2484 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2485 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 2486 | `							  }` |
|       ! 0 | 2487 | `							  return rc;` |
|         - | 2488 | `						  }` |
|      3465 | 2489 | `					  }` |
|     94472 | 2490 | `				  }else{` |
|         - | 2491 | `					  /* Syntax error */` |
|         3 | 2492 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 2493 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 2494 | `						  rc = SXERR_SYNTAX;` |
|         1 | 2495 | `					  }` |
|         3 | 2496 | `					  return rc;` |
|         - | 2497 | `				  }` |
|     94467 | 2498 | `			  }` |
|         - | 2499 | `			  /* Save terminal position */` |
|   6555758 | 2500 | `			  iLeft = iCur;` |
|   3273005 | 2501 | `		  }` |
|   6682314 | 2502 | `	  }` |
|         - | 2503 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 2504 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 2505 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 2506 | `	  * yielding a right-leaning tree. */` |
|  15565062 | 2507 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  13387248 | 2508 | `		 if( apNode[iCur] == 0 ){` |
|   7021310 | 2509 | `			 continue;` |
|         - | 2510 | `		 }` |
|   6365943 | 2511 | `		 pNode = apNode[iCur];` |
|   6365943 | 2512 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 2513 | `			 sxi32 iL, iR;` |
|         - | 2514 | `			 /* Find the right operand */` |
|       604 | 2515 | `			 iR = -1;` |
|         - | 2516 | `			 {` |
|         - | 2517 | `				 sxi32 j;` |
|      1026 | 2518 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      1026 | 2519 | `					 if( apNode[j] ){ iR = j; break; }` |
|       212 | 2520 | `				 }` |
|         - | 2521 | `			 }` |
|         - | 2522 | `			 /* Find the left operand */` |
|       604 | 2523 | `			 iL = -1;` |
|         - | 2524 | `			 {` |
|         - | 2525 | `				 sxi32 j;` |
|      1176 | 2526 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      1176 | 2527 | `					 if( apNode[j] ){ iL = j; break; }` |
|       287 | 2528 | `				 }` |
|         - | 2529 | `			 }` |
|       604 | 2530 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 2531 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2532 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2533 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2534 | `				 }` |
|       ! 0 | 2535 | `				 return rc;` |
|         - | 2536 | `			 }` |
|       604 | 2537 | `			 pNode->pLeft  = apNode[iL];` |
|       604 | 2538 | `			 pNode->pRight = apNode[iR];` |
|       604 | 2539 | `			 apNode[iL] = 0;` |
|       604 | 2540 | `			 apNode[iR] = 0;` |
|         - | 2541 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 2542 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 2543 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 2544 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 2545 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 2546 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 2547 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 2548 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 2549 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 2550 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 2551 | `			  * operands are respected. */` |
|       602 | 2552 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       354 | 2553 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        98 | 2554 | `				 && pNode->pLeft->pLeft != 0` |
|        92 | 2555 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 2556 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 2557 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 2558 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 2559 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 2560 | `				 while( pTail->pLeft` |
|        34 | 2561 | `					 && pTail->pLeft->pOp` |
|        23 | 2562 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 2563 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 2564 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 2565 | `					 pTail = pTail->pLeft;` |
|         1 | 2566 | `				 }` |
|         - | 2567 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 2568 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 2569 | `				 pTail->pLeft = pNode;` |
|        27 | 2570 | `				 apNode[iCur] = pHead;` |
|        13 | 2571 | `			 }` |
|       301 | 2572 | `		 }` |
|   3178242 | 2573 | `	 }` |
|         - | 2574 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  23955833 | 2575 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  21778037 | 2576 | `		 iLeft = -1;` |
| 155650021 | 2577 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 133872007 | 2578 | `			 if( apNode[iCur] == 0 ){` |
|  85136968 | 2579 | `				 continue;` |
|         - | 2580 | `			 }` |
|  48735044 | 2581 | `			 pNode = apNode[iCur];` |
|  48735044 | 2582 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|   1301712 | 2583 | ``				 ph7_expr_node *pRefUn = 0;      /* `=&` under a prefix unary */`` |
|   1301712 | 2584 | `				 ph7_expr_node *pRefUnOuter = 0;` |
|   1301712 | 2585 | ``				 ph7_expr_node *pRefCmp = 0;     /* comparison to re-hang a `=&` under */`` |
|         - | 2586 | `				 /* Get the right node */` |
|   1301712 | 2587 | `				 iRight = iCur + 1;` |
|   1746840 | 2588 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    445133 | 2589 | `					 iRight++;` |
|         5 | 2590 | `				 }` |
|   1301712 | 2591 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2592 | `					 /* Syntax error */` |
|        11 | 2593 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        11 | 2594 | `					 if( rc != SXERR_ABORT ){` |
|        11 | 2595 | `						 rc = SXERR_SYNTAX;` |
|         4 | 2596 | `					 }` |
|        11 | 2597 | `					 return rc;` |
|         - | 2598 | `				 }` |
|   1301704 | 2599 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2600 | `					 sxi32  iTmp;` |
|         - | 2601 | ``					 /* php gives `=&` ASSIGNMENT precedence -- looser than every`` |
|         - | 2602 | ``					  * comparison -- so `null === $x =& $a[$k]` is`` |
|         - | 2603 | ``					  * `null === ($x =& $a[$k])`. This table processes `=&` at 12, one`` |
|         - | 2604 | ``					  * step TIGHTER than `===` at 11, because the operator's own rules`` |
|         - | 2605 | `					  * (the refusals below, the unary hoist, the nullsafe screens) live` |
|         - | 2606 | `					  * in this pass and moving it to 18 would leave all of them behind.` |
|         - | 2607 | `					  * A comparison has therefore already taken the variable this bind` |
|         - | 2608 | ``					  * means, and the bind was left with `(null === $x)` as its target --`` |
|         - | 2609 | `					  * not a variable, so the file was a parse error. symfony/translation` |
|         - | 2610 | `					  * writes exactly this shape, and it cost the whole file.` |
|         - | 2611 | `					  *` |
|         - | 2612 | `					  * Take the comparison's RIGHT operand as the bind target and hang` |
|         - | 2613 | `					  * the finished bind back under it, which is the tree php builds.` |
|         - | 2614 | ``					  * Parenthesised groups are left alone: `($a === $b) =& $c` really is`` |
|         - | 2615 | `					  * a refusal. */` |
|       428 | 2616 | `					 if( iLeft >= 0 && apNode[iLeft] && apNode[iLeft]->pOp` |
|       285 | 2617 | `					  && (apNode[iLeft]->pOp->iPrec == 10 \|\| apNode[iLeft]->pOp->iPrec == 11)` |
|        71 | 2618 | `					  && apNode[iLeft]->pRight != 0` |
|        76 | 2619 | `					  && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2620 | `						 pRefCmp = apNode[iLeft];` |
|       ! 0 | 2621 | `						 apNode[iLeft] = pRefCmp->pRight;` |
|       ! 0 | 2622 | `					 }` |
|         - | 2623 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 2624 | ``					 /* A prefix unary covers the whole BIND too — `@$a[0] =& $x` is`` |
|         - | 2625 | ``					  * `@($a[0] =& $x)`, and so are its `-`/`+`/`!`/`~`/cast spellings,`` |
|         - | 2626 | `					  * every one of which php runs. Same hoist the assignment path makes` |
|         - | 2627 | `					  * below, re-wrapped after the operands are swapped and linked. */` |
|         - | 2628 | `					 {` |
|       433 | 2629 | `						 ph7_expr_node *pUn = apNode[iLeft];` |
|       573 | 2630 | `						 while( pUn->pOp && pUn->pLeft` |
|       144 | 2631 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|       364 | 2632 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|       143 | 2633 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|         3 | 2634 | `							 pRefUn = pUn;      /* innermost unary over the bind target */` |
|         3 | 2635 | `							 pUn = pUn->pLeft;` |
|         1 | 2636 | `						 }` |
|       433 | 2637 | `						 if( pRefUn ){` |
|         3 | 2638 | `							 pRefUnOuter = apNode[iLeft]; /* the chain's result node */` |
|         3 | 2639 | `							 apNode[iLeft] = pUn;` |
|         1 | 2640 | `						 }` |
|         - | 2641 | `					 }` |
|         - | 2642 | `					 /* PHP 8.0: a reference and a nullsafe chain do not mix, and php` |
|         - | 2643 | `					  * has a different sentence for each SIDE — the bind target is a` |
|         - | 2644 | `					  * write like any other, while the SOURCE gets a wording of its` |
|         - | 2645 | `					  * own. Both operands are still in written order here; the swap` |
|         - | 2646 | `					  * below turns them over. */` |
|       428 | 2647 | `					 if( PH7_ExprContainsNullsafe(apNode[iLeft])` |
|       433 | 2648 | `					  \|\| PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|         8 | 2649 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         4 | 2650 | `							 PH7_ExprContainsNullsafe(apNode[iLeft])` |
|         - | 2651 | `								 ? "Can't use nullsafe operator in write context"` |
|         - | 2652 | `								 : "Cannot take reference of a nullsafe chain");` |
|         6 | 2653 | `						 if( rc != SXERR_ABORT ){` |
|         6 | 2654 | `							 rc = SXERR_SYNTAX;` |
|         2 | 2655 | `						 }` |
|         6 | 2656 | `						 return rc;` |
|         - | 2657 | `					 }` |
|         - | 2658 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|         - | 2659 | `					  * reference target — PH7_ExprIsModifiableValue accepts` |
|         - | 2660 | ``					  * EXPR_OP_ARROW (`->`) and a static-PROPERTY `::`, and rejects`` |
|         - | 2661 | ``					  * both a class CONSTANT and the nullsafe `?->` form, so no extra`` |
|         - | 2662 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|         - | 2663 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|       429 | 2664 | `					 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2665 | ``						 /* The bind TARGET is not a variable: php stops at the `=`. */`` |
|       ! 0 | 2666 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2667 | `					 }` |
|       429 | 2668 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       357 | 2669 | `						 if(  PH7_ExprIsModifiableValue(apNode[iRight]) == FALSE ){` |
|         - | 2670 | ``							 /* The SOURCE has to be a variable too, and php's `&new` /`` |
|         - | 2671 | ``							  * `&clone` legacy productions each stop somewhere of their`` |
|         - | 2672 | ``							  * own: `$r =& clone $o` names the `clone` keyword (nothing`` |
|         - | 2673 | `` 							  * in a `variable` may start with it), while `$r =& new A` `` |
|         - | 2674 | ``							  * enters php 4's `&new` rule, which wants the ARGUMENT`` |
|         - | 2675 | ``							  * list — `expecting "("` — unless one was written, in`` |
|         - | 2676 | `							  * which case the rule completes and php asks for the` |
|         - | 2677 | `							  * dereference like everything else. PHL accepted BOTH` |
|         - | 2678 | `							  * spellings and silently bound a copy. */` |
|         6 | 2679 | `							 if( apNode[iRight]->pOp` |
|         8 | 2680 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_CLONE ){` |
|         3 | 2681 | `								 rc = PH7_GenSyntaxError(pGen,apNode[iRight]->pStart,0);` |
|         3 | 2682 | `								 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2683 | `							 }` |
|         4 | 2684 | `							 if( apNode[iRight]->pOp` |
|         5 | 2685 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_NEW ){` |
|         3 | 2686 | `								 SyToken *pNMin = 0, *pNMax = 0;` |
|         3 | 2687 | `								 PH7_ExprSubtreeSpan(apNode[iRight],&pNMin,&pNMax);` |
|         2 | 2688 | `								 if( pNMax == 0 \|\| pNMax <= apNode[iRight]->pStart` |
|         3 | 2689 | `								  \|\| (pNMax[-1].nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 2690 | `									 rc = PH7_GenSyntaxError(pGen,` |
|         1 | 2691 | `										 PH7_ExprTokenInStream(pGen,pNMax),"\"(\"");` |
|         3 | 2692 | `									 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2693 | `								 }` |
|       ! 0 | 2694 | `							 }` |
|         3 | 2695 | `							 return PH7_ExprOperandNotAVariable(pGen,apNode[iRight]);` |
|         - | 2696 | `						 }` |
|       173 | 2697 | `					 }` |
|         - | 2698 | `					 /* Swap operands */` |
|       423 | 2699 | `					 iTmp = iRight;` |
|       423 | 2700 | `					 iRight = iLeft;` |
|       423 | 2701 | `					 iLeft = iTmp;` |
|       209 | 2702 | `				 }` |
|         - | 2703 | `				 /* Link the node to the tree */` |
|   1301694 | 2704 | `				 pNode->pLeft = apNode[iLeft];` |
|   1301694 | 2705 | `				 pNode->pRight = apNode[iRight];` |
|   1301694 | 2706 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   1301694 | 2707 | `				 if( pRefUn ){` |
|         - | 2708 | `					 /* Re-wrap: the unary chain now covers the whole bind. */` |
|         3 | 2709 | `					 pRefUn->pLeft = pNode;` |
|         3 | 2710 | `					 apNode[iCur] = pRefUnOuter;` |
|         1 | 2711 | `				 }` |
|   1301694 | 2712 | `				 if( pRefCmp ){` |
|         - | 2713 | `					 /* Re-hang: the comparison now compares against the bind's value. */` |
|       ! 0 | 2714 | `					 pRefCmp->pRight = apNode[iCur] ? apNode[iCur] : pNode;` |
|       ! 0 | 2715 | `					 apNode[iCur] = pRefCmp;` |
|       ! 0 | 2716 | `				 }` |
|    649939 | 2717 | `			 }` |
|  48735026 | 2718 | `			 iLeft = iCur;` |
|  24330611 | 2719 | `		 }` |
|  10870752 | 2720 | `	 }` |
|         - | 2721 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2722 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2723 | `	  * we are dealing with a single operator.` |
|         - | 2724 | `	  */` |
|   2177801 | 2725 | `	  iLeft = -1;` |
|  15352872 | 2726 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  13206381 | 2727 | `		  if( apNode[iCur] == 0 ){` |
|   9538264 | 2728 | `			  continue;` |
|         - | 2729 | `		  }` |
|   3668122 | 2730 | `		  pNode = apNode[iCur];` |
|         - | 2731 | ``		  /* `pLeft == 0` alone does NOT mean "not linked yet" for this operator: a`` |
|         - | 2732 | ``		   * SHORT ternary (`a ?: b`) leaves pLeft NULL on purpose and records its`` |
|         - | 2733 | `		   * operands in pCond/pRight. A completed elvis node sitting in this slot —` |
|         - | 2734 | ``		   * which is what a parenthesised group leaves behind, `($a ?: $b)` — was`` |
|         - | 2735 | `		   * therefore re-entered here, and the term to its left is whatever the` |
|         - | 2736 | ``		   * enclosing expression put there (the `=` of `$x = ($a ?: $b);`), so the`` |
|         - | 2737 | `		   * "missing condition" branch fired on source php compiles. pCond is the` |
|         - | 2738 | `		   * real linked/not-linked marker, and the nesting scan below already` |
|         - | 2739 | `		   * reads it that way. */` |
|   3668117 | 2740 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0` |
|     31473 | 2741 | `			  && pNode->pCond == 0 ){` |
|     31310 | 2742 | `			  sxi32 iNest = 1;` |
|     31310 | 2743 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2744 | `				  /* Missing condition */` |
|         5 | 2745 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         5 | 2746 | `				  if( rc != SXERR_ABORT ){` |
|         5 | 2747 | `					  rc = SXERR_SYNTAX;` |
|         2 | 2748 | `				  }` |
|         5 | 2749 | `				  return rc;` |
|         - | 2750 | `			  }` |
|         - | 2751 | `			  /* Get the right node */` |
|     31306 | 2752 | `			  iRight = iCur + 1;` |
|     85851 | 2753 | `			  while( iRight < nToken  ){` |
|     85851 | 2754 | `				  if( apNode[iRight] ){` |
|     62453 | 2755 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2756 | `						  /* Increment nesting level */` |
|       ! 0 | 2757 | `						  ++iNest;` |
|     62453 | 2758 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2759 | `						  /* Decrement nesting level */` |
|     31306 | 2760 | `						  --iNest;` |
|     31306 | 2761 | `						  if( iNest <= 0 ){` |
|     31306 | 2762 | `							  break;` |
|         - | 2763 | `						  }` |
|       ! 0 | 2764 | `					  }` |
|     15553 | 2765 | `				  }` |
|     54550 | 2766 | `				  iRight++;` |
|         5 | 2767 | `			  }` |
|     31306 | 2768 | `			  if( iRight > iCur + 1 ){` |
|         - | 2769 | `				  /* Recurse and process the then expression */` |
|     31152 | 2770 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     31152 | 2771 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2772 | `					  return rc;` |
|         - | 2773 | `				  }` |
|         - | 2774 | `				  /* Link the node to the tree */` |
|     31152 | 2775 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     15553 | 2776 | `			  }else{` |
|         - | 2777 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2778 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2779 | `			  }` |
|     31306 | 2780 | `			  apNode[iCur + 1] = 0;` |
|     31306 | 2781 | `			  if( iRight + 1 < nToken ){` |
|         - | 2782 | `				  /* Recurse and process the else expression */` |
|     31306 | 2783 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     31306 | 2784 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2785 | `					  return rc;` |
|         - | 2786 | `				  }` |
|         - | 2787 | `				  /* Link the node to the tree */` |
|     31306 | 2788 | `				  pNode->pRight = apNode[iRight + 1];` |
|     31306 | 2789 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     15635 | 2790 | `			  }else{` |
|       ! 0 | 2791 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2792 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2793 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2794 | `				 }` |
|       ! 0 | 2795 | `				 return rc;` |
|         - | 2796 | `			  }` |
|         - | 2797 | `			  /* Point to the condition */` |
|     31306 | 2798 | `			  pNode->pCond  = apNode[iLeft];` |
|     31306 | 2799 | `			  apNode[iLeft] = 0;` |
|     31306 | 2800 | `			  break;` |
|         - | 2801 | `		  }` |
|   3636817 | 2802 | `		  iLeft = iCur;` |
|   1815572 | 2803 | `	  }` |
|         - | 2804 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2805 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2806 | `	  * so there is no need for a precedence loop here.` |
|         - | 2807 | `	  */` |
|   2177797 | 2808 | `	 iRight = -1;` |
|  15564740 | 2809 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  13387034 | 2810 | `		 if( apNode[iCur] == 0 ){` |
|  10479663 | 2811 | `			 continue;` |
|         - | 2812 | `		 }` |
|   2907376 | 2813 | `		 pNode = apNode[iCur];` |
|   2907376 | 2814 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2815 | `			 /* Get the left node */` |
|    729444 | 2816 | `			 iLeft = iCur - 1;` |
|    978283 | 2817 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    248844 | 2818 | `				 iLeft--;` |
|         5 | 2819 | `			 }` |
|    729444 | 2820 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2821 | `				 /* Syntax error */` |
|        70 | 2822 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2823 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2824 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2825 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2826 | `				 }else{` |
|        65 | 2827 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2828 | `				 }` |
|        70 | 2829 | `				 if( rc != SXERR_ABORT ){` |
|        68 | 2830 | `					 rc = SXERR_SYNTAX;` |
|        32 | 2831 | `				 }` |
|        70 | 2832 | `				 return rc;` |
|         - | 2833 | `			 }` |
|         - | 2834 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2835 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2836 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2837 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2838 | `			  * a write. */` |
|    729378 | 2839 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|        15 | 2840 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2841 | `					 "Can't use nullsafe operator in write context");` |
|        15 | 2842 | `				 if( rc != SXERR_ABORT ){` |
|        15 | 2843 | `					 rc = SXERR_SYNTAX;` |
|         6 | 2844 | `				 }` |
|        15 | 2845 | `				 return rc;` |
|         - | 2846 | `			 }` |
|         - | 2847 | `			 /* Every PREFIX unary in php covers the whole assignment rather than just` |
|         - | 2848 | ``			  * its target: `@$x = expr` is `@($x = expr)`, and so are `-$x = 5`,`` |
|         - | 2849 | ``			  * `+$x = 5`, `!$x = 5`, `~$x = 5`, `(int)$x = 5` and `clone $o = 5` —`` |
|         - | 2850 | `			  * every one of them RUNS in php, writing $x and then applying the` |
|         - | 2851 | `			  * operator to the result. The unary phase has already bound the operator` |
|         - | 2852 | `			  * to the LHS, which leaves the assignment staring at a non-lvalue, so` |
|         - | 2853 | `			  * walk down to the innermost operand, let the assignment bind THERE, and` |
|         - | 2854 | ``			  * re-wrap below. Only `@` was handled here, so the other eight spellings`` |
|         - | 2855 | ``			  * did not compile at all. A PARENTHESISED operand (`(-$x) = 5`) is a`` |
|         - | 2856 | ``			  * genuine non-lvalue and stays refused, and `new` keeps its own`` |
|         - | 2857 | `			  * production, where php refuses too.` |
|         - | 2858 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    729366 | 2859 | `			 pSuppress = 0;` |
|    729366 | 2860 | `			 pUnOuter = 0;` |
|         - | 2861 | `			 {` |
|    729366 | 2862 | `				 ph7_expr_node *pUn = apNode[iLeft];` |
|    889001 | 2863 | `				 while( pUn->pOp && pUn->pLeft` |
|    159619 | 2864 | `					 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|    524832 | 2865 | `					 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|    159598 | 2866 | `					  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        43 | 2867 | `					 pSuppress = pUn;  /* innermost unary seen so far */` |
|        43 | 2868 | `					 pUn = pUn->pLeft;` |
|         1 | 2869 | `				 }` |
|    729366 | 2870 | `				 if( pSuppress ){` |
|        37 | 2871 | `					 pUnOuter = apNode[iLeft]; /* the chain's result node */` |
|        37 | 2872 | `					 apNode[iLeft] = pUn;` |
|        18 | 2873 | `				 }` |
|         - | 2874 | `			 }` |
|    729366 | 2875 | `			 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2876 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|         - | 2877 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|         - | 2878 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|         - | 2879 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|         - | 2880 | `				  * assignment there, leaving the binary operator as the outer node.` |
|         - | 2881 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|       379 | 2882 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|        29 | 2883 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|        29 | 2884 | `					 ph7_expr_node *pParent = pHost;` |
|        40 | 2885 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|        24 | 2886 | `						 && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|       ! 0 | 2887 | `						 pParent = pParent->pRight;` |
|       ! 0 | 2888 | `					 }` |
|         - | 2889 | `					 /* The spine may end on a PREFIX unary rather than on the lvalue` |
|         - | 2890 | ``					  * itself -- `c && !$d = f()`, which php reads as`` |
|         - | 2891 | ``					  * `c && !($d = f())` exactly as it reads the unparenthesised`` |
|         - | 2892 | ``					  * `!$d = f()`. Walk that chain down the same way the top-level`` |
|         - | 2893 | `					  * hoist above does and let the assignment bind at its innermost` |
|         - | 2894 | `					  * operand, leaving the unary wrapped around the assignment. The` |
|         - | 2895 | `					  * spine walk stopped at the unary (a unary node has no pRight),` |
|         - | 2896 | ``					  * so the whole shape was `syntax error, unexpected token "="`. */`` |
|        29 | 2897 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|        19 | 2898 | `						 ph7_expr_node *pUn = pParent->pRight;` |
|        19 | 2899 | `						 ph7_expr_node *pInnerUn = 0;` |
|        37 | 2900 | `						 while( pUn->pOp && pUn->pLeft` |
|        14 | 2901 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|        32 | 2902 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|         7 | 2903 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        15 | 2904 | `							 pInnerUn = pUn;` |
|        15 | 2905 | `							 pUn = pUn->pLeft;` |
|         1 | 2906 | `						 }` |
|        16 | 2907 | `						 if( pInnerUn && PH7_ExprIsModifiableValue(pUn)` |
|        15 | 2908 | `							 && PH7_ExprContainsNullsafe(pUn) == 0 ){` |
|        13 | 2909 | `							 pNode->pLeft = apNode[iRight]; /* assignment RHS value */` |
|        13 | 2910 | `							 pNode->pRight = pUn;           /* the extracted lvalue */` |
|        13 | 2911 | `							 pInnerUn->pLeft = pNode;       /* the unary chain now covers it */` |
|        13 | 2912 | `							 apNode[iCur] = pHost;` |
|        13 | 2913 | `							 apNode[iLeft] = apNode[iRight] = 0;` |
|        13 | 2914 | `							 iRight = iCur;` |
|        13 | 2915 | `							 continue;` |
|         - | 2916 | `						 }` |
|         2 | 2917 | `					 }` |
|        14 | 2918 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight)` |
|        15 | 2919 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|        11 | 2920 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|        11 | 2921 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|        11 | 2922 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|        11 | 2923 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|        11 | 2924 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|        11 | 2925 | `						 iRight = iCur;` |
|        11 | 2926 | `						 continue;` |
|         - | 2927 | `					 }` |
|         2 | 2928 | `				 }` |
|       504 | 2929 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       348 | 2930 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 2931 | ``					 /* The target is not a `variable` in php's grammar: php stops at`` |
|         - | 2932 | `					  * the assignment operator itself, whatever the target was. */` |
|        11 | 2933 | `					 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2934 | `				 }` |
|       172 | 2935 | `			 }` |
|         - | 2936 | `			 /* Link the node to the tree (Reverse) */` |
|    729336 | 2937 | `			 pNode->pLeft = apNode[iRight];` |
|    729336 | 2938 | `			 pNode->pRight = apNode[iLeft];` |
|    729336 | 2939 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    729336 | 2940 | `			 if( pSuppress ){` |
|         - | 2941 | `				 /* Re-wrap: the unary chain now covers the whole assignment. */` |
|        37 | 2942 | `				 pSuppress->pLeft = pNode;` |
|        37 | 2943 | `				 apNode[iCur] = pUnOuter;` |
|        18 | 2944 | `			 }` |
|    364159 | 2945 | `		 }` |
|   2907268 | 2946 | `		 iRight = iCur;` |
|   1451304 | 2947 | `	 }` |
|         - | 2948 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  10888535 | 2949 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|   8710829 | 2950 | `		 iLeft = -1;` |
|  62258513 | 2951 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  53547689 | 2952 | `			 if( apNode[iCur] == 0 ){` |
|  44836315 | 2953 | `				 continue;` |
|         - | 2954 | `			 }` |
|   8711379 | 2955 | `			 pNode = apNode[iCur];` |
|   8711379 | 2956 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 2957 | `				 /* Get the right node */` |
|        65 | 2958 | `				 iRight = iCur + 1;` |
|        87 | 2959 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        24 | 2960 | `					 iRight++;` |
|         2 | 2961 | `				 }` |
|        65 | 2962 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2963 | `					 /* Syntax error */` |
|       ! 0 | 2964 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2965 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2966 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2967 | `					 }` |
|       ! 0 | 2968 | `					 return rc;` |
|         - | 2969 | `				 }` |
|         - | 2970 | `				 /* Link the node to the tree */` |
|        65 | 2971 | `				 pNode->pLeft = apNode[iLeft];` |
|        65 | 2972 | `				 pNode->pRight = apNode[iRight];` |
|        65 | 2973 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        31 | 2974 | `			 }` |
|   8711379 | 2975 | `			 iLeft = iCur;` |
|   4348388 | 2976 | `		 }` |
|   4348113 | 2977 | `	 }` |
|         - | 2978 | `	 /* Point to the root of the expression tree */` |
|  13386840 | 2979 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  11209204 | 2980 | `		 if( apNode[iCur] ){` |
|   2103359 | 2981 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         - | 2982 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|         - | 2983 | `				  * operator sitting at its slot. The "expecting" clause is the closer` |
|         - | 2984 | ``				  * the enclosing construct set (`;` after `return`, `,`/`;` after`` |
|         - | 2985 | ``				  * `echo`, `)` for a for() post clause …); a for() clause defaults to`` |
|         - | 2986 | ``				  * `;` when nothing more specific was set. php prints no clause for a`` |
|         - | 2987 | `				  * plain expression statement, so a NULL closer stays clauseless. */` |
|        75 | 2988 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|        75 | 2989 | `				 const char *zExpect = pGen->zClauseCloser;` |
|        75 | 2990 | `				 if( zExpect == 0 && pGen->nCommaExprOk > 0 ){` |
|       ! 0 | 2991 | `					 zExpect = "\";\"";` |
|       ! 0 | 2992 | `				 }` |
|        75 | 2993 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,zExpect);` |
|        75 | 2994 | `				  if( rc != SXERR_ABORT ){` |
|        75 | 2995 | `					  rc = SXERR_SYNTAX;` |
|        35 | 2996 | `				  }` |
|        75 | 2997 | `				  return rc;` |
|         - | 2998 | `			 }` |
|   2103289 | 2999 | `			 apNode[0] = apNode[iCur];` |
|   2103289 | 3000 | `			 apNode[iCur] = 0;` |
|   1049880 | 3001 | `		 }` |
|   5595083 | 3002 | `	 }` |
|   2177641 | 3003 | `	 return SXRET_OK;` |
|   2030456 | 3004 | ` }` |
|         - | 3005 | ` /*` |
|         - | 3006 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 3007 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 3008 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 3009 | `  * This is the public interface used by the most code generator routines.` |
|         - | 3010 | `  */` |
|   2304188 | 3011 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 3012 | `{` |
|         - | 3013 | `	ph7_expr_node *aStack[EXPR_STACK_NODES];` |
|         - | 3014 | `	ph7_expr_node **apNode;` |
|         - | 3015 | `	ph7_expr_node *pNode;` |
|         - | 3016 | `	sxu32 nNode;` |
|         - | 3017 | `	sxi32 rc;` |
|         - | 3018 | `	/* Reset node container */` |
|   2304193 | 3019 | `	SySetReset(pExprNode);` |
|   2304193 | 3020 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 3021 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 3022 | `	{` |
|   2304193 | 3023 | `		int iLastWasTerm = 0;` |
|   2304193 | 3024 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  13959036 | 3025 | `		while( pGen->pIn < pGen->pEnd ){` |
|  11654898 | 3026 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  11654898 | 3027 | `			if( rc != SXRET_OK ){` |
|        54 | 3028 | `				return rc;` |
|         - | 3029 | `			}` |
|         - | 3030 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  11654848 | 3031 | `			if( pNode->xCode ){` |
|         - | 3032 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   5960953 | 3033 | `				iLastWasTerm = 1;` |
|   8669297 | 3034 | `			}else if( pNode->pOp ){` |
|         - | 3035 | `				/* Operator node */` |
|   3147301 | 3036 | `				iLastWasTerm = 0;` |
|   1571199 | 3037 | `			}else{` |
|         - | 3038 | `				/* Delimiter: ')' and ']' end terms */` |
|   2546604 | 3039 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 3040 | `			}` |
|         - | 3041 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 3042 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 3043 | `			 * node kind, so this single test covers all branches. */` |
|  11654848 | 3044 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 3045 | `			/* Save the extracted node */` |
|  11654848 | 3046 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 3047 | `		}` |
|         - | 3048 | `	}` |
|   2304143 | 3049 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 3050 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 3051 | `		*ppRoot = 0;` |
|       ! 0 | 3052 | `		return SXRET_OK;` |
|         - | 3053 | `	}` |
|         - | 3054 | `	/* Tree building CONSUMES its array -- every slot it folds into a tree, and` |
|         - | 3055 | `	 * every delimiter a fold swallows, is nulled -- so it cannot run on the set` |
|         - | 3056 | `	 * that OWNS the nodes (PH7_ExprFreeTree). It gets a copy, and the copy dies` |
|         - | 3057 | `	 * here: nothing downstream reads it, because a fold copies the node POINTER` |
|         - | 3058 | `	 * into pLeft/pRight/pCond or into the operator's aNodeArgs. An expression of` |
|         - | 3059 | `	 * up to EXPR_STACK_NODES tokens -- which is nearly all of them -- borrows the` |
|         - | 3060 | `	 * copy from this frame and allocates nothing at all. */` |
|   2304143 | 3061 | `	nNode = SySetUsed(pExprNode);` |
|   2304143 | 3062 | `	apNode = aStack;` |
|   2304143 | 3063 | `	if( nNode > EXPR_STACK_NODES ){` |
|        90 | 3064 | `		apNode = (ph7_expr_node **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,` |
|        29 | 3065 | `			nNode * sizeof(ph7_expr_node *));` |
|        61 | 3066 | `		if( apNode == 0 ){` |
|       ! 0 | 3067 | `			*ppRoot = 0;` |
|       ! 0 | 3068 | `			return SXERR_MEM;` |
|         - | 3069 | `		}` |
|        29 | 3070 | `	}` |
|   2304143 | 3071 | `	SyMemcpy(SySetBasePtr(pExprNode),(void *)apNode,nNode * sizeof(ph7_expr_node *));` |
|         - | 3072 | `	/* Make sure we are dealing with valid nodes */` |
|   2304143 | 3073 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)nNode);` |
|   2304143 | 3074 | `	if( rc == SXRET_OK ){` |
|         - | 3075 | `		/* Build the tree */` |
|   2304083 | 3076 | `		rc = ExprMakeTree(&(*pGen),apNode,(sxi32)nNode);` |
|   1150068 | 3077 | `	}` |
|         - | 3078 | `	/* On a syntax error the nodes stay where they are: the extraction set still` |
|         - | 3079 | `	 * holds every one of them and the caller releases it. */` |
|   2304143 | 3080 | `	*ppRoot = (rc == SXRET_OK) ? apNode[0] : 0;` |
|   2304143 | 3081 | `	if( apNode != aStack ){` |
|        61 | 3082 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,apNode);` |
|        29 | 3083 | `	}` |
|   2304143 | 3084 | `	return rc;` |
|   1150128 | 3085 | `}` |
|         - | 3086 |  |
