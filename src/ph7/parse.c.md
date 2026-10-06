# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1591/1744 lines (91.23%)

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
|   4462467 |  290 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|         5 |  291 | `{` |
|   4462472 |  292 | `	sxu32 n = 0;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `	/* Do a linear lookup on the operators table */` |
|  91386251 |  295 | `	for(;;){` |
| 183068112 |  296 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|       ! 0 |  297 | `			break;` |
|         - |  298 | `		}` |
| 183068112 |  299 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|         - |  300 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  14865320 |  301 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|   7420456 |  302 | `		}else{` |
| 168202797 |  303 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|         - |  304 | `		}` |
| 183068112 |  305 | `		if( rc == 0 ){` |
|   4583562 |  306 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|         - |  307 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|   4408687 |  308 | `				return &aOpTable[n];` |
|         - |  309 | `			}` |
|         - |  310 | `			/* Handle ambiguity */` |
|    174880 |  311 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|         - |  312 | `				/* Unary opertors have prcedence here over binary operators */` |
|     27547 |  313 | `				return &aOpTable[n];` |
|         - |  314 | `			}` |
|    147338 |  315 | `			if( pLast->nType & PH7_TK_OP ){` |
|     26256 |  316 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|         - |  317 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|     26256 |  318 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|         - |  319 | `					/* Unary opertors have prcedence here over binary operators */` |
|     26248 |  320 | `					return &aOpTable[n];` |
|         - |  321 | `				}` |
|         - |  322 |  |
|         4 |  323 | `			}` |
|     60464 |  324 | `		}` |
| 178605645 |  325 | `		++n; /* Next operator in the table */` |
|         5 |  326 | `	}` |
|         - |  327 | `	/* No such operator */` |
|       ! 0 |  328 | `	return 0;` |
|   2227830 |  329 | `}` |
|         - |  330 | `/*` |
|         - |  331 | ` * Delimit a set of token stream.` |
|         - |  332 | ` * This function take care of handling the nesting level and stops when it hit` |
|         - |  333 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|         - |  334 | ` */` |
|   1194933 |  335 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|         5 |  336 | `{` |
|   1194938 |  337 | `	SyToken *pCur = pIn;` |
|   1194938 |  338 | `	sxi32 iNest = 1;` |
|   5196646 |  339 | `	for(;;){` |
|  10408028 |  340 | `		if( pCur >= pEnd ){` |
|       263 |  341 | `			break;` |
|         - |  342 | `		}` |
|  10407770 |  343 | `		if( pCur->nType & nTokStart ){` |
|         - |  344 | `			/* Increment nesting level */` |
|    570337 |  345 | `			iNest++;` |
|  10122238 |  346 | `		}else if( pCur->nType & nTokEnd ){` |
|         - |  347 | `			/* Decrement nesting level */` |
|   1765006 |  348 | `			iNest--;` |
|   1765006 |  349 | `			if( iNest <= 0 ){` |
|   1194680 |  350 | `				break;` |
|         - |  351 | `			}` |
|    284797 |  352 | `		}` |
|         - |  353 | `		/* Advance cursor */` |
|   9213095 |  354 | `		pCur++;` |
|         5 |  355 | `	}` |
|         - |  356 | `	/* Point to the end of the chunk */` |
|   1194938 |  357 | `	*ppEnd = pCur;` |
|   1194938 |  358 | `}` |
|         - |  359 | `/*` |
|         - |  360 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|         - |  361 | ` * Note on reserved keywords.` |
|         - |  362 | ` *  According to the PHP language reference manual:` |
|         - |  363 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|         - |  364 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|         - |  365 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|         - |  366 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|         - |  367 | ` */` |
|     21710 |  368 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|         5 |  369 | `{` |
|     21710 |  370 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|     21573 |  371 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|         - |  372 | `		){` |
|       641 |  373 | `			return TRUE;` |
|         - |  374 | `	}` |
|     21079 |  375 | `	if( bCheckFunc ){` |
|      1250 |  376 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      1127 |  377 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|       979 |  378 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       309 |  379 | `				return TRUE;` |
|         - |  380 | `		}` |
|       473 |  381 | `	}` |
|         - |  382 | `	/* Not a language construct */` |
|     20775 |  383 | `	return FALSE;` |
|     10849 |  384 | `}` |
|         - |  385 | `/*` |
|         - |  386 | ` * Make sure we are dealing with a valid expression tree.` |
|         - |  387 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|         - |  388 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - |  389 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|         - |  390 | ` */` |
|   2918348 |  391 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
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
|   2918353 |  406 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|         - |  407 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       153 |  408 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       153 |  409 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|        74 |  410 | `	}` |
|   2918353 |  411 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  17943970 |  412 | `	for( i = 0 ; i < nNode ; ++i ){` |
|         - |  413 | ``		/* A closure LITERAL is not dereferencable in php: `function () {…}` and`` |
|         - |  414 | ``		 * `fn (…) => …` may not be followed by `(`, `[`, `->`, `?->` or `::` unless`` |
|         - |  415 | `		 * the user parenthesised them, which is why every IIFE in the wild is` |
|         - |  416 | ``		 * written `(function () {…})()`. PHL accepted all five spellings — a`` |
|         - |  417 | `		 * silent acceptance of what php refuses, and the reason it was found: the` |
|         - |  418 | ``		 * statement-position rule above sends `function () {…};` down this path,`` |
|         - |  419 | ``		 * and without the screen `function () {…}();` would have become the one`` |
|         - |  420 | `		 * shape php rejects that PHL runs. */` |
|  15036075 |  421 | `		if( (apNode[i]->xCode == PH7_CompileAnnonFunc \|\| apNode[i]->xCode == PH7_CompileArrowFunc)` |
|   7520520 |  422 | `		 && (apNode[i]->iFlags & EXPR_NODE_PARENS) == 0` |
|     23454 |  423 | `		 && i + 1 < nNode && apNode[i+1] ){` |
|     15044 |  424 | `			ph7_expr_node *pAfter = apNode[i+1];` |
|     15044 |  425 | `			int bDeref = (pAfter->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB)) != 0;` |
|     15039 |  426 | `			if( !bDeref && pAfter->pOp` |
|      7904 |  427 | `			 && ( pAfter->pOp->iOp == EXPR_OP_ARROW` |
|       497 |  428 | `			   \|\| pAfter->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       495 |  429 | `			   \|\| pAfter->pOp->iOp == EXPR_OP_DC ) ){` |
|         7 |  430 | `				bDeref = 1;` |
|         3 |  431 | `			}` |
|     15044 |  432 | `			if( bDeref ){` |
|        11 |  433 | `				rc = PH7_GenSyntaxError(pGen,pAfter->pStart,0);` |
|        11 |  434 | `				if( rc != SXERR_ABORT ){` |
|        11 |  435 | `					rc = SXERR_SYNTAX;` |
|         5 |  436 | `				}` |
|        11 |  437 | `				return rc;` |
|         - |  438 | `			}` |
|      7400 |  439 | `		}` |
|  15036070 |  440 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|         - |  441 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     21129 |  442 | `			continue;` |
|         - |  443 | `		}` |
|  15004614 |  444 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|         - |  445 | `			/* A short-array literal is a SELF-CONTAINED node whose start token is '['` |
|         - |  446 | `			 * (its ']' was consumed), so the raw-token CSB test below can never see it —` |
|         - |  447 | ``			 * `[$obj, 'm']()` parsed the '(' as a grouping paren and silently DROPPED`` |
|         - |  448 | `			 * the call (the expression evaluated to the array). php invokes the literal` |
|         - |  449 | `			 * array callable exactly like the variable-held form. */` |
|   1524579 |  450 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    169289 |  451 | `				apNode[i-1]->xCode == PH7_CompileShortArray \|\|` |
|    169253 |  452 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|         - |  453 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis.` |
|         - |  454 | `					 * A self-contained short-array node is exempt: its start token is '[',` |
|         - |  455 | `					 * which carries PH7_TK_OP as the subscript operator, but the node is a` |
|         - |  456 | ``					 * complete array-literal TERM — `[$obj, 'm'](...)` is a call. */`` |
|   1204405 |  457 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0` |
|    601072 |  458 | `					 \|\| apNode[i-1]->xCode == PH7_CompileShortArray ){` |
|         - |  459 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|         - |  460 | `						 * not a simple left parenthesis. Mark the node.` |
|         - |  461 | `						 */` |
|   1204392 |  462 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   1204392 |  463 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   1204392 |  464 | `						apNode[i]->pOp = &sFCallOp;` |
|    601031 |  465 | `					}` |
|    601040 |  466 | `			}` |
|   1440061 |  467 | `			iParen++;` |
|  14283277 |  468 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   1440087 |  469 | `			if( iParen <= 0 ){` |
|        50 |  470 | `				rc = PH7_GenUnmatchedCloser(&(*pGen),apNode[i]->pStart);` |
|        50 |  471 | `				if( rc != SXERR_ABORT ){` |
|        50 |  472 | `					rc = SXERR_SYNTAX;` |
|        23 |  473 | `				}` |
|        50 |  474 | `				return rc;` |
|         - |  475 | `			}` |
|   1440041 |  476 | `			iParen--;` |
|  12843185 |  477 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    383664 |  478 | `			iSquare++;` |
|  11932392 |  479 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    383684 |  480 | `			if( iSquare <= 0 ){` |
|        27 |  481 | `				rc = PH7_GenUnmatchedCloser(&(*pGen),apNode[i]->pStart);` |
|        27 |  482 | `				if( rc != SXERR_ABORT ){` |
|        27 |  483 | `					rc = SXERR_SYNTAX;` |
|        12 |  484 | `				}` |
|        27 |  485 | `				return rc;` |
|         - |  486 | `			}` |
|    383660 |  487 | `			iSquare--;` |
|  11548711 |  488 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       196 |  489 | `			iBraces++;` |
|       196 |  490 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|         - |  491 | ``				/* php 8 REMOVED curly-brace offsets: `$a{0}` used to be rewritten here`` |
|         - |  492 | ``				 * into `$a[0]` (PH7's "dirty hack"), which quietly accepted source php`` |
|         - |  493 | `				 * rejects outright. It is a parse error now, like php's. */` |
|         3 |  494 | `				rc = PH7_GenSyntaxError(&(*pGen),apNode[i]->pStart,0);` |
|         3 |  495 | `				if( rc != SXERR_ABORT ){` |
|         3 |  496 | `					rc = SXERR_SYNTAX;` |
|         1 |  497 | `				}` |
|         3 |  498 | `				return rc;` |
|         4 |  499 | `			}` |
|  11357041 |  500 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       235 |  501 | `			if( iBraces <= 0 ){` |
|        45 |  502 | `				rc = PH7_GenUnmatchedCloser(&(*pGen),apNode[i]->pStart);` |
|        45 |  503 | `				if( rc != SXERR_ABORT ){` |
|        45 |  504 | `					rc = SXERR_SYNTAX;` |
|        20 |  505 | `				}` |
|        45 |  506 | `				return rc;` |
|         - |  507 | `			}` |
|       194 |  508 | `			iBraces--;` |
|  11356811 |  509 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     48741 |  510 | `			sxi32 iDepth = iParen + iSquare + iBraces;` |
|     48736 |  511 | `			if( iQuesty > 0` |
|     47977 |  512 | `			 && ( iQuesty > (sxi32)SX_ARRAYSIZE(aQuestyDepth)` |
|     47208 |  513 | `			   \|\| aQuestyDepth[iQuesty - 1] == iDepth ) ){` |
|     47195 |  514 | `				iQuesty--;` |
|     25116 |  515 | `			}else if( iParen <= 0 ){` |
|         - |  516 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|         - |  517 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|         - |  518 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|         6 |  519 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|         6 |  520 | `				if( rc != SXERR_ABORT ){` |
|         6 |  521 | `					rc = SXERR_SYNTAX;` |
|         2 |  522 | `				}` |
|         6 |  523 | `				return rc;` |
|         5 |  524 | `			}` |
|  11332316 |  525 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   3677531 |  526 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   3677531 |  527 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     47197 |  528 | `				if( iQuesty < (sxi32)SX_ARRAYSIZE(aQuestyDepth) ){` |
|     47197 |  529 | `					aQuestyDepth[iQuesty] = iParen + iSquare + iBraces;` |
|     23566 |  530 | `				}` |
|     47197 |  531 | `				iQuesty++;` |
|   3653905 |  532 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|     53334 |  533 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
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
|     26626 |  551 | `			}` |
|   1836030 |  552 | `		}` |
|   7490359 |  553 | `	}` |
|   2907895 |  554 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|        18 |  555 | `		rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        18 |  556 | `		if( rc != SXERR_ABORT ){` |
|        18 |  557 | `			rc = SXERR_SYNTAX;` |
|         7 |  558 | `		}` |
|        18 |  559 | `		return rc;` |
|         - |  560 | `	}` |
|   2907881 |  561 | `	return SXRET_OK;` |
|   1451705 |  562 | `}` |
|         - |  563 | `/*` |
|         - |  564 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|         - |  565 | ` * or a simple literal [i.e: PHP_EOL].` |
|         - |  566 | ` */` |
|   1853165 |  567 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|         5 |  568 | `{` |
|   1853170 |  569 | `	SyToken *pIn = *ppCur;` |
|         - |  570 | `	/* Jump the first literal seen */` |
|   1853170 |  571 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   1852642 |  572 | `		pIn++;` |
|    924660 |  573 | `	}` |
|    925757 |  574 | `	for(;;){` |
|   1854837 |  575 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      1672 |  576 | `			pIn++;` |
|      1672 |  577 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      1672 |  578 | `				pIn++;` |
|       833 |  579 | `			}` |
|       838 |  580 | `		}else{` |
|    924929 |  581 | `			break;` |
|         - |  582 | `		}` |
|         5 |  583 | `	}` |
|         - |  584 | `	/* Synchronize pointers */` |
|   1853170 |  585 | `	*ppCur = pIn;` |
|   1853170 |  586 | `}` |
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
|     20808 |  631 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  632 | `{` |
|     20813 |  633 | `	SyToken *pIn = *ppIn;` |
|     20813 |  634 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|       251 |  635 | `		pIn++; /* Skip ':' */` |
|       118 |  636 | `		for(;;){` |
|         - |  637 | `			/* Optional '?' nullable prefix */` |
|       263 |  638 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        15 |  639 | `				pIn++;` |
|         6 |  640 | `			}` |
|       263 |  641 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - |  642 | `				/* Parenthesized DNF group '(A&B)' */` |
|         5 |  643 | `				pIn++;` |
|         5 |  644 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|         5 |  645 | `				if( pIn < pEnd ){` |
|         5 |  646 | `					pIn++; /* ')' */` |
|         2 |  647 | `				}` |
|       257 |  648 | `			}else if( pIn < pEnd` |
|       259 |  649 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|         - |  650 | `				/* ['\']Name('\'Name)* */` |
|       259 |  651 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|       259 |  652 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       259 |  653 | `					pIn++;` |
|       267 |  654 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|         9 |  655 | `						pIn += 2;` |
|         1 |  656 | `					}` |
|       122 |  657 | `				}` |
|       127 |  658 | `			}else{` |
|         - |  659 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|       ! 0 |  660 | `				break;` |
|         - |  661 | `			}` |
|         - |  662 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|       258 |  663 | `			if( pIn < pEnd` |
|       263 |  664 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|       254 |  665 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|        14 |  666 | `				pIn++;` |
|        14 |  667 | `				continue;` |
|         - |  668 | `			}` |
|       251 |  669 | `			break;` |
|       ! 0 |  670 | `		}` |
|       118 |  671 | `	}` |
|     20813 |  672 | `	*ppIn = pIn;` |
|     20813 |  673 | `}` |
|      7781 |  674 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  675 | `{` |
|      7786 |  676 | `	SyToken *pIn = *ppCur;` |
|         - |  677 | `	sxi32 rc;` |
|         - |  678 | ``	/* Jump the leading keyword. The caller may hand us either `function (...)` or`` |
|         - |  679 | ``	 * `static function (...)`, so a 'function' keyword still sitting here belongs to`` |
|         - |  680 | `	 * the static form and is jumped too. An IDENTIFIER, on the other hand, is not a` |
|         - |  681 | `	 * name to skip over — a closure is anonymous, so that is precisely the syntax` |
|         - |  682 | `	 * error php reports ("unexpected identifier, expecting \"(\"") . */` |
|      7786 |  683 | `	pIn++;` |
|      7781 |  684 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      4115 |  685 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_FUNCTION ){` |
|       475 |  686 | `		pIn++;` |
|       235 |  687 | `	}` |
|         - |  688 | ``	/* `function &(…) {…}` returns by reference, exactly as the named form does, and`` |
|         - |  689 | ``	 * the `&` sits in the same place. The node assembler is what has to step over`` |
|         - |  690 | `	 * it (PH7_CompileAnnonFunc reads it again for the flag); leaving it here made` |
|         - |  691 | ``	 * every by-ref closure `syntax error, unexpected token "&", expecting "("`. */`` |
|      7786 |  692 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|        14 |  693 | `		pIn++;` |
|         6 |  694 | `	}` |
|      7786 |  695 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  696 | `		/* Syntax error */` |
|         6 |  697 | `		rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  698 | `		if( rc != SXERR_ABORT ){` |
|         6 |  699 | `			rc = SXERR_SYNTAX;` |
|         2 |  700 | `		}` |
|         6 |  701 | `		goto Synchronize;` |
|         - |  702 | `	}` |
|      7782 |  703 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      7782 |  704 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      7782 |  705 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
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
|      7776 |  721 | `	pIn++; /* Jump the trailing parenthesis */` |
|         - |  722 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      7776 |  723 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      7776 |  724 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|      2330 |  725 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|         - |  726 | `		/* Check if we are dealing with a closure */` |
|      2330 |  727 | `		if( nKey == PH7_TKWRD_USE ){` |
|      2322 |  728 | `			pIn++; /* Jump the 'use' keyword */` |
|      2322 |  729 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  730 | `				/* Syntax error */` |
|         6 |  731 | `				rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"(\"");` |
|         6 |  732 | `				if( rc != SXERR_ABORT ){` |
|         6 |  733 | `					rc = SXERR_SYNTAX;` |
|         2 |  734 | `				}` |
|         6 |  735 | `				goto Synchronize;` |
|         - |  736 | `			}` |
|      2318 |  737 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|         - |  738 | ``			/* A use-list is only `[&] $var` items separated by commas. php's parser`` |
|         - |  739 | `			 * has no nested structure to balance here, so the first token that is not` |
|         - |  740 | ``			 * part of that grammar is the one it names -- `use ($x {` reports the '{',`` |
|         - |  741 | `			 * not a run to the ')'. PH7_DelimitNestedTokens would instead treat '{' as` |
|         - |  742 | `			 * an open bracket and scan past it, so scan the list explicitly and stop at` |
|         - |  743 | `			 * the first foreign token. */` |
|         - |  744 | `			{` |
|      2318 |  745 | `				SyToken *pUse = pIn;` |
|      2318 |  746 | `				int bClosed = 0;` |
|      8362 |  747 | `				while( pUse < pEnd ){` |
|      8362 |  748 | `					if( pUse->nType & PH7_TK_RPAREN ){ bClosed = 1; break; }` |
|      6051 |  749 | `					if( pUse->nType & (PH7_TK_DOLLAR\|PH7_TK_COMMA\|PH7_TK_AMPER\|PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      6049 |  750 | `						pUse++;` |
|      6049 |  751 | `						continue;` |
|         - |  752 | `					}` |
|         3 |  753 | `					break; /* foreign token: php names this one */` |
|       ! 0 |  754 | `				}` |
|      2318 |  755 | `				if( !bClosed ){` |
|         - |  756 | `					/* php names the offending token and expects ')'; if the list simply` |
|         - |  757 | `					 * ran off the end of the slice, that token sits just past it. */` |
|         3 |  758 | `					SyToken *pBad = pUse < pEnd ? pUse : (pEnd < pGen->pEnd ? pEnd : 0);` |
|         3 |  759 | `					rc = PH7_GenSyntaxError(&(*pGen),pBad,"\")\"");` |
|         3 |  760 | `					if( rc != SXERR_ABORT ){` |
|         3 |  761 | `						rc = SXERR_SYNTAX;` |
|         1 |  762 | `					}` |
|         3 |  763 | `					goto Synchronize;` |
|         - |  764 | `				}` |
|      2316 |  765 | `				pIn = pUse; /* on the ')' */` |
|         - |  766 | `			}` |
|      2316 |  767 | `			if( &pIn[1] >= pEnd ){` |
|         - |  768 | ``				/* `use (...)` closed but nothing follows: the body '{' is missing. */`` |
|         3 |  769 | `				SyToken *pBad = pEnd < pGen->pEnd ? pEnd : 0;` |
|         3 |  770 | `				rc = PH7_GenSyntaxError(&(*pGen),pBad,"\"{\"");` |
|         3 |  771 | `				if( rc != SXERR_ABORT ){` |
|         3 |  772 | `					rc = SXERR_SYNTAX;` |
|         1 |  773 | `				}` |
|         3 |  774 | `				goto Synchronize;` |
|         - |  775 | `			}` |
|      2314 |  776 | `			pIn++;` |
|         - |  777 | `			/* php 7.1+: the return type may also follow the use clause —` |
|         - |  778 | ``			 * `function (...) use (...) : int {` */`` |
|      2314 |  779 | `			ExprSkipReturnType(&pIn,pEnd);` |
|      1152 |  780 | `		}else{` |
|         - |  781 | `			/* Syntax error */` |
|        11 |  782 | `			rc = PH7_GenSyntaxError(&(*pGen),pIn < pEnd ? pIn : 0,"\"{\"");` |
|        11 |  783 | `			if( rc != SXERR_ABORT ){` |
|        11 |  784 | `				rc = SXERR_SYNTAX;` |
|         4 |  785 | `			}` |
|        11 |  786 | `			goto Synchronize;` |
|         - |  787 | `		}` |
|      1147 |  788 | `	}` |
|         - |  789 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|         - |  790 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|         - |  791 | `	 * the type), and pEnd is one past the last token. */` |
|      7760 |  792 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      7760 |  793 | `		pIn++; /* Jump the leading curly '{' */` |
|      7760 |  794 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      7760 |  795 | `		if( pIn < pEnd ){` |
|      7760 |  796 | `			pIn++;` |
|      3861 |  797 | `		}` |
|      3866 |  798 | `	}else{` |
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
|      7760 |  809 | `	rc = SXRET_OK;` |
|      3907 |  810 | `Synchronize:` |
|         - |  811 | `	/* Synchronize pointers */` |
|      7786 |  812 | `	*ppCur = pIn;` |
|      7786 |  813 | `	return rc;` |
|      3879 |  814 | `}` |
|         - |  815 | `/*` |
|         - |  816 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|         - |  817 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|         - |  818 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|         - |  819 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|         - |  820 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|         - |  821 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|         - |  822 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|         - |  823 | ` */` |
|       288 |  824 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  825 | `{` |
|       293 |  826 | `	SyToken *pIn = *ppCur;` |
|       293 |  827 | `	sxu32 nLine = pIn->nLine;` |
|         - |  828 | `	sxi32 rc;` |
|       293 |  829 | `	if( GenStateIsReadonly(pIn) ){` |
|         7 |  830 | ``		pIn++; /* `new readonly class …` (PHP 8.3): step over the modifier */`` |
|         3 |  831 | `	}` |
|       293 |  832 | `	pIn++; /* Jump the 'class' keyword */` |
|         - |  833 | `	/* Optional constructor argument list */` |
|       293 |  834 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       114 |  835 | `		pIn++; /* Jump '(' */` |
|       114 |  836 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       114 |  837 | `		if( pIn < pEnd ){` |
|       114 |  838 | `			pIn++; /* Jump ')' */` |
|        55 |  839 | `		}` |
|        55 |  840 | `	}` |
|         - |  841 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|         - |  842 | `	 * (no braces appear between ')' and the class body). */` |
|       621 |  843 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       333 |  844 | `		pIn++;` |
|         5 |  845 | `	}` |
|       293 |  846 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|         - |  847 | `		/* Syntax error: missing class body */` |
|       ! 0 |  848 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  849 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|       ! 0 |  850 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  851 | `			rc = SXERR_SYNTAX;` |
|       ! 0 |  852 | `		}` |
|       ! 0 |  853 | `		*ppCur = pIn;` |
|       ! 0 |  854 | `		return rc;` |
|         - |  855 | `	}` |
|       293 |  856 | `	pIn++; /* Jump the leading '{' */` |
|       293 |  857 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       293 |  858 | `	if( pIn < pEnd ){` |
|       293 |  859 | `		pIn++; /* Jump the trailing '}' */` |
|       144 |  860 | `	}` |
|       293 |  861 | `	*ppCur = pIn;` |
|       293 |  862 | `	return SXRET_OK;` |
|       149 |  863 | `}` |
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
|     13186 |  892 | `PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd)` |
|         5 |  893 | `{` |
|     13191 |  894 | `	int bStatic = FALSE;` |
|     13191 |  895 | `	if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  896 | `		return FALSE;` |
|         - |  897 | `	}` |
|     13191 |  898 | `	if( pTok > pStart ){` |
|      1427 |  899 | `		SyToken *pPrev = &pTok[-1];` |
|      1427 |  900 | `		if( pPrev->nType & (PH7_TK_DOLLAR\|PH7_TK_NSSEP) ){` |
|       107 |  901 | `			return FALSE; /* $fn / \A\fn — the keyword IS the name */` |
|         - |  902 | `		}` |
|      1325 |  903 | `		if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData ){` |
|       635 |  904 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)pPrev->pUserData;` |
|       630 |  905 | `			if( pOp->iOp == EXPR_OP_ARROW \|\| pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       596 |  906 | `			 \|\| pOp->iOp == EXPR_OP_DC ){` |
|       183 |  907 | `				return FALSE; /* $o->fn, $o?->fn, C::fn — a member name */` |
|         - |  908 | `			}` |
|       226 |  909 | `		}` |
|       571 |  910 | `	}` |
|     12911 |  911 | `	if( SX_PTR_TO_INT(pTok->pUserData) == PH7_TKWRD_STATIC ){` |
|       571 |  912 | `		bStatic = TRUE;` |
|       571 |  913 | `		pTok++;` |
|       571 |  914 | `		if( pTok >= pEnd \|\| (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|       355 |  915 | `			return FALSE;` |
|         - |  916 | `		}` |
|       108 |  917 | `	}` |
|     12561 |  918 | `	if( SX_PTR_TO_INT(pTok->pUserData) != PH7_TKWRD_FN ){` |
|      1325 |  919 | `		return FALSE;` |
|         - |  920 | `	}` |
|     11241 |  921 | `	if( !bStatic && &pTok[1] < pEnd && (pTok[1].nType & PH7_TK_COLON) ){` |
|       ! 0 |  922 | `		return FALSE; /* f(fn: 1) — a named-argument label */` |
|         - |  923 | `	}` |
|     11241 |  924 | `	return TRUE;` |
|      6499 |  925 | `}` |
|         - |  926 | `/*` |
|         - |  927 | `` * Step over an arrow function's HEAD, `fn [&] ( params ) [: [?] type]` and the`` |
|         - |  928 | `` * `=>` after it, from the `fn` keyword at *ppIn. Boundary scanning only: a`` |
|         - |  929 | ` * malformed head stops at the first token that does not fit, and the compile` |
|         - |  930 | ` * pass (PH7_CompileArrowFunc) reports php's error for it. Shared by the arrow` |
|         - |  931 | ` * assembler for its own head and for every arrow nested in its body.` |
|         - |  932 | ` */` |
|     10728 |  933 | `static void ExprSkipArrowHead(SyToken **ppIn,SyToken *pEnd)` |
|         5 |  934 | `{` |
|     10733 |  935 | `	SyToken *pIn = *ppIn;` |
|     10733 |  936 | `	pIn++; /* Jump 'fn' */` |
|         - |  937 | `	/* Optional '&' for return-by-reference */` |
|     10733 |  938 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  939 | `		pIn++;` |
|       ! 0 |  940 | `	}` |
|     10733 |  941 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|     10731 |  942 | `		pIn++; /* '(' */` |
|     10731 |  943 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|     10731 |  944 | `		if( pIn < pEnd ){` |
|     10729 |  945 | `			pIn++; /* ')' */` |
|      5263 |  946 | `		}` |
|      5264 |  947 | `	}` |
|         - |  948 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|     10733 |  949 | `	ExprSkipReturnType(&pIn,pEnd);` |
|         - |  950 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|     10733 |  951 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|     10727 |  952 | `		pIn++;` |
|      5262 |  953 | `	}` |
|     10733 |  954 | `	*ppIn = pIn;` |
|     10733 |  955 | `}` |
|         - |  956 | `/*` |
|         - |  957 | ` * Assemble a PHP 7.4 arrow function token range:` |
|         - |  958 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|         - |  959 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|         - |  960 | ` * past the body expression — the body ends at the first top-level comma,` |
|         - |  961 | ` * semicolon, or unbalanced closing delimiter.` |
|         - |  962 | ` */` |
|     10530 |  963 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 |  964 | `{` |
|     10535 |  965 | `	SyToken *pIn = *ppCur;` |
|         - |  966 | `	SyToken *pBody;  /* first token of the body: bounds the nested-arrow look-back */` |
|         - |  967 | `	sxu32 nLine;` |
|         - |  968 | `	sxi32 rc;` |
|         - |  969 | `	int iNest;` |
|         - |  970 | `	int iTern;   /* ternary '?'s opened inside the body and not yet closed */` |
|     10535 |  971 | `	nLine = pIn->nLine;` |
|         - |  972 | `	/* Optional 'static' prefix */` |
|     10530 |  973 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|     10535 |  974 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       213 |  975 | `		pIn++;` |
|       104 |  976 | `	}` |
|         - |  977 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|     10530 |  978 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|     10535 |  979 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  980 | `		rc = SXERR_SYNTAX;` |
|       ! 0 |  981 | `		goto Synchronize;` |
|         - |  982 | `	}` |
|      5166 |  983 | `	SXUNUSED(nLine);` |
|      5166 |  984 | `	SXUNUSED(pGen);` |
|         - |  985 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|         - |  986 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|         - |  987 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|         - |  988 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|     10535 |  989 | `	ExprSkipArrowHead(&pIn,pEnd);` |
|     10535 |  990 | `	pBody = pIn;` |
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
|     10535 | 1001 | `	iNest = 0;` |
|     10535 | 1002 | `	iTern = 0;` |
|    106093 | 1003 | `	while( pIn < pEnd ){` |
|    104328 | 1004 | `		if( iNest == 0 && (pIn->nType &` |
|         - | 1005 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      8762 | 1006 | `			break;` |
|         - | 1007 | `		}` |
|     95571 | 1008 | `		if( (pIn->nType & PH7_TK_KEYWORD) && PH7_TokenOpensArrowFunc(pBody,pIn,pEnd) ){` |
|         - | 1009 | `			/* A NESTED arrow function: its body is the tail of this one, so the` |
|         - | 1010 | `			 * scan simply continues into it -- but its HEAD must be stepped over` |
|         - | 1011 | `			 * as a unit. Left to the loop, the colon of its return type read as` |
|         - | 1012 | `			 * an enclosing ternary's and ended the outer body right there, so` |
|         - | 1013 | ``			 * `fn($t) => fn($v): string => $v` (doctrine/orm's DQL cookbook) was`` |
|         - | 1014 | ``			 * `unexpected token "=>"`, and a `?` in `fn($v): ?string` would have`` |
|         - | 1015 | `			 * been counted as a ternary. */` |
|       203 | 1016 | `			if( SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         3 | 1017 | ``				pIn++; /* `static fn`: the head skipper starts at `fn` */`` |
|         1 | 1018 | `			}` |
|       203 | 1019 | `			ExprSkipArrowHead(&pIn,pEnd);` |
|       203 | 1020 | `			continue;` |
|         - | 1021 | `		}` |
|     95373 | 1022 | `		if( iNest == 0 && (pIn->nType & PH7_TK_COLON) ){` |
|        79 | 1023 | `			if( iTern < 1 ){` |
|        10 | 1024 | `				break;     /* the colon of an enclosing '?': the body ends here */` |
|         - | 1025 | `			}` |
|        71 | 1026 | `			iTern--;       /* ...or of a ternary this body opened itself */` |
|     95332 | 1027 | `		}else if( iNest == 0 && (pIn->nType & PH7_TK_OP)` |
|     20249 | 1028 | `			&& pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        71 | 1029 | `			iTern++;` |
|        33 | 1030 | `		}` |
|     95365 | 1031 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     16772 | 1032 | `			iNest++;` |
|     86836 | 1033 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     16772 | 1034 | `			iNest--;` |
|      8238 | 1035 | `		}` |
|     95365 | 1036 | `		pIn++;` |
|         5 | 1037 | `	}` |
|     10535 | 1038 | `	rc = SXRET_OK;` |
|      5364 | 1039 | `Synchronize:` |
|     10535 | 1040 | `	*ppCur = pIn;` |
|     10535 | 1041 | `	return rc;` |
|         5 | 1042 | `}` |
|         - | 1043 | `/*` |
|         - | 1044 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|         - | 1045 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|         - | 1046 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|         - | 1047 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|         - | 1048 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|         - | 1049 | ` */` |
|       170 | 1050 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|         5 | 1051 | `{` |
|       175 | 1052 | `	SyToken *pIn = *ppCur;` |
|         - | 1053 | `	sxi32 rc;` |
|        85 | 1054 | `	SXUNUSED(pGen);` |
|         - | 1055 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       170 | 1056 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       175 | 1057 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|       ! 0 | 1058 | `		rc = SXERR_SYNTAX;` |
|       ! 0 | 1059 | `		goto Synchronize;` |
|         - | 1060 | `	}` |
|       175 | 1061 | `	pIn++; /* Jump 'match' */` |
|         - | 1062 | `	/* Optional '(' subject ')' */` |
|       175 | 1063 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       175 | 1064 | `		pIn++;` |
|       175 | 1065 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       175 | 1066 | `		if( pIn < pEnd ){` |
|       175 | 1067 | `			pIn++; /* ')' */` |
|        85 | 1068 | `		}` |
|        85 | 1069 | `	}` |
|         - | 1070 | `	/* Optional '{' arms '}' */` |
|       175 | 1071 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       175 | 1072 | `		pIn++;` |
|       175 | 1073 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       175 | 1074 | `		if( pIn < pEnd ){` |
|       175 | 1075 | `			pIn++; /* '}' */` |
|        85 | 1076 | `		}` |
|        85 | 1077 | `	}` |
|       175 | 1078 | `	rc = SXRET_OK;` |
|        85 | 1079 | `Synchronize:` |
|       175 | 1080 | `	*ppCur = pIn;` |
|       175 | 1081 | `	return rc;` |
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
|      1172 | 1143 | `static void ExprDelimitKeywordOperand(SyToken *pIn,SyToken *pEnd,SyToken **ppEnd)` |
|         5 | 1144 | `{` |
|      1177 | 1145 | `	SyToken *pCur = pIn;` |
|      1177 | 1146 | `	sxi32 iNest = 1;` |
|      1177 | 1147 | ``	sxi32 iQuesty = 0;   /* `?`s opened inside the operand and still unclosed */`` |
|      2188 | 1148 | `	for(;;){` |
|      4381 | 1149 | `		if( pCur >= pEnd ){` |
|      1067 | 1150 | `			break;` |
|         - | 1151 | `		}` |
|      3319 | 1152 | `		if( (pCur->nType & PH7_TK_COMMA) && iNest <= 1 ){` |
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
|      3298 | 1166 | `		if( iNest <= 1 && (pCur->nType & PH7_TK_OP)` |
|      1447 | 1167 | `		 && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|         3 | 1168 | `			iQuesty++;` |
|      3302 | 1169 | `		}else if( iNest <= 1 && (pCur->nType & PH7_TK_COLON) ){` |
|        17 | 1170 | `			if( iQuesty < 1 ){` |
|        15 | 1171 | `				break;` |
|         - | 1172 | `			}` |
|         3 | 1173 | `			iQuesty--;` |
|         1 | 1174 | `		}` |
|      3289 | 1175 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB) ){` |
|       247 | 1176 | `			iNest++;` |
|      3168 | 1177 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|       327 | 1178 | `			iNest--;` |
|       327 | 1179 | `			if( iNest <= 0 ){` |
|        84 | 1180 | `				break;` |
|         - | 1181 | `			}` |
|       121 | 1182 | `		}` |
|      3209 | 1183 | `		pCur++;` |
|         5 | 1184 | `	}` |
|      1177 | 1185 | `	*ppEnd = pCur;` |
|      1177 | 1186 | `}` |
|  15026732 | 1187 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|         5 | 1188 | `{` |
|         - | 1189 | `	ph7_expr_node *pNode;` |
|         - | 1190 | `	SyToken *pCur;` |
|         - | 1191 | `	sxi32 rc;` |
|         - | 1192 | `	/* Allocate a new node */` |
|  15026737 | 1193 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  15026737 | 1194 | `	if( pNode == 0 ){` |
|         - | 1195 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1196 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1197 | `		 */` |
|       ! 0 | 1198 | `		return SXERR_MEM;` |
|         - | 1199 | `	}` |
|         - | 1200 | `	/* Zero the structure */` |
|  15026737 | 1201 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  15026737 | 1202 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|         - | 1203 | `	/* Point to the head of the token stream */` |
|  15026737 | 1204 | `	pCur = pNode->pStart = pGen->pIn;` |
|         - | 1205 | `	/* Start collecting tokens */` |
|  15026737 | 1206 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|      1146 | 1207 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|         - | 1208 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|         - | 1209 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|         - | 1210 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|         - | 1211 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       465 | 1212 | `			pNode->pEnd = pCur;` |
|       465 | 1213 | `			pCur++;` |
|       465 | 1214 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       465 | 1215 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       465 | 1216 | `			pGen->pIn = pCur;` |
|       465 | 1217 | `			*ppNode = pNode;` |
|       465 | 1218 | `			return SXRET_OK;` |
|         - | 1219 | `		}` |
|         - | 1220 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|         - | 1221 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|       686 | 1222 | `		pCur++;` |
|       686 | 1223 | `		pGen->pIn = pCur;` |
|       686 | 1224 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|       686 | 1225 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|       686 | 1226 | `		if( rc == SXRET_OK && *ppNode ){` |
|       686 | 1227 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|       340 | 1228 | `		}` |
|       686 | 1229 | `		return rc;` |
|         - | 1230 | `	}` |
|  15025596 | 1231 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|         - | 1232 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|         - | 1233 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|         - | 1234 | `		 */` |
|     21139 | 1235 | `		pCur++; /* Skip the opening '[' */` |
|     21139 | 1236 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     21139 | 1237 | `		if( pCur < pGen->pEnd ){` |
|     21131 | 1238 | `			pCur++; /* Skip past the closing ']' */` |
|     10520 | 1239 | `		}else{` |
|         - | 1240 | ``			/* php's scanner meets the closer that is not this `]` first:`` |
|         - | 1241 | ``			 * `[1)` is `Unclosed '[' does not match ')'`. Look past the slice --`` |
|         - | 1242 | ``			 * a statement head hands over only what its own `)` encloses -- but`` |
|         - | 1243 | ``			 * not past a `;` or a brace, where its parser would have spoken. */`` |
|         9 | 1244 | `			SyToken *pStreamEnd = pGen->pEnd;` |
|         9 | 1245 | `			SyToken *pBad = 0;` |
|         - | 1246 | `			char aNest[32];` |
|         9 | 1247 | `			sxi32 nNest = 1;` |
|         9 | 1248 | `			if( pGen->pTokenSet ){` |
|         9 | 1249 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         9 | 1250 | `				if( pGen->pEnd >= pBase && pGen->pEnd <= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         9 | 1251 | `					pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         4 | 1252 | `				}` |
|         4 | 1253 | `			}` |
|         9 | 1254 | `			aNest[0] = '[';` |
|        23 | 1255 | `			for( pCur = &pNode->pStart[1] ; pCur < pStreamEnd && nNest > 0 && nNest < (sxi32)sizeof(aNest) ; pCur++ ){` |
|        23 | 1256 | `				if( pCur->nType & (PH7_TK_SEMI\|PH7_TK_OCB\|PH7_TK_CCB) ){` |
|       ! 0 | 1257 | `					break;` |
|         - | 1258 | `				}` |
|        23 | 1259 | `				if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|         3 | 1260 | `					aNest[nNest++] = (pCur->nType & PH7_TK_LPAREN) ? '(' : '[';` |
|        22 | 1261 | `				}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|         9 | 1262 | `					if( aNest[nNest-1] != ((pCur->nType & PH7_TK_RPAREN) ? '(' : '[') ){` |
|         9 | 1263 | `						pBad = pCur;` |
|         9 | 1264 | `						break;` |
|         - | 1265 | `					}` |
|       ! 0 | 1266 | `					nNest--;` |
|       ! 0 | 1267 | `				}` |
|         8 | 1268 | `			}` |
|         9 | 1269 | `			if( pBad ){` |
|         9 | 1270 | `				rc = PH7_GenUnmatchedCloser(pGen,pBad);` |
|         5 | 1271 | `			}else{` |
|       ! 0 | 1272 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 1273 | `					"Short array: Missing closing bracket ']'");` |
|         - | 1274 | `			}` |
|         9 | 1275 | `			if( rc != SXERR_ABORT ){` |
|         9 | 1276 | `				rc = SXERR_SYNTAX;` |
|         4 | 1277 | `			}` |
|         9 | 1278 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         9 | 1279 | `			return rc;` |
|         - | 1280 | `		}` |
|         - | 1281 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|         - | 1282 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|         - | 1283 | `		 */` |
|     22305 | 1284 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      2334 | 1285 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      2334 | 1286 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       353 | 1287 | `				pNode->xCode = PH7_CompileShortList;` |
|       179 | 1288 | `			}else{` |
|      1986 | 1289 | `				pNode->xCode = PH7_CompileShortArray;` |
|         - | 1290 | `			}` |
|      1160 | 1291 | `		}else{` |
|     18802 | 1292 | `			pNode->xCode = PH7_CompileShortArray;` |
|         - | 1293 | `		}` |
|  15014977 | 1294 | `	}else if( !bAfterMemberOp && (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID))` |
|   8506561 | 1295 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_COLON) ){` |
|         - | 1296 | `		/* A RESERVED WORD immediately followed by a single ':' is a named-argument` |
|         - | 1297 | `		 * LABEL — php accepts all 73 of them there, since a parameter may be called` |
|         - | 1298 | ``		 * anything (`function f($new, $print, $and)`). Sixteen of them were claimed`` |
|         - | 1299 | ``		 * by their own construct branch below instead, so `f(new: 1)`, `f(print: 2)`,`` |
|         - | 1300 | ``		 * `f(match: $m)` and thirteen more were a compile fatal on source php runs.`` |
|         - | 1301 | ``		 * The lexer gives `::` its own operator token, so the only other shape this`` |
|         - | 1302 | `		 * can see — a bare word before a colon — is already a literal on the` |
|         - | 1303 | ``		 * fallthrough path; a `?:` cannot reach here at all, its '?' being neither`` |
|         - | 1304 | `		 * an identifier nor a keyword. The argument list is re-parsed from each` |
|         - | 1305 | `		 * argument's own first token, so there is no '(' to look back at and no way` |
|         - | 1306 | `		 * to scope this to call context; ExprProcessFuncArguments makes the` |
|         - | 1307 | `		 * POSITIONAL test that decides whether the label is really one. */` |
|     10088 | 1308 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|     10088 | 1309 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     10088 | 1310 | `		pNode->xCode = PH7_CompileLiteral;` |
|  14999415 | 1311 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|         - | 1312 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|         - | 1313 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|         - | 1314 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|         - | 1315 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|         - | 1316 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|         - | 1317 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|         - | 1318 | `		 * call, not the clone() intrinsic. */` |
|        25 | 1319 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        25 | 1320 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        25 | 1321 | `		pNode->xCode = PH7_CompileLiteral;` |
|  14994363 | 1322 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|   4061299 | 1323 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   2027850 | 1324 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN)` |
|       227 | 1325 | `		&& CloneCallFormFollows(pCur,pGen->pEnd) ){` |
|         - | 1326 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|         - | 1327 | ``		 * `clone` is a real internal FUNCTION in php 8.5, so this spelling is an`` |
|         - | 1328 | `		 * ordinary call — and every property the call machinery owns comes with` |
|         - | 1329 | ``		 * it: named arguments, spread, the first-class-callable `clone(...)`, and`` |
|         - | 1330 | `		 * the runtime ArgumentCountError/TypeError php raises for a degenerate` |
|         - | 1331 | ``		 * argument list (PHL used to refuse `clone()` and a three-argument call at`` |
|         - | 1332 | ``		 * COMPILE time, and had no FCC form at all). `clone` is an alpha-stream`` |
|         - | 1333 | `` 		 * operator token, so `clone(` is not auto-marked as a call the way `foo(` `` |
|         - | 1334 | `		 * is: clear PH7_TK_OP and leave a plain name TERM behind, and the postfix` |
|         - | 1335 | `		 * pass then binds the '(' to it. The bare operator/statement form` |
|         - | 1336 | ``		 * `clone $obj` (no immediately-following '(') keeps the precedence-1`` |
|         - | 1337 | `		 * operator path below. */` |
|        58 | 1338 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|        58 | 1339 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|        58 | 1340 | `		pNode->xCode = PH7_CompileLiteral;` |
|  14994327 | 1341 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|         - | 1342 | `		/* Point to the instance that describe this operator */` |
|   4061248 | 1343 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|         - | 1344 | `		/* Advance the stream cursor */` |
|   4061248 | 1345 | `		pCur++;` |
|  12960690 | 1346 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|         - | 1347 | `		/* Isolate variable */` |
|   7544621 | 1348 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|   3772340 | 1349 | `			pCur++; /* Variable variable */` |
|         5 | 1350 | `		}` |
|   3772286 | 1351 | `		if( pCur < pGen->pEnd ){` |
|   3772286 | 1352 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|         - | 1353 | `				/* Variable name */` |
|   3772236 | 1354 | `				pCur++;` |
|   1883584 | 1355 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|        46 | 1356 | `				pCur++;` |
|         - | 1357 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|        46 | 1358 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|        46 | 1359 | `				if( pCur < pGen->pEnd ){` |
|        44 | 1360 | `					pCur++;` |
|        24 | 1361 | `				}else{` |
|         - | 1362 | ``					/* Unterminated `${`. php names the token it ran out on (the ';'`` |
|         - | 1363 | ``					 * in `${unclosed;`), not the '$' the node started at -- pointing`` |
|         - | 1364 | `					 * back at pNode->pStart reported a nameless variable "$". The` |
|         - | 1365 | `					 * delimiter search stops at the slice end, so the token php names` |
|         - | 1366 | `					 * usually sits just past it, still inside the chunk stream. */` |
|         - | 1367 | `					{` |
|         3 | 1368 | `						SyToken *pBad = 0;` |
|         3 | 1369 | `						if( pGen->pTokenSet ){` |
|         3 | 1370 | `							SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         3 | 1371 | `							SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         3 | 1372 | `							if( pCur >= pBase && pCur < pStreamEnd ){` |
|       ! 0 | 1373 | `								pBad = pCur;` |
|       ! 0 | 1374 | `							}` |
|         1 | 1375 | `						}` |
|         3 | 1376 | `						rc = PH7_GenSyntaxError(pGen,pBad,0);` |
|         - | 1377 | `					}` |
|         3 | 1378 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1379 | `						rc = SXERR_SYNTAX;` |
|         1 | 1380 | `					}` |
|         3 | 1381 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1382 | `					return rc;` |
|         - | 1383 | `				}` |
|        24 | 1384 | `			}else{` |
|         - | 1385 | `				/* A '$' followed by anything else is a php syntax error naming that` |
|         - | 1386 | ``				 * token: `$(`, `$1`. This branch was MISSING, so the node silently`` |
|         - | 1387 | `				 * covered only the '$' and the offending token drifted into a later` |
|         - | 1388 | `				 * node -- surfacing as an error at the wrong place entirely ("$("` |
|         - | 1389 | `				 * reported the ';', "$1" reported a modifiable-l-value complaint). */` |
|        10 | 1390 | `				rc = PH7_GenSyntaxError(pGen,pCur,"variable or \"{\" or \"$\"");` |
|        10 | 1391 | `				if( rc != SXERR_ABORT ){` |
|        10 | 1392 | `					rc = SXERR_SYNTAX;` |
|         4 | 1393 | `				}` |
|        10 | 1394 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        10 | 1395 | `				return rc;` |
|         - | 1396 | `			}` |
|   1883550 | 1397 | `		}` |
|   3772276 | 1398 | `		pNode->xCode = PH7_CompileVariable;` |
|   9044325 | 1399 | `	 }else if( !bAfterMemberOp && GenStateStartsReadonlyAnonClass(pCur,pGen->pEnd) ){` |
|         - | 1400 | ``		 /* `new readonly class(args) [extends/implements] { body }` (PHP 8.3).`` |
|         - | 1401 | ``		  * `readonly` is a context-sensitive ID, so it never reaches the keyword`` |
|         - | 1402 | ``		  * chain below and the `class` after it read as the `::class` constant --`` |
|         - | 1403 | ``		  * `syntax error, unexpected token "class"`. It is the ONLY modifier php`` |
|         - | 1404 | ``		  * allows here (`new final class {}` and `new abstract class {}` are parse`` |
|         - | 1405 | `		  * errors in both engines), and pest writes one in its parallel result` |
|         - | 1406 | `		  * printer. */` |
|         7 | 1407 | `		 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|         7 | 1408 | `		 if( rc != SXRET_OK ){` |
|       ! 0 | 1409 | `			 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1410 | `			 return rc;` |
|         - | 1411 | `		 }` |
|         7 | 1412 | `		 pNode->xCode = PH7_CompileAnnonClass;` |
|   7160772 | 1413 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    145354 | 1414 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    145354 | 1415 | `		 if( bAfterMemberOp ){` |
|         - | 1416 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|         - | 1417 | `			  * method/property NAME, not a language construct — PHP allows any` |
|         - | 1418 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|         - | 1419 | `			  * as a plain literal like an ordinary identifier member name. Flag the` |
|         - | 1420 | `			  * token so GenStateLoadLiteral does not fold a reserved VALUE word` |
|         - | 1421 | `			  * (Enum::Null, C::Array, C::True) into its literal — the member name is` |
|         - | 1422 | `			  * the word itself. */` |
|       829 | 1423 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|       829 | 1424 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       829 | 1425 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    144942 | 1426 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|         - | 1427 | `			 /* List/Array node */` |
|    104767 | 1428 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1429 | `				 /* Assume a literal */` |
|       ! 0 | 1430 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1431 | `				 pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1432 | `			 }else{` |
|    104767 | 1433 | `				 pCur += 2;` |
|         - | 1434 | `				 /* Collect array/list tokens */` |
|    104767 | 1435 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    104767 | 1436 | `				 if( pCur < pGen->pEnd ){` |
|    104765 | 1437 | `					 pCur++;` |
|     52319 | 1438 | `				 }else{` |
|         - | 1439 | `					 /* Syntax error */` |
|         - | 1440 | `					 /* php names the token it stopped on and says it expected ")". */` |
|         3 | 1441 | `					 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\")\"");` |
|         3 | 1442 | `					 if( rc != SXERR_ABORT ){` |
|         3 | 1443 | `						 rc = SXERR_SYNTAX;` |
|         1 | 1444 | `					 }` |
|         3 | 1445 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1446 | `					 return rc;` |
|         - | 1447 | `				 }` |
|    104765 | 1448 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    104765 | 1449 | `				 if( pNode->xCode == PH7_CompileList ){` |
|        65 | 1450 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|        65 | 1451 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|         - | 1452 | ``						 /* php names the token that stopped it (the ';' after `list($a,$b)`),`` |
|         - | 1453 | ``						  * not the `list` the construct started at. */`` |
|         3 | 1454 | `						 rc = PH7_GenSyntaxError(pGen,pCur < pGen->pEnd ? pCur : 0,"\"=\"");` |
|         3 | 1455 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 1456 | `							 rc = SXERR_SYNTAX;` |
|         1 | 1457 | `						 }` |
|         3 | 1458 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|         3 | 1459 | `						 return rc;` |
|         - | 1460 | `					 }` |
|        29 | 1461 | `				 }` |
|         5 | 1462 | `			 }` |
|     92081 | 1463 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|         - | 1464 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|       739 | 1465 | `			 pCur++; /* Skip 'yield' keyword */` |
|       739 | 1466 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       739 | 1467 | `			 pNode->xCode = PH7_CompileYield;` |
|     39401 | 1468 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION` |
|     35701 | 1469 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       965 | 1470 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       816 | 1471 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FUNCTION ) ){` |
|         - | 1472 | `			 /* Annonymous function: function (...) {...} or static function (...) {...} */` |
|      7786 | 1473 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|         - | 1474 | `				 /* Assume a literal */` |
|       ! 0 | 1475 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       ! 0 | 1476 | `				pNode->xCode = PH7_CompileLiteral;` |
|       ! 0 | 1477 | `			 }else{` |
|         - | 1478 | `				 /* Assemble annonymous functions body */` |
|      7786 | 1479 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      7786 | 1480 | `				 if( rc != SXRET_OK ){` |
|        30 | 1481 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        30 | 1482 | `					 return rc;` |
|         - | 1483 | `				 }` |
|      7760 | 1484 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|         - | 1485 | `			  }` |
|     35114 | 1486 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       299 | 1487 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       179 | 1488 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|        76 | 1489 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        49 | 1490 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|         - | 1491 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|         - | 1492 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|         - | 1493 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|         - | 1494 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       287 | 1495 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       287 | 1496 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1497 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1498 | `				 return rc;` |
|         - | 1499 | `			 }` |
|       287 | 1500 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|     31112 | 1501 | `		 }else if( (nKeyword == PH7_TKWRD_FN \|\| nKeyword == PH7_TKWRD_STATIC)` |
|     20891 | 1502 | `			&& PH7_TokenOpensArrowFunc(pGen->pIn,pCur,pGen->pEnd) ){` |
|         - | 1503 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|     10535 | 1504 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|     10535 | 1505 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1506 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1507 | `				 return rc;` |
|         - | 1508 | `			 }` |
|     10535 | 1509 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|     25607 | 1510 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|         - | 1511 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       175 | 1512 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       175 | 1513 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 1514 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       ! 0 | 1515 | `				 return rc;` |
|         - | 1516 | `			 }` |
|       175 | 1517 | `			 pNode->xCode = PH7_CompileMatch;` |
|     20356 | 1518 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|         - | 1519 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|         - | 1520 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|         - | 1521 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|        51 | 1522 | `			 pCur++; /* Skip 'throw' */` |
|        51 | 1523 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|        51 | 1524 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|     20248 | 1525 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|         - | 1526 | `			 /* Language constructs [i.e: print,echo,die...] require special handling.` |
|         - | 1527 | `			  * Each of the six that reach here takes exactly ONE operand. */` |
|       397 | 1528 | `			 ExprDelimitKeywordOperand(pCur,pGen->pEnd,&pCur);` |
|       397 | 1529 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       201 | 1530 | `		 }else{` |
|         - | 1531 | `			 /* Assume a literal */` |
|     19833 | 1532 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|     19833 | 1533 | `			 pNode->xCode = PH7_CompileLiteral;` |
|         5 | 1534 | `		 }` |
|   7087887 | 1535 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|         - | 1536 | `		 /* Constants,function name,namespace path,class name... */` |
|   1822355 | 1537 | `		 if( bAfterMemberOp ){` |
|         - | 1538 | `			 /* An identifier used as a member NAME right after -> / ?-> / :: is the` |
|         - | 1539 | `			  * name itself. null/true/false are lexed as PH7_TK_ID (not keywords), so` |
|         - | 1540 | ``			  * `Enum::Null` / `C::True` would otherwise be folded to the literal VALUE`` |
|         - | 1541 | `			  * by GenStateLoadLiteral, leaving an empty member name. Flag the token. */` |
|     43971 | 1542 | `			 pCur->nType \|= PH7_TK_MEMBER_NAME;` |
|     21964 | 1543 | `		 }` |
|   1822355 | 1544 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   1822355 | 1545 | `		 pNode->xCode = PH7_CompileLiteral;` |
|    909538 | 1546 | `	 }else{` |
|   5193070 | 1547 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|         - | 1548 | `			 /* Point to the code generator routine */` |
|   1880041 | 1549 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   1880041 | 1550 | `			 if( pNode->xCode == 0 ){` |
|        34 | 1551 | `				 rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|        34 | 1552 | `				 if( rc != SXERR_ABORT ){` |
|        34 | 1553 | `					 rc = SXERR_SYNTAX;` |
|        16 | 1554 | `				 }` |
|        34 | 1555 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        34 | 1556 | `				 return rc;` |
|         - | 1557 | `			 }` |
|    938425 | 1558 | `		 }` |
|         - | 1559 | `		/* Advance the stream cursor */` |
|   5193038 | 1560 | `		pCur++;` |
|         - | 1561 | `	 }` |
|         - | 1562 | `	/* Point to the end of the token stream */` |
|  15025516 | 1563 | `	pNode->pEnd = pCur;` |
|         - | 1564 | `	/* Save the node for later processing */` |
|  15025516 | 1565 | `	*ppNode = pNode;` |
|         - | 1566 | `	/* Synchronize cursors */` |
|  15025516 | 1567 | `	pGen->pIn = pCur;` |
|  15025516 | 1568 | `	return SXRET_OK;` |
|   7501430 | 1569 | `}` |
|         - | 1570 | `/*` |
|         - | 1571 | ` * Point to the next expression that should be evaluated shortly.` |
|         - | 1572 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|         - | 1573 | ` * level is zero.` |
|         - | 1574 | ` */` |
|    424111 | 1575 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|         5 | 1576 | `{` |
|    424116 | 1577 | `	SyToken *pCur = pStart;` |
|    424116 | 1578 | `	sxi32 iNest = 0;` |
|    424116 | 1579 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|         - | 1580 | `		/* Last expression */` |
|    166215 | 1581 | `		return SXERR_EOF;` |
|         - | 1582 | `	}` |
|    827201 | 1583 | `	while( pCur < pEnd ){` |
|    784420 | 1584 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    215125 | 1585 | `			break;` |
|         - | 1586 | `		}` |
|    569300 | 1587 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     48844 | 1588 | `			iNest++;` |
|    544843 | 1589 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|     48850 | 1590 | `			iNest--;` |
|     24385 | 1591 | `		}` |
|    569300 | 1592 | `		pCur++;` |
|         5 | 1593 | `	}` |
|    257906 | 1594 | `	*ppNext = pCur;` |
|    257906 | 1595 | `	return SXRET_OK;` |
|    211673 | 1596 | `}` |
|         - | 1597 | `/*` |
|         - | 1598 | ` * Release one node -- its own storage and the argument set it carries, and` |
|         - | 1599 | ` * nothing else. The tree it is part of is NOT walked: every node an expression` |
|         - | 1600 | ` * ever produced is in the extraction set, and that set is what owns them (see` |
|         - | 1601 | ` * PH7_ExprFreeTree).` |
|         - | 1602 | ` */` |
|  15025955 | 1603 | `static void ExprFreeNode(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|         5 | 1604 | `{` |
|  15025960 | 1605 | `	SySetRelease(&pNode->aNodeArgs);` |
|  15025960 | 1606 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  15025960 | 1607 | `}` |
|         - | 1608 | `/*` |
|         - | 1609 | ` * Free every node of an expression.` |
|         - | 1610 | ` *` |
|         - | 1611 | ` * The EXTRACTION SET owns the nodes, one entry per node, in the order the` |
|         - | 1612 | ` * tokens were read -- which is the only container that ever sees all of them.` |
|         - | 1613 | ` * Tree building runs on a COPY of that list and consumes it by NULLING each` |
|         - | 1614 | ` * slot it folds in, so a walk from the roots reaches only what ended up in a` |
|         - | 1615 | ` * tree: every delimiter a fold swallowed ('(' ')' '[' ']' ',' and the label` |
|         - | 1616 | ` * and colon of a named argument) was dropped from the copy and then reachable` |
|         - | 1617 | ` * from nothing. That leaked 17% of all the nodes an ordinary program compiles` |
|         - | 1618 | ` * -- 40,072 of 235,466 on one 40-file lint run, 5.1 MB held for the life of` |
|         - | 1619 | ` * the process -- because the compiler's AST is pool-allocated and the pool is` |
|         - | 1620 | ` * only handed back at VM teardown, so no leak checker ever named it.` |
|         - | 1621 | ` *` |
|         - | 1622 | ` * Freeing from the set instead of from the roots also makes the count exact in` |
|         - | 1623 | ` * the other direction: a node cannot be reached twice, so the ownership rule` |
|         - | 1624 | ` * that used to have to be maintained at every fold site ("null it here, free` |
|         - | 1625 | ` * it there, and never both") is gone.` |
|         - | 1626 | ` */` |
|   2908112 | 1627 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|         5 | 1628 | `{` |
|         - | 1629 | `	ph7_expr_node **apNode;` |
|         - | 1630 | `	sxu32 n;` |
|   2908117 | 1631 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  17934072 | 1632 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  15025960 | 1633 | `		if( apNode[n] ){` |
|  15025960 | 1634 | `			ExprFreeNode(&(*pGen),apNode[n]);` |
|   7501037 | 1635 | `		}` |
|   7501042 | 1636 | `	}` |
|   2908117 | 1637 | `	SySetReset(pNodeSet);` |
|   2908117 | 1638 | `	return SXRET_OK;` |
|         5 | 1639 | `}` |
|         - | 1640 | `/*` |
|         - | 1641 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|         - | 1642 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|         - | 1643 | ` * references, and unset() that target any link of a nullsafe chain` |
|         - | 1644 | ` * (PHP 8.0 makes this a compile fatal:` |
|         - | 1645 | ` * "Can't use nullsafe operator in write context").` |
|         - | 1646 | ` */` |
|         - | 1647 | `/*` |
|         - | 1648 | `` * TRUE when this node's operator is a link of an ACCESS CHAIN -- `->`, `?->`,`` |
|         - | 1649 | `` * `::`, `[`, a call -- or one of the prefix unaries php hoists an assignment out`` |
|         - | 1650 | `` * of, so that `!$a?->b = 1` is `!($a?->b = 1)` and the `!` still stands over the`` |
|         - | 1651 | `` * target. Everything else (a comparison, `&&`, `+`) is a NEIGHBOUR of the target,`` |
|         - | 1652 | ` * not part of it.` |
|         - | 1653 | ` */` |
|   1138165 | 1654 | `static int ExprIsAccessChainRoot(ph7_expr_node *pNode)` |
|         5 | 1655 | `{` |
|   1138170 | 1656 | `	if( pNode == 0 \|\| pNode->pOp == 0 ){` |
|    928858 | 1657 | `		return 0;` |
|         - | 1658 | `	}` |
|    209312 | 1659 | `	if( pNode->pOp->iOp == EXPR_OP_ARROW \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|    206599 | 1660 | `	 \|\| pNode->pOp->iOp == EXPR_OP_DC \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    103176 | 1661 | `	 \|\| pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|    209211 | 1662 | `		return 1;` |
|         - | 1663 | `	}` |
|       158 | 1664 | `	return (pNode->iFlags & EXPR_NODE_PARENS) == 0` |
|       174 | 1665 | `		&& (pNode->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|        68 | 1666 | `		 \|\| pNode->pOp->iOp == EXPR_OP_CLONE);` |
|    568323 | 1667 | `}` |
|         - | 1668 | `/*` |
|         - | 1669 | `` * TRUE when the ACCESS CHAIN this node roots contains a `?->`. Unlike`` |
|         - | 1670 | ` * PH7_ExprContainsNullsafe it stops at the first link that is not one, so a` |
|         - | 1671 | `` * nullsafe standing in a NEIGHBOURING operand -- `$o?->m() !== null && $x = 1`,`` |
|         - | 1672 | `` * where php's target is `$x` alone -- is not mistaken for one in the target.`` |
|         - | 1673 | ` */` |
|    928927 | 1674 | `static int ExprChainHasNullsafe(ph7_expr_node *pNode)` |
|         5 | 1675 | `{` |
|   1138164 | 1676 | `	while( ExprIsAccessChainRoot(pNode) ){` |
|    209265 | 1677 | `		if( pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        32 | 1678 | `			return 1;` |
|         - | 1679 | `		}` |
|    209237 | 1680 | `		pNode = pNode->pLeft;` |
|         5 | 1681 | `	}` |
|    928904 | 1682 | `	return 0;` |
|    463841 | 1683 | `}` |
|   3810734 | 1684 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|         5 | 1685 | `{` |
|   3810739 | 1686 | `	if( pNode == 0 ){` |
|   2468595 | 1687 | `		return 0;` |
|         - | 1688 | `	}` |
|   1342149 | 1689 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        21 | 1690 | `		return 1;` |
|         - | 1691 | `	}` |
|   1342131 | 1692 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|       ! 0 | 1693 | `		return 1;` |
|         - | 1694 | `	}` |
|   1342131 | 1695 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|       ! 0 | 1696 | `		return 1;` |
|         - | 1697 | `	}` |
|   1342131 | 1698 | `	return 0;` |
|   1902800 | 1699 | `}` |
|         - | 1700 | `/*` |
|         - | 1701 | `` * TRUE when a `::` node names a class CONSTANT (`A::K`, `A::class`) rather than`` |
|         - | 1702 | `` * a static PROPERTY (`A::$s`, `A::$$name`).`` |
|         - | 1703 | ` *` |
|         - | 1704 | ` * php's grammar puts the two in different rules and only the property is a` |
|         - | 1705 | `` * `variable`: a class constant is a `constant`, which nothing but a DEREFERENCE`` |
|         - | 1706 | `` * (`A::K[0]`, `A::K->p`) can turn into one — and that dereference answers a`` |
|         - | 1707 | `` * TEMPORARY. So `A::K = 5`, `A::K++` and `unset(A::K)` are php PARSE errors`` |
|         - | 1708 | `` * while `A::K[0] = 5` is its "Cannot use temporary expression in write`` |
|         - | 1709 | `` * context". PHL treated every `::` as class-level STORAGE, so the first three`` |
|         - | 1710 | `` * were reported at runtime with a PH7-ism (or, for `A::K++` and `--A::K`, ran in`` |
|         - | 1711 | ` * silence) and the fourth wrote into a discarded copy of the constant.` |
|         - | 1712 | ` *` |
|         - | 1713 | `` * The token after `::` decides it: a static property always spells a `$`.`` |
|         - | 1714 | ` */` |
|       732 | 1715 | `PH7_PRIVATE int PH7_ExprNodeIsClassConst(ph7_expr_node *pNode)` |
|         5 | 1716 | `{` |
|       737 | 1717 | `	if( pNode == 0 \|\| pNode->pOp == 0 \|\| pNode->pOp->iOp != EXPR_OP_DC ){` |
|       215 | 1718 | `		return FALSE;` |
|         - | 1719 | `	}` |
|       527 | 1720 | `	if( pNode->pRight == 0 \|\| pNode->pRight->pStart == 0 ){` |
|         - | 1721 | `		/* Not linked yet / nothing to look at: keep the old permissive answer. */` |
|       ! 0 | 1722 | `		return FALSE;` |
|         - | 1723 | `	}` |
|       527 | 1724 | `	return (pNode->pRight->pStart->nType & PH7_TK_DOLLAR) == 0;` |
|       371 | 1725 | `}` |
|         - | 1726 | `/*` |
|         - | 1727 | ` * Check if the given node is a modifialbe l/r-value.` |
|         - | 1728 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|         - | 1729 | ` *` |
|         - | 1730 | ` * This is the SHAPE question only: is the target an access chain at all?` |
|         - | 1731 | ` * Whether the chain's BASE may be written through is php's separate rule, made` |
|         - | 1732 | ` * at codegen by GenStateWriteTargetCheck (php: zend_compile_var_inner) so every` |
|         - | 1733 | `` * write kind — `=`, `+=`, `=&`, `++`, `unset()`, a foreach target — reaches it`` |
|         - | 1734 | ` * and reports php's own two refusals instead of a message naming the operator.` |
|         - | 1735 | ` */` |
|    930243 | 1736 | `PH7_PRIVATE int PH7_ExprIsModifiableValue(ph7_expr_node *pNode)` |
|         5 | 1737 | `{` |
|         - | 1738 | `	sxi32 iExprOp;` |
|    930248 | 1739 | `	if( pNode->pOp == 0 ){` |
|    720966 | 1740 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|         - | 1741 | `	}` |
|    209287 | 1742 | `	iExprOp = pNode->pOp->iOp;` |
|    209287 | 1743 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ ){` |
|      2635 | 1744 | `			return TRUE;` |
|         - | 1745 | `	}` |
|    206657 | 1746 | `	if( iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|         - | 1747 | ``		/* `C::$s` is storage; `C::K` is a constant, and php's grammar will not`` |
|         - | 1748 | `		 * take one as a write target at all. */` |
|       183 | 1749 | `		return PH7_ExprNodeIsClassConst(pNode) ? FALSE : TRUE;` |
|         - | 1750 | `	}` |
|    206479 | 1751 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|         - | 1752 | `		/* A subscript is a writable shape whatever it subscripts. php compiles` |
|         - | 1753 | ``		 * and RUNS `f()[0] = 5` and `str_split("ab")[0] = "z"` — the write lands`` |
|         - | 1754 | `		 * on the temporary the call answered and is discarded — and refuses a` |
|         - | 1755 | ``		 * literal/cast/computed/`new` base with a wording of its own. Screening`` |
|         - | 1756 | ``		 * the base here rejected both alike with `'=': Left operand must be a`` |
|         - | 1757 | ``		 * modifiable l-value`, and did it BEFORE the codegen check that knows`` |
|         - | 1758 | `		 * php's rules could speak. */` |
|    206381 | 1759 | `		return TRUE;` |
|         - | 1760 | `	}` |
|       103 | 1761 | `	if( iExprOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1762 | ``		/* A call is a shape both ways: as a reference SOURCE (`$r =& f()`) it is`` |
|         - | 1763 | `		 * php-legal, and as a write TARGET it is php's own compile fatal naming` |
|         - | 1764 | `		 * the kind of call, which GenStateWriteTargetCheck raises. */` |
|        47 | 1765 | `		return TRUE;` |
|         - | 1766 | `	}` |
|         - | 1767 | `	/* Not a modifiable l or r-value */` |
|        59 | 1768 | `	return FALSE;` |
|    464499 | 1769 | `}` |
|         - | 1770 | `/*` |
|         - | 1771 | `` * php refuses a write to something that is not a `variable` in its GRAMMAR, so`` |
|         - | 1772 | ` * what comes out is a SYNTAX error naming a token — never a sentence about the` |
|         - | 1773 | ` * operator, which is all PHL had ("'=': Left operand must be a modifiable` |
|         - | 1774 | `` * l-value", "'++' operator needs l-value", and two more for `=&` and `unset()`).`` |
|         - | 1775 | ` * Which token php names depends on which side of the operator the offending` |
|         - | 1776 | ` * operand sits, and both shapes are here:` |
|         - | 1777 | ` *` |
|         - | 1778 | `` *   the operand LEFT of the operator (`5 = 1`, `A::K += 1`, `(1+2)++`) —  php`` |
|         - | 1779 | ` *   has already shifted it and stops AT the operator, so that is what it names.` |
|         - | 1780 | ` */` |
|        12 | 1781 | `static sxi32 ExprWriteTargetNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOpNode)` |
|         4 | 1782 | `{` |
|         - | 1783 | `	sxi32 rc;` |
|        16 | 1784 | `	if( pOpNode->pOp && pOpNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 1785 | ``		/* php lexes `=&` as `=` then `&`, and stops on the `=`. */`` |
|       ! 0 | 1786 | `		rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       ! 0 | 1787 | `			pOpNode->pStart ? pOpNode->pStart->nLine : 0,` |
|         - | 1788 | `			"syntax error, unexpected token \"=\"");` |
|       ! 0 | 1789 | `	}else{` |
|        16 | 1790 | `		rc = PH7_GenSyntaxError(pGen,pOpNode->pStart,0);` |
|         - | 1791 | `	}` |
|        16 | 1792 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         4 | 1793 | `}` |
|         - | 1794 | `/*` |
|         - | 1795 | ` * A token pointer one PAST the last token of a subtree is only a real token` |
|         - | 1796 | `` * while the subtree is not the last thing in the file. `<?php --A::K` (no`` |
|         - | 1797 | ` * terminator) walked off the end of the stream and named a garbage token on` |
|         - | 1798 | ` * line 0; php says "unexpected end of file" there, which is what` |
|         - | 1799 | ` * PH7_GenSyntaxError answers for a NULL. Returns pTok, or 0 when it is outside` |
|         - | 1800 | ` * the chunk's token stream.` |
|         - | 1801 | ` */` |
|        12 | 1802 | `PH7_PRIVATE SyToken * PH7_ExprTokenInStream(ph7_gen_state *pGen,SyToken *pTok)` |
|         3 | 1803 | `{` |
|         - | 1804 | `	SyToken *pBase, *pStreamEnd;` |
|        15 | 1805 | `	if( pTok == 0 \|\| pGen->pTokenSet == 0 ){` |
|       ! 0 | 1806 | `		return pTok;` |
|         - | 1807 | `	}` |
|        15 | 1808 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        15 | 1809 | `	pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        15 | 1810 | `	return (pTok >= pBase && pTok < pStreamEnd) ? pTok : 0;` |
|         9 | 1811 | `}` |
|         - | 1812 | `/*` |
|         - | 1813 | `` *   the operand RIGHT of the operator (`--A::K`, `$r =& "s"`, `unset(GK)`) — it`` |
|         - | 1814 | `` *   was shifted as a `constant`/`dereferencable_scalar`, so php runs past it and`` |
|         - | 1815 | ` *   stops on whatever FOLLOWS, still expecting the dereference that would have` |
|         - | 1816 | ` *   made it a variable. A bare integer/float is the exception: nothing in php's` |
|         - | 1817 | ` *   grammar dereferences one, so the literal itself is named.` |
|         - | 1818 | ` */` |
|        10 | 1819 | `PH7_PRIVATE sxi32 PH7_ExprOperandNotAVariable(ph7_gen_state *pGen,ph7_expr_node *pOperand)` |
|         2 | 1820 | `{` |
|        12 | 1821 | `	SyToken *pMin = 0, *pMax = 0;` |
|         - | 1822 | `	sxi32 rc;` |
|        10 | 1823 | `	if( pOperand && pOperand->pOp == 0 && pOperand->pStart` |
|         6 | 1824 | `	 && (pOperand->pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL)) ){` |
|       ! 0 | 1825 | `		rc = PH7_GenSyntaxError(pGen,pOperand->pStart,0);` |
|       ! 0 | 1826 | `	}else{` |
|         - | 1827 | `		/* The whole SUBTREE has to be stepped over, not just the node's own` |
|         - | 1828 | ``		 * tokens: `A::K` and `(1+2)` each named an inner token otherwise. The`` |
|         - | 1829 | `		 * span's max is already one past the last token. */` |
|        12 | 1830 | `		PH7_ExprSubtreeSpan(pOperand,&pMin,&pMax);` |
|        12 | 1831 | `		rc = PH7_GenSyntaxError(pGen,PH7_ExprTokenInStream(pGen,pMax),` |
|         - | 1832 | `			"\"->\" or \"?->\" or \"[\"");` |
|         - | 1833 | `	}` |
|        12 | 1834 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         2 | 1835 | `}` |
|         - | 1836 | `/* Forward declaration */` |
|         - | 1837 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|         - | 1838 | `/* How many nodes of tree-building scratch PH7_ExprMakeTree carries in its own` |
|         - | 1839 | ` * frame. One pointer each, and an expression longer than this borrows from the` |
|         - | 1840 | ` * pool instead -- 64 covers everything a hand-written statement is likely to` |
|         - | 1841 | ` * be, and the frame only exists while ONE expression is being folded (a nested` |
|         - | 1842 | ` * one is compiled later, from the tree). */` |
|         - | 1843 | `#define EXPR_STACK_NODES 64` |
|         - | 1844 | `/* Macro to check if the given node is a terminal.` |
|         - | 1845 | ` * A node is a term if it has no operator, or has already been linked into an` |
|         - | 1846 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|         - | 1847 | ` * linked ternary/elvis node). */` |
|         - | 1848 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|         - | 1849 | `/*` |
|         - | 1850 | ` * Buid an expression tree for each given function argument.` |
|         - | 1851 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 1852 | ` */` |
|   1158621 | 1853 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 1854 | `{` |
|         - | 1855 | `	sxi32 iNest,iCur,iNode;` |
|         - | 1856 | `	sxi32 rc;` |
|         - | 1857 | ``	/* php: a stray token in a call argument is `... expecting ")"`. Each arg's`` |
|         - | 1858 | `	 * tree is built by the shared ExprMakeTree below, whose leftover-node error` |
|         - | 1859 | `	 * reads this. Saved/restored so a nested call or array element inside an arg` |
|         - | 1860 | `	 * gets its own closer. */` |
|   1158626 | 1861 | `	const char *zSaveArg = pGen->zClauseCloser;` |
|   1158626 | 1862 | `	pGen->zClauseCloser = "\")\"";` |
|         - | 1863 | `	/* Process function arguments from left to right */` |
|   1158626 | 1864 | `	iCur = 0;` |
|   1430631 | 1865 | `	for(;;){` |
|   2866875 | 1866 | `		if( iCur >= nToken ){` |
|         - | 1867 | `			/* No more arguments to process */` |
|   1158594 | 1868 | `			break;` |
|         - | 1869 | `		}` |
|   1708286 | 1870 | `		iNode = iCur;` |
|   1708286 | 1871 | `		iNest = 0;` |
|   4681807 | 1872 | `		while( iCur < nToken ){` |
|   3523216 | 1873 | `			if( apNode[iCur] ){` |
|   3439909 | 1874 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    274213 | 1875 | `					break;` |
|   2890214 | 1876 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   1550948 | 1877 | `					&& apNode[iCur]->pLeft == 0` |
|    216641 | 1878 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    212449 | 1879 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|         - | 1880 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|         - | 1881 | `					 * self-contained node that already consumed its matching ']', so its` |
|         - | 1882 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|         - | 1883 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|         - | 1884 | `					 * following comma is never seen as an argument separator (collapsing` |
|         - | 1885 | `					 * e.g. array_merge([1],[2]) to just [2]). The same holds for any` |
|         - | 1886 | `					 * already-folded subtree (pLeft != 0): a nested call collapsed inside` |
|         - | 1887 | `					 * a parenthesised group -- (f())->m() -- keeps the LPAREN bit on its` |
|         - | 1888 | `					 * root while its ')' was nulled, so counting it would strand iNest > 0` |
|         - | 1889 | `					 * and swallow the following argument separator. */` |
|    208327 | 1890 | `					iNest++;` |
|   2785863 | 1891 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB))` |
|   1442790 | 1892 | `					&& apNode[iCur]->pLeft == 0 ){` |
|    208327 | 1893 | `					iNest--;` |
|    103966 | 1894 | `				}` |
|   1442395 | 1895 | `			}` |
|   2973526 | 1896 | `			iCur++;` |
|         5 | 1897 | `		}` |
|   1708286 | 1898 | `		if( iCur > iNode ){` |
|   1708280 | 1899 | `			SyString sArgName = {0, 0};` |
|         - | 1900 | `			/* Check for named argument pattern: identifier ':' expr.` |
|         - | 1901 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|         - | 1902 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   1708275 | 1903 | `			if( (iCur - iNode) >= 2` |
|   1014332 | 1904 | `				&& apNode[iNode]` |
|    318559 | 1905 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    211094 | 1906 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|    107401 | 1907 | `				&& apNode[iNode+1]` |
|    106171 | 1908 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|         - | 1909 | `				/* Named argument detected: save the name and drop the label and` |
|         - | 1910 | `				 * colon nodes from the working copy. Dropping is all a fold ever` |
|         - | 1911 | `				 * does now -- the extraction set frees them (PH7_ExprFreeTree). */` |
|      1547 | 1912 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      1547 | 1913 | `				apNode[iNode] = 0;` |
|      1547 | 1914 | `				apNode[iNode+1] = 0;` |
|      1547 | 1915 | `				iNode += 2;` |
|         - | 1916 | `				/* Guard: the value expression must not be empty.  Catches` |
|         - | 1917 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      1547 | 1918 | `				if( iNode >= iCur ){` |
|         4 | 1919 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|         2 | 1920 | `						pOp->pStart->nLine,` |
|         - | 1921 | `						"syntax error, expected expression after named argument '%z:'",` |
|         - | 1922 | `						&sArgName);` |
|         3 | 1923 | `					if( rc != SXERR_ABORT ){` |
|         3 | 1924 | `						rc = SXERR_SYNTAX;` |
|         1 | 1925 | `					}` |
|         3 | 1926 | `					pGen->zClauseCloser = zSaveArg;` |
|         3 | 1927 | `					return rc;` |
|         - | 1928 | `				}` |
|       770 | 1929 | `			}` |
|   1708273 | 1930 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|         5 | 1931 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|       ! 0 | 1932 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|         - | 1933 | `						"call-time pass-by-reference is depreceated");` |
|       ! 0 | 1934 | `					apNode[iNode] = 0;` |
|       ! 0 | 1935 | `			}` |
|         - | 1936 | `			{` |
|         - | 1937 | ``				/* `...$expr` flags the argument's FIRST node at extraction`` |
|         - | 1938 | `				 * time; when the expression is more than a lone terminal` |
|         - | 1939 | `				 * (a call, member access, ...) tree-building roots the span` |
|         - | 1940 | `				 * at a DIFFERENT node — carry the spread mark onto the root` |
|         - | 1941 | `				 * or the code generator never emits OP_SPREAD (f(...mk())` |
|         - | 1942 | `				 * used to pass the whole array as one argument). Scan for` |
|         - | 1943 | `				 * the first LIVE node: an outer paren pass may already have` |
|         - | 1944 | ``				 * collapsed a leading group — `...(new S)->pair()` — leaving`` |
|         - | 1945 | `				 * NULL slots ahead of the flagged subtree. */` |
|   1708278 | 1946 | `				int bSpreadArg = 0;` |
|         - | 1947 | `				sxi32 iScan;` |
|   1717752 | 1948 | `				for( iScan = iNode ; iScan < iCur ; iScan++ ){` |
|   1717752 | 1949 | `					if( apNode[iScan] ){` |
|   1708278 | 1950 | `						bSpreadArg = (apNode[iScan]->iFlags & EXPR_NODE_SPREAD) != 0;` |
|   1708278 | 1951 | `						break;` |
|         - | 1952 | `					}` |
|      4735 | 1953 | `				}` |
|   1708278 | 1954 | `				ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   1708278 | 1955 | `				if( bSpreadArg && apNode[iNode] ){` |
|       568 | 1956 | `					apNode[iNode]->iFlags \|= EXPR_NODE_SPREAD;` |
|       281 | 1957 | `				}` |
|         - | 1958 | `			}` |
|   1708278 | 1959 | `			if( apNode[iNode] ){` |
|   1708278 | 1960 | `				if( sArgName.nByte > 0 ){` |
|      1545 | 1961 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      1545 | 1962 | `					apNode[iNode]->sArgName = sArgName;` |
|       770 | 1963 | `				}` |
|         - | 1964 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   1708278 | 1965 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|    852421 | 1966 | `			}else{` |
|         - | 1967 | `				/* No expression before comma */` |
|       ! 0 | 1968 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|       ! 0 | 1969 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|         - | 1970 | `					"syntax error, unexpected token \",\"");` |
|       ! 0 | 1971 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 | 1972 | `					rc = SXERR_SYNTAX;` |
|       ! 0 | 1973 | `				}` |
|       ! 0 | 1974 | `				pGen->zClauseCloser = zSaveArg;` |
|       ! 0 | 1975 | `				return rc;` |
|         - | 1976 | `			}` |
|    852421 | 1977 | `		}else{` |
|         - | 1978 | `			/* Comma with no preceding argument */` |
|         9 | 1979 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|         9 | 1980 | `			if( rc != SXERR_ABORT ){` |
|         9 | 1981 | `				rc = SXERR_SYNTAX;` |
|         3 | 1982 | `			}` |
|         9 | 1983 | `			pGen->zClauseCloser = zSaveArg;` |
|         9 | 1984 | `			return rc;` |
|         - | 1985 | `		}` |
|         - | 1986 | `		/* Jump trailing comma */` |
|   1708278 | 1987 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    549689 | 1988 | `			iCur++;` |
|    549689 | 1989 | `			if( iCur >= nToken ){` |
|         - | 1990 | `				/* Trailing comma after last argument */` |
|        26 | 1991 | `				break;` |
|         - | 1992 | `			}` |
|    274193 | 1993 | `		}` |
|         5 | 1994 | `	}` |
|   1158618 | 1995 | `	pGen->zClauseCloser = zSaveArg;` |
|   1158618 | 1996 | `	return SXRET_OK;` |
|    578232 | 1997 | `}` |
|         - | 1998 | ` /*` |
|         - | 1999 | `  * The FIRST source token of a (sub)tree. A linked subtree keeps its OPERATOR` |
|         - | 2000 | ``  * node at the array slot (`$i < 3` lives at the `<` slot, `@@$b` at the first`` |
|         - | 2001 | ``  * `@`), so apNode[i]->pStart names an interior token for an infix op. php names`` |
|         - | 2002 | `  * the start of the stray expression — the leftmost SOURCE token. Tokens live in` |
|         - | 2003 | `  * one contiguous set, so that is simply the minimum pStart pointer across the` |
|         - | 2004 | ``  * whole subtree; a prefix operator (`@`) is its own leftmost token, an infix one`` |
|         - | 2005 | ``  * (`<`) is not, and this covers both without assuming which child a node uses.`` |
|         - | 2006 | `  */` |
|       254 | 2007 | ` static SyToken * ExprSubtreeFirstToken(ph7_expr_node *pNode)` |
|         5 | 2008 | ` {` |
|         - | 2009 | `	 SyToken *pMin;` |
|         - | 2010 | `	 SyToken *pChild;` |
|       259 | 2011 | `	 if( pNode == 0 ){` |
|       171 | 2012 | `		 return 0;` |
|         - | 2013 | `	 }` |
|        93 | 2014 | `	 pMin = pNode->pStart;` |
|        93 | 2015 | `	 pChild = ExprSubtreeFirstToken(pNode->pLeft);` |
|        93 | 2016 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|         6 | 2017 | `		 pMin = pChild;` |
|         2 | 2018 | `	 }` |
|        93 | 2019 | `	 pChild = ExprSubtreeFirstToken(pNode->pRight);` |
|        93 | 2020 | `	 if( pChild && (pMin == 0 \|\| pChild < pMin) ){` |
|       ! 0 | 2021 | `		 pMin = pChild;` |
|       ! 0 | 2022 | `	 }` |
|        93 | 2023 | `	 return pMin;` |
|       132 | 2024 | ` }` |
|         - | 2025 | `/*` |
|         - | 2026 | ` * The full RAW token extent of a linked subtree: minimum pStart / maximum pEnd` |
|         - | 2027 | ` * over every node (pLeft/pRight/pCond and postfix aNodeArgs children), widened` |
|         - | 2028 | ` * by one token on each side for a node whose group parens were consumed by` |
|         - | 2029 | ` * ExprMakeTree's paren pass (EXPR_NODE_PARENS — its '('/')' slots were nulled,` |
|         - | 2030 | ` * but token contiguity guarantees they sit exactly one token outside the inner` |
|         - | 2031 | ` * extent). Tokens live in one contiguous set, so pointer min/max IS source` |
|         - | 2032 | ` * order. Consumed by the assert() source-text capture, which` |
|         - | 2033 | ` * needs the argument's whole source span where a root's own pStart/pEnd name` |
|         - | 2034 | `` * only the operator token. Note: nested redundant groups `((x))` share the`` |
|         - | 2035 | ` * single PARENS bit, so only one paren layer is recovered — the renderer` |
|         - | 2036 | ` * strips redundant outermost parens anyway, matching php's export.` |
|         - | 2037 | ` */` |
|       626 | 2038 | `PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax)` |
|         5 | 2039 | `{` |
|         - | 2040 | `	SyToken *pMin;` |
|         - | 2041 | `	SyToken *pMax;` |
|       631 | 2042 | `	SyToken *pCMin = 0;` |
|       631 | 2043 | `	SyToken *pCMax = 0;` |
|         - | 2044 | `	ph7_expr_node **apArg;` |
|         - | 2045 | `	sxu32 n;` |
|       631 | 2046 | `	if( pNode == 0 ){` |
|       449 | 2047 | `		return;` |
|         - | 2048 | `	}` |
|       187 | 2049 | `	pMin = pNode->pStart;` |
|       187 | 2050 | `	pMax = pNode->pEnd;` |
|       187 | 2051 | `	PH7_ExprSubtreeSpan(pNode->pLeft,&pCMin,&pCMax);` |
|       187 | 2052 | `	PH7_ExprSubtreeSpan(pNode->pRight,&pCMin,&pCMax);` |
|       187 | 2053 | `	PH7_ExprSubtreeSpan(pNode->pCond,&pCMin,&pCMax);` |
|       187 | 2054 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       193 | 2055 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|         7 | 2056 | `		PH7_ExprSubtreeSpan(apArg[n],&pCMin,&pCMax);` |
|         4 | 2057 | `	}` |
|       187 | 2058 | `	if( pCMin && (pMin == 0 \|\| pCMin < pMin) ){` |
|        52 | 2059 | `		pMin = pCMin;` |
|        25 | 2060 | `	}` |
|       187 | 2061 | `	if( pCMax && (pMax == 0 \|\| pCMax > pMax) ){` |
|        59 | 2062 | `		pMax = pCMax;` |
|        28 | 2063 | `	}` |
|       187 | 2064 | `	if( (pNode->iFlags & EXPR_NODE_PARENS) && pMin && pMax ){` |
|         5 | 2065 | `		pMin--;` |
|         5 | 2066 | `		pMax++;` |
|         2 | 2067 | `	}` |
|       182 | 2068 | `	if( pNode->pOp && pMax` |
|        61 | 2069 | `	 && (pNode->pOp->iOp == EXPR_OP_FUNC_CALL \|\| pNode->pOp->iOp == EXPR_OP_SUBSCRIPT) ){` |
|         - | 2070 | `		/* A postfix call/subscript's extent stops AT its closing ')' / ']' (the` |
|         - | 2071 | `		 * closer's node was consumed building the postfix op); token contiguity` |
|         - | 2072 | `		 * puts the closer exactly at the extent, so widen one token past it. */` |
|         5 | 2073 | `		pMax++;` |
|         2 | 2074 | `	}` |
|       187 | 2075 | `	if( pMin && (*ppMin == 0 \|\| pMin < *ppMin) ){` |
|       137 | 2076 | `		*ppMin = pMin;` |
|        66 | 2077 | `	}` |
|       187 | 2078 | `	if( pMax && (*ppMax == 0 \|\| pMax > *ppMax) ){` |
|       185 | 2079 | `		*ppMax = pMax;` |
|        90 | 2080 | `	}` |
|       318 | 2081 | `}` |
|         - | 2082 | ` /*` |
|         - | 2083 | `  * Create an expression tree from an array of tokens.` |
|         - | 2084 | `  * If successful, the root of the tree is stored in apNode[0].` |
|         - | 2085 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 2086 | `  */` |
|   5235177 | 2087 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|         5 | 2088 | ` {` |
|         - | 2089 | `	 sxi32 i,iLeft,iRight;` |
|         - | 2090 | `	 ph7_expr_node *pNode;` |
|         - | 2091 | `	 ph7_expr_node *pSuppress;` |
|   5235182 | 2092 | `	 ph7_expr_node *pUnOuter = 0;` |
|         - | 2093 | `	 sxi32 iCur;` |
|         - | 2094 | `	 sxi32 rc;` |
|   5235182 | 2095 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|         - | 2096 | `		 /* TICKET 1433-17: self evaluating node */` |
|   2458362 | 2097 | `		 return SXRET_OK;` |
|         - | 2098 | `	 }` |
|         - | 2099 | `	 /* Process expressions enclosed in parenthesis first */` |
|  18624068 | 2100 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 2101 | `		 sxi32 iNest;` |
|         - | 2102 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 2103 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|         - | 2104 | `		  */` |
|  15847252 | 2105 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  15611601 | 2106 | `			 continue;` |
|         - | 2107 | `		 }` |
|    235656 | 2108 | `		 iNest = 1;` |
|    235656 | 2109 | `		 iLeft = iCur;` |
|         - | 2110 | `		 /* Find the closing parenthesis */` |
|    235656 | 2111 | `		 iCur++;` |
|   1557140 | 2112 | `		 while( iCur < nToken ){` |
|   1557140 | 2113 | `			 if( apNode[iCur] ){` |
|   1557140 | 2114 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|         - | 2115 | `					 /* Decrement nesting level */` |
|    349531 | 2116 | `					 iNest--;` |
|    349531 | 2117 | `					 if( iNest <= 0 ){` |
|    235656 | 2118 | `						 break;` |
|         5 | 2119 | `					 }` |
|   1264477 | 2120 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|         - | 2121 | `					 /* Increment nesting level */` |
|    113880 | 2122 | `					 iNest++;` |
|     56863 | 2123 | `				 }` |
|    659896 | 2124 | `			 }` |
|   1321489 | 2125 | `			 iCur++;` |
|         5 | 2126 | `		 }` |
|    235656 | 2127 | `		 if( iCur - iLeft > 1 ){` |
|         - | 2128 | `			 sxi32 j;` |
|         - | 2129 | `			 /* Recurse and process this expression */` |
|    235654 | 2130 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    235654 | 2131 | `			 if( rc != SXRET_OK ){` |
|         6 | 2132 | `				 return rc;` |
|         - | 2133 | `			 }` |
|         - | 2134 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|         - | 2135 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|         - | 2136 | `			  * hoist a unary operator that the user explicitly isolated.` |
|         - | 2137 | ``			  * A spread mark on the '(' itself — `...($expr)` flags the paren`` |
|         - | 2138 | `			  * node at extraction — must survive onto the root too, or the` |
|         - | 2139 | `			  * group's free below silently drops the unpacking. */` |
|    235650 | 2140 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    235650 | 2141 | `				 if( apNode[j] ){` |
|    235650 | 2142 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS` |
|    235645 | 2143 | `						 \| (apNode[iLeft]->iFlags & EXPR_NODE_SPREAD);` |
|    235650 | 2144 | `					 break;` |
|         - | 2145 | `				 }` |
|       ! 0 | 2146 | `			 }` |
|    117676 | 2147 | `		 }` |
|         - | 2148 | `		 /* Drop the enclosing delimiters; the extraction set frees them. */` |
|    235652 | 2149 | `		 apNode[iLeft] = 0;` |
|    235652 | 2150 | `		 apNode[iCur] = 0;` |
|    117682 | 2151 | `	 }` |
|         - | 2152 | `	  /* Process expressions enclosed in braces */` |
|  20180803 | 2153 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|         - | 2154 | `		 sxi32 iNest;` |
|         - | 2155 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|         - | 2156 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|         - | 2157 | `		  */` |
|  17403987 | 2158 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  17403801 | 2159 | `			 continue;` |
|         - | 2160 | `		 }` |
|       190 | 2161 | `		 iNest = 1;` |
|       190 | 2162 | `		 iLeft = iCur;` |
|         - | 2163 | `		 /* Find the closing parenthesis */` |
|       190 | 2164 | `		 iCur++;` |
|       382 | 2165 | `		 while( iCur < nToken ){` |
|       382 | 2166 | `			 if( apNode[iCur] ){` |
|       382 | 2167 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|         - | 2168 | `					 /* Decrement nesting level */` |
|       190 | 2169 | `					 iNest--;` |
|       190 | 2170 | `					 if( iNest <= 0 ){` |
|       190 | 2171 | `						 break;` |
|       ! 0 | 2172 | `					 }` |
|       194 | 2173 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|         - | 2174 | `					 /* Increment nesting level */` |
|       ! 0 | 2175 | `					 iNest++;` |
|       ! 0 | 2176 | `				 }` |
|        96 | 2177 | `			 }` |
|       194 | 2178 | `			 iCur++;` |
|         2 | 2179 | `		 }` |
|       190 | 2180 | `		 if( iCur - iLeft > 1 ){` |
|         - | 2181 | `			 /* Recurse and process this expression */` |
|       182 | 2182 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|       182 | 2183 | `			 if( rc != SXRET_OK ){` |
|       ! 0 | 2184 | `				 return rc;` |
|         - | 2185 | `			 }` |
|        90 | 2186 | `		 }` |
|         - | 2187 | `		 /* Drop the enclosing delimiters; the extraction set frees them. */` |
|       190 | 2188 | `		 apNode[iLeft] = 0;` |
|       190 | 2189 | `		 apNode[iCur] = 0;` |
|        97 | 2190 | `	 }` |
|         - | 2191 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   2776821 | 2192 | `	 iLeft = -1;` |
|  20181141 | 2193 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  17404339 | 2194 | `		 if( apNode[iCur] == 0 ){` |
|   7165618 | 2195 | `			 continue;` |
|         - | 2196 | `		 }` |
|  10238726 | 2197 | `		 pNode = apNode[iCur];` |
|  10238726 | 2198 | `		 if( pNode->xCode == PH7_CompileAnnonClass ){` |
|         - | 2199 | ``			 /* PHP 8.4: an anonymous class is as dereferencable as `new C()` --`` |
|         - | 2200 | ``			  * `new class {…}->m()`, `new class(1) {…}->p`, `new class {…}()`.`` |
|         - | 2201 | `			  * Its constructor arguments sit INSIDE the one term ExprAssembleAnnonClass` |
|         - | 2202 | ``			  * delimited, so the call fold below never sees them; fold the `new` in`` |
|         - | 2203 | `			  * front of it here instead, before a trailing postfix operator binds.` |
|         - | 2204 | ``			  * Left for the prefix pass, `->` reached the bare class term and`` |
|         - | 2205 | `			  * refused it as "Expecting a variable as left operand". */` |
|       293 | 2206 | `			 sxi32 iNew = iCur - 1;` |
|       293 | 2207 | `			 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|       ! 0 | 2208 | `				 iNew--;` |
|       ! 0 | 2209 | `			 }` |
|       288 | 2210 | `			 if( iNew >= 0 && apNode[iNew]->pOp` |
|       288 | 2211 | `				 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|       293 | 2212 | `				 && apNode[iNew]->pLeft == 0 ){` |
|       293 | 2213 | `				 apNode[iNew]->pLeft = pNode;` |
|       293 | 2214 | `				 apNode[iCur] = apNode[iNew];` |
|       293 | 2215 | `				 apNode[iNew] = 0;` |
|       293 | 2216 | `				 pNode = apNode[iCur];` |
|       144 | 2217 | `			 }` |
|       144 | 2218 | `		 }` |
|  10238726 | 2219 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   1633959 | 2220 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 2221 | `				 /* Collect function arguments */` |
|   1204382 | 2222 | `				 sxi32 iPtr = 0;` |
|   1204382 | 2223 | `				 sxi32 nFuncTok = 0;` |
|   5931972 | 2224 | `				 while( nFuncTok + iCur < nToken ){` |
|   5931972 | 2225 | `					 ph7_expr_node *pTok = apNode[nFuncTok+iCur];` |
|         - | 2226 | `					 /* Count only raw, unlinked paren tokens. A '(' that has already` |
|         - | 2227 | `					  * been folded into a subtree (a nested call collapsed inside a` |
|         - | 2228 | ``					  * parenthesised group, e.g. `(f())->m()`) still carries the`` |
|         - | 2229 | `					  * LPAREN bit on its pStart while its matching ')' has been` |
|         - | 2230 | `					  * nulled, so counting it here would over-count and never find` |
|         - | 2231 | `					  * this call's own ')'. A processed node has pLeft set. */` |
|   5931972 | 2232 | `					 if( pTok && pTok->pLeft == 0 ){` |
|   5838956 | 2233 | `						 if( pTok->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   1340112 | 2234 | `							 iPtr++;` |
|   5167593 | 2235 | `						 }else if ( pTok->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   1340112 | 2236 | `							 iPtr--;` |
|   1340112 | 2237 | `							 if( iPtr <= 0 ){` |
|   1204382 | 2238 | `								 break;` |
|         - | 2239 | `							 }` |
|     67718 | 2240 | `						 }` |
|   2312783 | 2241 | `					 }` |
|   4727595 | 2242 | `					 nFuncTok++;` |
|         5 | 2243 | `				 }` |
|   1204382 | 2244 | `				 if( nFuncTok + iCur >= nToken ){` |
|         - | 2245 | `					 /* Syntax error */` |
|       ! 0 | 2246 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|       ! 0 | 2247 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2248 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2249 | `					 }` |
|       ! 0 | 2250 | `					 return rc;` |
|         - | 2251 | `				 }` |
|   1204382 | 2252 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|         - | 2253 | `					 /* Syntax error */` |
|       ! 0 | 2254 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|       ! 0 | 2255 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2256 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2257 | `					 }` |
|       ! 0 | 2258 | `					 return rc;` |
|         - | 2259 | `				 }` |
|   1204382 | 2260 | `				 if( nFuncTok > 1 ){` |
|         - | 2261 | `					 /* Process function arguments */` |
|   1158626 | 2262 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   1158626 | 2263 | `					 if( rc != SXRET_OK ){` |
|        11 | 2264 | `						 return rc;` |
|         - | 2265 | `					 }` |
|    578223 | 2266 | `				 }` |
|         - | 2267 | `				 /* Link the node to the tree */` |
|   1204374 | 2268 | `				 pNode->pLeft = apNode[iLeft];` |
|   1204374 | 2269 | `				 apNode[iLeft] = 0;` |
|   5931940 | 2270 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|   4727571 | 2271 | `					 apNode[iCur+iPtr] = 0;` |
|   2359214 | 2272 | `				 }` |
|         - | 2273 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|         - | 2274 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|         - | 2275 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|         - | 2276 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|         - | 2277 | `				  * constructor call into that new-node NOW, before the postfix` |
|         - | 2278 | `				  * operators bind, and relocate the completed new-node onto this` |
|         - | 2279 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|         - | 2280 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|         - | 2281 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|         - | 2282 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|         - | 2283 | `				 {` |
|   1204374 | 2284 | `					 sxi32 iNew = iLeft - 1;` |
|   1257389 | 2285 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     53020 | 2286 | `						 iNew--;` |
|         5 | 2287 | `					 }` |
|   1204369 | 2288 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|    665985 | 2289 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|    388330 | 2290 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    120079 | 2291 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    120079 | 2292 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    120079 | 2293 | `						 apNode[iNew] = 0;` |
|    120079 | 2294 | `						 pNode = apNode[iCur];` |
|     59963 | 2295 | `					 }` |
|         - | 2296 | `				 }` |
|   1030604 | 2297 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|         - | 2298 | `				 /* Subscripting */` |
|    383660 | 2299 | `				 sxi32 iArrTok = iCur + 1;` |
|    383660 | 2300 | `				 sxi32 iNest = 1;` |
|         - | 2301 | ``				 /* php's `dereferencable` list has `'(' expr ')'` in it, so WHATEVER a`` |
|         - | 2302 | `				  * parenthesised group evaluates to may be subscripted:` |
|         - | 2303 | ``				  * `((array)$o)['k']`, `((string)$s)[1]`, `(clone $o)[0]`, `(1+2)[0]`,`` |
|         - | 2304 | ``				  * `(1)[0]`. The base test below is a whitelist of node SHAPES, and no`` |
|         - | 2305 | `				  * shape describes "the user wrote parentheses", so every such group` |
|         - | 2306 | `				  * whose root was not already a term or a postfix chain was refused as` |
|         - | 2307 | `` 				  * `Invalid array name` — a compile fatal on source php runs. The `->` `` |
|         - | 2308 | `				  * branch further down reads the same flag for the same reason. */` |
|    383655 | 2309 | `				 if( !(iLeft >= 0 && apNode[iLeft] && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS))` |
|    383628 | 2310 | `					 && ( iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|        62 | 2311 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|        56 | 2312 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|         - | 2313 | ``					 /* A CONSTANT is a valid subscript base too: `const X=[1,2]; X[0]`.`` |
|         - | 2314 | `					  * It was the one base php accepts that this whitelist omitted, so` |
|         - | 2315 | `					  * subscripting a global constant raised "Invalid array name" while` |
|         - | 2316 | `					  * the class-constant form (C::X[0]) and the via-variable detour both` |
|         - | 2317 | `					  * worked. */` |
|        56 | 2318 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        38 | 2319 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    383589 | 2320 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|         - | 2321 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|         - | 2322 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|      1054 | 2323 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ) ){` |
|         - | 2324 | `						 /* Syntax error */` |
|       ! 0 | 2325 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|       ! 0 | 2326 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2327 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2328 | `						 }` |
|       ! 0 | 2329 | `						 return rc;` |
|         - | 2330 | `				 }` |
|         - | 2331 | `				 /* Collect index tokens */` |
|    690289 | 2332 | `				 while( iArrTok < nToken ){` |
|    690289 | 2333 | `					 if( apNode[iArrTok] ){` |
|    690257 | 2334 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|         - | 2335 | `							 /* Increment nesting level */` |
|        11 | 2336 | `							 iNest++;` |
|    690253 | 2337 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|         - | 2338 | `							 /* Decrement nesting level */` |
|    383668 | 2339 | `							 iNest--;` |
|    383668 | 2340 | `							 if( iNest <= 0 ){` |
|    383660 | 2341 | `								 break;` |
|         - | 2342 | `							 }` |
|         4 | 2343 | `						 }` |
|    153097 | 2344 | `					 }` |
|    306634 | 2345 | `					 ++iArrTok;` |
|         5 | 2346 | `				 }` |
|    383660 | 2347 | `				 if( iArrTok > iCur + 1 ){` |
|         - | 2348 | ``					 /* php: a stray token in a subscript index is `... expecting "]"`. */`` |
|    289002 | 2349 | `					 const char *zSaveIdx = pGen->zClauseCloser;` |
|    289002 | 2350 | `					 pGen->zClauseCloser = "\"]\"";` |
|         - | 2351 | `					 /* Recurse and process this expression */` |
|    289002 | 2352 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    289002 | 2353 | `					 pGen->zClauseCloser = zSaveIdx;` |
|    289002 | 2354 | `					 if( rc != SXRET_OK ){` |
|       ! 0 | 2355 | `						 return rc;` |
|         - | 2356 | `					 }` |
|         - | 2357 | `					 /* Link the node to it's index */` |
|    289002 | 2358 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    144308 | 2359 | `				 }` |
|         - | 2360 | `				 /* Link the node to the tree */` |
|    383660 | 2361 | `				 pNode->pLeft = apNode[iLeft];` |
|    383660 | 2362 | `				 pNode->pRight = 0;` |
|    383660 | 2363 | `				 apNode[iLeft] = 0;` |
|   1073944 | 2364 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|    690289 | 2365 | `					 apNode[iNest] = 0;` |
|    344691 | 2366 | `				 }` |
|    191578 | 2367 | `			 }else{` |
|         - | 2368 | `				 /* Member access operators [i.e: '->','::'] */` |
|     45927 | 2369 | `				  iRight = iCur + 1;` |
|     46107 | 2370 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       182 | 2371 | `					 iRight++;` |
|         2 | 2372 | `				 }` |
|     45927 | 2373 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2374 | `					 /* Syntax error */` |
|         5 | 2375 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|         5 | 2376 | `					 if( rc != SXERR_ABORT ){` |
|         5 | 2377 | `						 rc = SXERR_SYNTAX;` |
|         2 | 2378 | `					 }` |
|         5 | 2379 | `					 return rc;` |
|         - | 2380 | `				 }` |
|         - | 2381 | `				 /* Validate the left operand BEFORE linking it. The refusal below` |
|         - | 2382 | `				  * used to be reached with the operand already installed as` |
|         - | 2383 | `				  * pNode->pLeft AND still standing in apNode[], which the recursive` |
|         - | 2384 | ``				  * release then freed twice -- a heap-use-after-free `1->x;`,`` |
|         - | 2385 | ``				  * `"s"->x;` and `[1]->x;` all reached. Nothing owns a node through`` |
|         - | 2386 | `				  * a tree any more (PH7_ExprFreeTree), so the order is no longer` |
|         - | 2387 | `				  * load-bearing; it is kept because refusing before mutating is the` |
|         - | 2388 | `				  * clearer shape either way. */` |
|     45918 | 2389 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|     42812 | 2390 | `					 && apNode[iLeft]->pOp == 0 &&` |
|     32655 | 2391 | `					 apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|         - | 2392 | ``					 /* A PARENTHESISED group is php's `( expr )` dereferencable: whatever it`` |
|         - | 2393 | ``					  * evaluates to may be reached through `->`, which is how the closure`` |
|         - | 2394 | ``					  * idioms are written — `(function(){ … })->bindTo($o)`,`` |
|         - | 2395 | ``					  * `(fn() => …)->call($o)`, `(match($k){ … })->m()`. PHL refused all of`` |
|         - | 2396 | `					  * them as "Expecting a variable as left operand", a compile fatal on` |
|         - | 2397 | `					  * valid php, because a literal TERM carries no operator. */` |
|        56 | 2398 | `					 (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 &&` |
|         - | 2399 | ``					 /* php's `dereferencable` also covers the SCALAR forms — a quoted`` |
|         - | 2400 | `					  * string (interpolated or not) and an array literal — plus a` |
|         - | 2401 | ``					  * CONSTANT, and it runs them: `"s"->p` warns `Attempt to read`` |
|         - | 2402 | ``					  * property "p" on string` and yields null, `"s"->m()` is the`` |
|         - | 2403 | `					  * member-function Error. Refusing them at COMPILE time killed the` |
|         - | 2404 | `					  * whole file instead. A NUMBER literal and a heredoc stay refused` |
|         - | 2405 | ``					  * — those are php's own parse error for `1->x`. */`` |
|        26 | 2406 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString &&` |
|        24 | 2407 | `					 apNode[iLeft]->xCode != PH7_CompileString &&` |
|        18 | 2408 | `					 apNode[iLeft]->xCode != PH7_CompileLiteral &&` |
|        11 | 2409 | `					 apNode[iLeft]->xCode != PH7_CompileArray &&` |
|         4 | 2410 | `					 apNode[iLeft]->xCode != PH7_CompileShortArray ){` |
|         - | 2411 | `						 /* Syntax error */` |
|         4 | 2412 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         2 | 2413 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|         3 | 2414 | `						 if( rc != SXERR_ABORT ){` |
|         3 | 2415 | `							 rc = SXERR_SYNTAX;` |
|         1 | 2416 | `						 }` |
|         3 | 2417 | `						 return rc;` |
|         - | 2418 | `				 }` |
|         - | 2419 | `				 /* Link the node to the tree */` |
|     45921 | 2420 | `				 pNode->pLeft = apNode[iLeft];` |
|     45921 | 2421 | `				 pNode->pRight = apNode[iRight];` |
|     45921 | 2422 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|         - | 2423 | `			 }` |
|    815534 | 2424 | `		 }` |
|  10238712 | 2425 | `		 iLeft = iCur;` |
|   5111824 | 2426 | `	 }` |
|         - | 2427 | `	 /* Handle the prefix (new, clone) operators. Walk RIGHT to LEFT: both take the` |
|         - | 2428 | `	  * operand on their right, so a nested one has to be linked before the outer` |
|         - | 2429 | ``	  * sees it. Left-to-right, `clone new Q` reached the still-unlinked `new` node —`` |
|         - | 2430 | `	  * not a term yet — and answered php's own valid source with the compile fatal` |
|         - | 2431 | ``	  * "'clone': Expecting class constructor call". `clone new Q()` worked only`` |
|         - | 2432 | `	  * because the postfix pass folds a constructor CALL into its new-node early. */` |
|  20181105 | 2433 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  17404303 | 2434 | `		 if( apNode[iCur] == 0 ){` |
|   8919920 | 2435 | `			 continue;` |
|         - | 2436 | `		 }` |
|   8484388 | 2437 | `		 pNode = apNode[iCur];` |
|   8484388 | 2438 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|         - | 2439 | `			 SyToken *pToken;` |
|         - | 2440 | `			 /* Get the left node */` |
|      3979 | 2441 | `			 iLeft = iCur + 1;` |
|      4135 | 2442 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|       157 | 2443 | `				 iLeft++;` |
|         1 | 2444 | `			 }` |
|      3979 | 2445 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2446 | `				  /* Syntax error */` |
|       ! 0 | 2447 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|       ! 0 | 2448 | `					 &pNode->pOp->sOp);` |
|       ! 0 | 2449 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2450 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2451 | `				 }` |
|       ! 0 | 2452 | `				 return rc;` |
|         - | 2453 | `			 }` |
|         - | 2454 | `			 /* Make sure the operand are of a valid type. CLONE takes ANY expression —` |
|         - | 2455 | ``			  * php's grammar is `clone expr`, and what that expression evaluates to is a`` |
|         - | 2456 | `			  * RUNTIME question: a non-object operand is php's catchable` |
|         - | 2457 | ``			  * `clone(): Argument #1 ($object) must be of type object, %s given`, which`` |
|         - | 2458 | `			  * OP_CLONE already raises. The whitelist that used to sit here (a variable,` |
|         - | 2459 | ``			  * or any operator node) refused `clone 5`, `clone []`, `clone null` and`` |
|         - | 2460 | ``			  * `clone match(…){…}` at COMPILE time — the first three with a diagnostic php`` |
|         - | 2461 | `			  * never prints, the last on source php runs. NEW keeps its own, because its` |
|         - | 2462 | `			  * operand is a class-name REFERENCE, not a value. */` |
|      3979 | 2463 | `			 if( pNode->pOp->iOp != EXPR_OP_CLONE ){` |
|         - | 2464 | `				 /* New */` |
|      3654 | 2465 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|         7 | 2466 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         - | 2467 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|         - | 2468 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|         - | 2469 | `					  * expression (PHP parse error). The postfix pass folds` |
|         - | 2470 | ``					  * `new C()` into a completed term, so guard against the`` |
|         - | 2471 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|         - | 2472 | `					  * (the inner is a parenthesized group). */` |
|       ! 0 | 2473 | `					 pToken = apNode[iLeft]->pStart;` |
|       ! 0 | 2474 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2475 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2476 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2477 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2478 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 2479 | `					 }` |
|       ! 0 | 2480 | `					 return rc;` |
|         - | 2481 | `				 }` |
|      3659 | 2482 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      3655 | 2483 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      3650 | 2484 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|         5 | 2485 | `						 && xCons != PH7_CompileAnnonClass){` |
|       ! 0 | 2486 | `						 pToken = apNode[iLeft]->pStart;` |
|         - | 2487 | `						 /* Syntax error */` |
|       ! 0 | 2488 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2489 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|       ! 0 | 2490 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|       ! 0 | 2491 | `						 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2492 | `							 rc = SXERR_SYNTAX;` |
|       ! 0 | 2493 | `						 }` |
|       ! 0 | 2494 | `						 return rc;` |
|         - | 2495 | `					 }` |
|      1823 | 2496 | `				 }` |
|      1825 | 2497 | `			 }` |
|         - | 2498 | `			  /* Link the node to the tree */` |
|      3979 | 2499 | `			 pNode->pLeft = apNode[iLeft];` |
|      3979 | 2500 | `			 apNode[iLeft] = 0;` |
|      3979 | 2501 | `			 pNode->pRight = 0; /* Paranoid */` |
|      1985 | 2502 | `		 }` |
|   4236177 | 2503 | `	 }` |
|         - | 2504 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   2776807 | 2505 | `	 iLeft = -1;` |
|  20181105 | 2506 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  17404303 | 2507 | `		 if( apNode[iCur] == 0 ){` |
|   8923894 | 2508 | `			 continue;` |
|         - | 2509 | `		 }` |
|   8480414 | 2510 | `		 pNode = apNode[iCur];` |
|   8480414 | 2511 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|     77436 | 2512 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */` |
|         - | 2513 | ``					 /* …but `A::K++` is php's parse error, not an increment of`` |
|         - | 2514 | `					  * class-level storage: a class CONSTANT is not a variable. */` |
|       209 | 2515 | `					 && !PH7_ExprNodeIsClassConst(apNode[iLeft]))` |
|     68699 | 2516 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|         - | 2517 | `					 /* Link the node to the tree */` |
|     68798 | 2518 | `					 pNode->pLeft = apNode[iLeft];` |
|     68798 | 2519 | `					 apNode[iLeft] = 0;` |
|     34350 | 2520 | `			 }` |
|     38612 | 2521 | `		  }` |
|   8480414 | 2522 | `		 iLeft = iCur;` |
|   4234192 | 2523 | `	  }` |
|   2776807 | 2524 | `	 iLeft = -1;` |
|  20181095 | 2525 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  17404297 | 2526 | `		 if( apNode[iCur] == 0 ){` |
|   8992683 | 2527 | `			 continue;` |
|         - | 2528 | `		 }` |
|   8411619 | 2529 | `		 pNode = apNode[iCur];` |
|   8411619 | 2530 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|      8535 | 2531 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|      8538 | 2532 | `				 \|\| ( apNode[iLeft]->pOp && (apNode[iLeft]->pOp->iPrec != 2 /* Postfix */` |
|        28 | 2533 | `					 \|\| PH7_ExprNodeIsClassConst(apNode[iLeft]))) ){` |
|         - | 2534 | `					 /* Not a variable. Nothing to the right at all means the operator` |
|         - | 2535 | `					  * was POSTFIX and its target (already passed over) was refused,` |
|         - | 2536 | `					  * which is where php stops; otherwise this is a PREFIX operator` |
|         - | 2537 | `					  * over a non-variable and php stops past that operand. */` |
|         6 | 2538 | `					 if( iLeft < 0 ){` |
|         3 | 2539 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2540 | `					 }` |
|         3 | 2541 | `					 return PH7_ExprOperandNotAVariable(pGen,apNode[iLeft]);` |
|         - | 2542 | `			 }` |
|         - | 2543 | `			 /* Link the node to the tree */` |
|      8536 | 2544 | `			 pNode->pLeft = apNode[iLeft];` |
|      8536 | 2545 | `			 apNode[iLeft] = 0;` |
|         - | 2546 | `			 /* Mark as pre-increment/decrement node */` |
|      8536 | 2547 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|      4260 | 2548 | `		  }` |
|   8411615 | 2549 | `		 iLeft = iCur;` |
|   4199839 | 2550 | `	 }` |
|         - | 2551 | ``	 /* Link `instanceof` BEFORE the unary(prec 4) pass. PHP gives instanceof a`` |
|         - | 2552 | ``	  * HIGHER precedence than logical NOT, so `!$x instanceof C` parses as`` |
|         - | 2553 | ``	  * `!($x instanceof C)`. instanceof lives in the generic binary pass (prec`` |
|         - | 2554 | ``	  * 7-16) which runs AFTER unary — leaving it there lets `!` capture $x first`` |
|         - | 2555 | ``	  * and yields the wrong `(!$x) instanceof C`. Collapse each `$obj instanceof`` |
|         - | 2556 | ``	  * Class` into a term here (left = object expr, right = class-name term) so a`` |
|         - | 2557 | ``	  * preceding `!`/`~`/cast then operates on the whole subtree. The generic`` |
|         - | 2558 | `	  * pass below skips it (pLeft != 0). */` |
|   2776803 | 2559 | `	 iLeft = -1;` |
|  20181085 | 2560 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  17404287 | 2561 | `		 if( apNode[iCur] == 0 ){` |
|   9018802 | 2562 | `			 continue;` |
|         - | 2563 | `		 }` |
|   8385490 | 2564 | `		 pNode = apNode[iCur];` |
|   8385490 | 2565 | `		 if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_INSTOF && pNode->pLeft == 0 ){` |
|     17597 | 2566 | `			 iRight = iCur + 1;` |
|     17601 | 2567 | `			 while( iRight < nToken && apNode[iRight] == 0 ){` |
|         5 | 2568 | `				 iRight++;` |
|         1 | 2569 | `			 }` |
|     17597 | 2570 | `			 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|       ! 0 | 2571 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2572 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2573 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2574 | `				 }` |
|       ! 0 | 2575 | `				 return rc;` |
|         - | 2576 | `			 }` |
|     17597 | 2577 | `			 pNode->pLeft = apNode[iLeft];` |
|     17597 | 2578 | `			 pNode->pRight = apNode[iRight];` |
|     17597 | 2579 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|      8785 | 2580 | `		 }` |
|   8385490 | 2581 | `		 iLeft = iCur;` |
|   4186793 | 2582 | `	 }` |
|         - | 2583 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   2776803 | 2584 | `	  iLeft = 0;` |
|  20181079 | 2585 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  17404283 | 2586 | `		  if( apNode[iCur] ){` |
|   8367894 | 2587 | `			  pNode = apNode[iCur];` |
|   8367894 | 2588 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    228342 | 2589 | `				  if( iLeft > 0 ){` |
|         - | 2590 | `					  /* Link the node to the tree */` |
|    228340 | 2591 | `					  pNode->pLeft = apNode[iLeft];` |
|    228340 | 2592 | `					  apNode[iLeft] = 0;` |
|    228340 | 2593 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|         - | 2594 | `						  /* "Is the operand a finished subtree?" — a binary node fills` |
|         - | 2595 | `						   * pLeft+pRight and a full ternary fills all three, but a SHORT` |
|         - | 2596 | ``						   * ternary (`a ?: b`, which is what `!($a ?: $b)` hands here)`` |
|         - | 2597 | `						   * fills pCond+pRight and leaves pLeft NULL on purpose. Reading` |
|         - | 2598 | `						   * pLeft alone called it unfinished and refused source php` |
|         - | 2599 | `						   * compiles — every unary and cast over a parenthesised elvis. */` |
|      8682 | 2600 | `						  if( pNode->pLeft->pRight == 0` |
|      8687 | 2601 | `							  \|\| (pNode->pLeft->pLeft == 0 && pNode->pLeft->pCond == 0) ){` |
|         - | 2602 | `							   /* Syntax error */` |
|       ! 0 | 2603 | `							  rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2604 | `							  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2605 | `								  rc = SXERR_SYNTAX;` |
|       ! 0 | 2606 | `							  }` |
|       ! 0 | 2607 | `							  return rc;` |
|         - | 2608 | `						  }` |
|      4335 | 2609 | `					  }` |
|    114011 | 2610 | `				  }else{` |
|         - | 2611 | `					  /* Syntax error */` |
|         3 | 2612 | `					  rc = PH7_GenSyntaxError(pGen,0,0);` |
|         3 | 2613 | `					  if( rc != SXERR_ABORT ){` |
|         3 | 2614 | `						  rc = SXERR_SYNTAX;` |
|         1 | 2615 | `					  }` |
|         3 | 2616 | `					  return rc;` |
|         - | 2617 | `				  }` |
|    114006 | 2618 | `			  }` |
|         - | 2619 | `			  /* Save terminal position */` |
|   8367892 | 2620 | `			  iLeft = iCur;` |
|   4178000 | 2621 | `		  }` |
|   8688469 | 2622 | `	  }` |
|         - | 2623 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|         - | 2624 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|         - | 2625 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|         - | 2626 | `	  * yielding a right-leaning tree. */` |
|  20181077 | 2627 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  17404281 | 2628 | `		 if( apNode[iCur] == 0 ){` |
|   9265331 | 2629 | `			 continue;` |
|         - | 2630 | `		 }` |
|   8138955 | 2631 | `		 pNode = apNode[iCur];` |
|   8138955 | 2632 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|         - | 2633 | `			 sxi32 iL, iR;` |
|         - | 2634 | `			 /* Find the right operand */` |
|       604 | 2635 | `			 iR = -1;` |
|         - | 2636 | `			 {` |
|         - | 2637 | `				 sxi32 j;` |
|      1026 | 2638 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      1026 | 2639 | `					 if( apNode[j] ){ iR = j; break; }` |
|       212 | 2640 | `				 }` |
|         - | 2641 | `			 }` |
|         - | 2642 | `			 /* Find the left operand */` |
|       604 | 2643 | `			 iL = -1;` |
|         - | 2644 | `			 {` |
|         - | 2645 | `				 sxi32 j;` |
|      1176 | 2646 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      1176 | 2647 | `					 if( apNode[j] ){ iL = j; break; }` |
|       287 | 2648 | `				 }` |
|         - | 2649 | `			 }` |
|       604 | 2650 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|       ! 0 | 2651 | `				 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 2652 | `				 if( rc != SXERR_ABORT ){` |
|       ! 0 | 2653 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2654 | `				 }` |
|       ! 0 | 2655 | `				 return rc;` |
|         - | 2656 | `			 }` |
|       604 | 2657 | `			 pNode->pLeft  = apNode[iL];` |
|       604 | 2658 | `			 pNode->pRight = apNode[iR];` |
|       604 | 2659 | `			 apNode[iL] = 0;` |
|       604 | 2660 | `			 apNode[iR] = 0;` |
|         - | 2661 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|         - | 2662 | `			  * The unary phase already attached its operand (pLeft) before` |
|         - | 2663 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|         - | 2664 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|         - | 2665 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|         - | 2666 | `			  * — the outermost unary stays outermost. The error-suppression` |
|         - | 2667 | `			  * operator '@' is treated identically to the other unaries:` |
|         - | 2668 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|         - | 2669 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|         - | 2670 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|         - | 2671 | `			  * operands are respected. */` |
|       602 | 2672 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       354 | 2673 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|        98 | 2674 | `				 && pNode->pLeft->pLeft != 0` |
|        92 | 2675 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        27 | 2676 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|        27 | 2677 | `				 ph7_expr_node *pTail = pHead;` |
|         - | 2678 | `				 /* Walk down to the innermost hoistable unary — the one` |
|         - | 2679 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|        43 | 2680 | `				 while( pTail->pLeft` |
|        34 | 2681 | `					 && pTail->pLeft->pOp` |
|        23 | 2682 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|        12 | 2683 | `					 && pTail->pLeft->pLeft != 0` |
|        30 | 2684 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|         9 | 2685 | `					 pTail = pTail->pLeft;` |
|         1 | 2686 | `				 }` |
|         - | 2687 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|        27 | 2688 | `				 pNode->pLeft = pTail->pLeft;` |
|        27 | 2689 | `				 pTail->pLeft = pNode;` |
|        27 | 2690 | `				 apNode[iCur] = pHead;` |
|        13 | 2691 | `			 }` |
|       301 | 2692 | `		 }` |
|   4063698 | 2693 | `	 }` |
|         - | 2694 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  30544597 | 2695 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  27767825 | 2696 | `		 iLeft = -1;` |
| 201810013 | 2697 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 174042217 | 2698 | `			 if( apNode[iCur] == 0 ){` |
| 111780161 | 2699 | `				 continue;` |
|         - | 2700 | `			 }` |
|  62262061 | 2701 | `			 pNode = apNode[iCur];` |
|  62262061 | 2702 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|   1657429 | 2703 | ``				 ph7_expr_node *pRefUn = 0;      /* `=&` under a prefix unary */`` |
|   1657429 | 2704 | `				 ph7_expr_node *pRefUnOuter = 0;` |
|   1657429 | 2705 | ``				 ph7_expr_node *pRefCmp = 0;     /* comparison to re-hang a `=&` under */`` |
|         - | 2706 | `				 /* Get the right node */` |
|   1657429 | 2707 | `				 iRight = iCur + 1;` |
|   2239926 | 2708 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|    582502 | 2709 | `					 iRight++;` |
|         5 | 2710 | `				 }` |
|   1657429 | 2711 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2712 | `					 /* Syntax error */` |
|        13 | 2713 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|        13 | 2714 | `					 if( rc != SXERR_ABORT ){` |
|        13 | 2715 | `						 rc = SXERR_SYNTAX;` |
|         5 | 2716 | `					 }` |
|        13 | 2717 | `					 return rc;` |
|         - | 2718 | `				 }` |
|   1657419 | 2719 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2720 | `					 sxi32  iTmp;` |
|         - | 2721 | ``					 /* php gives `=&` ASSIGNMENT precedence -- looser than every`` |
|         - | 2722 | ``					  * comparison -- so `null === $x =& $a[$k]` is`` |
|         - | 2723 | ``					  * `null === ($x =& $a[$k])`. This table processes `=&` at 12, one`` |
|         - | 2724 | ``					  * step TIGHTER than `===` at 11, because the operator's own rules`` |
|         - | 2725 | `					  * (the refusals below, the unary hoist, the nullsafe screens) live` |
|         - | 2726 | `					  * in this pass and moving it to 18 would leave all of them behind.` |
|         - | 2727 | `					  * A comparison has therefore already taken the variable this bind` |
|         - | 2728 | ``					  * means, and the bind was left with `(null === $x)` as its target --`` |
|         - | 2729 | `					  * not a variable, so the file was a parse error. symfony/translation` |
|         - | 2730 | `					  * writes exactly this shape, and it cost the whole file.` |
|         - | 2731 | `					  *` |
|         - | 2732 | `					  * Take the comparison's RIGHT operand as the bind target and hang` |
|         - | 2733 | `					  * the finished bind back under it, which is the tree php builds.` |
|         - | 2734 | ``					  * Parenthesised groups are left alone: `($a === $b) =& $c` really is`` |
|         - | 2735 | `					  * a refusal. */` |
|       472 | 2736 | `					 if( iLeft >= 0 && apNode[iLeft] && apNode[iLeft]->pOp` |
|       322 | 2737 | `					  && (apNode[iLeft]->pOp->iPrec == 10 \|\| apNode[iLeft]->pOp->iPrec == 11)` |
|        86 | 2738 | `					  && apNode[iLeft]->pRight != 0` |
|        91 | 2739 | `					  && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       ! 0 | 2740 | `						 pRefCmp = apNode[iLeft];` |
|       ! 0 | 2741 | `						 apNode[iLeft] = pRefCmp->pRight;` |
|       ! 0 | 2742 | `					 }` |
|         - | 2743 | `					 /* Reference operator [i.e: '&=' ]*/` |
|         - | 2744 | ``					 /* A prefix unary covers the whole BIND too — `@$a[0] =& $x` is`` |
|         - | 2745 | ``					  * `@($a[0] =& $x)`, and so are its `-`/`+`/`!`/`~`/cast spellings,`` |
|         - | 2746 | `					  * every one of which php runs. Same hoist the assignment path makes` |
|         - | 2747 | `					  * below, re-wrapped after the operands are swapped and linked. */` |
|         - | 2748 | `					 {` |
|       477 | 2749 | `						 ph7_expr_node *pUn = apNode[iLeft];` |
|       647 | 2750 | `						 while( pUn->pOp && pUn->pLeft` |
|       174 | 2751 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|       416 | 2752 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|       173 | 2753 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|         3 | 2754 | `							 pRefUn = pUn;      /* innermost unary over the bind target */` |
|         3 | 2755 | `							 pUn = pUn->pLeft;` |
|         1 | 2756 | `						 }` |
|       477 | 2757 | `						 if( pRefUn ){` |
|         3 | 2758 | `							 pRefUnOuter = apNode[iLeft]; /* the chain's result node */` |
|         3 | 2759 | `							 apNode[iLeft] = pUn;` |
|         1 | 2760 | `						 }` |
|         - | 2761 | `					 }` |
|         - | 2762 | `					 /* PHP 8.0: a reference and a nullsafe chain do not mix, and php` |
|         - | 2763 | `					  * has a different sentence for each SIDE — the bind target is a` |
|         - | 2764 | `					  * write like any other, while the SOURCE gets a wording of its` |
|         - | 2765 | `					  * own. Both operands are still in written order here; the swap` |
|         - | 2766 | `					  * below turns them over. */` |
|       472 | 2767 | `					 if( PH7_ExprContainsNullsafe(apNode[iLeft])` |
|       476 | 2768 | `					  \|\| PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        14 | 2769 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         8 | 2770 | `							 PH7_ExprContainsNullsafe(apNode[iLeft])` |
|         - | 2771 | `								 ? "Can't use nullsafe operator in write context"` |
|         - | 2772 | `								 : "Cannot take reference of a nullsafe chain");` |
|        10 | 2773 | `						 if( rc != SXERR_ABORT ){` |
|        10 | 2774 | `							 rc = SXERR_SYNTAX;` |
|         4 | 2775 | `						 }` |
|        10 | 2776 | `						 return rc;` |
|         - | 2777 | `					 }` |
|         - | 2778 | ``					 /* A member LHS (`$o->p =& $x`, `self::$s =& $x`) is a valid`` |
|         - | 2779 | `					  * reference target — PH7_ExprIsModifiableValue accepts` |
|         - | 2780 | ``					  * EXPR_OP_ARROW (`->`) and a static-PROPERTY `::`, and rejects`` |
|         - | 2781 | ``					  * both a class CONSTANT and the nullsafe `?->` form, so no extra`` |
|         - | 2782 | `					  * PH7_OP_MEMBER guard is needed here. The runtime member` |
|         - | 2783 | `					  * ref-store is emitted by the STORE_REF codegen below. */` |
|       469 | 2784 | `					 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 2785 | ``						 /* The bind TARGET is not a variable: php stops at the `=`. */`` |
|       ! 0 | 2786 | `						 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 2787 | `					 }` |
|       469 | 2788 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       373 | 2789 | `						 if(  PH7_ExprIsModifiableValue(apNode[iRight]) == FALSE ){` |
|         - | 2790 | ``							 /* The SOURCE has to be a variable too, and php's `&new` /`` |
|         - | 2791 | ``							  * `&clone` legacy productions each stop somewhere of their`` |
|         - | 2792 | ``							  * own: `$r =& clone $o` names the `clone` keyword (nothing`` |
|         - | 2793 | `` 							  * in a `variable` may start with it), while `$r =& new A` `` |
|         - | 2794 | ``							  * enters php 4's `&new` rule, which wants the ARGUMENT`` |
|         - | 2795 | ``							  * list — `expecting "("` — unless one was written, in`` |
|         - | 2796 | `							  * which case the rule completes and php asks for the` |
|         - | 2797 | `							  * dereference like everything else. PHL accepted BOTH` |
|         - | 2798 | `							  * spellings and silently bound a copy. */` |
|         6 | 2799 | `							 if( apNode[iRight]->pOp` |
|         8 | 2800 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_CLONE ){` |
|         3 | 2801 | `								 rc = PH7_GenSyntaxError(pGen,apNode[iRight]->pStart,0);` |
|         3 | 2802 | `								 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2803 | `							 }` |
|         4 | 2804 | `							 if( apNode[iRight]->pOp` |
|         5 | 2805 | `								 && apNode[iRight]->pOp->iOp == EXPR_OP_NEW ){` |
|         3 | 2806 | `								 SyToken *pNMin = 0, *pNMax = 0;` |
|         3 | 2807 | `								 PH7_ExprSubtreeSpan(apNode[iRight],&pNMin,&pNMax);` |
|         2 | 2808 | `								 if( pNMax == 0 \|\| pNMax <= apNode[iRight]->pStart` |
|         3 | 2809 | `								  \|\| (pNMax[-1].nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 2810 | `									 rc = PH7_GenSyntaxError(pGen,` |
|         1 | 2811 | `										 PH7_ExprTokenInStream(pGen,pNMax),"\"(\"");` |
|         3 | 2812 | `									 return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2813 | `								 }` |
|       ! 0 | 2814 | `							 }` |
|         3 | 2815 | `							 return PH7_ExprOperandNotAVariable(pGen,apNode[iRight]);` |
|         - | 2816 | `						 }` |
|       181 | 2817 | `					 }` |
|         - | 2818 | `					 /* Swap operands */` |
|       463 | 2819 | `					 iTmp = iRight;` |
|       463 | 2820 | `					 iRight = iLeft;` |
|       463 | 2821 | `					 iLeft = iTmp;` |
|       229 | 2822 | `				 }` |
|         - | 2823 | `				 /* Link the node to the tree */` |
|   1657405 | 2824 | `				 pNode->pLeft = apNode[iLeft];` |
|   1657405 | 2825 | `				 pNode->pRight = apNode[iRight];` |
|   1657405 | 2826 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   1657405 | 2827 | `				 if( pRefUn ){` |
|         - | 2828 | `					 /* Re-wrap: the unary chain now covers the whole bind. */` |
|         3 | 2829 | `					 pRefUn->pLeft = pNode;` |
|         3 | 2830 | `					 apNode[iCur] = pRefUnOuter;` |
|         1 | 2831 | `				 }` |
|   1657405 | 2832 | `				 if( pRefCmp ){` |
|         - | 2833 | `					 /* Re-hang: the comparison now compares against the bind's value. */` |
|       ! 0 | 2834 | `					 pRefCmp->pRight = apNode[iCur] ? apNode[iCur] : pNode;` |
|       ! 0 | 2835 | `					 apNode[iCur] = pRefCmp;` |
|       ! 0 | 2836 | `				 }` |
|    827586 | 2837 | `			 }` |
|  62262037 | 2838 | `			 iLeft = iCur;` |
|  31086095 | 2839 | `		 }` |
|  13862113 | 2840 | `	 }` |
|         - | 2841 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|         - | 2842 | `	  * Note that we do not need a precedence loop here since` |
|         - | 2843 | `	  * we are dealing with a single operator.` |
|         - | 2844 | `	  */` |
|   2776777 | 2845 | `	  iLeft = -1;` |
|  19847999 | 2846 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  17118417 | 2847 | `		  if( apNode[iCur] == 0 ){` |
|  12436373 | 2848 | `			  continue;` |
|         - | 2849 | `		  }` |
|   4682049 | 2850 | `		  pNode = apNode[iCur];` |
|         - | 2851 | ``		  /* `pLeft == 0` alone does NOT mean "not linked yet" for this operator: a`` |
|         - | 2852 | ``		   * SHORT ternary (`a ?: b`) leaves pLeft NULL on purpose and records its`` |
|         - | 2853 | `		   * operands in pCond/pRight. A completed elvis node sitting in this slot —` |
|         - | 2854 | ``		   * which is what a parenthesised group leaves behind, `($a ?: $b)` — was`` |
|         - | 2855 | `		   * therefore re-entered here, and the term to its left is whatever the` |
|         - | 2856 | ``		   * enclosing expression put there (the `=` of `$x = ($a ?: $b);`), so the`` |
|         - | 2857 | `		   * "missing condition" branch fired on source php compiles. pCond is the` |
|         - | 2858 | `		   * real linked/not-linked marker, and the nesting scan below already` |
|         - | 2859 | `		   * reads it that way. */` |
|   4682044 | 2860 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0` |
|     47375 | 2861 | `			  && pNode->pCond == 0 ){` |
|     47195 | 2862 | `			  sxi32 iNest = 1;` |
|     47195 | 2863 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2864 | `				  /* Missing condition */` |
|         6 | 2865 | `				  rc = PH7_GenSyntaxError(pGen,pNode->pStart,0);` |
|         6 | 2866 | `				  if( rc != SXERR_ABORT ){` |
|         6 | 2867 | `					  rc = SXERR_SYNTAX;` |
|         2 | 2868 | `				  }` |
|         6 | 2869 | `				  return rc;` |
|         - | 2870 | `			  }` |
|         - | 2871 | `			  /* Get the right node */` |
|     47191 | 2872 | `			  iRight = iCur + 1;` |
|    158281 | 2873 | `			  while( iRight < nToken  ){` |
|    158281 | 2874 | `				  if( apNode[iRight] ){` |
|     94215 | 2875 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|         - | 2876 | `						  /* Increment nesting level */` |
|         3 | 2877 | `						  ++iNest;` |
|     94214 | 2878 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|         - | 2879 | `						  /* Decrement nesting level */` |
|     47193 | 2880 | `						  --iNest;` |
|     47193 | 2881 | `						  if( iNest <= 0 ){` |
|     47191 | 2882 | `							  break;` |
|         - | 2883 | `						  }` |
|         1 | 2884 | `					  }` |
|     23482 | 2885 | `				  }` |
|    111095 | 2886 | `				  iRight++;` |
|         5 | 2887 | `			  }` |
|     47191 | 2888 | `			  if( iRight > iCur + 1 ){` |
|         - | 2889 | `				  /* Recurse and process the then expression */` |
|     47021 | 2890 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     47021 | 2891 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2892 | `					  return rc;` |
|         - | 2893 | `				  }` |
|         - | 2894 | `				  /* Link the node to the tree */` |
|     47021 | 2895 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     23478 | 2896 | `			  }else{` |
|         - | 2897 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|         - | 2898 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|         - | 2899 | `			  }` |
|     47191 | 2900 | `			  apNode[iCur + 1] = 0;` |
|     47191 | 2901 | `			  if( iRight + 1 < nToken ){` |
|         - | 2902 | `				  /* Recurse and process the else expression */` |
|     47191 | 2903 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     47191 | 2904 | `				  if( rc != SXRET_OK ){` |
|       ! 0 | 2905 | `					  return rc;` |
|         - | 2906 | `				  }` |
|         - | 2907 | `				  /* Link the node to the tree */` |
|     47191 | 2908 | `				  pNode->pRight = apNode[iRight + 1];` |
|     47191 | 2909 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     23568 | 2910 | `			  }else{` |
|       ! 0 | 2911 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|       ! 0 | 2912 | `				  if( rc != SXERR_ABORT ){` |
|       ! 0 | 2913 | `					 rc = SXERR_SYNTAX;` |
|       ! 0 | 2914 | `				 }` |
|       ! 0 | 2915 | `				 return rc;` |
|         - | 2916 | `			  }` |
|         - | 2917 | `			  /* Point to the condition */` |
|     47191 | 2918 | `			  pNode->pCond  = apNode[iLeft];` |
|     47191 | 2919 | `			  apNode[iLeft] = 0;` |
|     47191 | 2920 | `			  break;` |
|         - | 2921 | `		  }` |
|   4634859 | 2922 | `		  iLeft = iCur;` |
|   2313998 | 2923 | `	  }` |
|         - | 2924 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|         - | 2925 | `	  * Note: All right associative binary operators have precedence 18` |
|         - | 2926 | `	  * so there is no need for a precedence loop here.` |
|         - | 2927 | `	  */` |
|   2776773 | 2928 | `	 iRight = -1;` |
|  20180577 | 2929 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  17403941 | 2930 | `		 if( apNode[iCur] == 0 ){` |
|  13698068 | 2931 | `			 continue;` |
|         - | 2932 | `		 }` |
|   3705878 | 2933 | `		 pNode = apNode[iCur];` |
|   3705878 | 2934 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|         - | 2935 | `			 /* Get the left node */` |
|    928984 | 2936 | `			 iLeft = iCur - 1;` |
|   1249356 | 2937 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|    320377 | 2938 | `				 iLeft--;` |
|         5 | 2939 | `			 }` |
|    928984 | 2940 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 2941 | `				 /* Syntax error */` |
|        98 | 2942 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|         - | 2943 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|         8 | 2944 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|         4 | 2945 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|         4 | 2946 | `				 }else{` |
|        94 | 2947 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|         - | 2948 | `				 }` |
|        98 | 2949 | `				 if( rc != SXERR_ABORT ){` |
|        96 | 2950 | `					 rc = SXERR_SYNTAX;` |
|        46 | 2951 | `				 }` |
|        98 | 2952 | `				 return rc;` |
|         - | 2953 | `			 }` |
|         - | 2954 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|         - | 2955 | `			  * including deeper chains like $a?->b->c = 1 and` |
|         - | 2956 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|         - | 2957 | ``			  * chain still contains a `?->` that cannot participate in`` |
|         - | 2958 | `			  * a write.` |
|         - | 2959 | `			  *` |
|         - | 2960 | ``			  * The ACCESS CHAIN only. php binds `A op $lv = B` as `A op ($lv = B)`,`` |
|         - | 2961 | ``			  * so in `$o?->m() !== null && $x = 1` the target is `$x` and the`` |
|         - | 2962 | `			  * nullsafe belongs to a comparison that is merely READ -- a walk of the` |
|         - | 2963 | `			  * whole left operand refused that shape, which aws-sdk-php writes and so` |
|         - | 2964 | `			  * does anything that tests a nullsafe call and captures a value in one` |
|         - | 2965 | `			  * condition. When the left operand is such a NEIGHBOUR the spine walk` |
|         - | 2966 | `			  * below settles the target and asks the same question of it. */` |
|    928890 | 2967 | `			 if( ExprChainHasNullsafe(apNode[iLeft]) ){` |
|        28 | 2968 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 2969 | `					 "Can't use nullsafe operator in write context");` |
|        28 | 2970 | `				 if( rc != SXERR_ABORT ){` |
|        28 | 2971 | `					 rc = SXERR_SYNTAX;` |
|        12 | 2972 | `				 }` |
|        28 | 2973 | `				 return rc;` |
|         - | 2974 | `			 }` |
|         - | 2975 | `			 /* Every PREFIX unary in php covers the whole assignment rather than just` |
|         - | 2976 | ``			  * its target: `@$x = expr` is `@($x = expr)`, and so are `-$x = 5`,`` |
|         - | 2977 | ``			  * `+$x = 5`, `!$x = 5`, `~$x = 5`, `(int)$x = 5` and `clone $o = 5` —`` |
|         - | 2978 | `			  * every one of them RUNS in php, writing $x and then applying the` |
|         - | 2979 | `			  * operator to the result. The unary phase has already bound the operator` |
|         - | 2980 | `			  * to the LHS, which leaves the assignment staring at a non-lvalue, so` |
|         - | 2981 | `			  * walk down to the innermost operand, let the assignment bind THERE, and` |
|         - | 2982 | ``			  * re-wrap below. Only `@` was handled here, so the other eight spellings`` |
|         - | 2983 | ``			  * did not compile at all. A PARENTHESISED operand (`(-$x) = 5`) is a`` |
|         - | 2984 | ``			  * genuine non-lvalue and stays refused, and `new` keeps its own`` |
|         - | 2985 | `			  * production, where php refuses too.` |
|         - | 2986 | `			  * Same shape as the '**'-beneath-unary hoist further up. */` |
|    928866 | 2987 | `			 pSuppress = 0;` |
|    928866 | 2988 | `			 pUnOuter = 0;` |
|         - | 2989 | `			 {` |
|    928866 | 2990 | `				 ph7_expr_node *pUn = apNode[iLeft];` |
|   1137308 | 2991 | `				 while( pUn->pOp && pUn->pLeft` |
|    208426 | 2992 | `					 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|    673510 | 2993 | `					 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|    208405 | 2994 | `					  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        43 | 2995 | `					 pSuppress = pUn;  /* innermost unary seen so far */` |
|        43 | 2996 | `					 pUn = pUn->pLeft;` |
|         1 | 2997 | `				 }` |
|    928866 | 2998 | `				 if( pSuppress ){` |
|        37 | 2999 | `					 pUnOuter = apNode[iLeft]; /* the chain's result node */` |
|        37 | 3000 | `					 apNode[iLeft] = pUn;` |
|        18 | 3001 | `				 }` |
|         - | 3002 | `			 }` |
|    928866 | 3003 | `			 if( PH7_ExprIsModifiableValue(apNode[iLeft]) == FALSE ){` |
|         - | 3004 | ``				 /* php binds `A op $lv = B` as `A op ($lv = B)`: assignment takes the`` |
|         - | 3005 | `				  * immediate lvalue on its left, not the whole binary subtree. When the` |
|         - | 3006 | `				  * left operand is a (non-lvalue) binary/comparison/logical subtree, walk` |
|         - | 3007 | `				  * its right spine down to the deepest modifiable lvalue and reparent the` |
|         - | 3008 | `				  * assignment there, leaving the binary operator as the outer node.` |
|         - | 3009 | ``				  * Covers the very common `while (false !== $p = strpos(...))` idiom. */`` |
|       459 | 3010 | `				 if( pSuppress == 0 && apNode[iLeft]->pOp && apNode[iLeft]->pRight ){` |
|        45 | 3011 | `					 ph7_expr_node *pHost = apNode[iLeft];` |
|        45 | 3012 | `					 ph7_expr_node *pParent = pHost;` |
|        66 | 3013 | `					 while( pParent->pRight && pParent->pRight->pOp && pParent->pRight->pRight` |
|        13 | 3014 | `						 && ExprIsAccessChainRoot(pParent->pRight) == FALSE` |
|        27 | 3015 | `						 && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|       ! 0 | 3016 | `						 pParent = pParent->pRight;` |
|       ! 0 | 3017 | `					 }` |
|         - | 3018 | ``					 /* The spine ends on the target php would take. A `?->` in ITS chain`` |
|         - | 3019 | `					  * is the write refusal the head of this function makes for a target` |
|         - | 3020 | ``					  * standing alone -- `$q && $a?->b = 1` is `$q && ($a?->b = 1)`. */`` |
|        45 | 3021 | `					 if( ExprChainHasNullsafe(pParent->pRight) ){` |
|         4 | 3022 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|         - | 3023 | `							 "Can't use nullsafe operator in write context");` |
|         4 | 3024 | `						 if( rc != SXERR_ABORT ){` |
|         4 | 3025 | `							 rc = SXERR_SYNTAX;` |
|         2 | 3026 | `						 }` |
|         4 | 3027 | `						 return rc;` |
|         - | 3028 | `					 }` |
|         - | 3029 | `					 /* The spine may end on a PREFIX unary rather than on the lvalue` |
|         - | 3030 | ``					  * itself -- `c && !$d = f()`, which php reads as`` |
|         - | 3031 | ``					  * `c && !($d = f())` exactly as it reads the unparenthesised`` |
|         - | 3032 | ``					  * `!$d = f()`. Walk that chain down the same way the top-level`` |
|         - | 3033 | `					  * hoist above does and let the assignment bind at its innermost` |
|         - | 3034 | `					  * operand, leaving the unary wrapped around the assignment. The` |
|         - | 3035 | `					  * spine walk stopped at the unary (a unary node has no pRight),` |
|         - | 3036 | ``					  * so the whole shape was `syntax error, unexpected token "="`. */`` |
|        41 | 3037 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight) == FALSE ){` |
|        21 | 3038 | `						 ph7_expr_node *pUn = pParent->pRight;` |
|        21 | 3039 | `						 ph7_expr_node *pInnerUn = 0;` |
|        42 | 3040 | `						 while( pUn->pOp && pUn->pLeft` |
|        16 | 3041 | `							 && (pUn->iFlags & EXPR_NODE_PARENS) == 0` |
|        36 | 3042 | `							 && (pUn->pOp->iPrec == 4 /* -, +, !, ~, @, (cast) */` |
|         8 | 3043 | `							  \|\| pUn->pOp->iOp == EXPR_OP_CLONE) ){` |
|        17 | 3044 | `							 pInnerUn = pUn;` |
|        17 | 3045 | `							 pUn = pUn->pLeft;` |
|         1 | 3046 | `						 }` |
|        18 | 3047 | `						 if( pInnerUn && PH7_ExprIsModifiableValue(pUn)` |
|        17 | 3048 | `							 && PH7_ExprContainsNullsafe(pUn) == 0 ){` |
|        15 | 3049 | `							 pNode->pLeft = apNode[iRight]; /* assignment RHS value */` |
|        15 | 3050 | `							 pNode->pRight = pUn;           /* the extracted lvalue */` |
|        15 | 3051 | `							 pInnerUn->pLeft = pNode;       /* the unary chain now covers it */` |
|        15 | 3052 | `							 apNode[iCur] = pHost;` |
|        15 | 3053 | `							 apNode[iLeft] = apNode[iRight] = 0;` |
|        15 | 3054 | `							 iRight = iCur;` |
|        15 | 3055 | `							 continue;` |
|         - | 3056 | `						 }` |
|         2 | 3057 | `					 }` |
|        24 | 3058 | `					 if( pParent->pRight && PH7_ExprIsModifiableValue(pParent->pRight)` |
|        25 | 3059 | `						 && PH7_ExprContainsNullsafe(pParent->pRight) == 0 ){` |
|        21 | 3060 | `						 pNode->pLeft = apNode[iRight];   /* assignment RHS value */` |
|        21 | 3061 | `						 pNode->pRight = pParent->pRight; /* the extracted lvalue */` |
|        21 | 3062 | `						 pParent->pRight = pNode;         /* assignment becomes the op's right child */` |
|        21 | 3063 | `						 apNode[iCur] = pHost;            /* the binary subtree root is the result */` |
|        21 | 3064 | `						 apNode[iLeft] = apNode[iRight] = 0;` |
|        21 | 3065 | `						 iRight = iCur;` |
|        21 | 3066 | `						 continue;` |
|         - | 3067 | `					 }` |
|         2 | 3068 | `				 }` |
|       598 | 3069 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       412 | 3070 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|         - | 3071 | ``					 /* The target is not a `variable` in php's grammar: php stops at`` |
|         - | 3072 | `					  * the assignment operator itself, whatever the target was. */` |
|        14 | 3073 | `					 return ExprWriteTargetNotAVariable(pGen,pNode);` |
|         - | 3074 | `				 }` |
|       203 | 3075 | `			 }` |
|         - | 3076 | `			 /* Link the node to the tree (Reverse) */` |
|    928818 | 3077 | `			 pNode->pLeft = apNode[iRight];` |
|    928818 | 3078 | `			 pNode->pRight = apNode[iLeft];` |
|    928818 | 3079 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|    928818 | 3080 | `			 if( pSuppress ){` |
|         - | 3081 | `				 /* Re-wrap: the unary chain now covers the whole assignment. */` |
|        37 | 3082 | `				 pSuppress->pLeft = pNode;` |
|        37 | 3083 | `				 apNode[iCur] = pUnOuter;` |
|        18 | 3084 | `			 }` |
|    463779 | 3085 | `		 }` |
|   3705712 | 3086 | `		 iRight = iCur;` |
|   1850052 | 3087 | `	 }` |
|         - | 3088 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  13883185 | 3089 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  11106549 | 3090 | `		 iLeft = -1;` |
|  80721605 | 3091 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  69615061 | 3092 | `			 if( apNode[iCur] == 0 ){` |
|  58507915 | 3093 | `				 continue;` |
|         - | 3094 | `			 }` |
|  11107151 | 3095 | `			 pNode = apNode[iCur];` |
|  11107151 | 3096 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|         - | 3097 | `				 /* Get the right node */` |
|        71 | 3098 | `				 iRight = iCur + 1;` |
|        95 | 3099 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        26 | 3100 | `					 iRight++;` |
|         2 | 3101 | `				 }` |
|        71 | 3102 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|         - | 3103 | `					 /* Syntax error */` |
|       ! 0 | 3104 | `					 rc = PH7_GenSyntaxError(pGen,0,0);` |
|       ! 0 | 3105 | `					 if( rc != SXERR_ABORT ){` |
|       ! 0 | 3106 | `						 rc = SXERR_SYNTAX;` |
|       ! 0 | 3107 | `					 }` |
|       ! 0 | 3108 | `					 return rc;` |
|         - | 3109 | `				 }` |
|         - | 3110 | `				 /* Link the node to the tree */` |
|        71 | 3111 | `				 pNode->pLeft = apNode[iLeft];` |
|        71 | 3112 | `				 pNode->pRight = apNode[iRight];` |
|        71 | 3113 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        33 | 3114 | `			 }` |
|  11107151 | 3115 | `			 iLeft = iCur;` |
|   5544862 | 3116 | `		 }` |
|   5544561 | 3117 | `	 }` |
|         - | 3118 | `	 /* Point to the root of the expression tree */` |
|  17403675 | 3119 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  14627117 | 3120 | `		 if( apNode[iCur] ){` |
|   2683722 | 3121 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|         - | 3122 | ``				 /* Name the START of the stray subtree (`$i<3` -> `$i`), not the`` |
|         - | 3123 | `				  * operator sitting at its slot. The "expecting" clause is the closer` |
|         - | 3124 | ``				  * the enclosing construct set (`;` after `return`, `,`/`;` after`` |
|         - | 3125 | ``				  * `echo`, `)` for a for() post clause …); a for() clause defaults to`` |
|         - | 3126 | ``				  * `;` when nothing more specific was set. php prints no clause for a`` |
|         - | 3127 | `				  * plain expression statement, so a NULL closer stays clauseless. */` |
|        83 | 3128 | `				 SyToken *pBadTok = ExprSubtreeFirstToken(apNode[iCur]);` |
|        83 | 3129 | `				 const char *zExpect = pGen->zClauseCloser;` |
|        83 | 3130 | `				 if( zExpect == 0 && pGen->nCommaExprOk > 0 ){` |
|       ! 0 | 3131 | `					 zExpect = "\";\"";` |
|       ! 0 | 3132 | `				 }` |
|        83 | 3133 | `				 rc = PH7_GenSyntaxError(pGen,pBadTok ? pBadTok : apNode[iCur]->pStart,zExpect);` |
|        83 | 3134 | `				  if( rc != SXERR_ABORT ){` |
|        83 | 3135 | `					  rc = SXERR_SYNTAX;` |
|        39 | 3136 | `				  }` |
|        83 | 3137 | `				  return rc;` |
|         - | 3138 | `			 }` |
|   2683644 | 3139 | `			 apNode[0] = apNode[iCur];` |
|   2683644 | 3140 | `			 apNode[iCur] = 0;` |
|   1339715 | 3141 | `		 }` |
|   7302027 | 3142 | `	 }` |
|   2776563 | 3143 | `	 return SXRET_OK;` |
|   2613168 | 3144 | ` }` |
|         - | 3145 | ` /*` |
|         - | 3146 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|         - | 3147 | `  * If successful, the root of the tree is stored in ppRoot.` |
|         - | 3148 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|         - | 3149 | `  * This is the public interface used by the most code generator routines.` |
|         - | 3150 | `  */` |
|   2908112 | 3151 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|         5 | 3152 | `{` |
|         - | 3153 | `	ph7_expr_node *aStack[EXPR_STACK_NODES];` |
|         - | 3154 | `	ph7_expr_node **apNode;` |
|         - | 3155 | `	ph7_expr_node *pNode;` |
|         - | 3156 | `	sxu32 nNode;` |
|         - | 3157 | `	sxi32 rc;` |
|         - | 3158 | `	/* Reset node container */` |
|   2908117 | 3159 | `	SySetReset(pExprNode);` |
|   2908117 | 3160 | `	pNode = 0; /* Prevent compiler warning */` |
|         - | 3161 | `	/* Extract nodes one after one until we hit the end of the input */` |
|         - | 3162 | `	{` |
|   2908117 | 3163 | `		int iLastWasTerm = 0;` |
|   2908117 | 3164 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|   2908117 | 3165 | `		ph7_expr_node *pPrev = 0;` |
|  17934072 | 3166 | `		while( pGen->pIn < pGen->pEnd ){` |
|  15026056 | 3167 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  15026056 | 3168 | `			if( rc != SXRET_OK ){` |
|        85 | 3169 | `				return rc;` |
|         - | 3170 | `			}` |
|  15025971 | 3171 | `			if( pNode->xCode == PH7_CompileLiteral && pNode->pEnd == &pNode->pStart[1]` |
|   1852456 | 3172 | `			 && (pNode->pStart->nType & PH7_TK_KEYWORD)` |
|    934563 | 3173 | `			 && (pNode->pStart->nType & PH7_TK_MEMBER_NAME) == 0` |
|     20265 | 3174 | `			 && SX_PTR_TO_INT(pNode->pStart->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 3175 | ``				/* php's grammar takes a bare `static` in an expression in three`` |
|         - | 3176 | ``				 * places only: before `::`, and as the class of `new` and of`` |
|         - | 3177 | ``				 * `instanceof` (the closure forms were taken above). Anywhere else`` |
|         - | 3178 | ``				 * its parser names the token after it and asks for the `::` --`` |
|         - | 3179 | ``				 * `$x = static;` is a parse error there, where this went on to run`` |
|         - | 3180 | `				 * and failed at runtime on an undefined constant "static". */` |
|       305 | 3181 | `				int bOk = 0;` |
|       305 | 3182 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) ){` |
|       239 | 3183 | `					const ph7_expr_op *pOp = (const ph7_expr_op *)pGen->pIn->pUserData;` |
|       239 | 3184 | `					bOk = ( pOp && pOp->iOp == EXPR_OP_DC );` |
|       117 | 3185 | `				}` |
|       305 | 3186 | `				if( !bOk && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         - | 3187 | ``					/* `f(static: true)` -- a NAMED ARGUMENT may carry any semi-reserved`` |
|         - | 3188 | ``					 * word as its label, `static` included, and the label is the`` |
|         - | 3189 | `					 * keyword token followed by a single colon. */` |
|         3 | 3190 | `					bOk = 1;` |
|         1 | 3191 | `				}` |
|       300 | 3192 | `				if( !bOk && pPrev && pPrev->pOp` |
|        62 | 3193 | `				 && (pPrev->pOp->iOp == EXPR_OP_NEW \|\| pPrev->pOp->iOp == EXPR_OP_INSTOF) ){` |
|        57 | 3194 | `					bOk = 1;` |
|        26 | 3195 | `				}` |
|       305 | 3196 | `				if( !bOk ){` |
|        18 | 3197 | `					rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        18 | 3198 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        18 | 3199 | `					return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 3200 | `				}` |
|       142 | 3201 | `			}` |
|  15025960 | 3202 | `			pPrev = pNode;` |
|         - | 3203 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  15025960 | 3204 | `			if( pNode->xCode ){` |
|         - | 3205 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|   7651688 | 3206 | `				iLastWasTerm = 1;` |
|  11194068 | 3207 | `			}else if( pNode->pOp ){` |
|         - | 3208 | `				/* Operator node */` |
|   4061248 | 3209 | `				iLastWasTerm = 0;` |
|   2027639 | 3210 | `			}else{` |
|         - | 3211 | `				/* Delimiter: ')' and ']' end terms */` |
|   3313034 | 3212 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|         - | 3213 | `			}` |
|         - | 3214 | `			/* A keyword in the next node is a member name only right after a member` |
|         - | 3215 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|         - | 3216 | `			 * node kind, so this single test covers all branches. */` |
|  15025960 | 3217 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|         - | 3218 | `			/* Save the extracted node */` |
|  15025960 | 3219 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|         5 | 3220 | `		}` |
|         - | 3221 | `	}` |
|   2908021 | 3222 | `	if( SySetUsed(pExprNode) < 1 ){` |
|         - | 3223 | `		/* Empty expression [i.e: A semi-colon;] */` |
|       ! 0 | 3224 | `		*ppRoot = 0;` |
|       ! 0 | 3225 | `		return SXRET_OK;` |
|         - | 3226 | `	}` |
|         - | 3227 | `	/* Tree building CONSUMES its array -- every slot it folds into a tree, and` |
|         - | 3228 | `	 * every delimiter a fold swallows, is nulled -- so it cannot run on the set` |
|         - | 3229 | `	 * that OWNS the nodes (PH7_ExprFreeTree). It gets a copy, and the copy dies` |
|         - | 3230 | `	 * here: nothing downstream reads it, because a fold copies the node POINTER` |
|         - | 3231 | `	 * into pLeft/pRight/pCond or into the operator's aNodeArgs. An expression of` |
|         - | 3232 | `	 * up to EXPR_STACK_NODES tokens -- which is nearly all of them -- borrows the` |
|         - | 3233 | `	 * copy from this frame and allocates nothing at all. */` |
|   2908021 | 3234 | `	nNode = SySetUsed(pExprNode);` |
|   2908021 | 3235 | `	apNode = aStack;` |
|   2908021 | 3236 | `	if( nNode > EXPR_STACK_NODES ){` |
|       119 | 3237 | `		apNode = (ph7_expr_node **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,` |
|        38 | 3238 | `			nNode * sizeof(ph7_expr_node *));` |
|        81 | 3239 | `		if( apNode == 0 ){` |
|       ! 0 | 3240 | `			*ppRoot = 0;` |
|       ! 0 | 3241 | `			return SXERR_MEM;` |
|         - | 3242 | `		}` |
|        38 | 3243 | `	}` |
|   2908021 | 3244 | `	SyMemcpy(SySetBasePtr(pExprNode),(void *)apNode,nNode * sizeof(ph7_expr_node *));` |
|         - | 3245 | `	/* Make sure we are dealing with valid nodes */` |
|   2908021 | 3246 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)nNode);` |
|   2908021 | 3247 | `	if( rc == SXRET_OK ){` |
|         - | 3248 | `		/* Build the tree */` |
|   2907881 | 3249 | `		rc = ExprMakeTree(&(*pGen),apNode,(sxi32)nNode);` |
|   1451630 | 3250 | `	}` |
|         - | 3251 | `	/* On a syntax error the nodes stay where they are: the extraction set still` |
|         - | 3252 | `	 * holds every one of them and the caller releases it. */` |
|   2908021 | 3253 | `	*ppRoot = (rc == SXRET_OK) ? apNode[0] : 0;` |
|   2908021 | 3254 | `	if( apNode != aStack ){` |
|        81 | 3255 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,apNode);` |
|        38 | 3256 | `	}` |
|   2908021 | 3257 | `	return rc;` |
|   1451753 | 3258 | `}` |
|         - | 3259 |  |
