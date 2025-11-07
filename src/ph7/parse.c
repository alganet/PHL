/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/*
 * This file implement a hand-coded, thread-safe, full-reentrant and highly-efficient
 * expression parser for the PH7 engine.
 * Besides from the one introudced by PHP (Over 60), the PH7 engine have introduced three new
 * operators. These are 'eq', 'ne' and the comma operator ','.
 * The eq and ne operators are borrowed from the Perl world. They are used for strict
 * string comparison. The reason why they have been implemented in the PH7 engine
 * and introduced as an extension to the PHP programming language is due to the confusion
 * introduced by the standard PHP comparison operators ('==' or '===') especially if you
 * are comparing strings with numbers.
 * Take the following example: 
 * var_dump( 0xFF == '255' ); // bool(true) ???
 * // use the type equal operator by adding a single space to one of the operand
 * var_dump( '255  ' === '255' ); //bool(true) depending on the PHP version
 * That is, if one of the operand looks like a number (either integer or float) then PHP
 * will internally convert the two operands to numbers and then a numeric comparison is performed.
 * This is what the PHP language reference manual says:
 * If you compare a number with a string or the comparison involves numerical strings, then each
 * string is converted to a number and the comparison performed numerically. 
 * Bummer, if you ask me,this is broken, badly broken. I mean,the programmer cannot dictate
 * it's comparison rule, it's the underlying engine who decides in it's place and perform
 * the internal conversion. In most cases,PHP developers wants simple string comparison and they
 * are stuck to use the ugly and inefficient strcmp() function and it's variants instead.
 * This is the big reason why we have introduced these two operators.
 * The eq operator is used to compare two strings byte per byte. If you came from the C/C++ world
 * think of this operator as a barebone implementation of the memcmp() C standard library function.
 * Keep in mind that if you are comparing two ASCII strings then the capital letters and their lowercase
 * letters are completely different and so this example will output false.
 * var_dump('allo' eq 'Allo'); //bool(FALSE)
 * The ne operator perform the opposite operation of the eq operator and is used to test for string
 * inequality. This example will output true
 * var_dump('allo' ne 'Allo'); //bool(TRUE) unequal strings
 * The eq operator return a Boolean true if and only if the two strings are identical while the
 * ne operator return a Boolean true if and only if the two strings are different. Otherwise
 * a Boolean false is returned (equal strings).
 * Note that the comparison is performed only if the two strings are of the same length.
 * Otherwise the eq and ne operators return a Boolean false without performing any comparison
 * and avoid us wasting CPU time for nothing.
 * Again remember that we talk about a low level byte per byte comparison and nothing else.
 * Also remember that zero length strings are always equal.
 *
 * Again, another powerful mechanism borrowed from the C/C++ world and introduced as an extension
 * to the PHP programming language.
 * A comma expression contains two operands of any type separated by a comma and has left-to-right
 * associativity. The left operand is fully evaluated, possibly producing side effects, and its
 * value, if there is one, is discarded. The right operand is then evaluated. The type and value
 * of the result of a comma expression are those of its right operand, after the usual unary conversions. 
 * Any number of expressions separated by commas can form a single expression because the comma operator
 * is associative. The use of the comma operator guarantees that the sub-expressions will be evaluated
 * in left-to-right order, and the value of the last becomes the value of the entire expression.
 * The following example assign the value 25 to the variable $a, multiply the value of $a with 2
 * and assign the result to variable $b and finally we call a test function to output the value
 * of $a and $b. Keep-in mind that all theses operations are done in a single expression using
 * the comma operator to create side effect.
 * $a = 25,$b = $a << 1 ,test();
 * //Output the value of $a and $b 
 * function test(){
 *	 global $a,$b;
 *	 echo "\$a = $a \$b= $b\n"; // You should see: $a = 25 $b = 50
 * }
 *
 * For a full discussions on these extensions, please refer to  offical
 * documentation(http://ph7.symisc.net/features.html) or visit the offical forums
 * (http://forums.symisc.net/) if you want to share your point of view.
 *
 * Exprressions: According to the PHP language reference manual
 *
 * Expressions are the most important building stones of PHP. In PHP, almost anything you write is an expression.
 * The simplest yet most accurate way to define an expression is "anything that has a value".
 * The most basic forms of expressions are constants and variables. When you type "$a = 5", you're assigning
 * '5' into $a. '5', obviously, has the value 5, or in other words '5' is an expression with the value of 5
 * (in this case, '5' is an integer constant).
 * After this assignment, you'd expect $a's value to be 5 as well, so if you wrote $b = $a, you'd expect
 * it to behave just as if you wrote $b = 5. In other words, $a is an expression with the value of 5 as well.
 * If everything works right, this is exactly what will happen.
 * Slightly more complex examples for expressions are functions. For instance, consider the following function:
 * <?php
 * function foo ()
 * {
 *   return 5;
 * }
 * ?>
 * Assuming you're familiar with the concept of functions (if you're not, take a look at the chapter about functions)
 * you'd assume that typing $c = foo() is essentially just like writing $c = 5, and you're right.
 * Functions are expressions with the value of their return value. Since foo() returns 5, the value of the expression
 * 'foo()' is 5. Usually functions don't just return a static value but compute something.
 * Of course, values in PHP don't have to be integers, and very often they aren't.
 * PHP supports four scalar value types: integer values, floating point values (float), string values and boolean values
 * (scalar values are values that you can't 'break' into smaller pieces, unlike arrays, for instance).
 * PHP also supports two composite (non-scalar) types: arrays and objects. Each of these value types can be assigned
 * into variables or returned from functions.
 * PHP takes expressions much further, in the same way many other languages do. PHP is an expression-oriented language
 * in the sense that almost everything is an expression. Consider the example we've already dealt with, '$a = 5'.
 * It's easy to see that there are two values involved here, the value of the integer constant '5', and the value 
 * of $a which is being updated to 5 as well. But the truth is that there's one additional value involved here
 * and that's the value of the assignment itself. The assignment itself evaluates to the assigned value, in this case 5.
 * In practice, it means that '$a = 5', regardless of what it does, is an expression with the value 5. Thus, writing 
 * something like '$b = ($a = 5)' is like writing '$a = 5; $b = 5;' (a semicolon marks the end of a statement).
 * Since assignments are parsed in a right to left order, you can also write '$b = $a = 5'.
 * Another good example of expression orientation is pre- and post-increment and decrement.
 * Users of PHP and many other languages may be familiar with the notation of variable++ and variable--.
 * These are increment and decrement operators. In PHP, like in C, there are two types of increment - pre-increment
 * and post-increment. Both pre-increment and post-increment essentially increment the variable, and the effect
 * on the variable is identical. The difference is with the value of the increment expression. Pre-increment, which is written
 * '++$variable', evaluates to the incremented value (PHP increments the variable before reading its value, thus the name 'pre-increment').
 * Post-increment, which is written '$variable++' evaluates to the original value of $variable, before it was incremented 
 * (PHP increments the variable after reading its value, thus the name 'post-increment').
 * A very common type of expressions are comparison expressions. These expressions evaluate to either FALSE or TRUE.
 * PHP supports > (bigger than), >= (bigger than or equal to), == (equal), != (not equal), < (smaller than) and <= (smaller than or equal to).
 * The language also supports a set of strict equivalence operators: === (equal to and same type) and !== (not equal to or not same type).
 * These expressions are most commonly used inside conditional execution, such as if statements.
 * The last example of expressions we'll deal with here is combined operator-assignment expressions.
 * You already know that if you want to increment $a by 1, you can simply write '$a++' or '++$a'.
 * But what if you want to add more than one to it, for instance 3? You could write '$a++' multiple times, but this is obviously not a very
 * efficient or comfortable way. A much more common practice is to write '$a = $a + 3'. '$a + 3' evaluates to the value of $a plus 3
 * and is assigned back into $a, which results in incrementing $a by 3. In PHP, as in several other languages like C, you can write
 * this in a shorter way, which with time would become clearer and quicker to understand as well. Adding 3 to the current value of $a
 * can be written '$a += 3'. This means exactly "take the value of $a, add 3 to it, and assign it back into $a".
 * In addition to being shorter and clearer, this also results in faster execution. The value of '$a += 3', like the value of a regular
 * assignment, is the assigned value. Notice that it is NOT 3, but the combined value of $a plus 3 (this is the value that's assigned into $a).
 * Any two-place operator can be used in this operator-assignment mode, for example '$a -= 5' (subtract 5 from the value of $a), '$b *= 7'
 * (multiply the value of $b by 7), etc.
 * There is one more expression that may seem odd if you haven't seen it in other languages, the ternary conditional operator:
 * <?php
 * $first ? $second : $third
 * ?>
 * If the value of the first subexpression is TRUE (non-zero), then the second subexpression is evaluated, and that is the result
 * of the conditional expression. Otherwise, the third subexpression is evaluated, and that is the value. 
 */
