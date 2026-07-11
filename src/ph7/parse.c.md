# src/ph7/parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1147/1317 lines (87.09%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * This file implement a hand-coded, thread-safe, full-reentrant and highly-efficient` |
|        - |    9 | ` * expression parser for the PH7 engine.` |
|        - |   10 | ` * Besides from the one introudced by PHP (Over 60), the PH7 engine have introduced three new` |
|        - |   11 | ` * operators. These are 'eq', 'ne' and the comma operator ','.` |
|        - |   12 | ` * The eq and ne operators are borrowed from the Perl world. They are used for strict` |
|        - |   13 | ` * string comparison. The reason why they have been implemented in the PH7 engine` |
|        - |   14 | ` * and introduced as an extension to the PHP programming language is due to the confusion` |
|        - |   15 | ` * introduced by the standard PHP comparison operators ('==' or '===') especially if you` |
|        - |   16 | ` * are comparing strings with numbers.` |
|        - |   17 | ` * Take the following example:` |
|        - |   18 | ` * var_dump( 0xFF == '255' ); // bool(true) ???` |
|        - |   19 | ` * // use the type equal operator by adding a single space to one of the operand` |
|        - |   20 | ` * var_dump( '255  ' === '255' ); //bool(true) depending on the PHP version` |
|        - |   21 | ` * That is, if one of the operand looks like a number (either integer or float) then PHP` |
|        - |   22 | ` * will internally convert the two operands to numbers and then a numeric comparison is performed.` |
|        - |   23 | ` * This is what the PHP language reference manual says:` |
|        - |   24 | ` * If you compare a number with a string or the comparison involves numerical strings, then each` |
|        - |   25 | ` * string is converted to a number and the comparison performed numerically.` |
|        - |   26 | ` * Bummer, if you ask me,this is broken, badly broken. I mean,the programmer cannot dictate` |
|        - |   27 | ` * it's comparison rule, it's the underlying engine who decides in it's place and perform` |
|        - |   28 | ` * the internal conversion. In most cases,PHP developers wants simple string comparison and they` |
|        - |   29 | ` * are stuck to use the ugly and inefficient strcmp() function and it's variants instead.` |
|        - |   30 | ` * This is the big reason why we have introduced these two operators.` |
|        - |   31 | ` * The eq operator is used to compare two strings byte per byte. If you came from the C/C++ world` |
|        - |   32 | ` * think of this operator as a barebone implementation of the memcmp() C standard library function.` |
|        - |   33 | ` * Keep in mind that if you are comparing two ASCII strings then the capital letters and their lowercase` |
|        - |   34 | ` * letters are completely different and so this example will output false.` |
|        - |   35 | ` * var_dump('allo' eq 'Allo'); //bool(FALSE)` |
|        - |   36 | ` * The ne operator perform the opposite operation of the eq operator and is used to test for string` |
|        - |   37 | ` * inequality. This example will output true` |
|        - |   38 | ` * var_dump('allo' ne 'Allo'); //bool(TRUE) unequal strings` |
|        - |   39 | ` * The eq operator return a Boolean true if and only if the two strings are identical while the` |
|        - |   40 | ` * ne operator return a Boolean true if and only if the two strings are different. Otherwise` |
|        - |   41 | ` * a Boolean false is returned (equal strings).` |
|        - |   42 | ` * Note that the comparison is performed only if the two strings are of the same length.` |
|        - |   43 | ` * Otherwise the eq and ne operators return a Boolean false without performing any comparison` |
|        - |   44 | ` * and avoid us wasting CPU time for nothing.` |
|        - |   45 | ` * Again remember that we talk about a low level byte per byte comparison and nothing else.` |
|        - |   46 | ` * Also remember that zero length strings are always equal.` |
|        - |   47 | ` *` |
|        - |   48 | ` * Again, another powerful mechanism borrowed from the C/C++ world and introduced as an extension` |
|        - |   49 | ` * to the PHP programming language.` |
|        - |   50 | ` * A comma expression contains two operands of any type separated by a comma and has left-to-right` |
|        - |   51 | ` * associativity. The left operand is fully evaluated, possibly producing side effects, and its` |
|        - |   52 | ` * value, if there is one, is discarded. The right operand is then evaluated. The type and value` |
|        - |   53 | ` * of the result of a comma expression are those of its right operand, after the usual unary conversions.` |
|        - |   54 | ` * Any number of expressions separated by commas can form a single expression because the comma operator` |
|        - |   55 | ` * is associative. The use of the comma operator guarantees that the sub-expressions will be evaluated` |
|        - |   56 | ` * in left-to-right order, and the value of the last becomes the value of the entire expression.` |
|        - |   57 | ` * The following example assign the value 25 to the variable $a, multiply the value of $a with 2` |
|        - |   58 | ` * and assign the result to variable $b and finally we call a test function to output the value` |
|        - |   59 | ` * of $a and $b. Keep-in mind that all theses operations are done in a single expression using` |
|        - |   60 | ` * the comma operator to create side effect.` |
|        - |   61 | ` * $a = 25,$b = $a << 1 ,test();` |
|        - |   62 | ` * //Output the value of $a and $b` |
|        - |   63 | ` * function test(){` |
|        - |   64 | ` *	 global $a,$b;` |
|        - |   65 | ` *	 echo "\$a = $a \$b= $b\n"; // You should see: $a = 25 $b = 50` |
|        - |   66 | ` * }` |
|        - |   67 | ` *` |
|        - |   68 | ` * For a full discussions on these extensions, please refer to  offical` |
|        - |   69 | ` * documentation(http://ph7.symisc.net/features.html) or visit the offical forums` |
|        - |   70 | ` * (http://forums.symisc.net/) if you want to share your point of view.` |
|        - |   71 | ` *` |
|        - |   72 | ` * Exprressions: According to the PHP language reference manual` |
|        - |   73 | ` *` |
|        - |   74 | ` * Expressions are the most important building stones of PHP. In PHP, almost anything you write is an expression.` |
|        - |   75 | ` * The simplest yet most accurate way to define an expression is "anything that has a value".` |
|        - |   76 | ` * The most basic forms of expressions are constants and variables. When you type "$a = 5", you're assigning` |
|        - |   77 | ` * '5' into $a. '5', obviously, has the value 5, or in other words '5' is an expression with the value of 5` |
|        - |   78 | ` * (in this case, '5' is an integer constant).` |
|        - |   79 | ` * After this assignment, you'd expect $a's value to be 5 as well, so if you wrote $b = $a, you'd expect` |
|        - |   80 | ` * it to behave just as if you wrote $b = 5. In other words, $a is an expression with the value of 5 as well.` |
|        - |   81 | ` * If everything works right, this is exactly what will happen.` |
|        - |   82 | ` * Slightly more complex examples for expressions are functions. For instance, consider the following function:` |
|        - |   83 | ` * <?php` |
|        - |   84 | ` * function foo ()` |
|        - |   85 | ` * {` |
|        - |   86 | ` *   return 5;` |
|        - |   87 | ` * }` |
|        - |   88 | ` * ?>` |
|        - |   89 | ` * Assuming you're familiar with the concept of functions (if you're not, take a look at the chapter about functions)` |
|        - |   90 | ` * you'd assume that typing $c = foo() is essentially just like writing $c = 5, and you're right.` |
|        - |   91 | ` * Functions are expressions with the value of their return value. Since foo() returns 5, the value of the expression` |
|        - |   92 | ` * 'foo()' is 5. Usually functions don't just return a static value but compute something.` |
|        - |   93 | ` * Of course, values in PHP don't have to be integers, and very often they aren't.` |
|        - |   94 | ` * PHP supports four scalar value types: integer values, floating point values (float), string values and boolean values` |
|        - |   95 | ` * (scalar values are values that you can't 'break' into smaller pieces, unlike arrays, for instance).` |
|        - |   96 | ` * PHP also supports two composite (non-scalar) types: arrays and objects. Each of these value types can be assigned` |
|        - |   97 | ` * into variables or returned from functions.` |
|        - |   98 | ` * PHP takes expressions much further, in the same way many other languages do. PHP is an expression-oriented language` |
|        - |   99 | ` * in the sense that almost everything is an expression. Consider the example we've already dealt with, '$a = 5'.` |
|        - |  100 | ` * It's easy to see that there are two values involved here, the value of the integer constant '5', and the value` |
|        - |  101 | ` * of $a which is being updated to 5 as well. But the truth is that there's one additional value involved here` |
|        - |  102 | ` * and that's the value of the assignment itself. The assignment itself evaluates to the assigned value, in this case 5.` |
|        - |  103 | ` * In practice, it means that '$a = 5', regardless of what it does, is an expression with the value 5. Thus, writing` |
|        - |  104 | ` * something like '$b = ($a = 5)' is like writing '$a = 5; $b = 5;' (a semicolon marks the end of a statement).` |
|        - |  105 | ` * Since assignments are parsed in a right to left order, you can also write '$b = $a = 5'.` |
|        - |  106 | ` * Another good example of expression orientation is pre- and post-increment and decrement.` |
|        - |  107 | ` * Users of PHP and many other languages may be familiar with the notation of variable++ and variable--.` |
|        - |  108 | ` * These are increment and decrement operators. In PHP, like in C, there are two types of increment - pre-increment` |
|        - |  109 | ` * and post-increment. Both pre-increment and post-increment essentially increment the variable, and the effect` |
|        - |  110 | ` * on the variable is identical. The difference is with the value of the increment expression. Pre-increment, which is written` |
|        - |  111 | ` * '++$variable', evaluates to the incremented value (PHP increments the variable before reading its value, thus the name 'pre-increment').` |
|        - |  112 | ` * Post-increment, which is written '$variable++' evaluates to the original value of $variable, before it was incremented` |
|        - |  113 | ` * (PHP increments the variable after reading its value, thus the name 'post-increment').` |
|        - |  114 | ` * A very common type of expressions are comparison expressions. These expressions evaluate to either FALSE or TRUE.` |
|        - |  115 | ` * PHP supports > (bigger than), >= (bigger than or equal to), == (equal), != (not equal), < (smaller than) and <= (smaller than or equal to).` |
|        - |  116 | ` * The language also supports a set of strict equivalence operators: === (equal to and same type) and !== (not equal to or not same type).` |
|        - |  117 | ` * These expressions are most commonly used inside conditional execution, such as if statements.` |
|        - |  118 | ` * The last example of expressions we'll deal with here is combined operator-assignment expressions.` |
|        - |  119 | ` * You already know that if you want to increment $a by 1, you can simply write '$a++' or '++$a'.` |
|        - |  120 | ` * But what if you want to add more than one to it, for instance 3? You could write '$a++' multiple times, but this is obviously not a very` |
|        - |  121 | ` * efficient or comfortable way. A much more common practice is to write '$a = $a + 3'. '$a + 3' evaluates to the value of $a plus 3` |
|        - |  122 | ` * and is assigned back into $a, which results in incrementing $a by 3. In PHP, as in several other languages like C, you can write` |
|        - |  123 | ` * this in a shorter way, which with time would become clearer and quicker to understand as well. Adding 3 to the current value of $a` |
|        - |  124 | ` * can be written '$a += 3'. This means exactly "take the value of $a, add 3 to it, and assign it back into $a".` |
|        - |  125 | ` * In addition to being shorter and clearer, this also results in faster execution. The value of '$a += 3', like the value of a regular` |
|        - |  126 | ` * assignment, is the assigned value. Notice that it is NOT 3, but the combined value of $a plus 3 (this is the value that's assigned into $a).` |
|        - |  127 | ` * Any two-place operator can be used in this operator-assignment mode, for example '$a -= 5' (subtract 5 from the value of $a), '$b *= 7'` |
|        - |  128 | ` * (multiply the value of $b by 7), etc.` |
|        - |  129 | ` * There is one more expression that may seem odd if you haven't seen it in other languages, the ternary conditional operator:` |
|        - |  130 | ` * <?php` |
|        - |  131 | ` * $first ? $second : $third` |
|        - |  132 | ` * ?>` |
|        - |  133 | ` * If the value of the first subexpression is TRUE (non-zero), then the second subexpression is evaluated, and that is the result` |
|        - |  134 | ` * of the conditional expression. Otherwise, the third subexpression is evaluated, and that is the value.` |
|        - |  135 | ` */` |
|        - |  136 | `/* Operators associativity */` |
|        - |  137 | `#define EXPR_OP_ASSOC_LEFT   0x01 /* Left associative operator */` |
|        - |  138 | `#define EXPR_OP_ASSOC_RIGHT  0x02 /* Right associative operator */` |
|        - |  139 | `#define EXPR_OP_NON_ASSOC    0x04 /* Non-associative operator */` |
|        - |  140 | `/*` |
|        - |  141 | ` * Operators table` |
|        - |  142 | ` * This table is sorted by operators priority (highest to lowest) according` |
|        - |  143 | ` * the PHP language reference manual.` |
|        - |  144 | ` * PH7 implements all the 60 PHP operators and have introduced the eq and ne operators.` |
|        - |  145 | ` * The operators precedence table have been improved dramatically so that you can do same` |
|        - |  146 | ` * amazing things now such as array dereferencing,on the fly function call,anonymous function` |
|        - |  147 | ` * as array values,class member access on instantiation and so on.` |
|        - |  148 | ` * Refer to the following page for a full discussion on these improvements:` |
|        - |  149 | ` * http://ph7.symisc.net/features.html#improved_precedence` |
|        - |  150 | ` */` |
|        - |  151 | `static const ph7_expr_op aOpTable[] = {` |
|        - |  152 | `	/* Precedence 1: non-associative */` |
|        - |  153 | `	{ {"new",sizeof("new")-1},     EXPR_OP_NEW,   1, EXPR_OP_NON_ASSOC, PH7_OP_NEW  },` |
|        - |  154 | `	{ {"clone",sizeof("clone")-1}, EXPR_OP_CLONE, 1, EXPR_OP_NON_ASSOC, PH7_OP_CLONE},` |
|        - |  155 | `	                              /* Postfix operators */` |
|        - |  156 | `	/* Precedence 2(Highest),left-associative */` |
|        - |  157 | `	{ {"->",sizeof(char)*2}, EXPR_OP_ARROW,     2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|        - |  158 | `	{ {"?->",sizeof(char)*3},EXPR_OP_NULLSAFE_ARROW, 2, EXPR_OP_ASSOC_LEFT, PH7_OP_MEMBER},` |
|        - |  159 | `	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},` |
|        - |  160 | `	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},` |
|        - |  161 | `	/* Precedence 3,non-associative  */` |
|        - |  162 | `	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},` |
|        - |  163 | `	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},` |
|        - |  164 | `	                              /* Unary operators */` |
|        - |  165 | `	/* Precedence 4,right-associative  */` |
|        - |  166 | `	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },` |
|        - |  167 | `	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },` |
|        - |  168 | `	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },` |
|        - |  169 | `	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },` |
|        - |  170 | `	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},` |
|        - |  171 | `	                             /* Cast operators */` |
|        - |  172 | `	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },` |
|        - |  173 | `	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },` |
|        - |  174 | `	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },` |
|        - |  175 | `	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },` |
|        - |  176 | `	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},` |
|        - |  177 | `	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },` |
|        - |  178 | `	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },` |
|        - |  179 | `	                           /* Binary operators */` |
|        - |  180 | `	/* Precedence 5,right-associative: exponentiation (PHP 5.6) */` |
|        - |  181 | `	{ {"**",sizeof(char)*2}, EXPR_OP_POW, 5, EXPR_OP_ASSOC_RIGHT, PH7_OP_POW},` |
|        - |  182 | `	/* Precedence 7,left-associative */` |
|        - |  183 | `	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},` |
|        - |  184 | `	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},` |
|        - |  185 | `	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},` |
|        - |  186 | `	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},` |
|        - |  187 | `	/* Precedence 8,left-associative */` |
|        - |  188 | `	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},` |
|        - |  189 | `	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},` |
|        - |  190 | `	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},` |
|        - |  191 | `	/* Precedence 9,left-associative */` |
|        - |  192 | `	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},` |
|        - |  193 | `	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},` |
|        - |  194 | ``	/* PHP 8.5 pipe operator: `$x \|> f(...)` desugars to `f($x)`. It binds`` |
|        - |  195 | `	 * looser than shift/arithmetic and tighter than comparison — PHP places it` |
|        - |  196 | `	 * between precedence 9 and 10. We share level 9 (left-associative) so the` |
|        - |  197 | `	 * generic binary tree-builder links it correctly; the actual codegen is` |
|        - |  198 | `	 * custom (a one-argument call of the RHS callable), handled in` |
|        - |  199 | `	 * GenStateEmitExprCode. iVmOp is 0 like the other codegen-only operators. */` |
|        - |  200 | `	{ {"\|>",sizeof(char)*2}, EXPR_OP_PIPE, 9, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  201 | `	/* Precedence 10,non-associative */` |
|        - |  202 | `	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},` |
|        - |  203 | `	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},` |
|        - |  204 | `	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},` |
|        - |  205 | `	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},` |
|        - |  206 | `	{ {"<=>",sizeof(char)*3},EXPR_OP_SPACESHIP, 10, EXPR_OP_NON_ASSOC, PH7_OP_SPACESHIP},` |
|        - |  207 | `	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  208 | `	/* Precedence 11,non-associative */` |
|        - |  209 | `	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},` |
|        - |  210 | `	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},` |
|        - |  211 | `	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},` |
|        - |  212 | `	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},` |
|        - |  213 | `	/* Precedence 12,left-associative */` |
|        - |  214 | `	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},` |
|        - |  215 | `	/* Precedence 12,left-associative */` |
|        - |  216 | `	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},` |
|        - |  217 | `	                         /* Binary operators */` |
|        - |  218 | `	/* Precedence 13,left-associative */` |
|        - |  219 | `	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},` |
|        - |  220 | `	/* Precedence 14,left-associative */` |
|        - |  221 | `	{ {"\|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},` |
|        - |  222 | `	/* Precedence 15,left-associative */` |
|        - |  223 | `	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  224 | `	/* Precedence 16,left-associative */` |
|        - |  225 | `	{ {"\|\|",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  226 | `	                      /* Null coalescing operator */` |
|        - |  227 | `	/* Precedence 16 (same as \|\|),right-associative */` |
|        - |  228 | `	{ {"??",sizeof(char)*2}, EXPR_OP_NULLC,  16, EXPR_OP_ASSOC_RIGHT, 0 /* short-circuit, handled in codegen */},` |
|        - |  229 | `	                      /* Ternary operator */` |
|        - |  230 | `	/* Precedence 17,left-associative */` |
|        - |  231 | `    { {"?",sizeof(char)},    EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},` |
|        - |  232 | `	                     /* Combined binary operators */` |
|        - |  233 | `	/* Precedence 18,right-associative */` |
|        - |  234 | `	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},` |
|        - |  235 | `	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },` |
|        - |  236 | `	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },` |
|        - |  237 | `	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },` |
|        - |  238 | `	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },` |
|        - |  239 | `	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },` |
|        - |  240 | `	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },` |
|        - |  241 | `	{ {"**=",sizeof(char)*3}, EXPR_OP_POW_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_POW_STORE },` |
|        - |  242 | `	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },` |
|        - |  243 | `	{ {"\|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },` |
|        - |  244 | `	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },` |
|        - |  245 | `	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },` |
|        - |  246 | `	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },` |
|        - |  247 | `	/* The escape in the literal below avoids the C trigraph for two question` |
|        - |  248 | `	 * marks followed by '=' (which preprocesses to '#'). Do not collapse it` |
|        - |  249 | `	 * back to a raw three-char literal — under -Wtrigraphs the build will` |
|        - |  250 | `	 * either warn or be rewritten silently. The same applies anywhere else` |
|        - |  251 | `	 * in this file: keep one of the question marks escaped. */` |
|        - |  252 | `	{ {"?\?=",sizeof(char)*3},EXPR_OP_NULLC_ASSIGN,18, EXPR_OP_ASSOC_RIGHT, PH7_OP_NULLC_STORE },` |
|        - |  253 | `	/* Precedence 19,left-associative */` |
|        - |  254 | `	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},` |
|        - |  255 | `	/* Precedence 20,left-associative */` |
|        - |  256 | `	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},` |
|        - |  257 | `	/* Precedence 21,left-associative */` |
|        - |  258 | `	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},` |
|        - |  259 | `	/* Precedence 22,left-associative [Lowest operator] */` |
|        - |  260 | `	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */` |
|        - |  261 | `};` |
|        - |  262 | `/* Function call operator need special handling */` |
|        - |  263 | `static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};` |
|        - |  264 | `/*` |
|        - |  265 | ` * Check if the given token is a potential operator or not.` |
|        - |  266 | ` * This function is called by the lexer each time it extract a token that may` |
|        - |  267 | ` * look like an operator.` |
|        - |  268 | ` * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.` |
|        - |  269 | ` * Otherwise NULL.` |
|        - |  270 | ` * Note that the function take care of handling ambiguity [i.e: whether we are dealing with` |
|        - |  271 | ` * a binary minus or unary minus.]` |
|        - |  272 | ` */` |
|  1214584 |  273 | `PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)` |
|        5 |  274 | `{` |
|  1214589 |  275 | `	sxu32 n = 0;` |
|        - |  276 | `	sxi32 rc;` |
|        - |  277 | `	/* Do a linear lookup on the operators table */` |
| 21212375 |  278 | `	for(;;){` |
| 42424755 |  279 | `		if( n >= SX_ARRAYSIZE(aOpTable) ){` |
|      ! 0 |  280 | `			break;` |
|        - |  281 | `		}` |
| 42424755 |  282 | `		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){` |
|        - |  283 | `			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */` |
|  3723451 |  284 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);` |
|  1861728 |  285 | `		}else{` |
| 38701309 |  286 | `			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);` |
|        - |  287 | `		}` |
| 42424755 |  288 | `		if( rc == 0 ){` |
|  1219231 |  289 | `			if( aOpTable[n].sOp.nByte != sizeof(char) \|\| (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) \|\| pLast == 0 ){` |
|        - |  290 | `				/* There is no ambiguity here,simply return the first operator seen */` |
|  1214133 |  291 | `				return &aOpTable[n];` |
|        - |  292 | `			}` |
|        - |  293 | `			/* Handle ambiguity */` |
|     5103 |  294 | `			if( pLast->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_COLON/*:*/\|PH7_TK_COMMA/*,'*/) ){` |
|        - |  295 | `				/* Unary opertors have prcedence here over binary operators */` |
|      341 |  296 | `				return &aOpTable[n];` |
|        - |  297 | `			}` |
|     4767 |  298 | `			if( pLast->nType & PH7_TK_OP ){` |
|      131 |  299 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;` |
|        - |  300 | `				/* Ticket 1433-31: Handle the '++','--' operators case */` |
|      131 |  301 | `				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){` |
|        - |  302 | `					/* Unary opertors have prcedence here over binary operators */` |
|      123 |  303 | `					return &aOpTable[n];` |
|        - |  304 | `				}` |
|        - |  305 |  |
|        4 |  306 | `			}` |
|     2321 |  307 | `		}` |
| 41210171 |  308 | `		++n; /* Next operator in the table */` |
|        5 |  309 | `	}` |
|        - |  310 | `	/* No such operator */` |
|      ! 0 |  311 | `	return 0;` |
|   607297 |  312 | `}` |
|        - |  313 | `/*` |
|        - |  314 | ` * Delimit a set of token stream.` |
|        - |  315 | ` * This function take care of handling the nesting level and stops when it hit` |
|        - |  316 | ` * the end of the input or the ending token is found and the nesting level is zero.` |
|        - |  317 | ` */` |
|   743098 |  318 | `PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)` |
|        5 |  319 | `{` |
|   743103 |  320 | `	SyToken *pCur = pIn;` |
|   743103 |  321 | `	sxi32 iNest = 1;` |
|  4145961 |  322 | `	for(;;){` |
|  8291927 |  323 | `		if( pCur >= pEnd ){` |
|      471 |  324 | `			break;` |
|        - |  325 | `		}` |
|  8291461 |  326 | `		if( pCur->nType & nTokStart ){` |
|        - |  327 | `			/* Increment nesting level */` |
|   390915 |  328 | `			iNest++;` |
|  8096006 |  329 | `		}else if( pCur->nType & nTokEnd ){` |
|        - |  330 | `			/* Decrement nesting level */` |
|  1133547 |  331 | `			iNest--;` |
|  1133547 |  332 | `			if( iNest <= 0 ){` |
|   742637 |  333 | `				break;` |
|        - |  334 | `			}` |
|   195455 |  335 | `		}` |
|        - |  336 | `		/* Advance cursor */` |
|  7548829 |  337 | `		pCur++;` |
|        5 |  338 | `	}` |
|        - |  339 | `	/* Point to the end of the chunk */` |
|   743103 |  340 | `	*ppEnd = pCur;` |
|   743103 |  341 | `}` |
|        - |  342 | `/*` |
|        - |  343 | ` * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.` |
|        - |  344 | ` * Note on reserved keywords.` |
|        - |  345 | ` *  According to the PHP language reference manual:` |
|        - |  346 | ` *   These words have special meaning in PHP. Some of them represent things which look like` |
|        - |  347 | ` *   functions, some look like constants, and so on--but they're not, really: they are language` |
|        - |  348 | ` *   constructs. You cannot use any of the following words as constants, class names, function` |
|        - |  349 | ` *   or method names. Using them as variable names is generally OK, but could lead to confusion.` |
|        - |  350 | ` */` |
|    24078 |  351 | `PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)` |
|        5 |  352 | `{` |
|    36042 |  353 | `	if( nKeyID == PH7_TKWRD_ECHO \|\| nKeyID == PH7_TKWRD_PRINT \|\| nKeyID == PH7_TKWRD_INCLUDE` |
|    23980 |  354 | `		\|\| nKeyID == PH7_TKWRD_INCONCE \|\| nKeyID == PH7_TKWRD_REQUIRE \|\| nKeyID == PH7_TKWRD_REQONCE` |
|        - |  355 | `		){` |
|      167 |  356 | `			return TRUE;` |
|        - |  357 | `	}` |
|    23921 |  358 | `	if( bCheckFunc ){` |
|      512 |  359 | `		if(  nKeyID == PH7_TKWRD_ISSET \|\| nKeyID == PH7_TKWRD_UNSET \|\| nKeyID == PH7_TKWRD_EVAL` |
|      349 |  360 | `			\|\| nKeyID == PH7_TKWRD_EMPTY \|\| nKeyID == PH7_TKWRD_ARRAY \|\| nKeyID == PH7_TKWRD_LIST` |
|      331 |  361 | `			\|\| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW \|\| nKeyID == PH7_TKWRD_CLONE  ){` |
|       49 |  362 | `				return TRUE;` |
|        - |  363 | `		}` |
|      156 |  364 | `	}` |
|        - |  365 | `	/* Not a language construct */` |
|    23877 |  366 | `	return FALSE;` |
|    12044 |  367 | `}` |
|        - |  368 | `/*` |
|        - |  369 | ` * Make sure we are dealing with a valid expression tree.` |
|        - |  370 | ` * This function check for balanced parenthesis,braces,brackets and so on.` |
|        - |  371 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  372 | ` * Return SXRET_OK on success. Any other return value indicates syntax error.` |
|        - |  373 | ` */` |
|  1024374 |  374 | `static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)` |
|        5 |  375 | `{` |
|        - |  376 | `	sxi32 iParen,iSquare,iQuesty,iBraces;` |
|        - |  377 | `	sxi32 i,rc;` |
|        - |  378 |  |
|  1024379 |  379 | `	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD \|\| apNode[0]->pOp->iOp == EXPR_OP_SUB) ){` |
|        - |  380 | `		/* Fix and mark as an unary not binary plus/minus operator */` |
|       32 |  381 | `		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);` |
|       32 |  382 | `		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;` |
|       15 |  383 | `	}` |
|  1024379 |  384 | `	iParen = iSquare = iQuesty = iBraces = 0;` |
|  5552295 |  385 | `	for( i = 0 ; i < nNode ; ++i ){` |
|  4527955 |  386 | `		if( apNode[i]->xCode == PH7_CompileShortArray \|\| apNode[i]->xCode == PH7_CompileShortList ){` |
|        - |  387 | `			/* Short array/list literal: brackets are self-contained, skip */` |
|     1291 |  388 | `			continue;` |
|        - |  389 | `		}` |
|  4526669 |  390 | `		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){` |
|   514655 |  391 | `			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral \|\|` |
|    24104 |  392 | `				(apNode[i - 1]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/))) ){` |
|        - |  393 | `					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */` |
|   482233 |  394 | `					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){` |
|        - |  395 | `						/* We are dealing with a postfix [i.e: function call]  operator` |
|        - |  396 | `						 * not a simple left parenthesis. Mark the node.` |
|        - |  397 | `						 */` |
|   482233 |  398 | `						apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|   482233 |  399 | `						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */` |
|   482233 |  400 | `						apNode[i]->pOp = &sFCallOp;` |
|   241114 |  401 | `					}` |
|   241114 |  402 | `			}` |
|   514655 |  403 | `			iParen++;` |
|  4269344 |  404 | `		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){` |
|   514655 |  405 | `			if( iParen <= 0 ){` |
|       16 |  406 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");` |
|       16 |  407 | `				if( rc != SXERR_ABORT ){` |
|       16 |  408 | `					rc = SXERR_SYNTAX;` |
|        6 |  409 | `				}` |
|       16 |  410 | `				return rc;` |
|        - |  411 | `			}` |
|   514643 |  412 | `			iParen--;` |
|  3754688 |  413 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){` |
|    98369 |  414 | `			iSquare++;` |
|  3448187 |  415 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|    98383 |  416 | `			if( iSquare <= 0 ){` |
|        8 |  417 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");` |
|        8 |  418 | `				if( rc != SXERR_ABORT ){` |
|        8 |  419 | `					rc = SXERR_SYNTAX;` |
|        3 |  420 | `				}` |
|        8 |  421 | `				return rc;` |
|        - |  422 | `			}` |
|    98377 |  423 | `			iSquare--;` |
|  3349813 |  424 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){` |
|       20 |  425 | `			iBraces++;` |
|       20 |  426 | `			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable \|\| (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){` |
|        - |  427 | `				const ph7_expr_op *pOp,*pEnd;` |
|       11 |  428 | `				int iNest = 1;` |
|       11 |  429 | `				sxi32 j=i+1;` |
|        - |  430 | `				/*` |
|        - |  431 | `				 * Dirty Hack: $a{'x'} == > $a['x']` |
|        - |  432 | `				 */` |
|       11 |  433 | `				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;` |
|       11 |  434 | `				apNode[i]->pStart->nType \|= PH7_TK_OSB /*'['*/;` |
|       11 |  435 | `				pOp = aOpTable;` |
|       11 |  436 | `				pEnd = aOpTable + SX_ARRAYSIZE(aOpTable);` |
|       61 |  437 | `				while( pOp < pEnd ){` |
|       61 |  438 | `					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|       11 |  439 | `						break;` |
|        - |  440 | `					}` |
|       51 |  441 | `					pOp++;` |
|        1 |  442 | `				}` |
|       11 |  443 | `				if( pOp >= pEnd ){` |
|      ! 0 |  444 | `					pOp = 0;` |
|      ! 0 |  445 | `				}` |
|       11 |  446 | `				if( pOp ){` |
|       11 |  447 | `					apNode[i]->pOp = pOp;` |
|       11 |  448 | `					apNode[i]->pStart->nType \|= PH7_TK_OP;` |
|        5 |  449 | `				}` |
|       11 |  450 | `				iBraces--;` |
|       11 |  451 | `				iSquare++;` |
|       21 |  452 | `				while( j < nNode ){` |
|       21 |  453 | `					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){` |
|        - |  454 | `						/* Increment nesting level */` |
|      ! 0 |  455 | `						iNest++;` |
|       21 |  456 | `					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){` |
|        - |  457 | `						/* Decrement nesting level */` |
|       11 |  458 | `						iNest--;` |
|       11 |  459 | `						if( iNest < 1 ){` |
|       11 |  460 | `							break;` |
|        - |  461 | `						}` |
|      ! 0 |  462 | `					}` |
|       11 |  463 | `					j++;` |
|        1 |  464 | `				}` |
|       11 |  465 | `				if( j < nNode ){` |
|       11 |  466 | `					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;` |
|       11 |  467 | `					apNode[j]->pStart->nType \|= PH7_TK_CSB /*']'*/;` |
|        5 |  468 | `				}` |
|        - |  469 |  |
|        7 |  470 | `			}` |
|  3300618 |  471 | `		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){` |
|       24 |  472 | `			if( iBraces <= 0 ){` |
|       15 |  473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");` |
|       15 |  474 | `				if( rc != SXERR_ABORT ){` |
|       15 |  475 | `					rc = SXERR_SYNTAX;` |
|        6 |  476 | `				}` |
|       15 |  477 | `				return rc;` |
|        - |  478 | `			}` |
|       10 |  479 | `			iBraces--;` |
|  3300593 |  480 | `		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){` |
|     2907 |  481 | `			if( iQuesty > 0 ){` |
|     2653 |  482 | `				iQuesty--;` |
|     1583 |  483 | `			}else if( iParen <= 0 ){` |
|        - |  484 | `				/* Colon outside parentheses with no matching '?' — syntax error.` |
|        - |  485 | `				 * Colons inside parentheses may be named arguments (name: value)` |
|        - |  486 | `				 * and are validated later by ExprProcessFuncArguments. */` |
|        6 |  487 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");` |
|        6 |  488 | `				if( rc != SXERR_ABORT ){` |
|        6 |  489 | `					rc = SXERR_SYNTAX;` |
|        2 |  490 | `				}` |
|        6 |  491 | `				return rc;` |
|        5 |  492 | `			}` |
|  3299136 |  493 | `		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){` |
|   924429 |  494 | `			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;` |
|   924429 |  495 | `			if( pOp->iOp == EXPR_OP_QUESTY ){` |
|     2655 |  496 | `				iQuesty++;` |
|   923104 |  497 | `			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS \|\| pOp->iOp == EXPR_OP_UPLUS)){` |
|      401 |  498 | `				if( apNode[i-1]->xCode == PH7_CompileVariable \|\| apNode[i-1]->xCode == PH7_CompileLiteral ){` |
|        9 |  499 | `					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */` |
|        9 |  500 | `					sxu32 n = 0;` |
|        9 |  501 | `					if( pOp->iOp == EXPR_OP_UPLUS ){` |
|        5 |  502 | `						iExprOp = EXPR_OP_ADD; /* Binary plus */` |
|        2 |  503 | `					}` |
|        - |  504 | `					/*` |
|        - |  505 | `					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses` |
|        - |  506 | `					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].` |
|        - |  507 | `					 */` |
|      213 |  508 | `					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){` |
|      205 |  509 | `						++n;` |
|        1 |  510 | `					}` |
|        9 |  511 | `					pOp = &aOpTable[n];` |
|        - |  512 | `					/* Mark as binary '+' or '-',not an unary */` |
|        9 |  513 | `					apNode[i]->pOp = pOp;` |
|        9 |  514 | `					apNode[i]->pStart->pUserData = (void *)pOp;` |
|        4 |  515 | `				}` |
|      198 |  516 | `			}` |
|   462212 |  517 | `		}` |
|  2263320 |  518 | `	}` |
|  1024345 |  519 | `	if( iParen != 0 \|\| iSquare != 0 \|\| iQuesty != 0 \|\| iBraces != 0){` |
|       20 |  520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");` |
|       20 |  521 | `		if( rc != SXERR_ABORT ){` |
|       20 |  522 | `			rc = SXERR_SYNTAX;` |
|        8 |  523 | `		}` |
|       20 |  524 | `		return rc;` |
|        - |  525 | `	}` |
|  1024329 |  526 | `	return SXRET_OK;` |
|   512192 |  527 | `}` |
|        - |  528 | `/*` |
|        - |  529 | ` * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const]` |
|        - |  530 | ` * or a simple literal [i.e: PHP_EOL].` |
|        - |  531 | ` */` |
|   848954 |  532 | `static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)` |
|        5 |  533 | `{` |
|   848959 |  534 | `	SyToken *pIn = *ppCur;` |
|        - |  535 | `	/* Jump the first literal seen */` |
|   848959 |  536 | `	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){` |
|   845083 |  537 | `		pIn++;` |
|   422539 |  538 | `	}` |
|   426442 |  539 | `	for(;;){` |
|   852889 |  540 | `		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|     3935 |  541 | `			pIn++;` |
|     3935 |  542 | `			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     3933 |  543 | `				pIn++;` |
|     1964 |  544 | `			}` |
|     1970 |  545 | `		}else{` |
|   424482 |  546 | `			break;` |
|        - |  547 | `		}` |
|        5 |  548 | `	}` |
|        - |  549 | `	/* Synchronize pointers */` |
|   848959 |  550 | `	*ppCur = pIn;` |
|   848959 |  551 | `}` |
|        - |  552 | `/*` |
|        - |  553 | ` * Collect and assemble tokens holding annonymous functions/closure body.` |
|        - |  554 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  555 | ` * Note on annonymous functions.` |
|        - |  556 | ` *  According to the PHP language reference manual:` |
|        - |  557 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  558 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  559 | ` *  parameters, but they have many other uses.` |
|        - |  560 | ` *  Closures may also inherit variables from the parent scope. Any such variables` |
|        - |  561 | ` *  must be declared in the function header. Inheriting variables from the parent` |
|        - |  562 | ` *  scope is not the same as using global variables. Global variables exist in the global scope` |
|        - |  563 | ` *  which is the same no matter what function is executing. The parent scope of a closure is the` |
|        - |  564 | ` *  function in which the closure was declared (not necessarily the function it was called from).` |
|        - |  565 | ` *` |
|        - |  566 | ` * Some example:` |
|        - |  567 | ` *  $greet = function($name)` |
|        - |  568 | ` * {` |
|        - |  569 | ` *   printf("Hello %s\r\n", $name);` |
|        - |  570 | ` * };` |
|        - |  571 | ` *  $greet('World');` |
|        - |  572 | ` *  $greet('PHP');` |
|        - |  573 | ` *` |
|        - |  574 | ` * $double = function($a) {` |
|        - |  575 | ` *   return $a * 2;` |
|        - |  576 | ` * };` |
|        - |  577 | ` * // This is our range of numbers` |
|        - |  578 | ` * $numbers = range(1, 5);` |
|        - |  579 | ` * // Use the Annonymous function as a callback here to` |
|        - |  580 | ` * // double the size of each element in our` |
|        - |  581 | ` * // range` |
|        - |  582 | ` * $new_numbers = array_map($double, $numbers);` |
|        - |  583 | ` * print implode(' ', $new_numbers);` |
|        - |  584 | ` */` |
|        - |  585 | `/*` |
|        - |  586 | ` * Skip an optional return-type declaration at *ppIn:` |
|        - |  587 | ` *     ':' [?] atom ( ('\|' \| '&') [?] atom )*` |
|        - |  588 | ` * where atom is ['\']Name('\'Name)* or a parenthesized DNF group '(A&B)'.` |
|        - |  589 | ` * Shared by the anonymous-function positions php allows a return type in —` |
|        - |  590 | `` * after the parameter list, after the `use (...)` clause (php 7.1+`` |
|        - |  591 | `` * `function (...) use (...) : int {`) — and by arrow functions. This is`` |
|        - |  592 | ` * boundary scanning only; GenStateParseUnionTypeDecl (compile.c) does the` |
|        - |  593 | ` * authoritative type parse, so this must accept every shape it does` |
|        - |  594 | ` * (unions, 8.1 intersections, 8.2 DNF).` |
|        - |  595 | ` */` |
|      544 |  596 | `static void ExprSkipReturnType(SyToken **ppIn,SyToken *pEnd)` |
|        5 |  597 | `{` |
|      549 |  598 | `	SyToken *pIn = *ppIn;` |
|      549 |  599 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_COLON) ){` |
|       25 |  600 | `		pIn++; /* Skip ':' */` |
|       11 |  601 | `		for(;;){` |
|        - |  602 | `			/* Optional '?' nullable prefix */` |
|       29 |  603 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|        6 |  604 | `				pIn++;` |
|        2 |  605 | `			}` |
|       29 |  606 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - |  607 | `				/* Parenthesized DNF group '(A&B)' */` |
|      ! 0 |  608 | `				pIn++;` |
|      ! 0 |  609 | `				PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      ! 0 |  610 | `				if( pIn < pEnd ){` |
|      ! 0 |  611 | `					pIn++; /* ')' */` |
|      ! 0 |  612 | `				}` |
|       26 |  613 | `			}else if( pIn < pEnd` |
|       29 |  614 | `			 && ((pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) \|\| (pIn->nType & PH7_TK_NSSEP)) ){` |
|        - |  615 | `				/* ['\']Name('\'Name)* */` |
|       29 |  616 | `				if( pIn->nType & PH7_TK_NSSEP ){ pIn++; }` |
|       29 |  617 | `				if( pIn < pEnd && (pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       29 |  618 | `					pIn++;` |
|       29 |  619 | `					while( pIn + 1 < pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  620 | `						pIn += 2;` |
|      ! 0 |  621 | `					}` |
|       13 |  622 | `				}` |
|       16 |  623 | `			}else{` |
|        - |  624 | `				/* Malformed type — stop; the caller diagnoses the next token. */` |
|      ! 0 |  625 | `				break;` |
|        - |  626 | `			}` |
|        - |  627 | `			/* A '\|' (union) or single '&' (intersection) continues the type. */` |
|       37 |  628 | `			if( pIn < pEnd` |
|       29 |  629 | `			 && (((pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '\|')` |
|       26 |  630 | `			  \|\| (pIn->nType & PH7_TK_AMPER)) ){` |
|        5 |  631 | `				pIn++;` |
|        5 |  632 | `				continue;` |
|        - |  633 | `			}` |
|       25 |  634 | `			break;` |
|      ! 0 |  635 | `		}` |
|       11 |  636 | `	}` |
|      549 |  637 | `	*ppIn = pIn;` |
|      549 |  638 | `}` |
|      346 |  639 | `static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  640 | `{` |
|      351 |  641 | `	SyToken *pIn = *ppCur;` |
|        - |  642 | `	sxu32 nLine;` |
|        - |  643 | `	sxi32 rc;` |
|        - |  644 | `	/* Jump the 'function' keyword */` |
|      351 |  645 | `	nLine = pIn->nLine;` |
|      351 |  646 | `	pIn++;` |
|      351 |  647 | `	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  648 | `		pIn++;` |
|        1 |  649 | `	}` |
|      351 |  650 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  651 | `		/* Syntax error */` |
|        6 |  652 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");` |
|        6 |  653 | `		if( rc != SXERR_ABORT ){` |
|        6 |  654 | `			rc = SXERR_SYNTAX;` |
|        2 |  655 | `		}` |
|        6 |  656 | `		goto Synchronize;` |
|        - |  657 | `	}` |
|      347 |  658 | `	pIn++; /* Jump the leading parenthesis '(' */` |
|      347 |  659 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|      347 |  660 | `	if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  661 | `		/* Syntax error */` |
|        6 |  662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  663 | `		if( rc != SXERR_ABORT ){` |
|        6 |  664 | `			rc = SXERR_SYNTAX;` |
|        2 |  665 | `		}` |
|        6 |  666 | `		goto Synchronize;` |
|        - |  667 | `	}` |
|      343 |  668 | `	pIn++; /* Jump the trailing parenthesis */` |
|        - |  669 | `	/* Skip optional return type declaration (legacy pre-use position) */` |
|      343 |  670 | `	ExprSkipReturnType(&pIn,pEnd);` |
|      343 |  671 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|       45 |  672 | `		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);` |
|        - |  673 | `		/* Check if we are dealing with a closure */` |
|       45 |  674 | `		if( nKey == PH7_TKWRD_USE ){` |
|       37 |  675 | `			pIn++; /* Jump the 'use' keyword */` |
|       37 |  676 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  677 | `				/* Syntax error */` |
|        6 |  678 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  679 | `				if( rc != SXERR_ABORT ){` |
|        6 |  680 | `					rc = SXERR_SYNTAX;` |
|        2 |  681 | `				}` |
|        6 |  682 | `				goto Synchronize;` |
|        - |  683 | `			}` |
|       33 |  684 | `			pIn++; /* Jump the leading parenthesis '(' */` |
|       33 |  685 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|       33 |  686 | `			if( pIn >= pEnd \|\| &pIn[1] >= pEnd ){` |
|        - |  687 | `				/* Syntax error */` |
|        6 |  688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|        6 |  689 | `				if( rc != SXERR_ABORT ){` |
|        6 |  690 | `					rc = SXERR_SYNTAX;` |
|        2 |  691 | `				}` |
|        6 |  692 | `				goto Synchronize;` |
|        - |  693 | `			}` |
|       29 |  694 | `			pIn++;` |
|        - |  695 | `			/* php 7.1+: the return type may also follow the use clause —` |
|        - |  696 | ``			 * `function (...) use (...) : int {` */`` |
|       29 |  697 | `			ExprSkipReturnType(&pIn,pEnd);` |
|       17 |  698 | `		}else{` |
|        - |  699 | `			/* Syntax error */` |
|       11 |  700 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");` |
|       11 |  701 | `			if( rc != SXERR_ABORT ){` |
|       11 |  702 | `				rc = SXERR_SYNTAX;` |
|        4 |  703 | `			}` |
|       11 |  704 | `			goto Synchronize;` |
|        - |  705 | `		}` |
|       12 |  706 | `	}` |
|        - |  707 | `	/* The pIn < pEnd guard matters: the post-use return-type skip above can` |
|        - |  708 | `	 * legitimately consume up to pEnd on truncated source (EOF right after` |
|        - |  709 | `	 * the type), and pEnd is one past the last token. */` |
|      327 |  710 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) /*'{'*/ ){` |
|      327 |  711 | `		pIn++; /* Jump the leading curly '{' */` |
|      327 |  712 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|      327 |  713 | `		if( pIn < pEnd ){` |
|      327 |  714 | `			pIn++;` |
|      161 |  715 | `		}` |
|      166 |  716 | `	}else{` |
|        - |  717 | `		/* Syntax error */` |
|      ! 0 |  718 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");` |
|      ! 0 |  719 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  720 | `			return SXERR_ABORT;` |
|        - |  721 | `		}` |
|        - |  722 | `	}` |
|      327 |  723 | `	rc = SXRET_OK;` |
|      173 |  724 | `Synchronize:` |
|        - |  725 | `	/* Synchronize pointers */` |
|      351 |  726 | `	*ppCur = pIn;` |
|      351 |  727 | `	return rc;` |
|      178 |  728 | `}` |
|        - |  729 | `/*` |
|        - |  730 | ` * Assemble an anonymous-class token range (PHP 7.0):` |
|        - |  731 | ` *   class [ ( args ) ] [ extends Name ] [ implements N1, N2 … ] { body }` |
|        - |  732 | ` * On entry *ppCur points at the 'class' keyword. On exit *ppCur points just past` |
|        - |  733 | ` * the closing '}', so the whole construct becomes a single 'new' operand and the` |
|        - |  734 | ` * expression tree-builder never sees the inner braces/keywords. The header and` |
|        - |  735 | ` * body are re-parsed precisely later by GenStateCompileClassEx — here we only` |
|        - |  736 | ` * delimit the span (mirroring ExprAssembleAnnon for closures).` |
|        - |  737 | ` */` |
|       26 |  738 | `static sxi32 ExprAssembleAnnonClass(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  739 | `{` |
|       30 |  740 | `	SyToken *pIn = *ppCur;` |
|       30 |  741 | `	sxu32 nLine = pIn->nLine;` |
|        - |  742 | `	sxi32 rc;` |
|       30 |  743 | `	pIn++; /* Jump the 'class' keyword */` |
|        - |  744 | `	/* Optional constructor argument list */` |
|       30 |  745 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  746 | `		pIn++; /* Jump '(' */` |
|        7 |  747 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);` |
|        7 |  748 | `		if( pIn < pEnd ){` |
|        7 |  749 | `			pIn++; /* Jump ')' */` |
|        3 |  750 | `		}` |
|        3 |  751 | `	}` |
|        - |  752 | `	/* Optional 'extends Base' / 'implements I1, I2 …': skip up to the body '{'` |
|        - |  753 | `	 * (no braces appear between ')' and the class body). */` |
|       58 |  754 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_OCB/*'{'*/) == 0 ){` |
|       32 |  755 | `		pIn++;` |
|        4 |  756 | `	}` |
|       30 |  757 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_OCB) == 0 ){` |
|        - |  758 | `		/* Syntax error: missing class body */` |
|      ! 0 |  759 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  760 | `			"Syntax error while declaring anonymous class, missing '{'");` |
|      ! 0 |  761 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  762 | `			rc = SXERR_SYNTAX;` |
|      ! 0 |  763 | `		}` |
|      ! 0 |  764 | `		*ppCur = pIn;` |
|      ! 0 |  765 | `		return rc;` |
|        - |  766 | `	}` |
|       30 |  767 | `	pIn++; /* Jump the leading '{' */` |
|       30 |  768 | `	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);` |
|       30 |  769 | `	if( pIn < pEnd ){` |
|       30 |  770 | `		pIn++; /* Jump the trailing '}' */` |
|       13 |  771 | `	}` |
|       30 |  772 | `	*ppCur = pIn;` |
|       30 |  773 | `	return SXRET_OK;` |
|       17 |  774 | `}` |
|        - |  775 | `/*` |
|        - |  776 | ` * Assemble a PHP 7.4 arrow function token range:` |
|        - |  777 | ` *    [static] fn [&] ( params ) [: [?] type] => expression` |
|        - |  778 | ` * On entry *ppCur points at 'static' or 'fn'. On exit *ppCur points just` |
|        - |  779 | ` * past the body expression — the body ends at the first top-level comma,` |
|        - |  780 | ` * semicolon, or unbalanced closing delimiter.` |
|        - |  781 | ` */` |
|      182 |  782 | `static sxi32 ExprAssembleArrowFunc(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        4 |  783 | `{` |
|      186 |  784 | `	SyToken *pIn = *ppCur;` |
|        - |  785 | `	sxu32 nLine;` |
|        - |  786 | `	sxi32 rc;` |
|        - |  787 | `	int iNest;` |
|      186 |  788 | `	nLine = pIn->nLine;` |
|        - |  789 | `	/* Optional 'static' prefix */` |
|      182 |  790 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|      186 |  791 | `		&& SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  792 | `		pIn++;` |
|        1 |  793 | `	}` |
|        - |  794 | `	/* Expect 'fn' (dispatch in ExprExtractNode guarantees this) */` |
|      182 |  795 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|      186 |  796 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  797 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  798 | `		goto Synchronize;` |
|        - |  799 | `	}` |
|      186 |  800 | `	pIn++; /* Jump 'fn' */` |
|       91 |  801 | `	SXUNUSED(nLine);` |
|       91 |  802 | `	SXUNUSED(pGen);` |
|        - |  803 | `	/* Optional '&' for return-by-reference */` |
|      186 |  804 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  805 | `		pIn++;` |
|      ! 0 |  806 | `	}` |
|        - |  807 | `	/* The compile phase (PH7_CompileArrowFunc) performs the authoritative` |
|        - |  808 | `	 * structural validation and emits PHP-compatible parse errors. Here we` |
|        - |  809 | `	 * just scan token boundaries so the expression node's pEnd covers the` |
|        - |  810 | ``	 * whole `[static] fn(...) [:T] => body` range, even if malformed. */`` |
|      186 |  811 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|      183 |  812 | `		pIn++; /* '(' */` |
|      183 |  813 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|      183 |  814 | `		if( pIn < pEnd ){` |
|      181 |  815 | `			pIn++; /* ')' */` |
|       89 |  816 | `		}` |
|       90 |  817 | `	}` |
|        - |  818 | `	/* Optional return type — shared skipper (unions/intersections/DNF) */` |
|      186 |  819 | `	ExprSkipReturnType(&pIn,pEnd);` |
|        - |  820 | `	/* Consume '=>' if present; the compile pass diagnoses absence */` |
|      186 |  821 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|      179 |  822 | `		pIn++;` |
|       88 |  823 | `	}` |
|        - |  824 | `	/* Scan body until first top-level ',' ';' ')' ']' '}' */` |
|      186 |  825 | `	iNest = 0;` |
|     1100 |  826 | `	while( pIn < pEnd ){` |
|      996 |  827 | `		if( iNest == 0 && (pIn->nType &` |
|        - |  828 | `			(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       79 |  829 | `			break;` |
|        - |  830 | `		}` |
|      918 |  831 | `		if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       65 |  832 | `			iNest++;` |
|      887 |  833 | `		}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       65 |  834 | `			iNest--;` |
|       31 |  835 | `		}` |
|      918 |  836 | `		pIn++;` |
|        4 |  837 | `	}` |
|      186 |  838 | `	rc = SXRET_OK;` |
|       91 |  839 | `Synchronize:` |
|      186 |  840 | `	*ppCur = pIn;` |
|      186 |  841 | `	return rc;` |
|        4 |  842 | `}` |
|        - |  843 | `/*` |
|        - |  844 | ` * Scan token boundaries of a PHP 8.0 match expression:` |
|        - |  845 | ` *     match '(' <subject> ')' '{' <arms> '}'` |
|        - |  846 | ` * The compile pass (PH7_CompileMatch) performs authoritative validation` |
|        - |  847 | ` * and emits PHP-compatible parse errors. Here we just advance past the` |
|        - |  848 | ` * closing '}' so the expression node's pEnd covers the entire span.` |
|        - |  849 | ` */` |
|       70 |  850 | `static sxi32 ExprAssembleMatch(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)` |
|        5 |  851 | `{` |
|       75 |  852 | `	SyToken *pIn = *ppCur;` |
|        - |  853 | `	sxi32 rc;` |
|       35 |  854 | `	SXUNUSED(pGen);` |
|        - |  855 | `	/* Expect 'match' (dispatch in ExprExtractNode guarantees this) */` |
|       70 |  856 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       75 |  857 | `		\|\| SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_MATCH ){` |
|      ! 0 |  858 | `		rc = SXERR_SYNTAX;` |
|      ! 0 |  859 | `		goto Synchronize;` |
|        - |  860 | `	}` |
|       75 |  861 | `	pIn++; /* Jump 'match' */` |
|        - |  862 | `	/* Optional '(' subject ')' */` |
|       75 |  863 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       75 |  864 | `		pIn++;` |
|       75 |  865 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pIn);` |
|       75 |  866 | `		if( pIn < pEnd ){` |
|       75 |  867 | `			pIn++; /* ')' */` |
|       35 |  868 | `		}` |
|       35 |  869 | `	}` |
|        - |  870 | `	/* Optional '{' arms '}' */` |
|       75 |  871 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|       75 |  872 | `		pIn++;` |
|       75 |  873 | `		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB,PH7_TK_CCB,&pIn);` |
|       75 |  874 | `		if( pIn < pEnd ){` |
|       75 |  875 | `			pIn++; /* '}' */` |
|       35 |  876 | `		}` |
|       35 |  877 | `	}` |
|       75 |  878 | `	rc = SXRET_OK;` |
|       35 |  879 | `Synchronize:` |
|       75 |  880 | `	*ppCur = pIn;` |
|       75 |  881 | `	return rc;` |
|        5 |  882 | `}` |
|        - |  883 | `/*` |
|        - |  884 | ` * Extract a single expression node from the input.` |
|        - |  885 | ` * On success store the freshly extractd node in ppNode.` |
|        - |  886 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - |  887 | ` * An expression node can be a variable [i.e: $var],an operator [i.e: ++]` |
|        - |  888 | ` * an annonymous function [i.e: function(){ return "Hello"; }, a double/single` |
|        - |  889 | ` * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path` |
|        - |  890 | ` * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.` |
|        - |  891 | ` */` |
|  4532016 |  892 | `static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode,int iLastWasTerm,int bAfterMemberOp)` |
|        5 |  893 | `{` |
|        - |  894 | `	ph7_expr_node *pNode;` |
|        - |  895 | `	SyToken *pCur;` |
|        - |  896 | `	sxi32 rc;` |
|        - |  897 | `	/* Allocate a new node */` |
|  4532021 |  898 | `	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));` |
|  4532021 |  899 | `	if( pNode == 0 ){` |
|        - |  900 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  901 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  902 | `		 */` |
|      ! 0 |  903 | `		return SXERR_MEM;` |
|        - |  904 | `	}` |
|        - |  905 | `	/* Zero the structure */` |
|  4532021 |  906 | `	SyZero(pNode,sizeof(ph7_expr_node));` |
|  4532021 |  907 | `	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));` |
|        - |  908 | `	/* Point to the head of the token stream */` |
|  4532021 |  909 | `	pCur = pNode->pStart = pGen->pIn;` |
|        - |  910 | `	/* Start collecting tokens */` |
|  4532021 |  911 | `	if( pCur->nType & PH7_TK_ELLIPSIS ){` |
|     3997 |  912 | `		if( &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_RPAREN) ){` |
|        - |  913 | ``			/* First-class callable: `...` is the ENTIRE argument list — the next token is`` |
|        - |  914 | `			 * ')'. Consume only the '...' and return this node as a self-evaluating FCC` |
|        - |  915 | `			 * marker (xCode set so ExprMakeTree accepts it as a lone terminal); the` |
|        - |  916 | `			 * function-call code generator turns it into a Closure (OP_LOAD_FCC). */` |
|       77 |  917 | `			pNode->pEnd = pCur;` |
|       77 |  918 | `			pCur++;` |
|       77 |  919 | `			pNode->iFlags \|= EXPR_NODE_FCC;` |
|       77 |  920 | `			pNode->xCode = PH7_CompileFccMarker;` |
|       77 |  921 | `			pGen->pIn = pCur;` |
|       77 |  922 | `			*ppNode = pNode;` |
|       77 |  923 | `			return SXRET_OK;` |
|        - |  924 | `		}` |
|        - |  925 | `		/* Argument unpacking: ...$expr — skip '...' and extract the expression.` |
|        - |  926 | `		 * Mark the node so that the code generator emits PH7_OP_SPREAD after it. */` |
|     3921 |  927 | `		pCur++;` |
|     3921 |  928 | `		pGen->pIn = pCur;` |
|     3921 |  929 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator, pNode);` |
|     3921 |  930 | `		rc = ExprExtractNode(pGen, ppNode, iLastWasTerm, 0/* a spread element is never a member name */);` |
|     3921 |  931 | `		if( rc == SXRET_OK && *ppNode ){` |
|     3921 |  932 | `			(*ppNode)->iFlags \|= EXPR_NODE_SPREAD;` |
|     1958 |  933 | `		}` |
|     3921 |  934 | `		return rc;` |
|        - |  935 | `	}` |
|  4528029 |  936 | `	if( (pCur->nType & PH7_TK_OSB) && !iLastWasTerm ){` |
|        - |  937 | `		/* PHP 5.4 short array syntax: [1, 2, 3] or ['key' => 'value'].` |
|        - |  938 | `		 * This '[' does not follow a term, so it is an array literal, not subscript.` |
|        - |  939 | `		 */` |
|     1293 |  940 | `		pCur++; /* Skip the opening '[' */` |
|     1293 |  941 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OSB,PH7_TK_CSB,&pCur);` |
|     1293 |  942 | `		if( pCur < pGen->pEnd ){` |
|     1293 |  943 | `			pCur++; /* Skip past the closing ']' */` |
|      649 |  944 | `		}else{` |
|      ! 0 |  945 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  946 | `				"Short array: Missing closing bracket ']'");` |
|      ! 0 |  947 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  948 | `				rc = SXERR_SYNTAX;` |
|      ! 0 |  949 | `			}` |
|      ! 0 |  950 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 |  951 | `			return rc;` |
|        - |  952 | `		}` |
|        - |  953 | `		/* Check if ']' is followed by '=' — if so, this is symmetric array` |
|        - |  954 | `		 * destructuring (PHP 7.1 short list syntax), not an array literal.` |
|        - |  955 | `		 */` |
|     1380 |  956 | `		if( pCur < pGen->pEnd && (pCur->nType & PH7_TK_OP) ){` |
|      178 |  957 | `			ph7_expr_op *pOp = (ph7_expr_op *)pCur->pUserData;` |
|      178 |  958 | `			if( pOp && pOp->iVmOp == PH7_OP_STORE ){` |
|       54 |  959 | `				pNode->xCode = PH7_CompileShortList;` |
|       29 |  960 | `			}else{` |
|      125 |  961 | `				pNode->xCode = PH7_CompileShortArray;` |
|        - |  962 | `			}` |
|       91 |  963 | `		}else{` |
|     1119 |  964 | `			pNode->xCode = PH7_CompileShortArray;` |
|        5 |  965 | `		}` |
|  4527385 |  966 | `	}else if( bAfterMemberOp && (pCur->nType & PH7_TK_OP) && (pCur->nType & PH7_TK_ID) ){` |
|        - |  967 | `		/* An alpha-stream operator-keyword (clone/new/and/or/xor/instanceof) used` |
|        - |  968 | `		 * as a member NAME right after -> / ?-> / :: — e.g. $o->clone(), C::new(),` |
|        - |  969 | `		 * $o->and() — is a plain identifier, exactly like the TK_KEYWORD member-name` |
|        - |  970 | `		 * case below (PHP allows any keyword there). Clear PH7_TK_OP so ExprVerifyNodes` |
|        - |  971 | `		 * / ExprMakeTree treat this as a term, not an operator with a NULL pOp. This` |
|        - |  972 | ``		 * must precede the clone(...) call-form branch so `$o->clone(...)` is a method`` |
|        - |  973 | `		 * call, not the clone() intrinsic. */` |
|       17 |  974 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|       17 |  975 | `		ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|       17 |  976 | `		pNode->xCode = PH7_CompileLiteral;` |
|  4526756 |  977 | `	}else if( (pCur->nType & PH7_TK_OP) && pCur->pUserData` |
|  1022840 |  978 | `		&& ((const ph7_expr_op *)pCur->pUserData)->iOp == EXPR_OP_CLONE` |
|   511452 |  979 | `		&& &pCur[1] < pGen->pEnd && (pCur[1].nType & PH7_TK_LPAREN) ){` |
|        - |  980 | `		/* PHP 8.5 clone(...) call form: clone($object [, $withProperties]).` |
|        - |  981 | ``		 * `clone` is an alpha-stream operator, so `clone(` is NOT auto-marked`` |
|        - |  982 | ``		 * as a function call the way `foo(` is — collect the parenthesised`` |
|        - |  983 | `		 * argument list here and let PH7_CompileCloneCall reparse it (mirrors` |
|        - |  984 | `		 * how array(...)/list(...) are handled). The bare operator/statement` |
|        - |  985 | ``		 * form `clone $obj` (no immediately-following '(') keeps the`` |
|        - |  986 | `		 * precedence-1 operator path below. Clear PH7_TK_OP on the 'clone'` |
|        - |  987 | `		 * token: this node is now a self-evaluating term (xCode set, pOp NULL),` |
|        - |  988 | `		 * so ExprVerifyNodes / ExprMakeTree must not treat its start token as an` |
|        - |  989 | `		 * operator (which would dereference the NULL pOp). */` |
|       20 |  990 | `		pNode->pStart->nType &= ~PH7_TK_OP;` |
|       20 |  991 | `		pCur += 2; /* skip 'clone' and the opening '(' */` |
|       20 |  992 | `		PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       20 |  993 | `		if( pCur < pGen->pEnd ){` |
|       20 |  994 | `			pCur++; /* skip the closing ')' */` |
|       11 |  995 | `		}else{` |
|      ! 0 |  996 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - |  997 | `				"clone: Missing closing parenthesis ')'");` |
|      ! 0 |  998 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 |  999 | `				rc = SXERR_SYNTAX;` |
|      ! 0 | 1000 | `			}` |
|      ! 0 | 1001 | `			SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1002 | `			return rc;` |
|        - | 1003 | `		}` |
|       20 | 1004 | `		pNode->xCode = PH7_CompileCloneCall;` |
|  4526716 | 1005 | `	}else if( pCur->nType & PH7_TK_OP ){` |
|        - | 1006 | `		/* Point to the instance that describe this operator */` |
|  1022827 | 1007 | `		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;` |
|        - | 1008 | `		/* Advance the stream cursor */` |
|  1022827 | 1009 | `		pCur++;` |
|  4015296 | 1010 | `	}else if( pCur->nType & PH7_TK_DOLLAR ){` |
|        - | 1011 | `		/* Isolate variable */` |
|  2448161 | 1012 | `		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){` |
|  1224089 | 1013 | `			pCur++; /* Variable variable */` |
|        5 | 1014 | `		}` |
|  1224077 | 1015 | `		if( pCur < pGen->pEnd ){` |
|  1224077 | 1016 | `			if (pCur->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|        - | 1017 | `				/* Variable name */` |
|  1224049 | 1018 | `				pCur++;` |
|   612054 | 1019 | `			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){` |
|       24 | 1020 | `				pCur++;` |
|        - | 1021 | `				/* Dynamic variable name,Collect until the next non nested '}' */` |
|       24 | 1022 | `				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);` |
|       24 | 1023 | `				if( pCur < pGen->pEnd ){` |
|       19 | 1024 | `					pCur++;` |
|       11 | 1025 | `				}else{` |
|        6 | 1026 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");` |
|        6 | 1027 | `					if( rc != SXERR_ABORT ){` |
|        6 | 1028 | `						rc = SXERR_SYNTAX;` |
|        2 | 1029 | `					}` |
|        6 | 1030 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        6 | 1031 | `					return rc;` |
|        - | 1032 | `				}` |
|        8 | 1033 | `			}` |
|   612034 | 1034 | `		}` |
|  1224073 | 1035 | `		pNode->xCode = PH7_CompileVariable;` |
|  2891847 | 1036 | `	 }else if( pCur->nType & PH7_TK_KEYWORD ){` |
|    57309 | 1037 | `		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    57309 | 1038 | `		 if( bAfterMemberOp ){` |
|        - | 1039 | `			 /* A keyword immediately after a member operator (->, ?->, ::) is a` |
|        - | 1040 | `			  * method/property NAME, not a language construct — PHP allows any` |
|        - | 1041 | `			  * keyword there (e.g. $g->throw(), $o->list(), $o->print()). Treat it` |
|        - | 1042 | `			  * as a plain literal like an ordinary identifier member name. */` |
|      185 | 1043 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      185 | 1044 | `			 pNode->xCode = PH7_CompileLiteral;` |
|    57219 | 1045 | `		 }else if( nKeyword == PH7_TKWRD_ARRAY \|\|  nKeyword == PH7_TKWRD_LIST ){` |
|        - | 1046 | `			 /* List/Array node */` |
|    32489 | 1047 | `			 if( &pCur[1] >= pGen->pEnd \|\| (pCur[1].nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1048 | `				 /* Assume a literal */` |
|      ! 0 | 1049 | `				 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1050 | `				 pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1051 | `			 }else{` |
|    32489 | 1052 | `				 pCur += 2;` |
|        - | 1053 | `				 /* Collect array/list tokens */` |
|    32489 | 1054 | `				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);` |
|    32489 | 1055 | `				 if( pCur < pGen->pEnd ){` |
|    32487 | 1056 | `					 pCur++;` |
|    16246 | 1057 | `				 }else{` |
|        - | 1058 | `					 /* Syntax error */` |
|        4 | 1059 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        1 | 1060 | `						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");` |
|        3 | 1061 | `					 if( rc != SXERR_ABORT ){` |
|        3 | 1062 | `						 rc = SXERR_SYNTAX;` |
|        1 | 1063 | `					 }` |
|        3 | 1064 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1065 | `					 return rc;` |
|        - | 1066 | `				 }` |
|    32487 | 1067 | `				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;` |
|    32487 | 1068 | `				 if( pNode->xCode == PH7_CompileList ){` |
|       37 | 1069 | `					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;` |
|       37 | 1070 | `					 if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_OP) == 0  \|\| pOp == 0 \|\| pOp->iVmOp != PH7_OP_STORE /*'='*/){` |
|        - | 1071 | `						 /* Syntax error */` |
|        3 | 1072 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");` |
|        3 | 1073 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1074 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1075 | `						 }` |
|        3 | 1076 | `						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1077 | `						 return rc;` |
|        - | 1078 | `					 }` |
|       15 | 1079 | `				 }` |
|        5 | 1080 | `			 }` |
|    40885 | 1081 | `		 }else if( nKeyword == PH7_TKWRD_YIELD ){` |
|        - | 1082 | `			 /* yield expression: collect tokens for the yielded value(s) */` |
|      333 | 1083 | `			 pCur++; /* Skip 'yield' keyword */` |
|      333 | 1084 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1085 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1086 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|      333 | 1087 | `			 pNode->xCode = PH7_CompileYield;` |
|    24481 | 1088 | `		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){` |
|        - | 1089 | `			 /* Annonymous function */` |
|      351 | 1090 | `			  if( &pCur[1] >= pGen->pEnd ){` |
|        - | 1091 | `				 /* Assume a literal */` |
|      ! 0 | 1092 | `				ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|      ! 0 | 1093 | `				pNode->xCode = PH7_CompileLiteral;` |
|      ! 0 | 1094 | `			 }else{` |
|        - | 1095 | `				 /* Assemble annonymous functions body */` |
|      351 | 1096 | `				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);` |
|      351 | 1097 | `				 if( rc != SXRET_OK ){` |
|       28 | 1098 | `					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|       28 | 1099 | `					 return rc;` |
|        - | 1100 | `				 }` |
|      327 | 1101 | `				 pNode->xCode = PH7_CompileAnnonFunc;` |
|        - | 1102 | `			  }` |
|    24136 | 1103 | `		 }else if( nKeyword == PH7_TKWRD_CLASS && &pCur[1] < pGen->pEnd` |
|       35 | 1104 | `			 && ( (pCur[1].nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_LPAREN/*'('*/))` |
|       22 | 1105 | `				\|\| ( (pCur[1].nType & PH7_TK_KEYWORD)` |
|       12 | 1106 | `					&& ( SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_EXTENDS` |
|        9 | 1107 | `						\|\| SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_IMPLEMENTS ) ) ) ){` |
|        - | 1108 | `			 /* Anonymous class: new class(args) [extends/implements] { body }.` |
|        - | 1109 | `			  * Only when 'class' is followed by '{', '(', extends or implements —` |
|        - | 1110 | `			  * this excludes the '::class' constant (e.g. self::class), where` |
|        - | 1111 | `			  * 'class' is a plain name handled by the literal fallback below. */` |
|       30 | 1112 | `			 rc = ExprAssembleAnnonClass(&(*pGen),&pCur,pGen->pEnd);` |
|       30 | 1113 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1114 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1115 | `				 return rc;` |
|        - | 1116 | `			 }` |
|       30 | 1117 | `			 pNode->xCode = PH7_CompileAnnonClass;` |
|    23958 | 1118 | `		 }else if( nKeyword == PH7_TKWRD_FN` |
|    23855 | 1119 | `			\|\| ( nKeyword == PH7_TKWRD_STATIC && &pCur[1] < pGen->pEnd` |
|       30 | 1120 | `				 && (pCur[1].nType & PH7_TK_KEYWORD)` |
|       15 | 1121 | `				 && SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ) ){` |
|        - | 1122 | `			 /* PHP 7.4 arrow function: fn(...) => expr or static fn(...) => expr */` |
|      186 | 1123 | `			 rc = ExprAssembleArrowFunc(&(*pGen),&pCur,pGen->pEnd);` |
|      186 | 1124 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1125 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1126 | `				 return rc;` |
|        - | 1127 | `			 }` |
|      186 | 1128 | `			 pNode->xCode = PH7_CompileArrowFunc;` |
|    23854 | 1129 | `		 }else if( nKeyword == PH7_TKWRD_MATCH ){` |
|        - | 1130 | `			 /* PHP 8.0 match expression: match(subject){ cond => result, ... } */` |
|       75 | 1131 | `			 rc = ExprAssembleMatch(&(*pGen),&pCur,pGen->pEnd);` |
|       75 | 1132 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1133 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|      ! 0 | 1134 | `				 return rc;` |
|        - | 1135 | `			 }` |
|       75 | 1136 | `			 pNode->xCode = PH7_CompileMatch;` |
|    23728 | 1137 | `		 }else if( nKeyword == PH7_TKWRD_THROW ){` |
|        - | 1138 | `			 /* PHP 8.0 throw expression: throw <expr>` |
|        - | 1139 | `			  * Consume the 'throw' keyword and all tokens up to the enclosing` |
|        - | 1140 | `			  * close delimiter; PH7_CompileThrowExpr will reparse the body. */` |
|       38 | 1141 | `			 pCur++; /* Skip 'throw' */` |
|       38 | 1142 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,` |
|        - | 1143 | `				 PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB,` |
|        - | 1144 | `				 PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       38 | 1145 | `			 pNode->xCode = PH7_CompileThrowExpr;` |
|    23675 | 1146 | `		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){` |
|        - | 1147 | `			 /* Language constructs [i.e: print,echo,die...] require special handling */` |
|       93 | 1148 | `			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN\|PH7_TK_OCB\|PH7_TK_OSB, PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB,&pCur);` |
|       93 | 1149 | `			 pNode->xCode = PH7_CompileLangConstruct;` |
|       49 | 1150 | `		 }else{` |
|        - | 1151 | `			 /* Assume a literal */` |
|    23569 | 1152 | `			 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|    23569 | 1153 | `			 pNode->xCode = PH7_CompileLiteral;` |
|        5 | 1154 | `		 }` |
|  2251147 | 1155 | `	 }else if( pCur->nType & (PH7_TK_NSSEP\|PH7_TK_ID) ){` |
|        - | 1156 | `		 /* Constants,function name,namespace path,class name... */` |
|   825199 | 1157 | `		 ExprAssembleLiteral(&pCur,pGen->pEnd);` |
|   825199 | 1158 | `		 pNode->xCode = PH7_CompileLiteral;` |
|   412602 | 1159 | `	 }else{` |
|  1397315 | 1160 | `		 if( (pCur->nType & (PH7_TK_LPAREN\|PH7_TK_RPAREN\|PH7_TK_COMMA\|PH7_TK_COLON\|PH7_TK_CSB\|PH7_TK_OCB\|PH7_TK_CCB)) == 0 ){` |
|        - | 1161 | `			 /* Point to the code generator routine */` |
|   266675 | 1162 | `			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);` |
|   266675 | 1163 | `			 if( pNode->xCode == 0 ){` |
|        3 | 1164 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);` |
|        3 | 1165 | `				 if( rc != SXERR_ABORT ){` |
|        3 | 1166 | `					 rc = SXERR_SYNTAX;` |
|        1 | 1167 | `				 }` |
|        3 | 1168 | `				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|        3 | 1169 | `				 return rc;` |
|        - | 1170 | `			 }` |
|   133334 | 1171 | `		 }` |
|        - | 1172 | `		/* Advance the stream cursor */` |
|  1397313 | 1173 | `		pCur++;` |
|        - | 1174 | `	 }` |
|        - | 1175 | `	/* Point to the end of the token stream */` |
|  4527995 | 1176 | `	pNode->pEnd = pCur;` |
|        - | 1177 | `	/* Save the node for later processing */` |
|  4527995 | 1178 | `	*ppNode = pNode;` |
|        - | 1179 | `	/* Synchronize cursors */` |
|  4527995 | 1180 | `	pGen->pIn = pCur;` |
|  4527995 | 1181 | `	return SXRET_OK;` |
|  2266013 | 1182 | `}` |
|        - | 1183 | `/*` |
|        - | 1184 | ` * Point to the next expression that should be evaluated shortly.` |
|        - | 1185 | ` * The cursor stops when it hit a comma ',' or a semi-colon and the nesting` |
|        - | 1186 | ` * level is zero.` |
|        - | 1187 | ` */` |
|    99586 | 1188 | `PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)` |
|        5 | 1189 | `{` |
|    99591 | 1190 | `	SyToken *pCur = pStart;` |
|    99591 | 1191 | `	sxi32 iNest = 0;` |
|    99591 | 1192 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_SEMI/*';'*/) ){` |
|        - | 1193 | `		/* Last expression */` |
|    51603 | 1194 | `		return SXERR_EOF;` |
|        - | 1195 | `	}` |
|   197029 | 1196 | `	while( pCur < pEnd ){` |
|   179787 | 1197 | `		if( (pCur->nType & (PH7_TK_COMMA/*','*/\|PH7_TK_SEMI/*';'*/)) && iNest <= 0){` |
|    30751 | 1198 | `			break;` |
|        - | 1199 | `		}` |
|   149041 | 1200 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    10401 | 1201 | `			iNest++;` |
|   143843 | 1202 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}*/) ){` |
|    10403 | 1203 | `			iNest--;` |
|     5199 | 1204 | `		}` |
|   149041 | 1205 | `		pCur++;` |
|        5 | 1206 | `	}` |
|    47993 | 1207 | `	*ppNext = pCur;` |
|    47993 | 1208 | `	return SXRET_OK;` |
|    49798 | 1209 | `}` |
|        - | 1210 | `/*` |
|        - | 1211 | ` * Free an expression tree.` |
|        - | 1212 | ` */` |
|  3869434 | 1213 | `static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)` |
|        5 | 1214 | `{` |
|  3869439 | 1215 | `	if( pNode->pLeft ){` |
|        - | 1216 | `		/* Release the left tree */` |
|  1429429 | 1217 | `		ExprFreeTree(&(*pGen),pNode->pLeft);` |
|   714712 | 1218 | `	}` |
|  3869439 | 1219 | `	if( pNode->pRight ){` |
|        - | 1220 | `		/* Release the right tree */` |
|   772343 | 1221 | `		ExprFreeTree(&(*pGen),pNode->pRight);` |
|   386169 | 1222 | `	}` |
|  3869439 | 1223 | `	if( pNode->pCond ){` |
|        - | 1224 | `		/* Release the conditional tree used by the ternary operator */` |
|     2651 | 1225 | `		ExprFreeTree(&(*pGen),pNode->pCond);` |
|     1323 | 1226 | `	}` |
|  3869439 | 1227 | `	if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|        - | 1228 | `		ph7_expr_node **apArg;` |
|        - | 1229 | `		sxu32 n;` |
|        - | 1230 | `		/* Release node arguments */` |
|   499571 | 1231 | `		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  1074519 | 1232 | `		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   574953 | 1233 | `			ExprFreeTree(&(*pGen),apArg[n]);` |
|   287479 | 1234 | `		}` |
|   499571 | 1235 | `		SySetRelease(&pNode->aNodeArgs);` |
|   249783 | 1236 | `	}` |
|        - | 1237 | `	/* Finally,release this node */` |
|  3869439 | 1238 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);` |
|  3869439 | 1239 | `}` |
|        - | 1240 | `/*` |
|        - | 1241 | ` * Free an expression tree.` |
|        - | 1242 | ` * This function is a wrapper around ExprFreeTree() defined above.` |
|        - | 1243 | ` */` |
|  1024408 | 1244 | `PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)` |
|        5 | 1245 | `{` |
|        - | 1246 | `	ph7_expr_node **apNode;` |
|        - | 1247 | `	sxu32 n;` |
|  1024413 | 1248 | `	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);` |
|  5552479 | 1249 | `	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){` |
|  4528071 | 1250 | `		if( apNode[n] ){` |
|  1024747 | 1251 | `			ExprFreeTree(&(*pGen),apNode[n]);` |
|   512371 | 1252 | `		}` |
|  2264038 | 1253 | `	}` |
|  1024413 | 1254 | `	return SXRET_OK;` |
|        5 | 1255 | `}` |
|        - | 1256 | `/*` |
|        - | 1257 | ` * Return TRUE if any node in the expression subtree is the nullsafe` |
|        - | 1258 | `` * operator `?->`.  Used by write-context checks to reject assignments,`` |
|        - | 1259 | ` * references, and unset() that target any link of a nullsafe chain` |
|        - | 1260 | ` * (PHP 8.0 makes this a fatal parse error:` |
|        - | 1261 | ` * "Can't use nullsafe operator in write context").` |
|        - | 1262 | ` */` |
|  1398020 | 1263 | `PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode)` |
|        5 | 1264 | `{` |
|  1398025 | 1265 | `	if( pNode == 0 ){` |
|   862483 | 1266 | `		return 0;` |
|        - | 1267 | `	}` |
|   535547 | 1268 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       16 | 1269 | `		return 1;` |
|        - | 1270 | `	}` |
|   535535 | 1271 | `	if( PH7_ExprContainsNullsafe(pNode->pLeft) ){` |
|        6 | 1272 | `		return 1;` |
|        - | 1273 | `	}` |
|   535531 | 1274 | `	if( PH7_ExprContainsNullsafe(pNode->pRight) ){` |
|      ! 0 | 1275 | `		return 1;` |
|        - | 1276 | `	}` |
|   535531 | 1277 | `	return 0;` |
|   699015 | 1278 | `}` |
|        - | 1279 | `/*` |
|        - | 1280 | ` * Check if the given node is a modifialbe l/r-value.` |
|        - | 1281 | ` * Return TRUE if modifiable.FALSE otherwise.` |
|        - | 1282 | ` */` |
|   320294 | 1283 | `static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)` |
|        5 | 1284 | `{` |
|        - | 1285 | `	sxi32 iExprOp;` |
|   320299 | 1286 | `	if( pNode->pOp == 0 ){` |
|   196767 | 1287 | `		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;` |
|        - | 1288 | `	}` |
|   123537 | 1289 | `	iExprOp = pNode->pOp->iOp;` |
|   123537 | 1290 | `	if( iExprOp == EXPR_OP_ARROW /*'->' */ \|\| iExprOp == EXPR_OP_DC /*'::'*/ ){` |
|    84757 | 1291 | `			return TRUE;` |
|        - | 1292 | `	}` |
|    38785 | 1293 | `	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){` |
|    38779 | 1294 | `		if( pNode->pLeft->pOp ) {` |
|       68 | 1295 | `			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/` |
|       31 | 1296 | `				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){` |
|      ! 0 | 1297 | `				return FALSE;` |
|        5 | 1298 | `			}` |
|    38745 | 1299 | `		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1300 | `			return FALSE;` |
|        - | 1301 | `		}` |
|    38779 | 1302 | `		return TRUE;` |
|        - | 1303 | `	}` |
|        8 | 1304 | `	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){` |
|        8 | 1305 | `		return TRUE;` |
|        - | 1306 | `	}` |
|        - | 1307 | `	/* Not a modifiable l or r-value */` |
|      ! 0 | 1308 | `	return FALSE;` |
|   160152 | 1309 | `}` |
|        - | 1310 | `/* Forward declaration */` |
|        - | 1311 | `static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);` |
|        - | 1312 | `/* Macro to check if the given node is a terminal.` |
|        - | 1313 | ` * A node is a term if it has no operator, or has already been linked into an` |
|        - | 1314 | ` * expression tree (pLeft set for binary ops, or pCond+pRight for a fully` |
|        - | 1315 | ` * linked ternary/elvis node). */` |
|        - | 1316 | `#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp \|\| apNode[NODE]->pLeft \|\| (apNode[NODE]->pCond && apNode[NODE]->pRight) ))` |
|        - | 1317 | `/*` |
|        - | 1318 | ` * Buid an expression tree for each given function argument.` |
|        - | 1319 | ` * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1320 | ` */` |
|   420450 | 1321 | `static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1322 | `{` |
|        - | 1323 | `	sxi32 iNest,iCur,iNode;` |
|        - | 1324 | `	sxi32 rc;` |
|        - | 1325 | `	/* Process function arguments from left to right */` |
|   420455 | 1326 | `	iCur = 0;` |
|   458129 | 1327 | `	for(;;){` |
|   916263 | 1328 | `		if( iCur >= nToken ){` |
|        - | 1329 | `			/* No more arguments to process */` |
|   420429 | 1330 | `			break;` |
|        - | 1331 | `		}` |
|   495839 | 1332 | `		iNode = iCur;` |
|   495839 | 1333 | `		iNest = 0;` |
|  1227347 | 1334 | `		while( iCur < nToken ){` |
|   806921 | 1335 | `			if( apNode[iCur] ){` |
|   791609 | 1336 | `				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){` |
|    37709 | 1337 | `					break;` |
|   736268 | 1338 | `				}else if( (apNode[iCur]->pStart->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB))` |
|   378396 | 1339 | `					&& apNode[iCur]->xCode != PH7_CompileShortArray` |
|    40375 | 1340 | `					&& apNode[iCur]->xCode != PH7_CompileShortList ){` |
|        - | 1341 | `					/* A short-array/short-list literal ([...]) is extracted as a single` |
|        - | 1342 | `					 * self-contained node that already consumed its matching ']', so its` |
|        - | 1343 | `					 * opening '[' has no separate closing node to balance iNest. Treat it` |
|        - | 1344 | `					 * as a term, not an opening bracket, otherwise iNest stays >0 and the` |
|        - | 1345 | `					 * following comma is never seen as an argument separator (collapsing` |
|        - | 1346 | `					 * e.g. array_merge([1],[2]) to just [2]). */` |
|    40149 | 1347 | `					iNest++;` |
|   696129 | 1348 | `				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CCB\|PH7_TK_CSB) ){` |
|    40149 | 1349 | `					iNest--;` |
|    20072 | 1350 | `				}` |
|   358098 | 1351 | `			}` |
|   731513 | 1352 | `			iCur++;` |
|        5 | 1353 | `		}` |
|   495839 | 1354 | `		if( iCur > iNode ){` |
|   495833 | 1355 | `			SyString sArgName = {0, 0};` |
|        - | 1356 | `			/* Check for named argument pattern: identifier ':' expr.` |
|        - | 1357 | `			 * PHP allows reserved keywords as parameter names (e.g. function` |
|        - | 1358 | `			 * f($class){}), so accept PH7_TK_KEYWORD labels here too. */` |
|   500323 | 1359 | `			if( (iCur - iNode) >= 2` |
|   274280 | 1360 | `				&& apNode[iNode]` |
|    52732 | 1361 | `				&& (apNode[iNode]->pStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|    31003 | 1362 | `				&& apNode[iNode]->xCode == PH7_CompileLiteral` |
|     9132 | 1363 | `				&& apNode[iNode+1]` |
|     8995 | 1364 | `				&& (apNode[iNode+1]->pStart->nType & PH7_TK_COLON) ){` |
|        - | 1365 | `				/* Named argument detected: save name, free ID and colon nodes */` |
|      255 | 1366 | `				sArgName = apNode[iNode]->pStart->sData;` |
|      255 | 1367 | `				ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      255 | 1368 | `				apNode[iNode] = 0;` |
|      255 | 1369 | `				ExprFreeTree(&(*pGen),apNode[iNode+1]);` |
|      255 | 1370 | `				apNode[iNode+1] = 0;` |
|      255 | 1371 | `				iNode += 2;` |
|        - | 1372 | `				/* Guard: the value expression must not be empty.  Catches` |
|        - | 1373 | `				 * degenerate forms like f(a:) or f(a:,b:1). */` |
|      255 | 1374 | `				if( iNode >= iCur ){` |
|        4 | 1375 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        2 | 1376 | `						pOp->pStart->nLine,` |
|        - | 1377 | `						"syntax error, expected expression after named argument '%z:'",` |
|        - | 1378 | `						&sArgName);` |
|        3 | 1379 | `					if( rc != SXERR_ABORT ){` |
|        3 | 1380 | `						rc = SXERR_SYNTAX;` |
|        1 | 1381 | `					}` |
|        3 | 1382 | `					return rc;` |
|        - | 1383 | `				}` |
|      124 | 1384 | `			}` |
|   495826 | 1385 | `			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)` |
|        5 | 1386 | `				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){` |
|      ! 0 | 1387 | `					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,` |
|        - | 1388 | `						"call-time pass-by-reference is depreceated");` |
|      ! 0 | 1389 | `					ExprFreeTree(&(*pGen),apNode[iNode]);` |
|      ! 0 | 1390 | `					apNode[iNode] = 0;` |
|      ! 0 | 1391 | `			}` |
|   495831 | 1392 | `			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);` |
|   495831 | 1393 | `			if( apNode[iNode] ){` |
|   495831 | 1394 | `				if( sArgName.nByte > 0 ){` |
|      253 | 1395 | `					apNode[iNode]->iFlags \|= EXPR_NODE_NAMED_ARG;` |
|      253 | 1396 | `					apNode[iNode]->sArgName = sArgName;` |
|      124 | 1397 | `				}` |
|        - | 1398 | `				/* Put a pointer to the root of the tree in the arguments set */` |
|   495831 | 1399 | `				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);` |
|   247918 | 1400 | `			}else{` |
|        - | 1401 | `				/* No expression before comma */` |
|      ! 0 | 1402 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|      ! 0 | 1403 | `					(iCur < nToken && apNode[iCur]) ? apNode[iCur]->pStart->nLine : pOp->pStart->nLine,` |
|        - | 1404 | `					"syntax error, unexpected token \",\"");` |
|      ! 0 | 1405 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 | 1406 | `					rc = SXERR_SYNTAX;` |
|      ! 0 | 1407 | `				}` |
|      ! 0 | 1408 | `				return rc;` |
|        - | 1409 | `			}` |
|   247918 | 1410 | `		}else{` |
|        - | 1411 | `			/* Comma with no preceding argument */` |
|        9 | 1412 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[iCur]->pStart->nLine,"syntax error, unexpected token \",\"");` |
|        9 | 1413 | `			if( rc != SXERR_ABORT ){` |
|        9 | 1414 | `				rc = SXERR_SYNTAX;` |
|        3 | 1415 | `			}` |
|        9 | 1416 | `			return rc;` |
|        - | 1417 | `		}` |
|        - | 1418 | `		/* Jump trailing comma */` |
|   495831 | 1419 | `		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){` |
|    75407 | 1420 | `			iCur++;` |
|    75407 | 1421 | `			if( iCur >= nToken ){` |
|        - | 1422 | `				/* Trailing comma after last argument */` |
|       19 | 1423 | `				break;` |
|        - | 1424 | `			}` |
|    37692 | 1425 | `		}` |
|        5 | 1426 | `	}` |
|   420447 | 1427 | `	return SXRET_OK;` |
|   210230 | 1428 | `}` |
|        - | 1429 | ` /*` |
|        - | 1430 | `  * Create an expression tree from an array of tokens.` |
|        - | 1431 | `  * If successful, the root of the tree is stored in apNode[0].` |
|        - | 1432 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 1433 | `  */` |
|  1641088 | 1434 | ` static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)` |
|        5 | 1435 | ` {` |
|        - | 1436 | `	 sxi32 i,iLeft,iRight;` |
|        - | 1437 | `	 ph7_expr_node *pNode;` |
|        - | 1438 | `	 sxi32 iCur;` |
|        - | 1439 | `	 sxi32 rc;` |
|  1641093 | 1440 | `	 if( nToken <= 0 \|\| (nToken == 1 && apNode[0]->xCode) ){` |
|        - | 1441 | `		 /* TICKET 1433-17: self evaluating node */` |
|   768683 | 1442 | `		 return SXRET_OK;` |
|        - | 1443 | `	 }` |
|        - | 1444 | `	 /* Process expressions enclosed in parenthesis first */` |
|  5415591 | 1445 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1446 | `		 sxi32 iNest;` |
|        - | 1447 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1448 | `		  * since the LPAREN token can also be an operator [i.e: Function call].` |
|        - | 1449 | `		  */` |
|  4543183 | 1450 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){` |
|  4510771 | 1451 | `			 continue;` |
|        - | 1452 | `		 }` |
|    32417 | 1453 | `		 iNest = 1;` |
|    32417 | 1454 | `		 iLeft = iCur;` |
|        - | 1455 | `		 /* Find the closing parenthesis */` |
|    32417 | 1456 | `		 iCur++;` |
|   216083 | 1457 | `		 while( iCur < nToken ){` |
|   216083 | 1458 | `			 if( apNode[iCur] ){` |
|   216083 | 1459 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){` |
|        - | 1460 | `					 /* Decrement nesting level */` |
|    56299 | 1461 | `					 iNest--;` |
|    56299 | 1462 | `					 if( iNest <= 0 ){` |
|    32417 | 1463 | `						 break;` |
|        5 | 1464 | `					 }` |
|   171730 | 1465 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){` |
|        - | 1466 | `					 /* Increment nesting level */` |
|    23887 | 1467 | `					 iNest++;` |
|    11941 | 1468 | `				 }` |
|    91833 | 1469 | `			 }` |
|   183671 | 1470 | `			 iCur++;` |
|        5 | 1471 | `		 }` |
|    32417 | 1472 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1473 | `			 sxi32 j;` |
|        - | 1474 | `			 /* Recurse and process this expression */` |
|    32417 | 1475 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|    32417 | 1476 | `			 if( rc != SXRET_OK ){` |
|        3 | 1477 | `				 return rc;` |
|        - | 1478 | `			 }` |
|        - | 1479 | `			 /* Mark the subtree root as coming from an explicit parenthesised` |
|        - | 1480 | `			  * group. Consumed by the ** precedence-5 phase so it does not` |
|        - | 1481 | `			  * hoist a unary operator that the user explicitly isolated. */` |
|    32415 | 1482 | `			 for( j = iLeft + 1 ; j < iCur ; ++j ){` |
|    32415 | 1483 | `				 if( apNode[j] ){` |
|    32415 | 1484 | `					 apNode[j]->iFlags \|= EXPR_NODE_PARENS;` |
|    32415 | 1485 | `					 break;` |
|        - | 1486 | `				 }` |
|      ! 0 | 1487 | `			 }` |
|    16205 | 1488 | `		 }` |
|        - | 1489 | `		 /* Free the left and right nodes */` |
|    32415 | 1490 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|    32415 | 1491 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|    32415 | 1492 | `		 apNode[iLeft] = 0;` |
|    32415 | 1493 | `		 apNode[iCur] = 0;` |
|    16210 | 1494 | `	 }` |
|        - | 1495 | `	  /* Process expressions enclosed in braces */` |
|  5623295 | 1496 | `	 for( iCur =  0 ; iCur < nToken ; ++iCur ){` |
|        - | 1497 | `		 sxi32 iNest;` |
|        - | 1498 | `		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator` |
|        - | 1499 | `		  * since the OCB '{' token can also be an operator [i.e: subscripting].` |
|        - | 1500 | `		  */` |
|  4759235 | 1501 | `		 if( apNode[iCur] == 0 \|\| apNode[iCur]->pStart->nType != PH7_TK_OCB ){` |
|  4759227 | 1502 | `			 continue;` |
|        - | 1503 | `		 }` |
|       10 | 1504 | `		 iNest = 1;` |
|       10 | 1505 | `		 iLeft = iCur;` |
|        - | 1506 | `		 /* Find the closing parenthesis */` |
|       10 | 1507 | `		 iCur++;` |
|       16 | 1508 | `		 while( iCur < nToken ){` |
|       16 | 1509 | `			 if( apNode[iCur] ){` |
|       16 | 1510 | `				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){` |
|        - | 1511 | `					 /* Decrement nesting level */` |
|       10 | 1512 | `					 iNest--;` |
|       10 | 1513 | `					 if( iNest <= 0 ){` |
|       10 | 1514 | `						 break;` |
|      ! 0 | 1515 | `					 }` |
|        7 | 1516 | `				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){` |
|        - | 1517 | `					 /* Increment nesting level */` |
|      ! 0 | 1518 | `					 iNest++;` |
|      ! 0 | 1519 | `				 }` |
|        3 | 1520 | `			 }` |
|        7 | 1521 | `			 iCur++;` |
|        1 | 1522 | `		 }` |
|       10 | 1523 | `		 if( iCur - iLeft > 1 ){` |
|        - | 1524 | `			 /* Recurse and process this expression */` |
|        7 | 1525 | `			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);` |
|        7 | 1526 | `			 if( rc != SXRET_OK ){` |
|      ! 0 | 1527 | `				 return rc;` |
|        - | 1528 | `			 }` |
|        3 | 1529 | `		 }` |
|        - | 1530 | `		 /* Free the left and right nodes */` |
|       10 | 1531 | `		 ExprFreeTree(&(*pGen),apNode[iLeft]);` |
|       10 | 1532 | `		 ExprFreeTree(&(*pGen),apNode[iCur]);` |
|       10 | 1533 | `		 apNode[iLeft] = 0;` |
|       10 | 1534 | `		 apNode[iCur] = 0;` |
|        6 | 1535 | `	 }` |
|        - | 1536 | `	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */` |
|   864065 | 1537 | `	 iLeft = -1;` |
|  5623273 | 1538 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4759225 | 1539 | `		 if( apNode[iCur] == 0 ){` |
|  1875161 | 1540 | `			 continue;` |
|        - | 1541 | `		 }` |
|  2884069 | 1542 | `		 pNode = apNode[iCur];` |
|  2884069 | 1543 | `		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){` |
|   771995 | 1544 | `			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1545 | `				 /* Collect function arguments */` |
|   482229 | 1546 | `				 sxi32 iPtr = 0;` |
|   482229 | 1547 | `				 sxi32 nFuncTok = 0;` |
|  1771371 | 1548 | `				 while( nFuncTok + iCur < nToken ){` |
|  1771371 | 1549 | `					 if( apNode[nFuncTok+iCur] ){` |
|  1756059 | 1550 | `						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){` |
|   502781 | 1551 | `							 iPtr++;` |
|  1504671 | 1552 | `						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){` |
|   502781 | 1553 | `							 iPtr--;` |
|   502781 | 1554 | `							 if( iPtr <= 0 ){` |
|   482229 | 1555 | `								 break;` |
|        - | 1556 | `							 }` |
|    10276 | 1557 | `						 }` |
|   636915 | 1558 | `					 }` |
|  1289147 | 1559 | `					 nFuncTok++;` |
|        5 | 1560 | `				 }` |
|   482229 | 1561 | `				 if( nFuncTok + iCur >= nToken ){` |
|        - | 1562 | `					 /* Syntax error */` |
|      ! 0 | 1563 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");` |
|      ! 0 | 1564 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1565 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1566 | `					 }` |
|      ! 0 | 1567 | `					 return rc;` |
|        - | 1568 | `				 }` |
|   482229 | 1569 | `				 if(  iLeft < 0 \|\| !NODE_ISTERM(iLeft) /*\|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){` |
|        - | 1570 | `					 /* Syntax error */` |
|      ! 0 | 1571 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");` |
|      ! 0 | 1572 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1573 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1574 | `					 }` |
|      ! 0 | 1575 | `					 return rc;` |
|        - | 1576 | `				 }` |
|   482229 | 1577 | `				 if( nFuncTok > 1 ){` |
|        - | 1578 | `					 /* Process function arguments */` |
|   420455 | 1579 | `					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);` |
|   420455 | 1580 | `					 if( rc != SXRET_OK ){` |
|       11 | 1581 | `						 return rc;` |
|        - | 1582 | `					 }` |
|   210221 | 1583 | `				 }` |
|        - | 1584 | `				 /* Link the node to the tree */` |
|   482221 | 1585 | `				 pNode->pLeft = apNode[iLeft];` |
|   482221 | 1586 | `				 apNode[iLeft] = 0;` |
|  1771339 | 1587 | `				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){` |
|  1289123 | 1588 | `					 apNode[iCur+iPtr] = 0;` |
|   644564 | 1589 | `				 }` |
|        - | 1590 | ``				 /* PHP 8.4: `new ClassName(args)` may be the left operand of a`` |
|        - | 1591 | `` 				  * postfix operator without wrapping parens — `new C()->m()` `` |
|        - | 1592 | ``				  * means `(new C())->m()`, not `new (C()->m())`. If this call's`` |
|        - | 1593 | ``				  * callee is immediately preceded by a `new` operator, fold the`` |
|        - | 1594 | `				  * constructor call into that new-node NOW, before the postfix` |
|        - | 1595 | `				  * operators bind, and relocate the completed new-node onto this` |
|        - | 1596 | `				  * call slot so a trailing ->/::/[]/call picks it up as its left` |
|        - | 1597 | ``				  * operand. `new C` without a constructor-arg '(' never reaches`` |
|        - | 1598 | `				  * this branch, so it keeps the legacy precedence-1 path (and` |
|        - | 1599 | ``				  * `new C->m()` stays a parse error, like PHP). */`` |
|        - | 1600 | `				 {` |
|   482221 | 1601 | `					 sxi32 iNew = iLeft - 1;` |
|   484301 | 1602 | `					 while( iNew >= 0 && apNode[iNew] == 0 ){` |
|     2085 | 1603 | `						 iNew--;` |
|        5 | 1604 | `					 }` |
|   494467 | 1605 | `					 if( iNew >= 0 && apNode[iNew]->pOp` |
|   228888 | 1606 | `						 && apNode[iNew]->pOp->iOp == EXPR_OP_NEW` |
|   126695 | 1607 | `						 && apNode[iNew]->pLeft == 0 ){` |
|    24507 | 1608 | `						 apNode[iNew]->pLeft = pNode; /* new -> ClassName(args) */` |
|    24507 | 1609 | `						 apNode[iCur] = apNode[iNew]; /* relocate onto the call slot */` |
|    24507 | 1610 | `						 apNode[iNew] = 0;` |
|    24507 | 1611 | `						 pNode = apNode[iCur];` |
|    12256 | 1612 | `					 }` |
|        - | 1613 | `				 }` |
|   530879 | 1614 | `			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|        - | 1615 | `				 /* Subscripting */` |
|    98377 | 1616 | `				 sxi32 iArrTok = iCur + 1;` |
|    98377 | 1617 | `				 sxi32 iNest = 1;` |
|    98373 | 1618 | `				 if(  iLeft < 0 \|\| apNode[iLeft] == 0 \|\| (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&` |
|       18 | 1619 | `					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString &&` |
|       14 | 1620 | `					 apNode[iLeft]->xCode != PH7_CompileHereDoc && apNode[iLeft]->xCode != PH7_CompileNowDoc &&` |
|       13 | 1621 | `					 apNode[iLeft]->xCode != PH7_CompileArray && apNode[iLeft]->xCode != PH7_CompileShortArray ) ) \|\|` |
|    98372 | 1622 | `					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */` |
|        - | 1623 | ``						 /* PHP 8.4: a folded `new C()` (precedence-1 op) is a valid`` |
|        - | 1624 | ``						  * subscript base — `new C()[0]` means `(new C())[0]`. */`` |
|      217 | 1625 | `						 && apNode[iLeft]->pOp->iOp != EXPR_OP_NEW ) ){` |
|        - | 1626 | `						 /* Syntax error */` |
|      ! 0 | 1627 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");` |
|      ! 0 | 1628 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1629 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1630 | `						 }` |
|      ! 0 | 1631 | `						 return rc;` |
|        - | 1632 | `				 }` |
|        - | 1633 | `				 /* Collect index tokens */` |
|   177635 | 1634 | `				 while( iArrTok < nToken ){` |
|   177635 | 1635 | `					 if( apNode[iArrTok] ){` |
|   177603 | 1636 | `						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){` |
|        - | 1637 | `							 /* Increment nesting level */` |
|      ! 0 | 1638 | `							 iNest++;` |
|   177603 | 1639 | `						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){` |
|        - | 1640 | `							 /* Decrement nesting level */` |
|    98377 | 1641 | `							 iNest--;` |
|    98377 | 1642 | `							 if( iNest <= 0 ){` |
|    98377 | 1643 | `								 break;` |
|        - | 1644 | `							 }` |
|      ! 0 | 1645 | `						 }` |
|    39613 | 1646 | `					 }` |
|    79263 | 1647 | `					 ++iArrTok;` |
|        5 | 1648 | `				 }` |
|    98377 | 1649 | `				 if( iArrTok > iCur + 1 ){` |
|        - | 1650 | `					 /* Recurse and process this expression */` |
|    79127 | 1651 | `					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);` |
|    79127 | 1652 | `					 if( rc != SXRET_OK ){` |
|      ! 0 | 1653 | `						 return rc;` |
|        - | 1654 | `					 }` |
|        - | 1655 | `					 /* Link the node to it's index */` |
|    79127 | 1656 | `					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);` |
|    39561 | 1657 | `				 }` |
|        - | 1658 | `				 /* Link the node to the tree */` |
|    98377 | 1659 | `				 pNode->pLeft = apNode[iLeft];` |
|    98377 | 1660 | `				 pNode->pRight = 0;` |
|    98377 | 1661 | `				 apNode[iLeft] = 0;` |
|   276007 | 1662 | `				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){` |
|   177635 | 1663 | `					 apNode[iNest] = 0;` |
|    88820 | 1664 | `				 }` |
|    49191 | 1665 | `			 }else{` |
|        - | 1666 | `				 /* Member access operators [i.e: '->','::'] */` |
|   191399 | 1667 | `				  iRight = iCur + 1;` |
|   191405 | 1668 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|        7 | 1669 | `					 iRight++;` |
|        1 | 1670 | `				 }` |
|   191399 | 1671 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1672 | `					 /* Syntax error */` |
|        5 | 1673 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);` |
|        5 | 1674 | `					 if( rc != SXERR_ABORT ){` |
|        5 | 1675 | `						 rc = SXERR_SYNTAX;` |
|        2 | 1676 | `					 }` |
|        5 | 1677 | `					 return rc;` |
|        - | 1678 | `				 }` |
|        - | 1679 | `				 /* Link the node to the tree */` |
|   191395 | 1680 | `				 pNode->pLeft = apNode[iLeft];` |
|   286833 | 1681 | `				 if( (pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW /*'?->'*/)` |
|   191206 | 1682 | `					 && pNode->pLeft->pOp == 0 &&` |
|   190886 | 1683 | `					 pNode->pLeft->xCode != PH7_CompileVariable ){` |
|        - | 1684 | `						 /* Syntax error */` |
|      ! 0 | 1685 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1686 | `							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);` |
|      ! 0 | 1687 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1688 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1689 | `						 }` |
|      ! 0 | 1690 | `						 return rc;` |
|        - | 1691 | `				 }` |
|   191395 | 1692 | `				 pNode->pRight = apNode[iRight];` |
|   191395 | 1693 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|        - | 1694 | `			 }` |
|   385989 | 1695 | `		 }` |
|  2884057 | 1696 | `		 iLeft = iCur;` |
|  1442031 | 1697 | `	 }` |
|        - | 1698 | `	 /* Handle left associative (new, clone) operators */` |
|  5623241 | 1699 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4759193 | 1700 | `		 if( apNode[iCur] == 0 ){` |
|  2671895 | 1701 | `			 continue;` |
|        - | 1702 | `		 }` |
|  2087303 | 1703 | `		 pNode = apNode[iCur];` |
|  2087303 | 1704 | `		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){` |
|        - | 1705 | `			 SyToken *pToken;` |
|        - | 1706 | `			 /* Get the left node */` |
|      259 | 1707 | `			 iLeft = iCur + 1;` |
|      261 | 1708 | `			 while( iLeft < nToken && apNode[iLeft] == 0 ){` |
|        3 | 1709 | `				 iLeft++;` |
|        1 | 1710 | `			 }` |
|      259 | 1711 | `			 if( iLeft >= nToken \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1712 | `				  /* Syntax error */` |
|      ! 0 | 1713 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",` |
|      ! 0 | 1714 | `					 &pNode->pOp->sOp);` |
|      ! 0 | 1715 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1716 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1717 | `				 }` |
|      ! 0 | 1718 | `				 return rc;` |
|        - | 1719 | `			 }` |
|        - | 1720 | `			 /* Make sure the operand are of a valid type */` |
|      259 | 1721 | `			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1722 | `				 /* Clone:` |
|        - | 1723 | `				  * Symisc eXtension: 'clone' accepts now as it's left operand:` |
|        - | 1724 | `				  *  ++ function call (including annonymous)` |
|        - | 1725 | `				  *  ++ array member` |
|        - | 1726 | `				  *  ++ 'new' operator` |
|        - | 1727 | `				  * Example:` |
|        - | 1728 | `				  *   clone $pObj;` |
|        - | 1729 | `				  *   clone obj(); // function obj(){ return new Class(); }` |
|        - | 1730 | `				  *   clone $a['object']; // $a = array('object' => new Class());` |
|        - | 1731 | `				  */` |
|       40 | 1732 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|       38 | 1733 | `					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){` |
|      ! 0 | 1734 | `						 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1735 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",` |
|      ! 0 | 1736 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1737 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1738 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1739 | `						 }` |
|      ! 0 | 1740 | `						 return rc;` |
|        - | 1741 | `					 }` |
|       17 | 1742 | `				 }` |
|       22 | 1743 | `			 }else{` |
|        - | 1744 | `				 /* New */` |
|      218 | 1745 | `				 if( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iOp == EXPR_OP_NEW` |
|        5 | 1746 | `					 && (apNode[iLeft]->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        - | 1747 | ``					 /* `new new C()` — the operand of `new` is a class-name`` |
|        - | 1748 | `` 					  * reference and cannot itself be an unparenthesized `new` `` |
|        - | 1749 | `					  * expression (PHP parse error). The postfix pass folds` |
|        - | 1750 | ``					  * `new C()` into a completed term, so guard against the`` |
|        - | 1751 | ``					  * outer `new` accepting it here. `new (new C())` is allowed`` |
|        - | 1752 | `					  * (the inner is a parenthesized group). */` |
|      ! 0 | 1753 | `					 pToken = apNode[iLeft]->pStart;` |
|      ! 0 | 1754 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1755 | `						 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1756 | `						 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1757 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1758 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1759 | `					 }` |
|      ! 0 | 1760 | `					 return rc;` |
|        - | 1761 | `				 }` |
|      223 | 1762 | `				 if( apNode[iLeft]->pOp == 0 ){` |
|      223 | 1763 | `					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;` |
|      218 | 1764 | `					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString` |
|       31 | 1765 | `						 && xCons != PH7_CompileAnnonClass){` |
|      ! 0 | 1766 | `						 pToken = apNode[iLeft]->pStart;` |
|        - | 1767 | `						 /* Syntax error */` |
|      ! 0 | 1768 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1769 | `							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",` |
|      ! 0 | 1770 | `							 &pNode->pOp->sOp,&pToken->sData);` |
|      ! 0 | 1771 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1772 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1773 | `						 }` |
|      ! 0 | 1774 | `						 return rc;` |
|        - | 1775 | `					 }` |
|      109 | 1776 | `				 }` |
|        - | 1777 | `			 }` |
|        - | 1778 | `			  /* Link the node to the tree */` |
|      259 | 1779 | `			 pNode->pLeft = apNode[iLeft];` |
|      259 | 1780 | `			 apNode[iLeft] = 0;` |
|      259 | 1781 | `			 pNode->pRight = 0; /* Paranoid */` |
|      127 | 1782 | `		 }` |
|  1043654 | 1783 | `	 }` |
|        - | 1784 | `	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */` |
|   864053 | 1785 | `	 iLeft = -1;` |
|  5623241 | 1786 | `	 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4755019 | 1787 | `		 if( apNode[iCur] == 0 ){` |
|  2671895 | 1788 | `			 continue;` |
|        - | 1789 | `		 }` |
|  2083129 | 1790 | `		 pNode = apNode[iCur];` |
|  2083129 | 1791 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11553 | 1792 | `			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)` |
|     4211 | 1793 | `				 \|\| apNode[iLeft]->xCode == PH7_CompileVariable) ){` |
|        - | 1794 | `					 /* Link the node to the tree */` |
|     4223 | 1795 | `					 pNode->pLeft = apNode[iLeft];` |
|     4223 | 1796 | `					 apNode[iLeft] = 0;` |
|     2109 | 1797 | `			 }` |
|     7861 | 1798 | `		  }` |
|  2087303 | 1799 | `		 iLeft = iCur;` |
|  1043654 | 1800 | `	  }` |
|   868227 | 1801 | `	 iLeft = -1;` |
|  5627415 | 1802 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4759193 | 1803 | `		 if( apNode[iCur] == 0 ){` |
|  2676113 | 1804 | `			 continue;` |
|        - | 1805 | `		 }` |
|  2083085 | 1806 | `		 pNode = apNode[iCur];` |
|  2083085 | 1807 | `		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){` |
|    11507 | 1808 | `			 if( iLeft < 0 \|\| (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)` |
|    11509 | 1809 | `				 \|\| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){` |
|        - | 1810 | `					 /* Syntax error */` |
|      ! 0 | 1811 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);` |
|      ! 0 | 1812 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1813 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 1814 | `					 }` |
|      ! 0 | 1815 | `					 return rc;` |
|        - | 1816 | `			 }` |
|        - | 1817 | `			 /* Link the node to the tree */` |
|    11509 | 1818 | `			 pNode->pLeft = apNode[iLeft];` |
|    11509 | 1819 | `			 apNode[iLeft] = 0;` |
|        - | 1820 | `			 /* Mark as pre-increment/decrement node */` |
|    11509 | 1821 | `			 pNode->iFlags \|= EXPR_NODE_PRE_INCR;` |
|     5752 | 1822 | `		  }` |
|  2083085 | 1823 | `		 iLeft = iCur;` |
|  1041545 | 1824 | `	 }` |
|        - | 1825 | `	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/` |
|   868227 | 1826 | `	  iLeft = 0;` |
|  5627409 | 1827 | `	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){` |
|  4759189 | 1828 | `		  if( apNode[iCur] ){` |
|  2071577 | 1829 | `			  pNode = apNode[iCur];` |
|  2071577 | 1830 | `			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){` |
|    36095 | 1831 | `				  if( iLeft > 0 ){` |
|        - | 1832 | `					  /* Link the node to the tree */` |
|    36093 | 1833 | `					  pNode->pLeft = apNode[iLeft];` |
|    36093 | 1834 | `					  apNode[iLeft] = 0;` |
|    36093 | 1835 | `					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){` |
|       73 | 1836 | `						  if( pNode->pLeft->pLeft == 0 \|\| pNode->pLeft->pRight == 0 ){` |
|        - | 1837 | `							   /* Syntax error */` |
|      ! 0 | 1838 | `							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);` |
|      ! 0 | 1839 | `							  if( rc != SXERR_ABORT ){` |
|      ! 0 | 1840 | `								  rc = SXERR_SYNTAX;` |
|      ! 0 | 1841 | `							  }` |
|      ! 0 | 1842 | `							  return rc;` |
|        - | 1843 | `						  }` |
|       36 | 1844 | `					  }` |
|    18049 | 1845 | `				  }else{` |
|        - | 1846 | `					  /* Syntax error */` |
|        3 | 1847 | `					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);` |
|        3 | 1848 | `					  if( rc != SXERR_ABORT ){` |
|        3 | 1849 | `						  rc = SXERR_SYNTAX;` |
|        1 | 1850 | `					  }` |
|        3 | 1851 | `					  return rc;` |
|        - | 1852 | `				  }` |
|    18044 | 1853 | `			  }` |
|        - | 1854 | `			  /* Save terminal position */` |
|  2071575 | 1855 | `			  iLeft = iCur;` |
|  1035785 | 1856 | `		  }` |
|  2379596 | 1857 | `	  }` |
|        - | 1858 | `	 /* Process right-associative binary operators at precedence 5 (**):` |
|        - | 1859 | `	  * PHP's exponentiation is right-associative (2**3**2 == 512) and binds` |
|        - | 1860 | `	  * tighter than *. Walk right-to-left so the rightmost ** collapses first,` |
|        - | 1861 | `	  * yielding a right-leaning tree. */` |
|  5627407 | 1862 | `	 for( iCur = nToken - 1 ; iCur >= 0 ; --iCur ){` |
|  4759187 | 1863 | `		 if( apNode[iCur] == 0 ){` |
|  2723817 | 1864 | `			 continue;` |
|        - | 1865 | `		 }` |
|  2035375 | 1866 | `		 pNode = apNode[iCur];` |
|  2035375 | 1867 | `		 if( pNode->pOp && pNode->pOp->iPrec == 5 && pNode->pLeft == 0 ){` |
|        - | 1868 | `			 sxi32 iL, iR;` |
|        - | 1869 | `			 /* Find the right operand */` |
|      113 | 1870 | `			 iR = -1;` |
|        - | 1871 | `			 {` |
|        - | 1872 | `				 sxi32 j;` |
|      125 | 1873 | `				 for( j = iCur + 1 ; j < nToken ; ++j ){` |
|      125 | 1874 | `					 if( apNode[j] ){ iR = j; break; }` |
|        7 | 1875 | `				 }` |
|        - | 1876 | `			 }` |
|        - | 1877 | `			 /* Find the left operand */` |
|      113 | 1878 | `			 iL = -1;` |
|        - | 1879 | `			 {` |
|        - | 1880 | `				 sxi32 j;` |
|      181 | 1881 | `				 for( j = iCur - 1 ; j >= 0 ; --j ){` |
|      181 | 1882 | `					 if( apNode[j] ){ iL = j; break; }` |
|       35 | 1883 | `				 }` |
|        - | 1884 | `			 }` |
|      113 | 1885 | `			 if( iR < 0 \|\| iL < 0 \|\| !NODE_ISTERM(iR) \|\| !NODE_ISTERM(iL) ){` |
|      ! 0 | 1886 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|      ! 0 | 1887 | `					 "'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 1888 | `				 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1889 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 1890 | `				 }` |
|      ! 0 | 1891 | `				 return rc;` |
|        - | 1892 | `			 }` |
|      113 | 1893 | `			 pNode->pLeft  = apNode[iL];` |
|      113 | 1894 | `			 pNode->pRight = apNode[iR];` |
|      113 | 1895 | `			 apNode[iL] = 0;` |
|      113 | 1896 | `			 apNode[iR] = 0;` |
|        - | 1897 | `			 /* PHP compat: ** binds tighter than unary -,+,~,!,(cast),@.` |
|        - | 1898 | `			  * The unary phase already attached its operand (pLeft) before` |
|        - | 1899 | ``			  * we ran, so `-X ** Y` currently looks like (-X) ** Y. Push`` |
|        - | 1900 | `			  * the ** beneath the deepest unary so we get -(X ** Y). For` |
|        - | 1901 | ``			  * chains (e.g. `- -2 ** 2`), preserve the original unary order`` |
|        - | 1902 | `			  * — the outermost unary stays outermost. The error-suppression` |
|        - | 1903 | `			  * operator '@' is treated identically to the other unaries:` |
|        - | 1904 | ``			  * PHP also parses `**` as tighter than `@`, so `@-2 ** 2` must`` |
|        - | 1905 | ``			  * become `@(-(2 ** 2))`, with `@` simply passing through. Stop`` |
|        - | 1906 | `			  * the walk at parenthesised sub-trees so explicitly isolated` |
|        - | 1907 | `			  * operands are respected. */` |
|      129 | 1908 | `			 if( pNode->pLeft && pNode->pLeft->pOp` |
|       74 | 1909 | `				 && pNode->pLeft->pOp->iPrec == 4` |
|       35 | 1910 | `				 && pNode->pLeft->pLeft != 0` |
|       35 | 1911 | `				 && (pNode->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|       27 | 1912 | `				 ph7_expr_node *pHead = pNode->pLeft;` |
|       27 | 1913 | `				 ph7_expr_node *pTail = pHead;` |
|        - | 1914 | `				 /* Walk down to the innermost hoistable unary — the one` |
|        - | 1915 | `				  * whose pLeft is a term or a parenthesised subtree. */` |
|       43 | 1916 | `				 while( pTail->pLeft` |
|       34 | 1917 | `					 && pTail->pLeft->pOp` |
|       23 | 1918 | `					 && pTail->pLeft->pOp->iPrec == 4` |
|       12 | 1919 | `					 && pTail->pLeft->pLeft != 0` |
|       30 | 1920 | `					 && (pTail->pLeft->iFlags & EXPR_NODE_PARENS) == 0 ){` |
|        9 | 1921 | `					 pTail = pTail->pLeft;` |
|        1 | 1922 | `				 }` |
|        - | 1923 | `				 /* Splice pNode (**) between pTail and its former operand. */` |
|       27 | 1924 | `				 pNode->pLeft = pTail->pLeft;` |
|       27 | 1925 | `				 pTail->pLeft = pNode;` |
|       27 | 1926 | `				 apNode[iCur] = pHead;` |
|       13 | 1927 | `			 }` |
|       56 | 1928 | `		 }` |
|  1017690 | 1929 | `	 }` |
|        - | 1930 | `	 /* Process left and non-associative binary operators [i.e: *,/,&&,\|\|...]*/` |
|  9550339 | 1931 | `	 for( i = 7 ; i < 17 ; i++ ){` |
|  8682129 | 1932 | `		 iLeft = -1;` |
| 56273655 | 1933 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 47591541 | 1934 | `			 if( apNode[iCur] == 0 ){` |
| 30687567 | 1935 | `				 continue;` |
|        - | 1936 | `			 }` |
| 16903979 | 1937 | `			 pNode = apNode[iCur];` |
| 16903979 | 1938 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 1939 | `				 /* Get the right node */` |
|   257937 | 1940 | `				 iRight = iCur + 1;` |
|   368843 | 1941 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|   110911 | 1942 | `					 iRight++;` |
|        5 | 1943 | `				 }` |
|   257937 | 1944 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 1945 | `					 /* Syntax error */` |
|       11 | 1946 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|       11 | 1947 | `					 if( rc != SXERR_ABORT ){` |
|       11 | 1948 | `						 rc = SXERR_SYNTAX;` |
|        4 | 1949 | `					 }` |
|       11 | 1950 | `					 return rc;` |
|        - | 1951 | `				 }` |
|   257929 | 1952 | `				 if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1953 | `					 sxi32  iTmp;` |
|        - | 1954 | `					 /* Reference operator [i.e: '&=' ]*/` |
|        - | 1955 | ``					 /* PHP 8.0: `&$a?->b` is a parse error — references`` |
|        - | 1956 | `					  * cannot target a nullsafe chain anywhere. Check the` |
|        - | 1957 | `					  * right operand first since EXPR_OP_REF's operand order` |
|        - | 1958 | `					  * is swapped below. */` |
|       60 | 1959 | `					 if( PH7_ExprContainsNullsafe(apNode[iRight]) ){` |
|        3 | 1960 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 1961 | `							 "Can't use nullsafe operator in write context");` |
|        3 | 1962 | `						 if( rc != SXERR_ABORT ){` |
|        3 | 1963 | `							 rc = SXERR_SYNTAX;` |
|        1 | 1964 | `						 }` |
|        3 | 1965 | `						 return rc;` |
|        - | 1966 | `					 }` |
|       57 | 1967 | `					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE \|\| (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){` |
|        - | 1968 | `						 /* Left operand must be a modifiable l-value */` |
|      ! 0 | 1969 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");` |
|      ! 0 | 1970 | `						 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1971 | `							 rc = SXERR_SYNTAX;` |
|      ! 0 | 1972 | `						 }` |
|      ! 0 | 1973 | `						 return rc;` |
|        - | 1974 | `					 }` |
|       57 | 1975 | `					 if( apNode[iLeft]->pOp == 0 \|\| apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {` |
|       41 | 1976 | `						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){` |
|      ! 0 | 1977 | `							 if( apNode[iRight]->pOp == 0 \|\|  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */` |
|      ! 0 | 1978 | `								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){` |
|      ! 0 | 1979 | `									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        - | 1980 | `										 "Reference operator '&' require a variable not a constant expression as it's right operand");` |
|      ! 0 | 1981 | `									 if( rc != SXERR_ABORT ){` |
|      ! 0 | 1982 | `										 rc = SXERR_SYNTAX;` |
|      ! 0 | 1983 | `									 }` |
|      ! 0 | 1984 | `									 return rc;` |
|        - | 1985 | `							 }` |
|      ! 0 | 1986 | `						 }` |
|       19 | 1987 | `					 }` |
|        - | 1988 | `					 /* Swap operands */` |
|       57 | 1989 | `					 iTmp = iRight;` |
|       57 | 1990 | `					 iRight = iLeft;` |
|       57 | 1991 | `					 iLeft = iTmp;` |
|       27 | 1992 | `				 }` |
|        - | 1993 | `				 /* Link the node to the tree */` |
|   257927 | 1994 | `				 pNode->pLeft = apNode[iLeft];` |
|   257927 | 1995 | `				 pNode->pRight = apNode[iRight];` |
|   257927 | 1996 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|   128961 | 1997 | `			 }` |
| 16903969 | 1998 | `			 iLeft = iCur;` |
|  8451987 | 1999 | `		 }` |
|  4341062 | 2000 | `	 }` |
|        - | 2001 | `	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3)` |
|        - | 2002 | `	  * Note that we do not need a precedence loop here since` |
|        - | 2003 | `	  * we are dealing with a single operator.` |
|        - | 2004 | `	  */` |
|   868215 | 2005 | `	  iLeft = -1;` |
|  5615887 | 2006 | `	  for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
|  4750325 | 2007 | `		  if( apNode[iCur] == 0 ){` |
|  3238839 | 2008 | `			  continue;` |
|        - | 2009 | `		  }` |
|  1511491 | 2010 | `		  pNode = apNode[iCur];` |
|  1511491 | 2011 | `		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){` |
|     2653 | 2012 | `			  sxi32 iNest = 1;` |
|     2653 | 2013 | `			  if( iLeft < 0 \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2014 | `				  /* Missing condition */` |
|        3 | 2015 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);` |
|        3 | 2016 | `				  if( rc != SXERR_ABORT ){` |
|        3 | 2017 | `					  rc = SXERR_SYNTAX;` |
|        1 | 2018 | `				  }` |
|        3 | 2019 | `				  return rc;` |
|        - | 2020 | `			  }` |
|        - | 2021 | `			  /* Get the right node */` |
|     2651 | 2022 | `			  iRight = iCur + 1;` |
|     5581 | 2023 | `			  while( iRight < nToken  ){` |
|     5581 | 2024 | `				  if( apNode[iRight] ){` |
|     5229 | 2025 | `					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){` |
|        - | 2026 | `						  /* Increment nesting level */` |
|      ! 0 | 2027 | `						  ++iNest;` |
|     5229 | 2028 | `					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){` |
|        - | 2029 | `						  /* Decrement nesting level */` |
|     2651 | 2030 | `						  --iNest;` |
|     2651 | 2031 | `						  if( iNest <= 0 ){` |
|     2651 | 2032 | `							  break;` |
|        - | 2033 | `						  }` |
|      ! 0 | 2034 | `					  }` |
|     1289 | 2035 | `				  }` |
|     2935 | 2036 | `				  iRight++;` |
|        5 | 2037 | `			  }` |
|     2651 | 2038 | `			  if( iRight > iCur + 1 ){` |
|        - | 2039 | `				  /* Recurse and process the then expression */` |
|     2583 | 2040 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);` |
|     2583 | 2041 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 2042 | `					  return rc;` |
|        - | 2043 | `				  }` |
|        - | 2044 | `				  /* Link the node to the tree */` |
|     2583 | 2045 | `				  pNode->pLeft = apNode[iCur + 1];` |
|     1289 | 2046 | `			  }else{` |
|        - | 2047 | `				  /* Elvis operator (?:): pLeft stays NULL.` |
|        - | 2048 | `				   * NODE_ISTERM() recognizes this node as complete via pCond. */` |
|        - | 2049 | `			  }` |
|     2651 | 2050 | `			  apNode[iCur + 1] = 0;` |
|     2651 | 2051 | `			  if( iRight + 1 < nToken ){` |
|        - | 2052 | `				  /* Recurse and process the else expression */` |
|     2651 | 2053 | `				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);` |
|     2651 | 2054 | `				  if( rc != SXRET_OK ){` |
|      ! 0 | 2055 | `					  return rc;` |
|        - | 2056 | `				  }` |
|        - | 2057 | `				  /* Link the node to the tree */` |
|     2651 | 2058 | `				  pNode->pRight = apNode[iRight + 1];` |
|     2651 | 2059 | `				  apNode[iRight + 1] =  apNode[iRight] = 0;` |
|     1328 | 2060 | `			  }else{` |
|      ! 0 | 2061 | `				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);` |
|      ! 0 | 2062 | `				  if( rc != SXERR_ABORT ){` |
|      ! 0 | 2063 | `					 rc = SXERR_SYNTAX;` |
|      ! 0 | 2064 | `				 }` |
|      ! 0 | 2065 | `				 return rc;` |
|        - | 2066 | `			  }` |
|        - | 2067 | `			  /* Point to the condition */` |
|     2651 | 2068 | `			  pNode->pCond  = apNode[iLeft];` |
|     2651 | 2069 | `			  apNode[iLeft] = 0;` |
|     2651 | 2070 | `			  break;` |
|        - | 2071 | `		  }` |
|  1508843 | 2072 | `		  iLeft = iCur;` |
|   754424 | 2073 | `	  }` |
|        - | 2074 | `	 /* Process right associative binary operators [i.e: '=','+=','/=']` |
|        - | 2075 | `	  * Note: All right associative binary operators have precedence 18` |
|        - | 2076 | `	  * so there is no need for a precedence loop here.` |
|        - | 2077 | `	  */` |
|   868213 | 2078 | `	 iRight = -1;` |
|  5627211 | 2079 | `	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){` |
|  4759057 | 2080 | `		 if( apNode[iCur] == 0 ){` |
|  3570475 | 2081 | `			 continue;` |
|        - | 2082 | `		 }` |
|  1188587 | 2083 | `		 pNode = apNode[iCur];` |
|  1188587 | 2084 | `		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){` |
|        - | 2085 | `			 /* Get the left node */` |
|   320257 | 2086 | `			 iLeft = iCur - 1;` |
|   463389 | 2087 | `			 while( iLeft >= 0 && apNode[iLeft] == 0 ){` |
|   143137 | 2088 | `				 iLeft--;` |
|        5 | 2089 | `			 }` |
|   320257 | 2090 | `			 if( iLeft < 0 \|\| iRight < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2091 | `				 /* Syntax error */` |
|       44 | 2092 | `				 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2093 | `					 /* PHP-compatible parse error for a malformed null coalescing assignment */` |
|        8 | 2094 | `					 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        4 | 2095 | `						 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        4 | 2096 | `				 }else{` |
|       40 | 2097 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|        - | 2098 | `				 }` |
|       44 | 2099 | `				 if( rc != SXERR_ABORT ){` |
|       42 | 2100 | `					 rc = SXERR_SYNTAX;` |
|       20 | 2101 | `				 }` |
|       44 | 2102 | `				 return rc;` |
|        - | 2103 | `			 }` |
|        - | 2104 | `			 /* PHP 8.0: reject any nullsafe link in an assignment LHS,` |
|        - | 2105 | `			  * including deeper chains like $a?->b->c = 1 and` |
|        - | 2106 | `			  * $a?->b[0] = 1 where the outer op is '->' or '[' but the` |
|        - | 2107 | ``			  * chain still contains a `?->` that cannot participate in`` |
|        - | 2108 | `			  * a write. */` |
|   320215 | 2109 | `			 if( PH7_ExprContainsNullsafe(apNode[iLeft]) ){` |
|       11 | 2110 | `				 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        - | 2111 | `					 "Can't use nullsafe operator in write context");` |
|       11 | 2112 | `				 if( rc != SXERR_ABORT ){` |
|       11 | 2113 | `					 rc = SXERR_SYNTAX;` |
|        4 | 2114 | `				 }` |
|       11 | 2115 | `				 return rc;` |
|        - | 2116 | `			 }` |
|   320207 | 2117 | `			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){` |
|      115 | 2118 | `				 if( pNode->pOp->iVmOp != PH7_OP_STORE \|\|` |
|       82 | 2119 | `					 (apNode[iLeft]->xCode != PH7_CompileList && apNode[iLeft]->xCode != PH7_CompileShortList) ){` |
|        - | 2120 | `					 /* Left operand must be a modifiable l-value */` |
|        6 | 2121 | `					 if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        - | 2122 | `						 /* PHP-compatible parse error for a non-lvalue LHS to null coalescing assignment */` |
|        4 | 2123 | `						 rc = PH7_GenCompileError(pGen,E_PARSE,pNode->pStart->nLine,` |
|        2 | 2124 | `							 "syntax error, unexpected token \"%z\"",&pNode->pOp->sOp);` |
|        2 | 2125 | `					 }else{` |
|        4 | 2126 | `						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,` |
|        2 | 2127 | `							 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);` |
|        - | 2128 | `					 }` |
|        6 | 2129 | `					 if( rc != SXERR_ABORT ){` |
|        6 | 2130 | `						 rc = SXERR_SYNTAX;` |
|        2 | 2131 | `					 }` |
|        6 | 2132 | `					 return rc;` |
|        - | 2133 | `				 }` |
|       40 | 2134 | `			 }` |
|        - | 2135 | `			 /* Link the node to the tree (Reverse) */` |
|   320203 | 2136 | `			 pNode->pLeft = apNode[iRight];` |
|   320203 | 2137 | `			 pNode->pRight = apNode[iLeft];` |
|   320203 | 2138 | `			 apNode[iLeft] = apNode[iRight] = 0;` |
|   160099 | 2139 | `		 }` |
|  1188533 | 2140 | `		 iRight = iCur;` |
|   594269 | 2141 | `	 }` |
|        - | 2142 | `	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */` |
|  4340775 | 2143 | `	 for( i = 19 ; i < 23 ; i++ ){` |
|  3472621 | 2144 | `		 iLeft = -1;` |
| 22508557 | 2145 | `		 for( iCur = 0 ; iCur < nToken ; ++iCur ){` |
| 19035941 | 2146 | `			 if( apNode[iCur] == 0 ){` |
| 15562919 | 2147 | `				 continue;` |
|        - | 2148 | `			 }` |
|  3473027 | 2149 | `			 pNode = apNode[iCur];` |
|  3473027 | 2150 | `			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){` |
|        - | 2151 | `				 /* Get the right node */` |
|       72 | 2152 | `				 iRight = iCur + 1;` |
|      110 | 2153 | `				 while( iRight < nToken && apNode[iRight] == 0 ){` |
|       40 | 2154 | `					 iRight++;` |
|        2 | 2155 | `				 }` |
|       72 | 2156 | `				 if( iRight >= nToken \|\| iLeft < 0 \|\| !NODE_ISTERM(iRight) \|\| !NODE_ISTERM(iLeft) ){` |
|        - | 2157 | `					 /* Syntax error */` |
|      ! 0 | 2158 | `					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);` |
|      ! 0 | 2159 | `					 if( rc != SXERR_ABORT ){` |
|      ! 0 | 2160 | `						 rc = SXERR_SYNTAX;` |
|      ! 0 | 2161 | `					 }` |
|      ! 0 | 2162 | `					 return rc;` |
|        - | 2163 | `				 }` |
|        - | 2164 | `				 /* Link the node to the tree */` |
|       72 | 2165 | `				 pNode->pLeft = apNode[iLeft];` |
|       72 | 2166 | `				 pNode->pRight = apNode[iRight];` |
|       72 | 2167 | `				 apNode[iLeft] = apNode[iRight] = 0;` |
|       35 | 2168 | `			 }` |
|  3473027 | 2169 | `			 iLeft = iCur;` |
|  1736516 | 2170 | `		 }` |
|  1736313 | 2171 | `	 }` |
|        - | 2172 | `	 /* Point to the root of the expression tree */` |
|  4758961 | 2173 | `	 for( iCur = 1 ; iCur < nToken ; ++iCur ){` |
|  3890825 | 2174 | `		 if( apNode[iCur] ){` |
|   825013 | 2175 | `			 if( (apNode[iCur]->pOp \|\| apNode[iCur]->xCode ) && apNode[0] != 0){` |
|       23 | 2176 | `				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);` |
|       23 | 2177 | `				  if( rc != SXERR_ABORT ){` |
|       23 | 2178 | `					  rc = SXERR_SYNTAX;` |
|        9 | 2179 | `				  }` |
|       23 | 2180 | `				  return rc;` |
|        - | 2181 | `			 }` |
|   824995 | 2182 | `			 apNode[0] = apNode[iCur];` |
|   824995 | 2183 | `			 apNode[iCur] = 0;` |
|   412495 | 2184 | `		 }` |
|  1945406 | 2185 | `	 }` |
|   868141 | 2186 | `	 return SXRET_OK;` |
|   818462 | 2187 | ` }` |
|        - | 2188 | ` /*` |
|        - | 2189 | `  * Build an expression tree from the freshly extracted raw tokens.` |
|        - | 2190 | `  * If successful, the root of the tree is stored in ppRoot.` |
|        - | 2191 | `  * When errors,PH7 take care of generating the appropriate error message.` |
|        - | 2192 | `  * This is the public interface used by the most code generator routines.` |
|        - | 2193 | `  */` |
|  1024408 | 2194 | `PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)` |
|        5 | 2195 | `{` |
|        - | 2196 | `	ph7_expr_node **apNode;` |
|        - | 2197 | `	ph7_expr_node *pNode;` |
|        - | 2198 | `	sxi32 rc;` |
|        - | 2199 | `	/* Reset node container */` |
|  1024413 | 2200 | `	SySetReset(pExprNode);` |
|  1024413 | 2201 | `	pNode = 0; /* Prevent compiler warning */` |
|        - | 2202 | `	/* Extract nodes one after one until we hit the end of the input */` |
|        - | 2203 | `	{` |
|  1024413 | 2204 | `		int iLastWasTerm = 0;` |
|  1024413 | 2205 | `		int bAfterMemberOp = 0; /* TRUE iff the previous node was -> / ?-> / :: */` |
|  5552479 | 2206 | `		while( pGen->pIn < pGen->pEnd ){` |
|  4528105 | 2207 | `			rc = ExprExtractNode(&(*pGen),&pNode,iLastWasTerm,bAfterMemberOp);` |
|  4528105 | 2208 | `			if( rc != SXRET_OK ){` |
|       38 | 2209 | `				return rc;` |
|        - | 2210 | `			}` |
|        - | 2211 | `			/* Determine if this node is a term for short-array disambiguation */` |
|  4528071 | 2212 | `			if( pNode->xCode ){` |
|        - | 2213 | `				/* Node with compile handler: variable, literal, string, array, etc. */` |
|  2374609 | 2214 | `				iLastWasTerm = 1;` |
|  3340769 | 2215 | `			}else if( pNode->pOp ){` |
|        - | 2216 | `				/* Operator node */` |
|  1022827 | 2217 | `				iLastWasTerm = 0;` |
|   511416 | 2218 | `			}else{` |
|        - | 2219 | `				/* Delimiter: ')' and ']' end terms */` |
|  1130645 | 2220 | `				iLastWasTerm = (pNode->pStart->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB)) ? 1 : 0;` |
|        - | 2221 | `			}` |
|        - | 2222 | `			/* A keyword in the next node is a member name only right after a member` |
|        - | 2223 | `			 * operator (-> / ?-> / :: — the PH7_OP_MEMBER ops); null in every other` |
|        - | 2224 | `			 * node kind, so this single test covers all branches. */` |
|  4528071 | 2225 | `			bAfterMemberOp = ( pNode->pOp && pNode->pOp->iVmOp == PH7_OP_MEMBER );` |
|        - | 2226 | `			/* Save the extracted node */` |
|  4528071 | 2227 | `			SySetPut(pExprNode,(const void *)&pNode);` |
|        5 | 2228 | `		}` |
|        - | 2229 | `	}` |
|  1024379 | 2230 | `	if( SySetUsed(pExprNode) < 1 ){` |
|        - | 2231 | `		/* Empty expression [i.e: A semi-colon;] */` |
|      ! 0 | 2232 | `		*ppRoot = 0;` |
|      ! 0 | 2233 | `		return SXRET_OK;` |
|        - | 2234 | `	}` |
|  1024379 | 2235 | `	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);` |
|        - | 2236 | `	/* Make sure we are dealing with valid nodes */` |
|  1024379 | 2237 | `	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1024379 | 2238 | `	if( rc != SXRET_OK ){` |
|        - | 2239 | `		/* Don't worry about freeing memory,upper layer will` |
|        - | 2240 | `		 * cleanup the mess left behind.` |
|        - | 2241 | `		 */` |
|       54 | 2242 | `		*ppRoot = 0;` |
|       54 | 2243 | `		return rc;` |
|        - | 2244 | `	}` |
|        - | 2245 | `	/* Build the tree */` |
|  1024329 | 2246 | `	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));` |
|  1024329 | 2247 | `	if( rc != SXRET_OK ){` |
|        - | 2248 | `		/* Something goes wrong [i.e: Syntax error] */` |
|      103 | 2249 | `		*ppRoot = 0;` |
|      103 | 2250 | `		return rc;` |
|        - | 2251 | `	}` |
|        - | 2252 | `	/* Point to the root of the tree */` |
|  1024231 | 2253 | `	*ppRoot = apNode[0];` |
|  1024231 | 2254 | `	return SXRET_OK;` |
|   512209 | 2255 | `}` |
|        - | 2256 |  |