/* Operators associativity */
#define EXPR_OP_ASSOC_LEFT   0x01 /* Left associative operator */
#define EXPR_OP_ASSOC_RIGHT  0x02 /* Right associative operator */
#define EXPR_OP_NON_ASSOC    0x04 /* Non-associative operator */
/* 
 * Operators table
 * This table is sorted by operators priority (highest to lowest) according
 * the PHP language reference manual.
 * PH7 implements all the 60 PHP operators and have introduced the eq and ne operators.
 * The operators precedence table have been improved dramatically so that you can do same
 * amazing things now such as array dereferencing,on the fly function call,anonymous function
 * as array values,class member access on instantiation and so on.
 * Refer to the following page for a full discussion on these improvements:
 * http://ph7.symisc.net/features.html#improved_precedence
 */
static const ph7_expr_op aOpTable[] = {
	/* Precedence 1: non-associative */
	{ {"new",sizeof("new")-1},     EXPR_OP_NEW,   1, EXPR_OP_NON_ASSOC, PH7_OP_NEW  },
	{ {"clone",sizeof("clone")-1}, EXPR_OP_CLONE, 1, EXPR_OP_NON_ASSOC, PH7_OP_CLONE},
	                              /* Postfix operators */
	/* Precedence 2(Highest),left-associative */
	{ {"->",sizeof(char)*2}, EXPR_OP_ARROW,     2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},
	{ {"::",sizeof(char)*2}, EXPR_OP_DC,        2, EXPR_OP_ASSOC_LEFT , PH7_OP_MEMBER},
	{ {"[",sizeof(char)},    EXPR_OP_SUBSCRIPT, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_LOAD_IDX},
	/* Precedence 3,non-associative  */
	{ {"++",sizeof(char)*2}, EXPR_OP_INCR, 3, EXPR_OP_NON_ASSOC , PH7_OP_INCR},
	{ {"--",sizeof(char)*2}, EXPR_OP_DECR, 3, EXPR_OP_NON_ASSOC , PH7_OP_DECR},
	                              /* Unary operators */
	/* Precedence 4,right-associative  */
	{ {"-",sizeof(char)},                 EXPR_OP_UMINUS,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UMINUS },
	{ {"+",sizeof(char)},                 EXPR_OP_UPLUS,     4, EXPR_OP_ASSOC_RIGHT, PH7_OP_UPLUS },
	{ {"~",sizeof(char)},                 EXPR_OP_BITNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_BITNOT },
	{ {"!",sizeof(char)},                 EXPR_OP_LOGNOT,    4, EXPR_OP_ASSOC_RIGHT, PH7_OP_LNOT },
	{ {"@",sizeof(char)},                 EXPR_OP_ALT,       4, EXPR_OP_ASSOC_RIGHT, PH7_OP_ERR_CTRL},
	                             /* Cast operators */
	{ {"(int)",    sizeof("(int)")-1   }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_INT  },
	{ {"(bool)",   sizeof("(bool)")-1  }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_BOOL },
	{ {"(string)", sizeof("(string)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_STR  },
	{ {"(float)",  sizeof("(float)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_REAL },
	{ {"(array)",  sizeof("(array)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_ARRAY},
	{ {"(object)", sizeof("(object)")-1}, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_OBJ  },
	{ {"(unset)",  sizeof("(unset)")-1 }, EXPR_OP_TYPECAST, 4, EXPR_OP_ASSOC_RIGHT, PH7_OP_CVT_NULL },
	                           /* Binary operators */
	/* Precedence 7,left-associative */
	{ {"instanceof",sizeof("instanceof")-1}, EXPR_OP_INSTOF, 7, EXPR_OP_NON_ASSOC, PH7_OP_IS_A},
	{ {"*",sizeof(char)}, EXPR_OP_MUL, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MUL},
	{ {"/",sizeof(char)}, EXPR_OP_DIV, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_DIV},
	{ {"%",sizeof(char)}, EXPR_OP_MOD, 7, EXPR_OP_ASSOC_LEFT , PH7_OP_MOD},
	/* Precedence 8,left-associative */
	{ {"+",sizeof(char)}, EXPR_OP_ADD, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_ADD},
	{ {"-",sizeof(char)}, EXPR_OP_SUB, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_SUB},
	{ {".",sizeof(char)}, EXPR_OP_DOT, 8,  EXPR_OP_ASSOC_LEFT, PH7_OP_CAT},
	/* Precedence 9,left-associative */
	{ {"<<",sizeof(char)*2}, EXPR_OP_SHL, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHL},
	{ {">>",sizeof(char)*2}, EXPR_OP_SHR, 9, EXPR_OP_ASSOC_LEFT, PH7_OP_SHR},
	/* Precedence 10,non-associative */
	{ {"<",sizeof(char)},    EXPR_OP_LT,  10, EXPR_OP_NON_ASSOC, PH7_OP_LT},
	{ {">",sizeof(char)},    EXPR_OP_GT,  10, EXPR_OP_NON_ASSOC, PH7_OP_GT},
	{ {"<=",sizeof(char)*2}, EXPR_OP_LE,  10, EXPR_OP_NON_ASSOC, PH7_OP_LE},
	{ {">=",sizeof(char)*2}, EXPR_OP_GE,  10, EXPR_OP_NON_ASSOC, PH7_OP_GE},
	{ {"<>",sizeof(char)*2}, EXPR_OP_NE,  10, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},
	/* Precedence 11,non-associative */
	{ {"==",sizeof(char)*2},  EXPR_OP_EQ,  11, EXPR_OP_NON_ASSOC, PH7_OP_EQ},
	{ {"!=",sizeof(char)*2},  EXPR_OP_NE,  11, EXPR_OP_NON_ASSOC, PH7_OP_NEQ},
	{ {"eq",sizeof(char)*2},  EXPR_OP_SEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_SEQ}, /* IMP-0137-EQ: Symisc eXtension */
	{ {"ne",sizeof(char)*2},  EXPR_OP_SNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_SNE}, /* IMP-0138-NE: Symisc eXtension */
	{ {"===",sizeof(char)*3}, EXPR_OP_TEQ, 11, EXPR_OP_NON_ASSOC, PH7_OP_TEQ},
	{ {"!==",sizeof(char)*3}, EXPR_OP_TNE, 11, EXPR_OP_NON_ASSOC, PH7_OP_TNE},
	/* Precedence 12,left-associative */
	{ {"&",sizeof(char)}, EXPR_OP_BAND, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_BAND},
	/* Precedence 12,left-associative */
	{ {"=&",sizeof(char)*2}, EXPR_OP_REF, 12, EXPR_OP_ASSOC_LEFT, PH7_OP_STORE_REF},
	                         /* Binary operators */
	/* Precedence 13,left-associative */
	{ {"^",sizeof(char)}, EXPR_OP_XOR,13, EXPR_OP_ASSOC_LEFT, PH7_OP_BXOR},
	/* Precedence 14,left-associative */
	{ {"|",sizeof(char)}, EXPR_OP_BOR,14, EXPR_OP_ASSOC_LEFT, PH7_OP_BOR},
	/* Precedence 15,left-associative */
	{ {"&&",sizeof(char)*2}, EXPR_OP_LAND,15, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},
	/* Precedence 16,left-associative */
	{ {"||",sizeof(char)*2}, EXPR_OP_LOR, 16, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},
	                      /* Ternary operator */
	/* Precedence 17,left-associative */
    { {"?",sizeof(char)}, EXPR_OP_QUESTY, 17, EXPR_OP_ASSOC_LEFT, 0},
	                     /* Combined binary operators */
	/* Precedence 18,right-associative */
	{ {"=",sizeof(char)},     EXPR_OP_ASSIGN,     18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_STORE},
	{ {"+=",sizeof(char)*2},  EXPR_OP_ADD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_ADD_STORE },
	{ {"-=",sizeof(char)*2},  EXPR_OP_SUB_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SUB_STORE },
	{ {".=",sizeof(char)*2},  EXPR_OP_DOT_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_CAT_STORE },
	{ {"*=",sizeof(char)*2},  EXPR_OP_MUL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MUL_STORE },
	{ {"/=",sizeof(char)*2},  EXPR_OP_DIV_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_DIV_STORE },
	{ {"%=",sizeof(char)*2},  EXPR_OP_MOD_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_MOD_STORE },
	{ {"&=",sizeof(char)*2},  EXPR_OP_AND_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BAND_STORE },
	{ {"|=",sizeof(char)*2},  EXPR_OP_OR_ASSIGN,  18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BOR_STORE  },
	{ {"^=",sizeof(char)*2},  EXPR_OP_XOR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_BXOR_STORE },
	{ {"<<=",sizeof(char)*3}, EXPR_OP_SHL_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHL_STORE },
	{ {">>=",sizeof(char)*3}, EXPR_OP_SHR_ASSIGN, 18,  EXPR_OP_ASSOC_RIGHT, PH7_OP_SHR_STORE },
	/* Precedence 19,left-associative */
	{ {"and",sizeof("and")-1},   EXPR_OP_LAND, 19, EXPR_OP_ASSOC_LEFT, PH7_OP_LAND},
	/* Precedence 20,left-associative */
	{ {"xor", sizeof("xor") -1}, EXPR_OP_LXOR, 20, EXPR_OP_ASSOC_LEFT, PH7_OP_LXOR},
	/* Precedence 21,left-associative */
	{ {"or",sizeof("or")-1},     EXPR_OP_LOR,  21, EXPR_OP_ASSOC_LEFT, PH7_OP_LOR},
	/* Precedence 22,left-associative [Lowest operator] */
	{ {",",sizeof(char)},        EXPR_OP_COMMA,22, EXPR_OP_ASSOC_LEFT, 0}, /* IMP-0139-COMMA: Symisc eXtension */
};
/* Function call operator need special handling */
static const ph7_expr_op sFCallOp = {{"(",sizeof(char)}, EXPR_OP_FUNC_CALL, 2, EXPR_OP_ASSOC_LEFT , PH7_OP_CALL};
/*
 * Check if the given token is a potential operator or not.
 * This function is called by the lexer each time it extract a token that may 
 * look like an operator.
 * Return a structure [i.e: ph7_expr_op instnace ] that describe the operator on success.
 * Otherwise NULL.
 * Note that the function take care of handling ambiguity [i.e: whether we are dealing with
 * a binary minus or unary minus.]
 */
PH7_PRIVATE const ph7_expr_op *  PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast)
{
	sxu32 n = 0;
	sxi32 rc;
	/* Do a linear lookup on the operators table */
	for(;;){
		if( n >= SX_ARRAYSIZE(aOpTable) ){
			break;
		}
		if( SyisAlpha(aOpTable[n].sOp.zString[0]) ){
			/* TICKET 1433-012: Alpha stream operators [i.e: and,or,xor,new...] */
			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyStrnicmp);
		}else{
			rc = SyStringCmp(pStr,&aOpTable[n].sOp,SyMemcmp);
		}
		if( rc == 0 ){
			if( aOpTable[n].sOp.nByte != sizeof(char) || (aOpTable[n].iOp != EXPR_OP_UMINUS && aOpTable[n].iOp != EXPR_OP_UPLUS) || pLast == 0 ){
				/* There is no ambiguity here,simply return the first operator seen */
				return &aOpTable[n];
			}
			/* Handle ambiguity */
			if( pLast->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OCB/*'{'*/|PH7_TK_OSB/*'['*/|PH7_TK_COLON/*:*/|PH7_TK_COMMA/*,'*/) ){
				/* Unary opertors have prcedence here over binary operators */
				return &aOpTable[n];
			}
			if( pLast->nType & PH7_TK_OP ){
				const ph7_expr_op *pOp = (const ph7_expr_op *)pLast->pUserData;
				/* Ticket 1433-31: Handle the '++','--' operators case */
				if( pOp->iOp != EXPR_OP_INCR && pOp->iOp != EXPR_OP_DECR ){
					/* Unary opertors have prcedence here over binary operators */
					return &aOpTable[n];
				}
			
			}
		}
		++n; /* Next operator in the table */
	}
	/* No such operator */
	return 0;
}
/*
 * Delimit a set of token stream.
 * This function take care of handling the nesting level and stops when it hit
 * the end of the input or the ending token is found and the nesting level is zero.
 */
PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd)
{
	SyToken *pCur = pIn;
	sxi32 iNest = 1;
	for(;;){
		if( pCur >= pEnd ){
			break;
		}
		if( pCur->nType & nTokStart ){
			/* Increment nesting level */
			iNest++;
		}else if( pCur->nType & nTokEnd ){
			/* Decrement nesting level */
			iNest--;
			if( iNest <= 0 ){
				break;
			}
		}
		/* Advance cursor */
		pCur++;
	}
	/* Point to the end of the chunk */
	*ppEnd = pCur;
}
/*
 * Retrun TRUE if the given ID represent a language construct [i.e: print,echo..]. FALSE otherwise.
 * Note on reserved keywords.
 *  According to the PHP language reference manual:
 *   These words have special meaning in PHP. Some of them represent things which look like 
 *   functions, some look like constants, and so on--but they're not, really: they are language
 *   constructs. You cannot use any of the following words as constants, class names, function
 *   or method names. Using them as variable names is generally OK, but could lead to confusion.
 */
PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc)
{
	if( nKeyID == PH7_TKWRD_ECHO || nKeyID == PH7_TKWRD_PRINT || nKeyID == PH7_TKWRD_INCLUDE
		|| nKeyID == PH7_TKWRD_INCONCE || nKeyID == PH7_TKWRD_REQUIRE || nKeyID == PH7_TKWRD_REQONCE
		){
			return TRUE;
	}
	if( bCheckFunc ){
		if(  nKeyID == PH7_TKWRD_ISSET || nKeyID == PH7_TKWRD_UNSET || nKeyID == PH7_TKWRD_EVAL
			|| nKeyID == PH7_TKWRD_EMPTY || nKeyID == PH7_TKWRD_ARRAY || nKeyID == PH7_TKWRD_LIST
			|| /* TICKET 1433-012 */ nKeyID == PH7_TKWRD_NEW || nKeyID == PH7_TKWRD_CLONE  ){
				return TRUE;
		}
	}
	/* Not a language construct */
	return FALSE; 
}
/*
 * Make sure we are dealing with a valid expression tree.
 * This function check for balanced parenthesis,braces,brackets and so on.
 * When errors,PH7 take care of generating the appropriate error message.
 * Return SXRET_OK on success. Any other return value indicates syntax error.
 */
static sxi32 ExprVerifyNodes(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nNode)
{
	sxi32 iParen,iSquare,iQuesty,iBraces;
	sxi32 i,rc;

	if( nNode > 0 && apNode[0]->pOp && (apNode[0]->pOp->iOp == EXPR_OP_ADD || apNode[0]->pOp->iOp == EXPR_OP_SUB) ){
		/* Fix and mark as an unary not binary plus/minus operator */
		apNode[0]->pOp = PH7_ExprExtractOperator(&apNode[0]->pStart->sData,0);
		apNode[0]->pStart->pUserData = (void *)apNode[0]->pOp;
	}
	iParen = iSquare = iQuesty = iBraces = 0;
	for( i = 0 ; i < nNode ; ++i ){
		if( apNode[i]->pStart->nType & PH7_TK_LPAREN /*'('*/){
			if( i > 0 && ( apNode[i-1]->xCode == PH7_CompileVariable || apNode[i-1]->xCode == PH7_CompileLiteral ||
				(apNode[i - 1]->pStart->nType & (PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_SSTR|PH7_TK_DSTR|PH7_TK_RPAREN/*')'*/|PH7_TK_CSB/*']'*/|PH7_TK_CCB/*'}'*/))) ){
					/* Ticket 1433-033: Take care to ignore alpha-stream [i.e: or,xor] operators followed by an opening parenthesis */
					if( (apNode[i - 1]->pStart->nType & PH7_TK_OP) == 0 ){
						/* We are dealing with a postfix [i.e: function call]  operator
						 * not a simple left parenthesis. Mark the node.
						 */
						apNode[i]->pStart->nType |= PH7_TK_OP;
						apNode[i]->pStart->pUserData = (void *)&sFCallOp; /* Function call operator */
						apNode[i]->pOp = &sFCallOp;
					}
			}
			iParen++;
		}else if( apNode[i]->pStart->nType & PH7_TK_RPAREN/*')*/){
			if( iParen <= 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ')'");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				return rc;
			}
			iParen--;
		}else if( apNode[i]->pStart->nType & PH7_TK_OSB /*'['*/){
			iSquare++;
		}else if (apNode[i]->pStart->nType & PH7_TK_CSB /*']'*/){
			if( iSquare <= 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ']'");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				return rc;
			}
			iSquare--;
		}else if( apNode[i]->pStart->nType & PH7_TK_OCB /*'{'*/){
			iBraces++;
			if( i > 0 && ( apNode[i - 1]->xCode == PH7_CompileVariable || (apNode[i - 1]->pStart->nType & PH7_TK_CSB/*]*/)) ){
				const ph7_expr_op *pOp,*pEnd;
				int iNest = 1;
				sxi32 j=i+1;
				/* 
				 * Dirty Hack: $a{'x'} == > $a['x']
				 */
				apNode[i]->pStart->nType &= ~PH7_TK_OCB /*'{'*/;
				apNode[i]->pStart->nType |= PH7_TK_OSB /*'['*/;
				pOp =  aOpTable;
				pEnd = &pOp[sizeof(aOpTable)];
				while( pOp < pEnd ){
					if( pOp->iOp == EXPR_OP_SUBSCRIPT ){
						break;
					}
					pOp++;
				}
				if( pOp >= pEnd ){
					pOp = 0;
				}
				if( pOp ){
					apNode[i]->pOp = pOp;
					apNode[i]->pStart->nType |= PH7_TK_OP;
				}
				iBraces--;
				iSquare++;
				while( j < nNode ){
					if( apNode[j]->pStart->nType & PH7_TK_OCB /*{*/){
						/* Increment nesting level */
						iNest++;
					}else if( apNode[j]->pStart->nType & PH7_TK_CCB/*}*/ ){
						/* Decrement nesting level */
						iNest--;
						if( iNest < 1 ){
							break;
						}
					}
					j++;
				}
				if( j < nNode ){
					apNode[j]->pStart->nType &= ~PH7_TK_CCB /*'}'*/;
					apNode[j]->pStart->nType |= PH7_TK_CSB /*']'*/;
				}

			}
		}else if (apNode[i]->pStart->nType & PH7_TK_CCB /*'}'*/){
			if( iBraces <= 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token '}'");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				return rc;
			}
			iBraces--;
		}else if ( apNode[i]->pStart->nType & PH7_TK_COLON ){
			if( iQuesty <= 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[i]->pStart->nLine,"Syntax error: Unexpected token ':'");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				return rc;
			}
			iQuesty--;
		}else if( apNode[i]->pStart->nType & PH7_TK_OP ){
			const ph7_expr_op *pOp = (const ph7_expr_op *)apNode[i]->pOp;
			if( pOp->iOp == EXPR_OP_QUESTY ){
				iQuesty++;
			}else if( i > 0 && (pOp->iOp == EXPR_OP_UMINUS || pOp->iOp == EXPR_OP_UPLUS)){
				if( apNode[i-1]->xCode == PH7_CompileVariable || apNode[i-1]->xCode == PH7_CompileLiteral ){
					sxi32 iExprOp = EXPR_OP_SUB; /* Binary minus */
					sxu32 n = 0;
					if( pOp->iOp == EXPR_OP_UPLUS ){
						iExprOp = EXPR_OP_ADD; /* Binary plus */
					}
					/*
					 * TICKET 1433-013: This is a fix around an obscure bug when the user uses
					 * a variable name which is an alpha-stream operator [i.e: $and,$xor,$eq..].
					 */
					while( n < SX_ARRAYSIZE(aOpTable) && aOpTable[n].iOp != iExprOp ){
						++n;
					}
					pOp = &aOpTable[n];
					/* Mark as binary '+' or '-',not an unary */
					apNode[i]->pOp = pOp;
					apNode[i]->pStart->pUserData = (void *)pOp;
				}
			}
		}
	}
	if( iParen != 0 || iSquare != 0 || iQuesty != 0 || iBraces != 0){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[0]->pStart->nLine,"Syntax error,mismatched '(','[','{' or '?'");
		if( rc != SXERR_ABORT ){
			rc = SXERR_SYNTAX;
		}
		return rc;
	}
	return SXRET_OK;
}
/*
 * Collect and assemble tokens holding a namespace path [i.e: namespace\to\const] 
 * or a simple literal [i.e: PHP_EOL].
 */
static void ExprAssembleLiteral(SyToken **ppCur,SyToken *pEnd)
{
	SyToken *pIn = *ppCur;
	/* Jump the first literal seen */
	if( (pIn->nType & PH7_TK_NSSEP) == 0 ){
		pIn++;
	}
	for(;;){
		if(pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){
			pIn++;
			if(pIn < pEnd && (pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
				pIn++;
			}
		}else{
			break;
		}
	}
	/* Synchronize pointers */
	*ppCur = pIn;
}
/*
 * Collect and assemble tokens holding annonymous functions/closure body.
 * When errors,PH7 take care of generating the appropriate error message.
 * Note on annonymous functions.
 *  According to the PHP language reference manual:
 *  Anonymous functions, also known as closures, allow the creation of functions
 *  which have no specified name. They are most useful as the value of callback
 *  parameters, but they have many other uses. 
 *  Closures may also inherit variables from the parent scope. Any such variables
 *  must be declared in the function header. Inheriting variables from the parent
 *  scope is not the same as using global variables. Global variables exist in the global scope
 *  which is the same no matter what function is executing. The parent scope of a closure is the 
 *  function in which the closure was declared (not necessarily the function it was called from).
 *
 * Some example:
 *  $greet = function($name)
 * {
 *   printf("Hello %s\r\n", $name);
 * };
 *  $greet('World');
 *  $greet('PHP');
 *
 * $double = function($a) {
 *   return $a * 2;
 * };
 * // This is our range of numbers
 * $numbers = range(1, 5);
 * // Use the Annonymous function as a callback here to 
 * // double the size of each element in our 
 * // range
 * $new_numbers = array_map($double, $numbers);
 * print implode(' ', $new_numbers);
 */
static sxi32 ExprAssembleAnnon(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd)
{
	SyToken *pIn = *ppCur;
	sxu32 nLine;
	sxi32 rc;
	/* Jump the 'function' keyword */
	nLine = pIn->nLine;
	pIn++;
	if( pIn < pEnd && (pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
		pIn++;
	}
	if( pIn >= pEnd || (pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing opening parenthesis '(' while declaring annonymous function");
		if( rc != SXERR_ABORT ){
			rc = SXERR_SYNTAX;
		}
		goto Synchronize;
	}
	pIn++; /* Jump the leading parenthesis '(' */
	PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);
	if( pIn >= pEnd || &pIn[1] >= pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");
		if( rc != SXERR_ABORT ){
			rc = SXERR_SYNTAX;
		}
		goto Synchronize;
	}
	pIn++; /* Jump the trailing parenthesis */
	if( pIn->nType & PH7_TK_KEYWORD ){
		sxu32 nKey = SX_PTR_TO_INT(pIn->pUserData);
		/* Check if we are dealing with a closure */
		if( nKey == PH7_TKWRD_USE ){
			pIn++; /* Jump the 'use' keyword */
			if( pIn >= pEnd || (pIn->nType & PH7_TK_LPAREN) == 0 ){
				/* Syntax error */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				goto Synchronize;
			}
			pIn++; /* Jump the leading parenthesis '(' */
			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pIn);
			if( pIn >= pEnd || &pIn[1] >= pEnd ){
				/* Syntax error */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				goto Synchronize;
			}
			pIn++;
		}else{
			/* Syntax error */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function");
			if( rc != SXERR_ABORT ){
				rc = SXERR_SYNTAX;
			}
			goto Synchronize;
		}
	}
	if( pIn->nType & PH7_TK_OCB /*'{'*/ ){
		pIn++; /* Jump the leading curly '{' */
		PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pIn);
		if( pIn < pEnd ){
			pIn++;
		}
	}else{
		/* Syntax error */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Syntax error while declaring annonymous function,missing '{'");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}			
	}
	rc = SXRET_OK;
Synchronize:
	/* Synchronize pointers */
	*ppCur = pIn;
	return rc;
}
/*
 * Extract a single expression node from the input.
 * On success store the freshly extractd node in ppNode.
 * When errors,PH7 take care of generating the appropriate error message.
 * An expression node can be a variable [i.e: $var],an operator [i.e: ++] 
 * an annonymous function [i.e: function(){ return "Hello"; }, a double/single
 * quoted string, a heredoc/nowdoc,a literal [i.e: PHP_EOL],a namespace path
 * [i.e: namespaces\path\to..],a array/list [i.e: array(4,5,6)] and so on.
 */
static sxi32 ExprExtractNode(ph7_gen_state *pGen,ph7_expr_node **ppNode)
{
	ph7_expr_node *pNode;
	SyToken *pCur;
	sxi32 rc;
	/* Allocate a new node */
	pNode = (ph7_expr_node *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_expr_node));
	if( pNode == 0 ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here.
		 */
		return SXERR_MEM;
	}
	/* Zero the structure */
	SyZero(pNode,sizeof(ph7_expr_node));
	SySetInit(&pNode->aNodeArgs,&pGen->pVm->sAllocator,sizeof(ph7_expr_node **));
	/* Point to the head of the token stream */
	pCur = pNode->pStart = pGen->pIn;
	/* Start collecting tokens */
	if( pCur->nType & PH7_TK_OP ){
		/* Point to the instance that describe this operator */
		pNode->pOp = (const ph7_expr_op *)pCur->pUserData;
		/* Advance the stream cursor */
		pCur++;
	}else if( pCur->nType & PH7_TK_DOLLAR ){
		/* Isolate variable */
		while( pCur < pGen->pEnd && (pCur->nType & PH7_TK_DOLLAR) ){
			pCur++; /* Variable variable */
		}
		if( pCur < pGen->pEnd ){
			if (pCur->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
				/* Variable name */
				pCur++;
			}else if( pCur->nType & PH7_TK_OCB /* '{' */ ){
				pCur++;
				/* Dynamic variable name,Collect until the next non nested '}' */
				PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_OCB, PH7_TK_CCB,&pCur);
				if( pCur < pGen->pEnd ){
					pCur++;
				}else{
					rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Missing closing brace '}'");
					if( rc != SXERR_ABORT ){
						rc = SXERR_SYNTAX;
					}
					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);
					return rc;
				}
			}
		}
		pNode->xCode = PH7_CompileVariable;
	 }else if( pCur->nType & PH7_TK_KEYWORD ){
		 sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pCur->pUserData);
		 if( nKeyword == PH7_TKWRD_ARRAY ||  nKeyword == PH7_TKWRD_LIST ){
			 /* List/Array node */
			 if( &pCur[1] >= pGen->pEnd || (pCur[1].nType & PH7_TK_LPAREN) == 0 ){
				 /* Assume a literal */
				 ExprAssembleLiteral(&pCur,pGen->pEnd);
				 pNode->xCode = PH7_CompileLiteral;
			 }else{
				 pCur += 2; 
				 /* Collect array/list tokens */
				 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN /* '(' */, PH7_TK_RPAREN /* ')' */,&pCur);
				 if( pCur < pGen->pEnd ){
					 pCur++;
				 }else{
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,
						 "%s: Missing closing parenthesis ')'",nKeyword == PH7_TKWRD_LIST ? "list" : "array");
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);
					 return rc; 
				 }
				 pNode->xCode = (nKeyword == PH7_TKWRD_LIST) ? PH7_CompileList : PH7_CompileArray;
				 if( pNode->xCode == PH7_CompileList ){
					 ph7_expr_op *pOp = (pCur < pGen->pEnd) ? (ph7_expr_op *)pCur->pUserData : 0;
					 if( pCur >= pGen->pEnd || (pCur->nType & PH7_TK_OP) == 0  || pOp == 0 || pOp->iVmOp != PH7_OP_STORE /*'='*/){
						 /* Syntax error */
						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"list(): expecting '=' after construct");
						 if( rc != SXERR_ABORT ){
							 rc = SXERR_SYNTAX;
						 }
						 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);
						 return rc; 
					 }
				 }
			 }
		 }else if( nKeyword == PH7_TKWRD_FUNCTION ){
			 /* Annonymous function */
			  if( &pCur[1] >= pGen->pEnd ){
				 /* Assume a literal */
				ExprAssembleLiteral(&pCur,pGen->pEnd);
				pNode->xCode = PH7_CompileLiteral;
			 }else{
				 /* Assemble annonymous functions body */
				 rc = ExprAssembleAnnon(&(*pGen),&pCur,pGen->pEnd);
				 if( rc != SXRET_OK ){
					 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);
					 return rc; 
				 }
				 pNode->xCode = PH7_CompileAnnonFunc;
			  }
		 }else if( PH7_IsLangConstruct(nKeyword,FALSE) == TRUE && &pCur[1] < pGen->pEnd ){
			 /* Language constructs [i.e: print,echo,die...] require special handling */
			 PH7_DelimitNestedTokens(pCur,pGen->pEnd,PH7_TK_LPAREN|PH7_TK_OCB|PH7_TK_OSB, PH7_TK_RPAREN|PH7_TK_CCB|PH7_TK_CSB,&pCur);
			 pNode->xCode = PH7_CompileLangConstruct;
		 }else{
			 /* Assume a literal */
			 ExprAssembleLiteral(&pCur,pGen->pEnd);
			 pNode->xCode = PH7_CompileLiteral;
		 }
	 }else if( pCur->nType & (PH7_TK_NSSEP|PH7_TK_ID) ){
		 /* Constants,function name,namespace path,class name... */
		 ExprAssembleLiteral(&pCur,pGen->pEnd);
		 pNode->xCode = PH7_CompileLiteral;
	 }else{
		 if( (pCur->nType & (PH7_TK_LPAREN|PH7_TK_RPAREN|PH7_TK_COMMA|PH7_TK_COLON|PH7_TK_CSB|PH7_TK_OCB|PH7_TK_CCB)) == 0 ){
			 /* Point to the code generator routine */
			 pNode->xCode = PH7_GetNodeHandler(pCur->nType);
			 if( pNode->xCode == 0 ){
				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Syntax error: Unexpected token '%z'",&pNode->pStart->sData);
				 if( rc != SXERR_ABORT ){
					 rc = SXERR_SYNTAX;
				 }
				 SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);
				 return rc;
			 }
		 }
		/* Advance the stream cursor */
		pCur++;
	 }
	/* Point to the end of the token stream */
	pNode->pEnd = pCur;
	/* Save the node for later processing */
	*ppNode = pNode;
	/* Synchronize cursors */
	pGen->pIn = pCur;
	return SXRET_OK;
}
/*
 * Point to the next expression that should be evaluated shortly.
 * The cursor stops when it hit a comma ',' or a semi-colon and the nesting
 * level is zero.
 */
PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext)
{
	SyToken *pCur = pStart;
	sxi32 iNest = 0;
	if( pCur >= pEnd || (pCur->nType & PH7_TK_SEMI/*';'*/) ){
		/* Last expression */
		return SXERR_EOF;
	}
	while( pCur < pEnd ){
		if( (pCur->nType & (PH7_TK_COMMA/*','*/|PH7_TK_SEMI/*';'*/)) && iNest <= 0){
			break;
		}
		if( pCur->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OSB/*'['*/|PH7_TK_OCB/*'{'*/) ){
			iNest++;
		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/|PH7_TK_CSB/*']'*/|PH7_TK_CCB/*'}*/) ){
			iNest--;
		}
		pCur++;
	}
	*ppNext = pCur;
	return SXRET_OK;
}
/*
 * Free an expression tree.
 */
static void ExprFreeTree(ph7_gen_state *pGen,ph7_expr_node *pNode)
{
	if( pNode->pLeft ){
		/* Release the left tree */
		ExprFreeTree(&(*pGen),pNode->pLeft);
	}
	if( pNode->pRight ){
		/* Release the right tree */
		ExprFreeTree(&(*pGen),pNode->pRight);
	}
	if( pNode->pCond ){
		/* Release the conditional tree used by the ternary operator */
		ExprFreeTree(&(*pGen),pNode->pCond);
	}
	if( SySetUsed(&pNode->aNodeArgs) > 0 ){
		ph7_expr_node **apArg;
		sxu32 n;
		/* Release node arguments */
		apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
		for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; ++n ){
			ExprFreeTree(&(*pGen),apArg[n]);
		}
		SySetRelease(&pNode->aNodeArgs);
	}
	/* Finally,release this node */
	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pNode);
}
/*
 * Free an expression tree.
 * This function is a wrapper around ExprFreeTree() defined above.
 */
PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet)
{
	ph7_expr_node **apNode;
	sxu32 n;
	apNode = (ph7_expr_node **)SySetBasePtr(pNodeSet);
	for( n = 0  ; n < SySetUsed(pNodeSet) ; ++n ){
		if( apNode[n] ){
			ExprFreeTree(&(*pGen),apNode[n]);
		}
	}
	return SXRET_OK;
}
/*
 * Check if the given node is a modifialbe l/r-value.
 * Return TRUE if modifiable.FALSE otherwise.
 */
static int ExprIsModifiableValue(ph7_expr_node *pNode,sxu8 bFunc)
{
	sxi32 iExprOp;
	if( pNode->pOp == 0 ){
		return pNode->xCode == PH7_CompileVariable ? TRUE : FALSE;
	}
	iExprOp = pNode->pOp->iOp;
	if( iExprOp == EXPR_OP_ARROW /*'->' */ || iExprOp == EXPR_OP_DC /*'::'*/ ){
			return TRUE;
	}
	if( iExprOp == EXPR_OP_SUBSCRIPT/*'[]'*/ ){
		if( pNode->pLeft->pOp ) {
			if( pNode->pLeft->pOp->iOp != EXPR_OP_SUBSCRIPT /*'['*/ && pNode->pLeft->pOp->iOp != EXPR_OP_ARROW /*'->'*/
				&& pNode->pLeft->pOp->iOp != EXPR_OP_DC /*'::'*/){
				return FALSE;
			}
		}else if( pNode->pLeft->xCode != PH7_CompileVariable ){
			return FALSE;
		}
		return TRUE;
	}
	if( bFunc && iExprOp == EXPR_OP_FUNC_CALL ){
		return TRUE;
	}
	/* Not a modifiable l or r-value */
	return FALSE;
}
/* Forward declaration */
static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken);
/* Macro to check if the given node is a terminal */
#define NODE_ISTERM(NODE) (apNode[NODE] && (!apNode[NODE]->pOp || apNode[NODE]->pLeft ))
/*
 * Buid an expression tree for each given function argument.
 * When errors,PH7 take care of generating the appropriate error message.
 */
static sxi32 ExprProcessFuncArguments(ph7_gen_state *pGen,ph7_expr_node *pOp,ph7_expr_node **apNode,sxi32 nToken)
{
	sxi32 iNest,iCur,iNode;
	sxi32 rc;
	/* Process function arguments from left to right */
	iCur = 0;
	for(;;){
		if( iCur >= nToken ){
			/* No more arguments to process */
			break;
		}
		iNode = iCur;
		iNest = 0;
		while( iCur < nToken ){
			if( apNode[iCur] ){
				if( (apNode[iCur]->pStart->nType & PH7_TK_COMMA) && apNode[iCur]->pLeft == 0 && iNest <= 0 ){
					break;
				}else if( apNode[iCur]->pStart->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
					iNest++;
				}else if( apNode[iCur]->pStart->nType & (PH7_TK_RPAREN|PH7_TK_CCB|PH7_TK_CSB) ){
					iNest--;
				}
			}
			iCur++;
		}
		if( iCur > iNode ){
			if( apNode[iNode] && (apNode[iNode]->pStart->nType & PH7_TK_AMPER /*'&'*/) && ((iCur - iNode) == 2)
				&& apNode[iNode+1]->xCode == PH7_CompileVariable ){
					PH7_GenCompileError(&(*pGen),E_WARNING,apNode[iNode]->pStart->nLine,
						"call-time pass-by-reference is depreceated");
					ExprFreeTree(&(*pGen),apNode[iNode]);
					apNode[iNode] = 0;
			}
			ExprMakeTree(&(*pGen),&apNode[iNode],iCur-iNode);
			if( apNode[iNode] ){
				/* Put a pointer to the root of the tree in the arguments set */
				SySetPut(&pOp->aNodeArgs,(const void *)&apNode[iNode]);
			}else{
				/* Empty function argument */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Empty function argument");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				return rc;
			}
		}else{
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");
			if( rc != SXERR_ABORT ){
				rc = SXERR_SYNTAX;
			}
			return rc;
		}
		/* Jump trailing comma */
		if( iCur < nToken && apNode[iCur] && (apNode[iCur]->pStart->nType & PH7_TK_COMMA) ){
			iCur++;
			if( iCur >= nToken ){
				/* missing function argument */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pOp->pStart->nLine,"Missing function argument");
				if( rc != SXERR_ABORT ){
					rc = SXERR_SYNTAX;
				}
				return rc;
			}
		}
	}
	return SXRET_OK;
}
 /*
  * Create an expression tree from an array of tokens.
  * If successful, the root of the tree is stored in apNode[0].
  * When errors,PH7 take care of generating the appropriate error message.
  */
 static sxi32 ExprMakeTree(ph7_gen_state *pGen,ph7_expr_node **apNode,sxi32 nToken)
 {
	 sxi32 i,iLeft,iRight;
	 ph7_expr_node *pNode;
	 sxi32 iCur;
	 sxi32 rc;
	 if( nToken <= 0 || (nToken == 1 && apNode[0]->xCode) ){
		 /* TICKET 1433-17: self evaluating node */
		 return SXRET_OK;
	 }
	 /* Process expressions enclosed in parenthesis first */
	 for( iCur =  0 ; iCur < nToken ; ++iCur ){
		 sxi32 iNest;
		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator
		  * since the LPAREN token can also be an operator [i.e: Function call].
		  */
		 if( apNode[iCur] == 0 || apNode[iCur]->pStart->nType != PH7_TK_LPAREN ){
			 continue;
		 }
		 iNest = 1;
		 iLeft = iCur;
		 /* Find the closing parenthesis */
		 iCur++;
		 while( iCur < nToken ){
			 if( apNode[iCur] ){
				 if( apNode[iCur]->pStart->nType & PH7_TK_RPAREN /* ')' */){
					 /* Decrement nesting level */
					 iNest--;
					 if( iNest <= 0 ){
						 break;
					 }
				 }else if( apNode[iCur]->pStart->nType & PH7_TK_LPAREN /* '(' */ ){
					 /* Increment nesting level */
					 iNest++;
				 }
			 }
			 iCur++;
		 }
		 if( iCur - iLeft > 1 ){
			 /* Recurse and process this expression */
			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);
			 if( rc != SXRET_OK ){
				 return rc;
			 }
		 }
		 /* Free the left and right nodes */
		 ExprFreeTree(&(*pGen),apNode[iLeft]);
		 ExprFreeTree(&(*pGen),apNode[iCur]);
		 apNode[iLeft] = 0;
		 apNode[iCur] = 0;
	 }
	  /* Process expressions enclosed in braces */
	 for( iCur =  0 ; iCur < nToken ; ++iCur ){
		 sxi32 iNest;
		 /* Note that, we use strict comparison here '!=' instead of the bitwise and '&' operator
		  * since the OCB '{' token can also be an operator [i.e: subscripting].
		  */
		 if( apNode[iCur] == 0 || apNode[iCur]->pStart->nType != PH7_TK_OCB ){
			 continue;
		 }
		 iNest = 1;
		 iLeft = iCur;
		 /* Find the closing parenthesis */
		 iCur++;
		 while( iCur < nToken ){
			 if( apNode[iCur] ){
				 if( apNode[iCur]->pStart->nType & PH7_TK_CCB/*'}'*/){
					 /* Decrement nesting level */
					 iNest--;
					 if( iNest <= 0 ){
						 break;
					 }
				 }else if( apNode[iCur]->pStart->nType & PH7_TK_OCB /*'{'*/ ){
					 /* Increment nesting level */
					 iNest++;
				 }
			 }
			 iCur++;
		 }
		 if( iCur - iLeft > 1 ){
			 /* Recurse and process this expression */
			 rc = ExprMakeTree(&(*pGen),&apNode[iLeft + 1],iCur - iLeft - 1);
			 if( rc != SXRET_OK ){
				 return rc;
			 }
		 }
		 /* Free the left and right nodes */
		 ExprFreeTree(&(*pGen),apNode[iLeft]);
		 ExprFreeTree(&(*pGen),apNode[iCur]);
		 apNode[iLeft] = 0;
		 apNode[iCur] = 0;
	 }
	 /* Handle postfix [i.e: function call,subscripting,member access] operators with precedence 2 */
	 iLeft = -1;
	 for( iCur = 0 ; iCur < nToken ; ++iCur ){
		 if( apNode[iCur] == 0 ){
			 continue;
		 }
		 pNode = apNode[iCur];
		 if( pNode->pOp && pNode->pOp->iPrec == 2 && pNode->pLeft == 0  ){
			 if( pNode->pOp->iOp == EXPR_OP_FUNC_CALL ){
				 /* Collect function arguments */
				 sxi32 iPtr = 0;
				 sxi32 nFuncTok = 0;
				 while( nFuncTok + iCur < nToken ){
					 if( apNode[nFuncTok+iCur] ){
						 if( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_LPAREN /*'('*/ ){
							 iPtr++;
						 }else if ( apNode[nFuncTok+iCur]->pStart->nType & PH7_TK_RPAREN /*')'*/){
							 iPtr--;
							 if( iPtr <= 0 ){
								 break;
							 }
						 }
					 }
					 nFuncTok++;
				 }
				 if( nFuncTok + iCur >= nToken ){
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Missing right parenthesis ')'");
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc; 
				 }
				 if(  iLeft < 0 || !NODE_ISTERM(iLeft) /*|| ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2)*/ ){
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid function name");
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc;
				 }
				 if( nFuncTok > 1 ){
					 /* Process function arguments */
					 rc = ExprProcessFuncArguments(&(*pGen),pNode,&apNode[iCur+1],nFuncTok-1);
					 if( rc != SXRET_OK ){
						 return rc;
					 }
				 }
				 /* Link the node to the tree */
				 pNode->pLeft = apNode[iLeft];
				 apNode[iLeft] = 0;
				 for( iPtr = 1; iPtr <= nFuncTok ; iPtr++ ){
					 apNode[iCur+iPtr] = 0;
				 }
			 }else if (pNode->pOp->iOp == EXPR_OP_SUBSCRIPT ){
				 /* Subscripting */
				 sxi32 iArrTok = iCur + 1;
				 sxi32 iNest = 1;
				 if(  iLeft < 0 || apNode[iLeft] == 0 || (apNode[iLeft]->pOp == 0 && (apNode[iLeft]->xCode != PH7_CompileVariable &&
					 apNode[iLeft]->xCode != PH7_CompileSimpleString && apNode[iLeft]->xCode != PH7_CompileString ) ) ||
					 ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* postfix */) ){
						 /* Syntax error */
						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"Invalid array name");
						 if( rc != SXERR_ABORT ){
							 rc = SXERR_SYNTAX;
						 }
						 return rc;
				 }
				 /* Collect index tokens */
				 while( iArrTok < nToken ){
					 if( apNode[iArrTok] ){
						 if( apNode[iArrTok]->pOp && apNode[iArrTok]->pOp->iOp == EXPR_OP_SUBSCRIPT &&  apNode[iArrTok]->pLeft == 0){
							 /* Increment nesting level */
							 iNest++;
						 }else if( apNode[iArrTok]->pStart->nType & PH7_TK_CSB /*']'*/){
							 /* Decrement nesting level */
							 iNest--;
							 if( iNest <= 0 ){
								 break;
							 }
						 }
					 }
					 ++iArrTok;
				 }
				 if( iArrTok > iCur + 1 ){
					 /* Recurse and process this expression */
					 rc = ExprMakeTree(&(*pGen),&apNode[iCur+1],iArrTok - iCur - 1);
					 if( rc != SXRET_OK ){
						 return rc;
					 }
					 /* Link the node to it's index */
					 SySetPut(&pNode->aNodeArgs,(const void *)&apNode[iCur+1]);
				 }
				 /* Link the node to the tree */
				 pNode->pLeft = apNode[iLeft];
				 pNode->pRight = 0;
				 apNode[iLeft] = 0;
				 for( iNest = iCur + 1 ; iNest <= iArrTok ; ++iNest ){
					 apNode[iNest] = 0;
				 }
			 }else{
				 /* Member access operators [i.e: '->','::'] */
				  iRight = iCur + 1;
				 while( iRight < nToken && apNode[iRight] == 0 ){
					 iRight++;
				 }
				 if( iRight >= nToken || iLeft < 0 || !NODE_ISTERM(iRight) || !NODE_ISTERM(iLeft) ){
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid member name",&pNode->pOp->sOp);
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc;
				 }
				 /* Link the node to the tree */
				 pNode->pLeft = apNode[iLeft];
				 if( pNode->pOp->iOp == EXPR_OP_ARROW /*'->'*/ && pNode->pLeft->pOp == 0 &&
					 pNode->pLeft->xCode != PH7_CompileVariable ){
						 /* Syntax error */
						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,
							 "'%z': Expecting a variable as left operand",&pNode->pOp->sOp);
						 if( rc != SXERR_ABORT ){
							 rc = SXERR_SYNTAX;
						 }
						 return rc;
				 }
				 pNode->pRight = apNode[iRight];
				 apNode[iLeft] = apNode[iRight] = 0;
			 }
		 }
		 iLeft = iCur;
	 }
	 /* Handle left associative (new, clone) operators */
	 for( iCur = 0 ; iCur < nToken ; ++iCur ){
		 if( apNode[iCur] == 0 ){
			 continue;
		 }
		 pNode = apNode[iCur];
		 if( pNode->pOp && pNode->pOp->iPrec == 1 && pNode->pLeft == 0 ){
			 SyToken *pToken;
			 /* Get the left node */
			 iLeft = iCur + 1;
			 while( iLeft < nToken && apNode[iLeft] == 0 ){
				 iLeft++;
			 }
			 if( iLeft >= nToken || !NODE_ISTERM(iLeft) ){
				  /* Syntax error */
				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Expecting class constructor call",
					 &pNode->pOp->sOp);
				 if( rc != SXERR_ABORT ){
					 rc = SXERR_SYNTAX;
				 }
				 return rc;
			 }
			 /* Make sure the operand are of a valid type */
			 if( pNode->pOp->iOp == EXPR_OP_CLONE ){
				 /* Clone:
				  * Symisc eXtension: 'clone' accepts now as it's left operand:
				  *  ++ function call (including annonymous)
				  *  ++ array member
				  *  ++ 'new' operator
				  * Example:
				  *   clone $pObj;
				  *   clone obj(); // function obj(){ return new Class(); }
				  *   clone $a['object']; // $a = array('object' => new Class());   
				  */
				 if( apNode[iLeft]->pOp == 0 ){
					 if( apNode[iLeft]->xCode != PH7_CompileVariable  ){
						 pToken = apNode[iLeft]->pStart;
						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Unexpected token '%z'",
							 &pNode->pOp->sOp,&pToken->sData);
						 if( rc != SXERR_ABORT ){
							 rc = SXERR_SYNTAX;
						 }
						 return rc; 
					 }
				 }
			 }else{
				 /* New */
				 if( apNode[iLeft]->pOp == 0 ){
					 ProcNodeConstruct xCons = apNode[iLeft]->xCode;
					 if( xCons != PH7_CompileVariable && xCons != PH7_CompileLiteral && xCons != PH7_CompileSimpleString){
						 pToken = apNode[iLeft]->pStart;
						 /* Syntax error */
						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,
							 "'%z': Unexpected token '%z', expecting literal, variable or constructor call",
							 &pNode->pOp->sOp,&pToken->sData);
						 if( rc != SXERR_ABORT ){
							 rc = SXERR_SYNTAX;
						 }
						 return rc; 
					 }
				 }
			 }
			  /* Link the node to the tree */
			 pNode->pLeft = apNode[iLeft];
			 apNode[iLeft] = 0;
			 pNode->pRight = 0; /* Paranoid */
		 }
	 }
	  /* Handle post/pre icrement/decrement [i.e: ++/--] operators with precedence 3 */
	 iLeft = -1;
	 for( iCur = 0 ; iCur < nToken ; ++iCur ){
		 if( apNode[iCur] == 0 ){
			 continue;
		 }
		 pNode = apNode[iCur];
		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){
			 if( iLeft >= 0 && ((apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec == 2 /* Postfix */)
				 || apNode[iLeft]->xCode == PH7_CompileVariable) ){
					 /* Link the node to the tree */
					 pNode->pLeft = apNode[iLeft];
					 apNode[iLeft] = 0; 
			 }
		  }
		 iLeft = iCur;
	  }
	 iLeft = -1;
	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){
		 if( apNode[iCur] == 0 ){
			 continue;
		 }
		 pNode = apNode[iCur];
		 if( pNode->pOp && pNode->pOp->iPrec == 3 && pNode->pLeft == 0){
			 if( iLeft < 0 || (apNode[iLeft]->pOp == 0 && apNode[iLeft]->xCode != PH7_CompileVariable)
				 || ( apNode[iLeft]->pOp && apNode[iLeft]->pOp->iPrec != 2 /* Postfix */) ){
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z' operator needs l-value",&pNode->pOp->sOp);
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc;
			 }
			 /* Link the node to the tree */
			 pNode->pLeft = apNode[iLeft];
			 apNode[iLeft] = 0;
			 /* Mark as pre-increment/decrement node */
			 pNode->iFlags |= EXPR_NODE_PRE_INCR;
		  }
		 iLeft = iCur;
	 }
	 /* Handle right associative unary and cast operators [i.e: !,(string),~...]  with precedence 4*/
	  iLeft = 0;
	  for( iCur = nToken -  1 ; iCur >= 0 ; iCur-- ){
		  if( apNode[iCur] ){
			  pNode = apNode[iCur];
			  if( pNode->pOp && pNode->pOp->iPrec == 4 && pNode->pLeft == 0){
				  if( iLeft > 0 ){
					  /* Link the node to the tree */
					  pNode->pLeft = apNode[iLeft];
					  apNode[iLeft] = 0;
					  if( pNode->pLeft && pNode->pLeft->pOp && pNode->pLeft->pOp->iPrec > 4 ){
						  if( pNode->pLeft->pLeft == 0 || pNode->pLeft->pRight == 0 ){
							   /* Syntax error */
							  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pLeft->pStart->nLine,"'%z': Missing operand",&pNode->pLeft->pOp->sOp);
							  if( rc != SXERR_ABORT ){
								  rc = SXERR_SYNTAX;
							  }
							  return rc;
						  }
					  }
				  }else{
					  /* Syntax error */
					  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing operand",&pNode->pOp->sOp);
					  if( rc != SXERR_ABORT ){
						  rc = SXERR_SYNTAX;
					  }
					  return rc;
				  }
			  }
			  /* Save terminal position */
			  iLeft = iCur;
		  }
	  }	 
	 /* Process left and non-associative binary operators [i.e: *,/,&&,||...]*/
	 for( i = 7 ; i < 17 ; i++ ){
		 iLeft = -1;
		 for( iCur = 0 ; iCur < nToken ; ++iCur ){
			 if( apNode[iCur] == 0 ){
				 continue;
			 }
			 pNode = apNode[iCur];
			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){
				 /* Get the right node */
				 iRight = iCur + 1;
				 while( iRight < nToken && apNode[iRight] == 0 ){
					 iRight++;
				 }
				 if( iRight >= nToken || iLeft < 0 || !NODE_ISTERM(iRight) || !NODE_ISTERM(iLeft) ){
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc; 
				 }
				 if( pNode->pOp->iOp == EXPR_OP_REF ){
					 sxi32  iTmp;
					 /* Reference operator [i.e: '&=' ]*/
					 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE || (apNode[iLeft]->pOp && apNode[iLeft]->pOp->iVmOp == PH7_OP_MEMBER /*->,::*/) ){
						 /* Left operand must be a modifiable l-value */
						 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'&': Left operand must be a modifiable l-value");
						 if( rc != SXERR_ABORT ){
							 rc = SXERR_SYNTAX;
						 }
						 return rc; 
					 }
					 if( apNode[iLeft]->pOp == 0 || apNode[iLeft]->pOp->iOp != EXPR_OP_SUBSCRIPT /*$a[] =& 14*/) {
						 if(  ExprIsModifiableValue(apNode[iRight],TRUE) == FALSE ){
							 if( apNode[iRight]->pOp == 0 ||  (apNode[iRight]->pOp->iOp != EXPR_OP_NEW /* new */ 
								 && apNode[iRight]->pOp->iOp != EXPR_OP_CLONE /* clone */) ){
									 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,
										 "Reference operator '&' require a variable not a constant expression as it's right operand");
									 if( rc != SXERR_ABORT ){
										 rc = SXERR_SYNTAX;
									 }
									 return rc; 
							 }
						 }
					 }
					 /* Swap operands */
					 iTmp = iRight;
					 iRight = iLeft;
					 iLeft = iTmp;
				 }
				 /* Link the node to the tree */
				 pNode->pLeft = apNode[iLeft];
				 pNode->pRight = apNode[iRight];
				 apNode[iLeft] = apNode[iRight] = 0;
			 }
			 iLeft = iCur;
		 }
	 }
	 /* Handle the ternary operator. (expr1) ? (expr2) : (expr3) 
	  * Note that we do not need a precedence loop here since
	  * we are dealing with a single operator.
	  */
	  iLeft = -1;
	  for( iCur = 0 ; iCur < nToken ; ++iCur ){
		  if( apNode[iCur] == 0 ){
			  continue;
		  }
		  pNode = apNode[iCur];
		  if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_QUESTY && pNode->pLeft == 0 ){
			  sxi32 iNest = 1;
			  if( iLeft < 0 || !NODE_ISTERM(iLeft) ){
				  /* Missing condition */
				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Syntax error",&pNode->pOp->sOp);
				  if( rc != SXERR_ABORT ){
					  rc = SXERR_SYNTAX;
				  }
				  return rc;
			  }
			  /* Get the right node */
			  iRight = iCur + 1;
			  while( iRight < nToken  ){
				  if( apNode[iRight] ){
					  if( apNode[iRight]->pOp && apNode[iRight]->pOp->iOp == EXPR_OP_QUESTY && apNode[iRight]->pCond == 0){
						  /* Increment nesting level */
						  ++iNest;
					  }else if( apNode[iRight]->pStart->nType & PH7_TK_COLON /*:*/ ){
						  /* Decrement nesting level */
						  --iNest;
						  if( iNest <= 0 ){
							  break;
						  }
					  }
				  }
				  iRight++;
			  }
			  if( iRight > iCur + 1 ){
				  /* Recurse and process the then expression */
				  rc = ExprMakeTree(&(*pGen),&apNode[iCur + 1],iRight - iCur - 1);
				  if( rc != SXRET_OK ){
					  return rc;
				  }
				  /* Link the node to the tree */
				  pNode->pLeft = apNode[iCur + 1];
			  }else{
				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'then' expression",&pNode->pOp->sOp);
				  if( rc != SXERR_ABORT ){
					 rc = SXERR_SYNTAX;
				 }
				 return rc;
			  }
			  apNode[iCur + 1] = 0;
			  if( iRight + 1 < nToken ){
				  /* Recurse and process the else expression */
				  rc = ExprMakeTree(&(*pGen),&apNode[iRight + 1],nToken - iRight - 1);
				  if( rc != SXRET_OK ){
					  return rc;
				  }
				  /* Link the node to the tree */
				  pNode->pRight = apNode[iRight + 1];
				  apNode[iRight + 1] =  apNode[iRight] = 0;
			  }else{
				  rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing 'else' expression",&pNode->pOp->sOp);
				  if( rc != SXERR_ABORT ){
					 rc = SXERR_SYNTAX;
				 }
				 return rc;
			  }
			  /* Point to the condition */
			  pNode->pCond  = apNode[iLeft];
			  apNode[iLeft] = 0;
			  break;
		  }
		  iLeft = iCur;
	  }
	 /* Process right associative binary operators [i.e: '=','+=','/='] 
	  * Note: All right associative binary operators have precedence 18
	  * so there is no need for a precedence loop here.
	  */
	 iRight = -1;
	 for( iCur = nToken -  1 ; iCur >= 0 ; iCur--){
		 if( apNode[iCur] == 0 ){
			 continue;
		 }
		 pNode = apNode[iCur];
		 if( pNode->pOp && pNode->pOp->iPrec == 18 && pNode->pLeft == 0 ){
			 /* Get the left node */
			 iLeft = iCur - 1;
			 while( iLeft >= 0 && apNode[iLeft] == 0 ){
				 iLeft--;
			 }
			 if( iLeft < 0 || iRight < 0 || !NODE_ISTERM(iRight) || !NODE_ISTERM(iLeft) ){
				 /* Syntax error */
				 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);
				 if( rc != SXERR_ABORT ){
					 rc = SXERR_SYNTAX;
				 }
				 return rc;
			 }
			 if( ExprIsModifiableValue(apNode[iLeft],FALSE) == FALSE ){
				 if( pNode->pOp->iVmOp != PH7_OP_STORE || apNode[iLeft]->xCode != PH7_CompileList ){
					 /* Left operand must be a modifiable l-value */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,
						 "'%z': Left operand must be a modifiable l-value",&pNode->pOp->sOp);
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc; 
				 }
			 }
			 /* Link the node to the tree (Reverse) */
			 pNode->pLeft = apNode[iRight];
			 pNode->pRight = apNode[iLeft];
			 apNode[iLeft] = apNode[iRight] = 0;
		 }
		 iRight = iCur;
	 }
	 /* Process left associative binary operators that have the lowest precedence [i.e: and,or,xor] */
	 for( i = 19 ; i < 23 ; i++ ){
		 iLeft = -1;
		 for( iCur = 0 ; iCur < nToken ; ++iCur ){
			 if( apNode[iCur] == 0 ){
				 continue;
			 }
			 pNode = apNode[iCur];
			 if( pNode->pOp && pNode->pOp->iPrec == i && pNode->pLeft == 0 ){
				 /* Get the right node */
				 iRight = iCur + 1;
				 while( iRight < nToken && apNode[iRight] == 0 ){
					 iRight++;
				 }
				 if( iRight >= nToken || iLeft < 0 || !NODE_ISTERM(iRight) || !NODE_ISTERM(iLeft) ){
					 /* Syntax error */
					 rc = PH7_GenCompileError(pGen,E_ERROR,pNode->pStart->nLine,"'%z': Missing/Invalid operand",&pNode->pOp->sOp);
					 if( rc != SXERR_ABORT ){
						 rc = SXERR_SYNTAX;
					 }
					 return rc;  
				 }
				 /* Link the node to the tree */
				 pNode->pLeft = apNode[iLeft];
				 pNode->pRight = apNode[iRight];
				 apNode[iLeft] = apNode[iRight] = 0;
			 }
			 iLeft = iCur;
		 }
	 }
	 /* Point to the root of the expression tree */
	 for( iCur = 1 ; iCur < nToken ; ++iCur ){
		 if( apNode[iCur] ){
			 if( (apNode[iCur]->pOp || apNode[iCur]->xCode ) && apNode[0] != 0){
				 rc = PH7_GenCompileError(pGen,E_ERROR,apNode[iCur]->pStart->nLine,"Unexpected token '%z'",&apNode[iCur]->pStart->sData);
				  if( rc != SXERR_ABORT ){
					  rc = SXERR_SYNTAX;
				  }
				  return rc;  
			 }
			 apNode[0] = apNode[iCur];
			 apNode[iCur] = 0;
		 }
	 }
	 return SXRET_OK;
 }
 /*
  * Build an expression tree from the freshly extracted raw tokens.
  * If successful, the root of the tree is stored in ppRoot.
  * When errors,PH7 take care of generating the appropriate error message.
  * This is the public interface used by the most code generator routines.
  */
PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot)
{
	ph7_expr_node **apNode;
	ph7_expr_node *pNode;
	sxi32 rc;
	/* Reset node container */
	SySetReset(pExprNode);
	pNode = 0; /* Prevent compiler warning */
	/* Extract nodes one after one until we hit the end of the input */
	while( pGen->pIn < pGen->pEnd ){
		rc = ExprExtractNode(&(*pGen),&pNode);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Save the extracted node */
		SySetPut(pExprNode,(const void *)&pNode);
	}
	if( SySetUsed(pExprNode) < 1 ){
		/* Empty expression [i.e: A semi-colon;] */
		*ppRoot = 0;
		return SXRET_OK;
	}
	apNode = (ph7_expr_node **)SySetBasePtr(pExprNode);
	/* Make sure we are dealing with valid nodes */
	rc = ExprVerifyNodes(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));
	if( rc != SXRET_OK ){
		/* Don't worry about freeing memory,upper layer will
		 * cleanup the mess left behind.
		 */
		*ppRoot = 0;
		return rc;
	}
	/* Build the tree */
	rc = ExprMakeTree(&(*pGen),apNode,(sxi32)SySetUsed(pExprNode));
	if( rc != SXRET_OK ){
		/* Something goes wrong [i.e: Syntax error] */
		*ppRoot = 0;
		return rc;
	}
	/* Point to the root of the tree */
	*ppRoot = apNode[0];
	return SXRET_OK;
}